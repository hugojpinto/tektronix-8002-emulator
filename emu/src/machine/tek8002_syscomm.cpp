// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A System Communications module -- see tek8002_syscomm.h.
#include "machine/tek8002_syscomm.h"

tek8002_syscomm::tek8002_syscomm()
{
	// 16 x 2400 baud from the Baud Rate Logic (070-2711 §5, the 2400 jumpers); in 1.25 MHz 2650 clocks
	for (auto &a : acia) { a.char_cycles = 5208; a.clock_hz = 38400; a.cpu_hz = 1.25e6; }
}

u8 tek8002_syscomm::read(u8 port)
{
	if (port == 0xc9) {
		const u8 v = u8(0xfe | (m_map[m_counter] ? 1 : 0));
		m_counter = (m_counter + 1) & 511;
		return v;
	}
	const int n = (port - 0xca) >> 1;
	return acia[n].read(port & 1);
}

void tek8002_syscomm::write(u8 port, u8 d)
{
	if (port == 0xc9) {
		if (d & 0x02) { m_counter = 0; return; }
		m_map[m_counter] = d & 1;
		m_counter = (m_counter + 1) & 511;
		return;
	}
	const int n = (port - 0xca) >> 1;
	acia[n].write(port & 1, d);
}
