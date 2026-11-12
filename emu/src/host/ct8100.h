// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 CRT Terminal, the 8002's system terminal: Tektronix's version of its 4023
// Computer Display Terminal (CT8100 Users Manual 070-2359-00, preface; notes/ct8100.md), 24
// lines of 80 on the 8002's RS-232 terminal port. Modelled in DIRECT mode, with the format
// effectors TEKDOS uses, as Table 2-2 (pp. 2-7 to 2-10) gives them: printable characters at
// the cursor; BS one position left, none at the left margin (not destructive: TEKDOS's BACK
// SPACE echoes a bare BS, so a deleted character stays until it is typed over); HT a
// non-destructive space right; LF down a line, rolling the screen up past the 24th; CR to the
// left margin; BEL the tone; DEL neither writes nor spaces. Not modelled (TEKDOS sends none of
// them): buffer mode and formatted fields (US, FAC codes), cursor addressing (FS), the ESC
// commands (clear page, send, read cursor, hard copy), the rulings character set (SO/SI).
// Other control characters are ignored. A line wraps at column 80 (inferred).
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
	// a key: to the 8002. TTY LOCK (a latching key, 070-2359 p. 1-14): "All lower case
	// characters are then displayed and/or transmitted as upper case characters, regardless
	// of condition of SHIFT key". TEKDOS takes commands in upper case only.
	void type(uint8_t c) { if (tty_lock && c >= 'a' && c <= 'z') c -= 0x20; if (on_key) on_key(c); }
	bool tty_lock = true;
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
