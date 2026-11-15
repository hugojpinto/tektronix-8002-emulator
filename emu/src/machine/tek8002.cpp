// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A µProcessor Lab mainframe (see tek8002.h).
#include "machine/tek8002.h"
#include <cstdio>
#include <cstdlib>

tek8002::tek8002() : cpu(*this), m_sysram(0x4000, 0), m_progmem(4 * 0x4000, 0), m_sysbad(0x4000, false), m_progbad(4 * 0x4000, false) {}

// The bootstrap PROM (U6010, a 1702A) sits behind inverting address buffers
// (U6030/U6040): CPU address a reads PROM location 0xFF - a. Its published
// listing (070-2393 Fig. 6-2, bitsavers 800x/firmware/tekdos_boot.bin) is in
// PROM order, so it is reversed here to CPU order. Established by disassembly:
// reversed, it is clean 2650 code from the reset vector on (notes/boot-prom.md).
bool tek8002::load_boot_prom(const std::string &path, std::string &err)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) { err = path + ": cannot open"; return false; }
	std::vector<u8> p(256);
	size_t n = fread(p.data(), 1, p.size(), f);
	fclose(f);
	if (n != 256) { err = path + ": the bootstrap PROM is 256 bytes"; return false; }
	m_prom.assign(p.rbegin(), p.rend());
	return true;
}

void tek8002::power_on()
{
	m_pending = 0; m_bank = 0; m_serial_ctl = 0; m_par_ctl = 0x07; m_timer_ctl = 0;
	m_da_was = false; m_tbmt_was = true; m_flag_was = false;
	fdu.control(0x07);
	fdu.reset();
	debug.reset();
	asmproc.command(0xe7, 0x00);
	cpu.reset();
	update_irq();
}

void tek8002::restart()
{
	// SYSTEM RESTART resets the 2650 (it starts at 0000, the bootstrap PROM)
	m_pending = 0; update_irq();
	cpu.reset();
}

// --------------------------------------------------------------------------
// memory: A14 low selects System Memory; A14 high selects the program memory bank
// latched by port EE (070-2711 §3 "Program Memory Bank Select Logic")

u8 tek8002::peek(offs_t a) const
{
	a &= 0x7fff;
	if (a < 0x4000) {
		if (a < 0x100 && !m_prom.empty()) return m_prom[a];   // the PROM overrides the first 256 bytes of RAM
		return m_sysram[a];
	}
	const u16 pm = u16(m_bank * 0x4000 + (a & 0x3fff));
	u8 d;
	for (const auto *s : m_slaves) if (s->master_read(pm, d)) return d;   // an emulator board's PROM
	if (m_bank >= program_boards) return OPEN_BUS;           // no board answers
	return m_progmem[pm];
}

u8 tek8002::read(offs_t a)
{
	a &= 0x7fff;
	if (a < 0x4000) {
		if (!(a < 0x100 && !m_prom.empty()) && m_sysbad[a]) { raise(INT_SYSMEM_PARITY); update_irq(); }
	} else if (m_bank < program_boards && m_progbad[m_bank * 0x4000 + (a & 0x3fff)]) { raise(INT_PROGMEM_PARITY); update_irq(); }
	return peek(a);
}

void tek8002::write(offs_t a, u8 d)
{
	a &= 0x7fff;
	if (a < 0x4000) { m_sysram[a] = d; m_sysbad[a] = false; return; }   // writes under the PROM reach the RAM
	if (m_bank >= program_boards) return;
	const u32 i = m_bank * 0x4000 + (a & 0x3fff);
	m_progmem[i] = d; m_progbad[i] = false;
}

bool tek8002::parity_fault(bool program, u32 a)
{
	if (!program) { if (a >= 0x4000) return false; m_sysbad[a] = true; return true; }
	if (a >= 0x10000 || int(a >> 14) >= program_boards) return false;
	m_progbad[a] = true;
	return true;
}

// --------------------------------------------------------------------------
// I/O (070-2711 Tables 3-4, 3-5, 3-6, 3-8)

u8 tek8002::io_read(offs_t port)
{
	u8 v = OPEN_BUS;
	switch (port) {
	case 0xe8: v = term.read_data(); break;
	case 0xe9:                                               // UART status
		v = u8(0xe0 | (term.overrun() ? 0x04 : 0) | (term.framing() ? 0x08 : 0) | (term.tbmt() ? 0x02 : 0) | (term.da() ? 0x01 : 0));
		break;
	case 0xea:                                               // bit 7 FLAG; bit 1 printer fault (1 = no fault), bit 0 printer busy
		v = u8((fdu.flag() ? 0x80 : 0) | 0x02);
		break;
	case 0xeb: v = fdu.data_in(); break;
	default:
		if (debug.decodes(u8(port))) { v = debug.read(u8(port)); break; }
		if (syscomm.decodes(u8(port))) { v = syscomm.read(u8(port)); break; }
		{
			bool slave = false;
			for (auto *s : m_slaves) if (s->decodes(u8(port))) { v = s->status(u8(port)); slave = true; break; }
			if (slave) break;
		}
		if (trace_io) fprintf(stderr, "[%.4f IN  %03x -> unmapped (pc %04x)]\n", m_cycles / CPU_HZ, unsigned(port), cpu.ppc());
		return v;
	}
	if (trace_io && port != 0xe9 && port != 0xea) fprintf(stderr, "[%.4f IN  %02x -> %02x]\n", m_cycles / CPU_HZ, unsigned(port), v);
	return v;
}

