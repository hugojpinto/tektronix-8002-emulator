// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002A µProcessor Lab as one system -- see tek8002_system.h.
#include "host/paths.h"
#include "system/tek8002_system.h"
#include "cpu/s2650/2650dasm.h"
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace {
struct cfg_t : s2650_disassembler::config { bool get_z80_mnemonics_mode() const override { return false; } } g_cfg;
}

tek8002_system::~tek8002_system()
{
	if (logf) fclose(logf);
	if (tracef) fclose(tracef);
}

std::string tek8002_system::dasm(u16 pc, int *len)
{
	s2650_disassembler d(&g_cfg); data_buffer b;
	b.rd = [&](offs_t a) -> u8 { return m->peek(a); };
	std::ostringstream s; u32 r = d.disassemble(s, pc, b, b);
	if (len) *len = int(r & util::disasm_interface::LENGTHMASK);
	return s.str();
}

void tek8002_system::show(FILE *f)
{
	auto &c = m->cpu;
	fprintf(f, "R0=%02x R1=%02x R2=%02x R3=%02x R1'=%02x R2'=%02x R3'=%02x PSU=%02x PSL=%02x PC=%04x %s INT=%08x cyc=%llu insn=%llu\r\n",
		c.reg(0), c.reg(1), c.reg(2), c.reg(3), c.reg(4), c.reg(5), c.reg(6), c.psu(), c.psl(), c.pc(),
		c.halted() ? "HALT" : "run", m->pending(), (unsigned long long)m->cycles(), (unsigned long long)c.insn_count());
	fprintf(f, "  %04x: %s\r\n", c.pc(), dasm(c.pc()).c_str());
}

bool tek8002_system::start(std::string &err)
{
	std::string prom = opt.prom;
	if (prom.empty())   // default: the repository's roms/, from the repository root or from emu/, or beside the program
		for (const std::string &p : { std::string("roms/tekdos_boot.bin"), host::data_path("../roms/tekdos_boot.bin") })
			if (FILE *f = fopen(p.c_str(), "rb")) { fclose(f); prom = p; break; }
	if (prom.empty()) { err = "bootstrap PROM not found: give -prom FILE (roms/tekdos_boot.bin)"; return false; }
	if (!m->load_boot_prom(prom, err)) return false;
	// the disc controller's microprogram and the 6800 board's PROM sit next to the bootstrap
	// PROM (roms/fdc, roms/ep6800, roms/ep8080)
	const size_t slash = prom.find_last_of("/\\");
	const std::string roms = slash == std::string::npos ? std::string(".") : prom.substr(0, slash);
	if (!opt.fdc_hle && !m->fdu.use_controller(roms + "/fdc", err)) return false;
	if (!opt.ep6800_hle && !m->ep6800.load_firmware(roms + "/ep6800", err)) return false;
	if (!m->ep8080.load_firmware(roms + "/ep8080", err)) return false;
	if (!opt.epz80_hle && !m->epz80.load_firmware(roms + "/epz80", err)) return false;
	for (int d = 0; d < tek8002_fdu::DRIVES; d++)
		if (!opt.disks[d].empty() && !mount(d, opt.disks[d], err)) return false;
	// the prototype, for the emulator boards' modes 1 and 2 (notes/prototype-link.md)
	if (!opt.targetspec.empty()) {
		if (!target.open(opt.targetspec, err)) return false;
		m->ep8080.target = m->ep6800.target = m->epz80.target = &target;
	}
	m->trace_io = opt.io || getenv("TEK_IOTRACE");
	m->fdu.trace = opt.fdutrace || getenv("TEK_FDCTRACE");
	m->debug.trace = getenv("TEK_EPTRACE");
	m->ep8080.trace = m->ep6800.trace = m->epz80.trace = getenv("TEK_EPTRACE");
	if (!opt.logfile.empty() && !(logf = fopen(opt.logfile.c_str(), "wb"))) { err = opt.logfile + ": cannot write"; return false; }
	if (!opt.tracefile.empty()) {
		if (!(tracef = fopen(opt.tracefile.c_str(), "w"))) { err = opt.tracefile + ": cannot write"; return false; }
		m->cpu.trace_hook = [this](u16 pc) { fprintf(tracef, "%04x: %s\n", pc, dasm(pc).c_str()); };
	}
	// the terminal port: the CT8100 sees every character; so does the front end
	ct.on_key = [this](uint8_t c) { key_in(c); };
	ct.tty_lock = opt.tty_lock;
	m->term.on_tx = [this](u8 c) {
		c &= 0x7f;
		if (!c) return;
		ct.receive(c);
		terminal_dirty = true;
		if (on_term_tx) on_term_tx(c);
		if (logf) { fputc(c, logf); fflush(logf); }
		// TEKDOS's prompt: "> " at the start of a line
		if (c != 0x7f) { m_last2[0] = m_last2[1]; m_last2[1] = char(c); m_prompt_seen = m_last2[0] == '>' && m_last2[1] == ' '; m_prompt_t = now(); }
	};
	// the System Communications ports: modem (REMI/REMO), printer (LPT1), paper tape (PPTR/PPTP)
	static const char *const port_name[] = { "modem port", "printer port", "paper tape port" };
	const std::string *spec[] = { &opt.remspec, &opt.lptspec, &opt.pptspec };
	for (int n = 0; n < tek8002_syscomm::PORTS; n++) {
		if (spec[n]->empty()) continue;
		if (!link[n].open(*spec[n], err, port_name[n])) return false;
		mc6850 &acia = m->syscomm.acia[n];
		acia.on_tx = [this, n](u8 c) { link[n].send(c); };
		if (link[n].loopback()) {
			// the wrap-back connector (the diagnostics' test fixture): RTS back to CTS and DCD, at
			// once; and nothing paces what comes back, so the receiver can overrun as on the chip
			acia.on_control = [this, n] { loop_lines(n); };
			acia.pace_rx = false;
		}
	}
	for (auto &t : opt.typed) if (t.at < 0) { m_prompt_typed.push_back(t); m_prompts_used = true; }
	out.set("8002:power", 1); out.set("fdu:power", 1); out.set("8002:restart", 0); out.set("8002:diag", 0);
	m->power_on();
	return true;
}

