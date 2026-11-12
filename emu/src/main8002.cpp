// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002: run the Tektronix 8002A µProcessor Lab; the system terminal is this terminal.
//   tek8002 [-prom FILE] [-0 IMG] [-1 IMG] [-2 IMG] [-3 IMG] [-rw] [-cycles N] [-type T TEXT]
//           [-rem SPEC] [-lpt SPEC] [-ppt SPEC] [-trace FILE] [-io] [-log FILE]
// SPEC: what is plugged into a System Communications port (host/serial_link.h):
// tcp:HOST:PORT, listen:PORT, unix:PATH, file:PATH (output only), loop
// Ctrl-] enters the monitor.
#include "machine/tek8002.h"
#include "host/serial_link.h"
#include "cpu/s2650/2650dasm.h"
#include "host/tty.h"
#include <cstdio>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <cstdlib>

struct cfg_t : s2650_disassembler::config { bool get_z80_mnemonics_mode() const override { return false; } } g_cfg;

static std::string dasm(tek8002 &m, u16 pc, int *len = nullptr)
{
	s2650_disassembler d(&g_cfg); data_buffer b;
	b.rd = [&](offs_t a) -> u8 { return m.read(a & 0x7fff); };
	std::ostringstream s; u32 r = d.disassemble(s, pc, b, b);
	if (len) *len = int(r & util::disasm_interface::LENGTHMASK);
	return s.str();
}

static void show_regs(tek8002 &m, FILE *out = stdout)
{
	auto &c = m.cpu;
	fprintf(out, "R0=%02x R1=%02x R2=%02x R3=%02x R1'=%02x R2'=%02x R3'=%02x PSU=%02x PSL=%02x PC=%04x %s INT=%08x cyc=%llu insn=%llu\r\n",
		c.reg(0), c.reg(1), c.reg(2), c.reg(3), c.reg(4), c.reg(5), c.reg(6), c.psu(), c.psl(), c.pc(),
		c.halted() ? "HALT" : "run", m.pending(), (unsigned long long)m.cycles(), (unsigned long long)c.insn_count());
	fprintf(out, "  %04x: %s\r\n", c.pc(), dasm(m, c.pc()).c_str());
}

static bool monitor(tek8002 &m)
{
	host::tty_raw(false);
	printf("\n[monitor] "); show_regs(m);
	char line[256];
	for (;;) {
		printf("mon> "); fflush(stdout);
		if (!fgets(line, sizeof line, stdin)) return false;
		unsigned a = 0, n = 0; int k = sscanf(line + 1, "%x %x", &a, &n);
		switch (line[0]) {
		case 'q': return false;
		case 'c': case '\n': host::tty_raw(true); return true;
		case 'r': show_regs(m); break;
		case 'd': { if (k < 1) a = m.cpu.pc(); if (k < 2) n = 64;
			for (unsigned i = 0; i < n; i++) { if (i % 16 == 0) printf("%s%04x:", i ? "\n" : "", (a + i) & 0x7fff); printf(" %02x", m.read((a + i) & 0x7fff)); }
			printf("\n"); break; }
		case 'u': { if (k < 1) a = m.cpu.pc(); if (k < 2) n = 16; u16 pc = u16(a);
			for (unsigned i = 0; i < n; i++) { int len; std::string s = dasm(m, pc, &len); printf("%04x: %s\n", pc, s.c_str()); pc += len ? len : 1; }
			break; }
		case 's': { if (k < 1) a = 1; for (unsigned i = 0; i < a; i++) m.cpu.run(1); show_regs(m); break; }
		case 'i': m.trace_io = !m.trace_io; printf("io trace %s\n", m.trace_io ? "on" : "off"); break;
		case 'R': m.restart(); show_regs(m); break;
		case 'D': m.debug.diag_switch(); printf("DIAG INTRPT\n"); break;
		case 'h': printf("c continue | q quit | r regs | d [addr [n]] | u [addr [n]] | s [n] step | i io-trace | R restart (SYSTEM RESTART) | D DIAG INTRPT\n"); break;
		default: printf("?\n");
		}
	}
}

