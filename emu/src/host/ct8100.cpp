// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 CRT Terminal -- see ct8100.h.
#include "host/ct8100.h"
#include <cstring>

void ct8100::clear()
{
	memset(m_cell, ' ', sizeof m_cell);
	memset(m_attr, 0, sizeof m_attr);
	m_row = m_col = 0;
	m_esc = m_us = m_so = false;
	m_fs = 0;
}

void ct8100::line_feed()
{
	if (m_row < ROWS - 1) { m_row++; return; }
	memmove(m_cell[0], m_cell[1], sizeof m_cell[0] * (ROWS - 1));
	memmove(m_attr[0], m_attr[1], sizeof m_attr[0] * (ROWS - 1));
	memset(m_cell[ROWS - 1], ' ', COLS);
	memset(m_attr[ROWS - 1], 0, COLS);
}

void ct8100::put(uint8_t c, uint8_t a)
{
	if (m_col >= COLS) { m_col = 0; line_feed(); }
	m_cell[m_row][m_col] = c;
	m_attr[m_row][m_col++] = a;
}

// ESC O: "Sets all unprotected data fields to NUL characters" (p. 2-9): the cells after an
// unprotected FAC (Table 2-1: @ A D E, H I L M, P Q T U, X Y \\ ]) up to the next FAC
void ct8100::clear_input()
{
	for (int r = 0; r < ROWS; r++) {
		bool unprot = false;
		for (int c = 0; c < COLS; c++) {
			if (m_attr[r][c] & A_FAC) { unprot = !(m_cell[r][c] & 2); continue; }
			if (unprot) { m_cell[r][c] = ' '; m_attr[r][c] = 0; }
		}
	}
}

void ct8100::receive(uint8_t c)
{
	c &= 0x7f;
	if (m_esc) {                                 // the second character of an ESC command
		m_esc = false;
		switch (c) {
		case 0x0c: clear(); break;               // ESC FF: clear page
		case ']':                                // ESC ]: read cursor -> GS X Y END (CR)
			if (on_key) { on_key(0x1d); on_key(uint8_t(m_col + 32)); on_key(uint8_t(m_row + 32)); on_key(0x0d); }
			break;
		case 'O': case 'o': case '/': clear_input(); break;
		default: break;                          // ESC BEL (hard copy), ESC SO / SI (send): no effect here
		}
		return;
	}
	if (m_fs == 1) { m_fsx = c - 32; m_fs = 2; return; }
	if (m_fs == 2) {
		m_fs = 0;
		const int y = c - 32;
		if (m_fsx >= 0 && m_fsx < COLS && y >= 0 && y < ROWS) { m_col = m_fsx; m_row = y; }
		return;
	}
	if (m_us) { m_us = false; put(c, A_FAC); return; }
	switch (c) {
	case 0x07: m_bell = true; return;
	case 0x08: if (m_col > 0) m_col--; return;
	case 0x09: if (m_col < COLS - 1) m_col++; return;              // DIRECT: a non-destructive space
	case 0x0a: line_feed(); return;
	case 0x0d: m_col = 0; return;
	case 0x0e: m_so = true; return;                                 // SO: the rulings set
	case 0x0f: m_so = false; return;                                // SI: the standard set
	case 0x1b: m_esc = true; return;
	case 0x1c: m_fs = 1; return;                                    // FS: cursor addressing
	case 0x1f: m_us = true; return;                                 // US: a field attribute code follows
	default: break;
	}
	if (c < 0x20 || c == 0x7f) return;
	put(c, m_so && c < 0x40 ? A_RULING : 0);
}

// a ruling character as text (for -snap): its nearest ASCII
static char ruling_text(uint8_t c)
{
	const int n = c - 32;
	if (n >= 28) return n == 31 ? ' ' : '#';                      // 60-62: blocks, 63: blank
	if (n >= 24) return (n - 24) / 2 ? '|' : ((n & 1) ? '=' : '-'); // single elements
	return '+';                                                     // joins
}

std::string ct8100::text() const
{
	std::string s;
	for (int r = 0; r < ROWS; r++) {
		std::string line(COLS, ' ');
		for (int c = 0; c < COLS; c++)
			line[c] = (m_attr[r][c] & A_FAC) ? ' ' : (m_attr[r][c] & A_RULING) ? ruling_text(m_cell[r][c]) : char(m_cell[r][c]);
		while (!line.empty() && line.back() == ' ') line.pop_back();
		s += line + '\n';
	}
	return s;
}
