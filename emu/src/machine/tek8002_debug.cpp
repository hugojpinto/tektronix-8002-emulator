// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Debug and Front Panel I/O module (see tek8002_debug.h).
#include "machine/tek8002_debug.h"
#include <cstdio>

void tek8002_debug::write(u8 port, u8 d)
{
	switch (port) {
	case 0xf8: m_control = d; break;
	case 0xf9:
		// forced reset and forced jump act on a 1 -> 0 transition (ztpe.nl Annex B, F9);
		// they are taken when the slave next runs (forced interrupt: no slave uses it)
		if ((m_command & 0x01) && !(d & 0x01)) m_reset_pending = true;
		if ((m_command & 0x04) && !(d & 0x04)) m_jump_pending = true;
		m_command = d;
		break;
	case 0xfa: m_jump_lo = d; break;
	case 0xfb: m_jump_hi = d; break;
	default:   m_bp[port - 0xfc] = d; break;
	}
}

u8 tek8002_debug::read(u8 port) const
{
	switch (port) {
	case 0xfc: return u8(~m_pc_last);
	case 0xfd: return u8(~m_pc_last >> 8);
	case 0xfe: return u8(~m_pc_next);
	case 0xff: return u8(~m_pc_next >> 8);
	default:   return 0xff;                // F8-FB are write-only
	}
}

int tek8002_debug::master_halted(int cycles)
{
	if (m_halt_reported) return -1;
	if ((m_halt_cycles += cycles) < io_clock_cycles) return -1;
	m_halt_reported = true;               // the detector sets once per hand-over
	return INT_SLAVE_HALTED;
}

bool tek8002_debug::slave_out(u16 port)
{
	const u8 p = u8(port);
	if (p < 0xf0 || p > 0xf7 || !(m_command & 0x10)) return false;
	m_event = INT_SVC1 + (0xf7 - p);       // F7 = SVC 1 ... F0 = SVC 8
	return true;
}

void tek8002_debug::run_slave(tek8002_slave &s, int cycles2650, double master_hz, debug_host &h)
{
	master_running();                      // a slave holds the bus: no "slave halted" from the detector
	if (h.master_interrupt_pending()) return;
	if (m_reset_pending) { m_reset_pending = false; s.reset(); }
	if (m_jump_pending) {
		m_jump_pending = false;
		if (trace) fprintf(stderr, "[debug: %s forced jump to %04x]\n", s.name(), jump_address());
		s.jump(jump_address());
	}
	m_frac += cycles2650 * (s.clock_hz() / master_hz);
	while (m_frac > 0) {
		const u64 c0 = s.cycles();
		const u16 pc = s.pc();
		m_event = -1;
		s.step();
		m_frac -= double(s.cycles() - c0);
		if (m_event < 0 && s.halted()) m_event = INT_SLAVE_HALTED;
		if (m_event >= 0) {
			m_pc_last = pc; m_pc_next = s.pc();
			// An SVC only pauses the slave: it continues after its OUT once the master halts
			// again (TEKDOS 3.3's SVC handler leaves the jump address alone; ASM works so).
			// The other debug interrupts arm a forced jump: the master sets the address
			// (ztpe.nl §8.2.2; with a halted slave there is nothing to continue)
			if (m_event < INT_SVC1 || m_event >= INT_SVC1 + 8) m_jump_pending = true;
			if (trace) fprintf(stderr, "[debug: %s interrupt %d at %04x, next %04x]\n", s.name(), m_event, pc, m_pc_next);
			h.debug_interrupt(m_event);
			m_event = -1;
			break;
		}
		if (h.master_interrupt_pending()) break;
	}
	if (m_frac > 0) m_frac = 0;
}
