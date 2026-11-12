// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A µProcessor Lab mainframe (see tek8002.h).
#include "machine/tek8002.h"
#include <cstdio>
#include <cstdlib>

tek8002::tek8002() : cpu(*this), m_sysram(0x4000, 0), m_progmem(4 * 0x4000, 0) {}

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
	debug.reset();
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

u8 tek8002::read(offs_t a)
{
	a &= 0x7fff;
	if (a < 0x4000) {
		if (a < 0x100 && !m_prom.empty()) return m_prom[a];   // the PROM overrides the first 256 bytes of RAM
		return m_sysram[a];
	}
	if (m_bank >= program_boards) return 0xff;               // no board answers
	return m_progmem[m_bank * 0x4000 + (a & 0x3fff)];
}

void tek8002::write(offs_t a, u8 d)
{
	a &= 0x7fff;
	if (a < 0x4000) { m_sysram[a] = d; return; }             // writes under the PROM reach the RAM
	if (m_bank >= program_boards) return;
	m_progmem[m_bank * 0x4000 + (a & 0x3fff)] = d;
}

// --------------------------------------------------------------------------
// I/O (070-2711 Tables 3-4, 3-5, 3-6, 3-8)

u8 tek8002::io_read(offs_t port)
{
	u8 v = 0xff;
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
		if (debug.decodes(u8(port))) debug.write(u8(port), d);
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
	// TTY interrupt flip-flops are clocked by DA and TBMT going high, when enabled (E9 bit 2)
	const bool tty_en = m_serial_ctl & 0x04;
	if (term.da() && !m_da_was && tty_en) raise(INT_CONSOLE_IN);
	if (term.tbmt() && !m_tbmt_was && tty_en) raise(INT_CONSOLE_OUT);
	m_da_was = term.da(); m_tbmt_was = term.tbmt();
	// the disc interrupt flip-flop is clocked by FLAG going high, when enabled (EA bit 4)
	if (fdu.flag() && !m_flag_was && (m_par_ctl & 0x10)) raise(INT_DISC);
	m_flag_was = fdu.flag();
	// interval timer: 10 ms or 100 ms from the baud-rate divider chain
	const u64 period = u64(CPU_HZ * (timer_100ms ? 0.100 : 0.010));
	while (m_cycles >= m_next_tick) {
		m_next_tick += period;
		if (m_timer_ctl & 1) raise(INT_TIMER);
	}
	// a HALT hands the bus to the slave; with none to take it, the Debug module reports SLAVE HALTED
	if (cpu.halted()) { const int n = debug.master_halted(c); if (n >= 0) raise(n); }
	else debug.master_running();
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
