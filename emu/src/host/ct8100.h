// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 CRT Terminal, the 8002's system terminal, as far as it is documented: a 9-inch
// refreshed crt "to display up to 1920 characters" (24 lines of 80) in the "ASCII set of 96
// upper/lower case characters" (1978 catalogue), with a TAB key (070-2701 §4), on the 8002's
// RS-232 terminal port. Its control sequences are not documented, and TEKDOS needs none: it
// writes text, CR, LF, BS, BEL and TAB. Modelled as a glass terminal: printable characters
// at the cursor, CR, LF (scrolling at the bottom), BS, BEL, HT (stops every 8 columns), a
// line wraps at column 80; other control characters are ignored.
#ifndef TEK_HOST_CT8100_H
#define TEK_HOST_CT8100_H
#include <cstdint>
#include <deque>
#include <functional>
#include <string>

class ct8100
{
public:
	static constexpr int COLS = 80, ROWS = 24;

	ct8100() { clear(); }
	void clear();
	void receive(uint8_t c);                     // a character from the 8002
	void type(uint8_t c) { if (on_key) on_key(c); }   // a key: to the 8002
	std::function<void(uint8_t)> on_key;

	uint8_t cell(int row, int col) const { return m_cell[row][col]; }
	int cursor_row() const { return m_row; }
	int cursor_col() const { return m_col; }
	bool take_bell() { bool b = m_bell; m_bell = false; return b; }
	std::string text() const;                    // the screen as text, trailing spaces trimmed

private:
	uint8_t m_cell[ROWS][COLS];
	int m_row = 0, m_col = 0;
	bool m_bell = false;
	void line_feed();
};
#endif
