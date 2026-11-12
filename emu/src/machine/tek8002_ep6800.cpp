// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 6800 Emulator Processor -- see tek8002_ep6800.h.
#include "machine/tek8002_ep6800.h"

// The SVC window answers while the Debug module takes SVCs (TEKDOS disables them in modes
// 1 and 2, where FEF0-FEF7 are the prototype's like any address). The 6800 has no
// instruction-fetch pin, so the prototype sees reads and writes only.
u8 tek8002_ep6800::read(u16 a, bool opcode)
{
	if ((a & 0xfff8) == SVC_BASE && m_bus.svcs_enabled()) { svc(a); return m_bus.slave_read(a, false); }
	if (proto_mem(a)) return proto_read(a, false);
	return m_bus.slave_read(a, opcode);
}

void tek8002_ep6800::write(u16 a, u8 d)
{
	if ((a & 0xfff8) == SVC_BASE && m_bus.svcs_enabled()) { svc(a); return; }
	if (proto_mem(a)) { proto_write(a, d); return; }
	m_bus.slave_write(a, d);
}

void tek8002_ep6800::target_lines(bool reset_edge, bool nmi_edge, bool irq)
{
	if (reset_edge) cpu.reset();
	if (nmi_edge) { cpu.set_input_line(INPUT_LINE_NMI, ASSERT_LINE); cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
	cpu.set_input_line(M6800_IRQ_LINE, irq ? ASSERT_LINE : CLEAR_LINE);
}

// The register block at 0000-0006: A, B, X (high, low), SP (high, low), CC (DSTAT 18E9-1937)
bool tek8002_ep6800::firmware(u16 entry)
{
	if (entry == FW_DUMP) {
		const u8 cc = cpu.cc();
		put(0, cpu.a()); put(1, cpu.b());
		put(2, u8(cpu.x() >> 8)); put(3, u8(cpu.x()));
		put(4, u8(cpu.sp() >> 8)); put(5, u8(cpu.sp()));
		put(6, cc);
		dump_block("dump", entry, 7);
	} else if (entry == FW_RESTORE) {
		dump_block("restore", entry, 7);
		cpu.set_a(mem(0)); cpu.set_b(mem(1));
		cpu.set_x(u16(mem(2) << 8 | mem(3)));
		cpu.set_sp(u16(mem(4) << 8 | mem(5)));
		cpu.set_cc(mem(6));
	} else if (entry == FW_PUT || entry == FW_GET) {
		get_put(entry == FW_PUT);
		return true;
	} else return false;
	svc(1);                                  // port F1: the debug SVC 1
	return true;
}