bool tek8002_system::timed_typing_left() const
{
	return std::any_of(opt.typed.begin(), opt.typed.end(), [](const tek8002_options::timed_t &x) { return x.at >= 0; });
}

bool tek8002_system::finished() const
{
	if (opt.max_cycles >= 0 && (long long)clock() >= opt.max_cycles) return true;
	return batch && m_script_over;
}

void tek8002_system::tick()
{
	scripted_events();
	for (int n = 0; n < tek8002_syscomm::PORTS; n++) {
		mc6850 &acia = m->syscomm.acia[n];
		link[n].poll([&acia](u8 c) { acia.host_rx(c); });
		if (!link[n].loopback()) continue;
		// a break held by the transmitter comes back as one null with a framing error: the
		// receiver then waits for the line to return to mark before it looks for a start bit
		if (acia.sending_break() && !m_breaking[n]) acia.host_break();
		m_breaking[n] = acia.sending_break();
	}
	if (m_power && !m_held) m->run_slice(SLICE);
	else m_held_cycles += SLICE;
	update_outputs(SLICE);
}

void tek8002_system::scripted_events()
{
	const double t = now();
	for (auto it = opt.typed.begin(); it != opt.typed.end(); )
		if (it->at >= 0 && it->at <= t) { for (char c : it->text) key_in(u8(c)); it = opt.typed.erase(it); } else ++it;
	// at a prompt that has stood for 0.1 s: the next -type prompt text; after the last one,
	// a batch run ends at the next prompt
	if (m_prompt_seen && t - m_prompt_t > 0.1) {
		if (!m_prompt_typed.empty()) {
			for (char c : m_prompt_typed.front().text) key_in(u8(c));
			m_prompt_typed.erase(m_prompt_typed.begin());
			m_prompt_seen = false;
		} else if (m_prompts_used && std::none_of(opt.typed.begin(), opt.typed.end(), [](const tek8002_options::timed_t &x) { return x.at >= 0; }))
			m_script_over = true;
	}
	for (auto it = opt.snaps.begin(); it != opt.snaps.end(); )
		if (it->at <= t) {
			if (FILE *f = fopen(it->text.c_str(), "w")) { fputs(ct.text().c_str(), f); fclose(f); }
			it = opt.snaps.erase(it);
		} else ++it;
	for (auto it = opt.switches.begin(); it != opt.switches.end(); )
		if (it->at <= t) {
			const size_t eq = it->text.find('=');
			if (eq == std::string::npos) press(it->text);
			else input(it->text.substr(0, eq), atoi(it->text.c_str() + eq + 1));
			it = opt.switches.erase(it);
		} else ++it;
	for (auto it = opt.parity.begin(); it != opt.parity.end(); )
		if (it->at <= t) {
			const bool u = it->text.compare(0, 2, "U:") == 0 || it->text.compare(0, 2, "u:") == 0;
			const u32 a = u32(strtoul(it->text.c_str() + (u ? 2 : 0), nullptr, 16));
			if (!m->parity_fault(u, a)) fprintf(stderr, "[-parity %s: no memory there]\n", it->text.c_str());
			it = opt.parity.erase(it);
		} else ++it;
	for (auto it = opt.discs.begin(); it != opt.discs.end(); )
		if (it->at <= t) {
			std::string err;
			const size_t eq = it->text.find('=');
			const int d = atoi(it->text.c_str());
			if (eq == std::string::npos ? !eject(d, err) : !mount(d, it->text.substr(eq + 1), err)) fprintf(stderr, "[drive %d: %s]\n", d, err.c_str());
			it = opt.discs.erase(it);
		} else ++it;
	for (auto it = m_releases.begin(); it != m_releases.end(); )
		if (it->at <= t) { input(it->name, 0); it = m_releases.erase(it); } else ++it;
}