void tek8002::io_write(offs_t port, u8 d)
{
	if (trace_io) fprintf(stderr, "[%.4f OUT %03x <- %02x (pc %04x)]\n", m_cycles / CPU_HZ, unsigned(port), d, cpu.ppc());
	switch (port) {
	case 0xe8: term.write_data(d); break;
	case 0xe9: m_serial_ctl = d & 0x07; break;
	case 0xea: m_par_ctl = d & 0x37; fdu.control(d & 0x07); break;
	case 0xeb: fdu.data_out(d); break;
	case 0xec: m_timer_ctl = d & 1; break;
	case 0xee: m_bank = d & 3; break;
	default:
		if (debug.decodes(u8(port))) { debug.write(u8(port), d); break; }
		if (syscomm.decodes(u8(port))) { syscomm.write(u8(port), d); break; }
		for (auto *s : m_slaves) if (s->decodes(u8(port))) s->command(u8(port), d);
		break;
	}
	update_irq();
}

// The 2650 takes the highest-priority (lowest-numbered) pending interrupt; the
// vector generator puts 2n on the bus during INTACK and the 2650 performs a
// ZBSR to page-zero address 2n, where the bootstrap PROM branches on through its
// vector table (070-2711 Table 3-1). The interrupt flip-flop is reset as its
// vector is taken.
int tek8002::intack()
{
	for (int n = 0; n < 32; n++)
		if (m_pending & (1u << n)) {
			m_pending &= ~(1u << n);
			update_irq();
			static const bool int_trace = getenv("TEK_INTTRACE") != nullptr;
			if (int_trace) fprintf(stderr, "[%.4f 2650 takes INT %d (vector %04x)]\n", m_cycles / CPU_HZ, n, 2 * n);
			return 2 * n;
		}
	update_irq();
	return 0;
}

// --------------------------------------------------------------------------

void tek8002::devices_tick(int c)
{
	term.tick(c);
	fdu.tick(c);
	syscomm.tick(c);
	// the ACIAs' IRQ outputs are levels: the interrupt is pending exactly while one is
	// asserted (a request the ACIA withdrew before INTACK is not taken: TEKDOS's printer
	// driver, which writes ahead into the transmit buffer, otherwise overwrote characters)
	for (int n = 0; n < tek8002_syscomm::PORTS; n++) {
		const u32 bit = 1u << (tek8002_syscomm::FIRST_INT + n);
		if (syscomm.irq(n)) m_pending |= bit; else m_pending &= ~bit;
	}
	// TTY interrupt flip-flops are clocked by DA and TBMT going high, when enabled (E9 bit 2)
	const bool tty_en = m_serial_ctl & 0x04;
	if (term.da() && !m_da_was && tty_en) raise(INT_CONSOLE_IN);
	if (term.tbmt() && !m_tbmt_was && tty_en) raise(INT_CONSOLE_OUT);
	m_da_was = term.da(); m_tbmt_was = term.tbmt();
	// the disc interrupt flip-flop is clocked by FLAG going low, when enabled (EA bit 4): the
	// controller holds FLAG from the last address (or data) byte to the end of the operation
	// (070-2587 microcode, notes/fdc.md "Stage C"), and TEKDOS enables the interrupt with that
	// FLAG already up (EA <- 37 at 2620) to be woken at the end
	if (!fdu.flag() && m_flag_was && (m_par_ctl & 0x10)) raise(INT_DISC);
	m_flag_was = fdu.flag();
	// interval timer: 10 ms or 100 ms from the baud-rate divider chain
	const u64 period = u64(CPU_HZ * (timer_100ms ? 0.100 : 0.010));
	while (m_cycles >= m_next_tick) {
		m_next_tick += period;
		if (m_timer_ctl & 1) raise(INT_TIMER);
	}
	// a HALT hands the bus to the active slave; with none to take it, the Debug module
	// reports SLAVE HALTED
	if (cpu.halted()) {
		if (tek8002_slave *s = active_slave()) {
			debug.run_slave(*s, c, CPU_HZ);
			bus_slave += u64(c);
			if (!s->halted()) run_slave += u64(c);
		} else debug.master_halted(c);
	} else { debug.master_running(); run_master += u64(c); }
	update_irq();
}

void tek8002::run_slice(int cycles)
{
	const int step = 50;   // device granularity, in 2650 clocks
	for (int done = 0; done < cycles; ) {
		cpu.run(step);
		const int used = step - cpu.icount();   // the last instruction may overrun the slice
		m_cycles += u64(used); done += used;
		devices_tick(used);
	}
}

// --------------------------------------------------------------------------
// slaves

tek8002_slave *tek8002::active_slave() const
{
	for (auto *s : m_slaves) if (s->active()) return s;
	return nullptr;
}

u8 tek8002::slave_read(u16 a, bool)
{
	debug.slave_access(a, false);
	const int board = a >> 14;
	if (board >= program_boards) return OPEN_BUS;
	if (m_progbad[a]) { raise(INT_PROGMEM_PARITY); update_irq(); }   // a Program Memory parity error, whoever reads
	return m_progmem[a];
}

void tek8002::slave_write(u16 a, u8 d)
{
	debug.slave_access(a, true);
	if ((a >> 14) < program_boards) { m_progmem[a] = d; m_progbad[a] = false; }
}

u8 tek8002::slave_in(u16 port)
{
	if (trace_io) fprintf(stderr, "[%.4f slave IN  %04x -> unmapped]\n", m_cycles / CPU_HZ, unsigned(port));
	return OPEN_BUS;
}

void tek8002::slave_out(u16 port, u8 d)
{
	if (debug.slave_out(port)) return;
	if (trace_io) fprintf(stderr, "[%.4f slave OUT %04x <- %02x unmapped]\n", m_cycles / CPU_HZ, unsigned(port), d);
}