// "\r" "\n" "\t" "\\" and "\xHH" in -type text
static std::string unescape(const std::string &t)
{
	std::string o;
	for (size_t i = 0; i < t.size(); i++) {
		if (t[i] != '\\' || i + 1 == t.size()) { o += t[i]; continue; }
		char c = t[++i];
		if (c == 'r') o += '\r'; else if (c == 'n') o += '\n'; else if (c == 't') o += '\t';
		else if (c == 'x' && i + 2 < t.size() + 1) { o += char(strtol(t.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
		else o += c;
	}
	return o;
}

int main(int argc, char **argv)
{
	std::string prom, tracefile, logfile, img[tek8002_fdu::DRIVES];
	long long max_cycles = -1; bool io = false, rw = false, fdu_trace = false; std::string fdc_mode = "hle";
	std::string link_spec[tek8002_syscomm::PORTS];
	struct typed { double t; std::string s; }; std::vector<typed> types;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
		if (a == "-prom") prom = next();
		else if (a.size() == 2 && a[0] == '-' && a[1] >= '0' && a[1] <= '3') img[a[1] - '0'] = next();
		else if (a == "-rw") rw = true;
		else if (a == "-cycles") max_cycles = atoll(next().c_str());
		else if (a == "-type") { double t = atof(next().c_str()); types.push_back({ t, unescape(next()) }); }
		else if (a == "-trace") tracefile = next();
		else if (a == "-log") logfile = next();
		else if (a == "-io") io = true;
		else if (a == "-rem") link_spec[tek8002_syscomm::MODEM] = next();
		else if (a == "-lpt") link_spec[tek8002_syscomm::PRINTER] = next();
		else if (a == "-ppt") link_spec[tek8002_syscomm::TAPE] = next();
		else if (a == "-fdutrace") fdu_trace = true;
		else if (a == "-fdc" && i + 1 < argc && (std::string(argv[i + 1]) == "hle" || std::string(argv[i + 1]) == "lle")) fdc_mode = next();
		else { fprintf(stderr, "usage: %s [-prom FILE] [-0..-3 IMG] [-rw] [-fdc hle|lle] [-cycles N] [-type SEC TEXT] [-rem SPEC] [-lpt SPEC] [-ppt SPEC] [-trace FILE] [-io] [-fdutrace] [-log FILE]\n", argv[0]); return 1; }
	}
	auto m = std::make_unique<tek8002>();
	std::string err;
	if (prom.empty())   // default: the repository's roms/, from the repository root or from emu/
		for (const char *p : { "roms/tekdos_boot.bin", "../roms/tekdos_boot.bin" })
			if (FILE *f = fopen(p, "rb")) { fclose(f); prom = p; break; }
	if (prom.empty()) { fprintf(stderr, "bootstrap PROM not found: give -prom FILE (roms/tekdos_boot.bin)\n"); return 1; }
	if (!m->load_boot_prom(prom, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
	if (fdc_mode == "lle") {
		// the disc controller's microprogram sits next to the bootstrap PROM (roms/fdc)
		const size_t slash = prom.find_last_of("/\\");
		const std::string dir = (slash == std::string::npos ? std::string(".") : prom.substr(0, slash)) + "/fdc";
		if (!m->fdu.use_controller(dir, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
		if (rw) fprintf(stderr, "[-fdc lle: discs are write protected until the controller's write path is modelled]\n");
	}
	for (int d = 0; d < tek8002_fdu::DRIVES; d++)
		if (!img[d].empty() && !m->fdu.mount(d, img[d], rw, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
	m->trace_io = io || getenv("TEK_IOTRACE"); m->fdu.trace = fdu_trace || getenv("TEK_FDCTRACE"); m->debug.trace = getenv("TEK_EPTRACE");
	FILE *logf = logfile.empty() ? nullptr : fopen(logfile.c_str(), "wb");
	m->term.on_tx = [&](u8 c) { c &= 0x7f; if (c) { fputc(c, stdout); fflush(stdout); } if (logf) { fputc(c, logf); fflush(logf); } };
	// the System Communications ports: modem (REMI/REMO), printer (LPT1), paper tape (PPTR/PPTP)
	static const char *const port_name[] = { "modem port", "printer port", "paper tape port" };
	serial_link link[tek8002_syscomm::PORTS];
	for (int n = 0; n < tek8002_syscomm::PORTS; n++) {
		if (link_spec[n].empty()) continue;
		if (!link[n].open(link_spec[n], err, port_name[n])) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
		m->syscomm.acia[n].on_tx = [&link, n](u8 c) { link[n].send(c); };
	}
	FILE *tf = tracefile.empty() ? nullptr : fopen(tracefile.c_str(), "w");
	if (tf) m->cpu.trace_hook = [&](u16 pc) { fprintf(tf, "%04x: %s\n", pc, dasm(*m, pc).c_str()); };

	m->power_on();
	bool interactive = host::is_tty(0) && max_cycles < 0;
	if (interactive) { host::tty_raw(true); fprintf(stderr, "[tek8002: Ctrl-] for monitor]\r\n"); }
	using clk = std::chrono::steady_clock;
	auto t0 = clk::now();
	const int slice = int(tek8002::CPU_HZ / 100);
	for (;;) {
		if (max_cycles >= 0 && (long long)m->cycles() >= max_cycles) break;
		const double now = m->cycles() / tek8002::CPU_HZ;
		for (auto it = types.begin(); it != types.end(); )
			if (it->t <= now) { for (char c : it->s) m->term.host_rx(u8(c)); it = types.erase(it); } else ++it;
		int k;
		while ((k = host::poll_key()) >= 0) {
			if (k == 035) { if (!monitor(*m)) goto done; continue; }
			m->term.host_rx(u8(k == '\n' ? '\r' : k));
		}
		if (k == -2 && !interactive && max_cycles < 0 && types.empty()) break;   // stdin ended: nothing more will be typed
		for (int n = 0; n < tek8002_syscomm::PORTS; n++) link[n].poll([&](u8 c) { m->syscomm.acia[n].host_rx(c); });
		m->run_slice(slice);
		if (interactive) {
			auto want = t0 + std::chrono::microseconds(long(m->cycles() / tek8002::CPU_HZ * 1e6));
			if (want > clk::now()) std::this_thread::sleep_until(want);
		}
	}
done:
	host::tty_raw(false);
	if (!interactive) { fprintf(stderr, "\n"); show_regs(*m, stderr); }   // the console text alone on stdout
	for (int d = 0; d < tek8002_fdu::DRIVES; d++) if (!m->fdu.save(d, err)) fprintf(stderr, "%s\n", err.c_str());
	if (tf) fclose(tf);
	if (logf) fclose(logf);
	return 0;
}
