// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 6800 Emulator Processor -- see tek8002_ep6800.h.
#include "machine/tek8002_ep6800.h"

u8 tek8002_ep6800::read(u16 a, bool opcode)
{
	if ((a & 0xfff8) == SVC_BASE) { if (m_svc < 0) m_svc = a & 7; return m_bus.slave_read(a, false); }
	return m_bus.slave_read(a, opcode);
}

void tek8002_ep6800::write(u16 a, u8 d)
{
	if ((a & 0xfff8) == SVC_BASE) { if (m_svc < 0) m_svc = a & 7; return; }
	m_bus.slave_write(a, d);
}

// The SYSTEM PROM's two routines (tek8002_ep6800.h), as one step each; then the board
// waits in the PROM. Their instruction timings are not known: a save or restore is
// counted as the 6800 instructions it would take (seven direct stores or loads, ~35 cycles).
void tek8002_ep6800::prom(u16 entry)
{
	if (entry == PROM_DUMP) {
		const u8 cc = cpu.cc();
		m_bus.slave_write(REGS + 0, cpu.a());
		m_bus.slave_write(REGS + 1, cpu.b());
		m_bus.slave_write(REGS + 2, u8(cpu.x() >> 8));
		m_bus.slave_write(REGS + 3, u8(cpu.x()));
		m_bus.slave_write(REGS + 4, u8(cpu.sp() >> 8));
		m_bus.slave_write(REGS + 5, u8(cpu.sp()));
		m_bus.slave_write(REGS + 6, cc);
	} else {
		cpu.set_a(m_bus.slave_read(REGS + 0, false));
		cpu.set_b(m_bus.slave_read(REGS + 1, false));
		cpu.set_x(u16(m_bus.slave_read(REGS + 2, false) << 8 | m_bus.slave_read(REGS + 3, false)));
		cpu.set_sp(u16(m_bus.slave_read(REGS + 4, false) << 8 | m_bus.slave_read(REGS + 5, false)));
		cpu.set_cc(m_bus.slave_read(REGS + 6, false));
	}
	m_svc = 1;                              // port F1: the debug SVC 1
	m_in_prom = true;
	m_prom_pc = entry;
	m_prom_cycles += 35;
}

void tek8002_ep6800::step()
{
	m_svc = -1;
	if (m_in_prom) m_prom_cycles += 4;     // its wait loop (BRA *)
	else if (!cpu.waiting() && (cpu.pc() == PROM_DUMP || cpu.pc() == PROM_RESTORE)) prom(cpu.pc());
	else cpu.step();
	if (m_svc >= 0) m_bus.slave_out(u16(0xf0 + m_svc), 0);
}
