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
			target->poll((unsigned long)(double(cpu_cycles()) * 4.0e6 / clock_hz()));
			const bool rs = target->reset_line(), n = target->nmi();
			target_lines(rs && !m_trst, n && !m_tnmi, target->irq());
			m_trst = rs; m_tnmi = n;
		}
		cpu_step();
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
