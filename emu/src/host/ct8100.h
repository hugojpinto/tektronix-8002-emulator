// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 CRT Terminal, the 8002's system terminal: Tektronix's version of its 4023
// Computer Display Terminal (CT8100 Users Manual 070-2359-00, preface; notes/ct8100.md), 24
// lines of 80 on the 8002's RS-232 terminal port, in DIRECT mode. From Table 2-2 (pp. 2-7 to
// 2-10) and pp. 2-2 to 2-5, 2-10 to 2-12:
// - printable characters at the cursor; BS one position left, none at the left margin (not
//   destructive); HT a non-destructive space right; LF down a line, rolling the screen up past
//   the 24th; CR to the left margin; BEL the tone; DEL neither writes nor spaces;
// - FS X Y: cursor addressing (X = column + 32, Y = line + 32);
// - ESC FF: clear page (screen cleared, cursor home, terminal logic initialised);
//   ESC ]: read cursor (the terminal sends GS, X + 32, Y + 32, CR); ESC O (or o, /): clear
//   input (unprotected fields blanked); ESC BEL (hard copy) does nothing without a hard copy
//   unit;
// - US FAC: a field attribute code stored at the cursor, setting the look of the field up to
//   the next FAC or the end of the line (Table 2-1): normal, inverse, blinking or blanked, dim
//   when protected-dim;
// - SO / SI: the optional rulings character set for codes 32-63 (Fig. 2-2, Table 2-3).
// - the LF DOES CR strap (p. 2-1: "The LF (line feed) code ... can be strapped to also perform a
//   carriage return") is set: TEKDOS echoes RETURN as LF (and DC3) with no CR, and its next output
//   would otherwise start where the command line ended (inferred from that echo). CR DOES LF is
//   not set: TEKDOS ends its lines with CR LF.
// Not modelled: BUFFER mode (form fill-out, keyboard editing in terminal memory, ESC SO / ESC SI
// send page), VT (back tab: "does not work in DIRECT"). Other control characters are ignored.
// A line wraps at column 80 (inferred), and a US FAC moves the cursor on by one (inferred).
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
	bool lf_does_cr = true;                      // the LF DOES CR strap (see above)
	std::function<void(uint8_t)> on_key;

	uint8_t cell(int row, int col) const { return m_cell[row][col]; }
	enum { A_RULING = 1, A_FAC = 2 };
	uint8_t attr(int row, int col) const { return m_attr[row][col]; }   // A_RULING: from the rulings set; A_FAC: a field attribute code
	int cursor_row() const { return m_row; }
	int cursor_col() const { return m_col; }
	bool take_bell() { bool b = m_bell; m_bell = false; return b; }
	std::string text() const;                    // the screen as text, trailing spaces trimmed

private:
	uint8_t m_cell[ROWS][COLS], m_attr[ROWS][COLS];
	int m_row = 0, m_col = 0;
	bool m_bell = false;
	bool m_esc = false, m_us = false, m_so = false;
	int m_fs = 0, m_fsx = 0;                     // FS: 1 = the X byte next, 2 = the Y byte
	void line_feed();
	void put(uint8_t c, uint8_t a);
	void clear_input();
};
#endif
