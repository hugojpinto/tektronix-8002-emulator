// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// General Instrument AY-5-1013 UART (programmer's model), host side as byte queues.
// The chip has no registers of its own: the board wires its status outputs to an
// input port and its "reset data available" (RDA) to the data read strobe.
#ifndef TEK_AY31015_H
#define TEK_AY31015_H
#include "emu.h"
#include <deque>
#include <functional>

class ay31015
{
public:
	// status outputs, in the order the 8002 System Processor puts them on port E9
	// (8001/8002A service manual 070-2711, Table 3-5)
	bool da() const { return m_da; }            // data available
	bool tbmt() const { return m_tbmt; }        // transmitter buffer empty
	bool overrun() const { return m_or; }
	bool framing() const { return m_fe; }

	u8 read_data() { m_da = false; m_or = false; return m_rdr; }   // reading the data also pulses RDA
	void write_data(u8 d) { m_tdr = d; m_tbmt = false; m_tx_timer = char_cycles; }

	void tick(int cycles)
	{
		if (m_tx_timer > 0 && (m_tx_timer -= cycles) <= 0) {
			m_tx_timer = 0; m_tbmt = true;
			if (on_tx) on_tx(m_tdr);
		}
		if (m_rx_gap > 0) m_rx_gap -= cycles;
		if (!m_rxq.empty() && m_rx_gap <= 0) {
			if (m_da) { if (pace_rx) return; m_or = true; }
			m_rdr = m_rxq.front(); m_rxq.pop_front();
			m_da = true; m_rx_gap = char_cycles;
		}
	}
	void host_rx(u8 c) { m_rxq.push_back(c); }
	bool rx_pending() const { return !m_rxq.empty(); }

	std::function<void(u8)> on_tx;
	int  char_cycles = 5208;   // 10 bits at 2400 baud, in 1.25 MHz 2650 clocks
	bool pace_rx = true;       // host input waits for the program to read (no overruns)

private:
	u8 m_rdr = 0, m_tdr = 0;
	bool m_da = false, m_tbmt = true, m_or = false, m_fe = false;
	int m_tx_timer = 0, m_rx_gap = 0;
	std::deque<u8> m_rxq;
};
#endif
