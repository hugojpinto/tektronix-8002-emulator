// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 2650 Emulator Processor -- see tek8002_ep2650.h.
#include "machine/tek8002_ep2650.h"

tek8002_ep2650::tek8002_ep2650(slave_bus &bus) : tek8002_emboard(bus, 0xe0), cpu(*this)
{
	// the 2650 has no fetch pin: the first read at the instruction's address is its fetch
	cpu.exec_hook = [this](u16 pc) { m_fetch_at = pc; };
}

// the 2650's two-cycle instructions (from the core's timings: m_icount -= 6)
const u8 tek8002_ep2650::two_cycle[32] = { 0xff, 0x00, 0x0c, 0x00, 0xff, 0x00, 0x0f, 0x00, 0xff, 0x00, 0x0f, 0x00, 0xff, 0x00, 0x0f, 0x00, 0xff, 0x00, 0x0c, 0x00, 0xff, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0xff, 0x00, 0x0f, 0x00 };

void tek8002_ep2650::finish_fetched()
{
	if (cpu.halted()) return;
	const u8 op = m_bus.slave_read(cpu.pc(), false);
	if (two_cycle[op >> 3] & (1 << (op & 7))) cpu_step();
}

void tek8002_ep2650::cpu_reset()
{
	cpu.reset_pin();
}

u8 tek8002_ep2650::read(offs_t a)
{
	const u16 ad = u16(a & 0x7fff);
	const bool fetch = int(ad) == m_fetch_at;
	if (fetch) m_fetch_at = -1;
	if (proto_mem(ad)) return proto_read(ad, fetch);
	return m_bus.slave_read(ad, fetch);
}

void tek8002_ep2650::write(offs_t a, u8 d)
{
	const u16 ad = u16(a & 0x7fff);
	if (proto_mem(ad)) { proto_write(ad, d); return; }
	m_bus.slave_write(ad, d);
}

// extended I/O (REDE/WRTE) carries a port number; the non-extended control and data ports
// (REDC/WRTC, REDD/WRTD) arrive as 0x100/0x101
u8 tek8002_ep2650::io_read(offs_t port)
{
	if (port >= 0x100) return 0xff;
	if (proto_io()) return proto_in(u16(port));
	return m_bus.slave_in(u16(port));
}

void tek8002_ep2650::io_write(offs_t port, u8 d)
{
	if (port >= 0x100) return;
	if ((port & 0xf8) == 0xf0 && m_bus.svcs_enabled()) svc(int(port), u16(port), d);
	else if (proto_io()) proto_out(u16(port), d);
	else m_bus.slave_out(u16(port), d);
}

void tek8002_ep2650::target_lines(bool reset_edge, bool, bool irq)
{
	if (reset_edge) cpu.reset();
	cpu.set_irq(irq ? 1 : 0);
}
