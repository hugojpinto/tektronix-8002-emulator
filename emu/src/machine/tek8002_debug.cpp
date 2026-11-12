// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Debug and Front Panel I/O module (see tek8002_debug.h).
#include "machine/tek8002_debug.h"

void tek8002_debug::write(u8 port, u8 d)
{
	switch (port) {
	case 0xf8: m_control = d; break;
	case 0xf9: m_command = d; break;     // forced reset/interrupt/jump act on the slave board, none yet
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
