// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002A Assembler Processor -- see tek8002_asmproc.h.
#include "machine/tek8002_asmproc.h"

tek8002_asmproc::tek8002_asmproc(slave_bus &bus) : tek8002_slave(bus) { reset(); }

void tek8002_asmproc::reset()
{
	const unsigned long cyc = cpu.cyc;
	z80_init(&cpu);
	cpu.cyc = cyc;                         // its clock keeps counting
	cpu.read_byte = &mem_rd;
	cpu.write_byte = &mem_wr;
	cpu.port_in = &port_in;
	cpu.port_out = &port_out;
	cpu.userdata = this;
}

void tek8002_asmproc::command(u8, u8 d)
{
	const bool was = active();
	m_cmd = d;
	if (active() && !was) reset();         // ACTIVE pulses the Z80's reset: it starts at 0000
}

u8 tek8002_asmproc::mem_rd(void *u, u16 a)
{
	tek8002_asmproc &s = *static_cast<tek8002_asmproc *>(u);
	return s.m_bus.slave_read(a, s.cpu.m1);
}

void tek8002_asmproc::mem_wr(void *u, u16 a, u8 d) { static_cast<tek8002_asmproc *>(u)->m_bus.slave_write(a, d); }
u8 tek8002_asmproc::port_in(z80 *z, u16 port) { return static_cast<tek8002_asmproc *>(z->userdata)->m_bus.slave_in(port); }
void tek8002_asmproc::port_out(z80 *z, u16 port, u8 d) { static_cast<tek8002_asmproc *>(z->userdata)->m_bus.slave_out(port, d); }
