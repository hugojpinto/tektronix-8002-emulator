// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002's emulator processor boards -- see tek8002_emboard.h.
#include "machine/tek8002_emboard.h"
#include <cstdio>

void tek8002_emboard::step()
{
	m_svc = -1;
	if (m_in_prom) m_prom_cycles += 4;     // its wait loop
	else if (firmware_mapped() && firmware(cpu_pc())) {
		m_prom_pc = cpu_pc();
		m_in_prom = true;
		m_prom_cycles += 40;               // the routine (its real timing is not known)
	} else {
		if (proto_io() && target->connected()) {
			// the link's time is in 4 MHz T-states (its microseconds); each stretch is converted at
			// the clock it ran at (the board's own, or the prototype's under link v4)
			const u64 c = cpu_cycles();
			if (c >= m_link_last) m_link_cyc += double(c - m_link_last) * 4.0e6 / clock_hz();
			m_link_last = c;
			target->poll((unsigned long)m_link_cyc);
			const bool rs = target->reset_line(), n = target->nmi();
			const bool user = command_byte() & 0x10;          // control bit 4: the prototype's interrupts reach the processor
			target_lines(rs && !m_trst, user && n && !m_tnmi, user && target->irq());
			m_trst = rs; m_tnmi = n;
		}
		const u16 pc = cpu_pc();
		cpu_step();
		// a firmware routine that ran on the processor (a real PROM) ends with its debug SVC:
		// the board then waits there for the next forced jump, as the high-level routines do.
		// Only an SVC from the PROM: a program's own SVC while the firmware is mapped goes on
		// after the master halts again (the diagnostics' SVC test, DE310 subtest 8, with E3 <- 94)
		if (m_svc >= 0 && firmware_mapped() && firmware_code(pc)) { m_prom_pc = cpu_pc(); m_in_prom = true; }
	}
	if (m_svc >= 0) m_bus.slave_out(u16(0xf0 + m_svc), 0);
}

// GET/PUT (tek8002_emboard.h): the prototype's byte at block[1]:block[2] to block[0], or
// block[0] to it with a read-back; block[3] = 00 when done, 01 when a PUT does not read back
void tek8002_emboard::get_put(bool put_entry)
{
	const u16 a = u16(mem(1) << 8 | mem(2));
	dump_block(put_entry ? "put" : "get", a, 4);
	if (put_entry) {
		const u8 d = mem(0);
		target->mem_write(a, d);
		put(3, target->mem_read(a) == d ? 0x00 : 0x01);
	} else {
		put(0, target->mem_read(a));
		put(3, 0x00);
	}
	svc(0);                                  // port F0: the debug SVC 2
}

void tek8002_emboard::dump_block(const char *what, u16 entry, int size)
{
	if (!trace) return;
	fprintf(stderr, "[%s firmware %04x %s:", name(), entry, what);
	for (int i = 0; i < size; i++) fprintf(stderr, " %02x", mem(u16(i)));
	fprintf(stderr, "]\n");
}
