// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 CRT Terminal -- see ct8100.h.
#include "host/ct8100.h"
#include <cstring>

void ct8100::clear()
{
	memset(m_cell, ' ', sizeof m_cell);
	m_row = m_col = 0;
}

void ct8100::line_feed()
{
	if (m_row < ROWS - 1) { m_row++; return; }
	memmove(m_cell[0], m_cell[1], sizeof m_cell[0] * (ROWS - 1));
	memset(m_cell[ROWS - 1], ' ', COLS);
}

void ct8100::receive(uint8_t c)
{
	c &= 0x7f;
	switch (c) {
	case 0x07: m_bell = true; return;
	case 0x08: if (m_col > 0) m_col--; return;
	case 0x09: m_col = (m_col / 8 + 1) * 8; if (m_col >= COLS) m_col = COLS - 1; return;
	case 0x0a: line_feed(); return;
	case 0x0d: m_col = 0; return;
	default: break;
	}
	if (c < 0x20 || c == 0x7f) return;
	if (m_col >= COLS) { m_col = 0; line_feed(); }
	m_cell[m_row][m_col++] = c;
}

std::string ct8100::text() const
{
	std::string s;
	for (int r = 0; r < ROWS; r++) {
		std::string line(reinterpret_cast<const char *>(m_cell[r]), COLS);
		while (!line.empty() && line.back() == ' ') line.pop_back();
		s += line + '\n';
	}
	return s;
}