// The front panel lamps (070-2711 schematics 9 and 4A/4B; notes/8002.md "Front panel lamps"):
// SYS = the MSTR/SLV flip-flop gives the bus to the System Processor; RUN = the bus line RUN
// (a processor runs) while HOLD is not active (no bus hold is modelled); PWR = +5.2 V. Each
// is the state over the slice. Dark SYS and RUN while SYSTEM RESTART is held: inferred.
void tek8002_system::update_outputs(int slice)
{
	const u64 mr = m->run_master - m_last_master, bs = m->bus_slave - m_last_bus, sr = m->run_slave - m_last_slave;
	m_last_master = m->run_master; m_last_bus = m->bus_slave; m_last_slave = m->run_slave;
	const bool on = m_power && !m_held;
	out.set("8002:pwr", m_power);
	out.set("8002:sys", on && bs * 2 < u64(slice));
	out.set("8002:run", on && (mr + sr) * 2 >= u64(slice));
	out.set("fdu:pwr", m->fdu.powered());
	for (int d = 0; d < tek8002_fdu::DRIVES; d++) out.set("fdu:drive" + std::to_string(d), m->fdu.activity(d));
}

// A wrap-back connector on System Communications port n (the diagnostics' test fixture) ties RTS
// to CTS and DCD. The port's strap (inferred from the diagnostics' SYS COMM test, which asks
// for position A, then B, and ends in B): A passes CTS from the connector, B holds it active.
void tek8002_system::loop_lines(int n)
{
	if (!link[n].loopback()) return;
	mc6850 &acia = m->syscomm.acia[n];
	acia.set_cts(m_strap_a[n] && acia.rts());
	acia.set_dcd(acia.rts());
}

bool tek8002_system::input(const std::string &name, int value)
{
	if (name == "8002:power") {
		if (bool(value) == m_power) return true;
		m_power = value;
		if (m_power) m->power_on();                   // power-up: the bootstrap PROM runs again
	} else if (name == "fdu:power") {
		m->fdu.set_power(value);
	} else if (name == "8002:restart") {
		if (m_held && !value) m->restart();           // the 2650 starts when SYSTEM RESTART is let go
		m_held = value;
	} else if (name == "8002:diag") {
		if (value && !out.get(name)) m->debug.diag_switch();
	} else if (name.size() == 14 && name.compare(0, 13, "syscomm:strap") == 0 && name[13] >= '1' && name[13] <= '3') {
		m_strap_a[name[13] - '1'] = value;
		loop_lines(name[13] - '1');
	} else return false;
	out.set(name, value);
	return true;
}

bool tek8002_system::press(const std::string &name, double seconds)
{
	if (!input(name, 1)) return false;
	release_at(name, now() + seconds);
	return true;
}

void tek8002_system::release_at(const std::string &name, double at) { m_releases.push_back({ std::max(at, now()), name }); }

bool tek8002_system::mount(int d, const std::string &path, std::string &err)
{
	if (d < 0 || d >= tek8002_fdu::DRIVES) { err = "drive must be 0-3"; return false; }
	if (!m_disc[d].empty() && !eject(d, err)) return false;
	if (!m->fdu.mount(d, path, opt.writeback, err)) return false;
	m_disc[d] = path.compare(0, 4, "new:") == 0 ? path.substr(4) : path;
	return true;
}

bool tek8002_system::eject(int d, std::string &err)
{
	if (d < 0 || d >= tek8002_fdu::DRIVES) { err = "drive must be 0-3"; return false; }
	if (m_disc[d].empty()) return true;
	const bool ok = m->fdu.save(d, err);
	m->fdu.eject(d);
	m_disc[d].clear();
	return ok;
}

void tek8002_system::shutdown(bool show_state)
{
	if (show_state) { fprintf(stderr, "\n"); show(stderr); }
	std::string err;
	for (int d = 0; d < tek8002_fdu::DRIVES; d++) if (!m->fdu.save(d, err)) fprintf(stderr, "%s\n", err.c_str());
	if (tracef) { fclose(tracef); tracef = nullptr; }
	if (logf) { fclose(logf); logf = nullptr; }
}
