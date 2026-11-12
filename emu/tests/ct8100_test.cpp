// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100 terminal model against its manual (070-2359-00, Table 2-2 and pp. 2-2 to 2-12):
// format effectors, cursor addressing, read cursor, clear page, fields, clear input, rulings.
//   ct8100_test [FILE.pgm]   (also writes a picture of a formatted screen, for a look)
#include "host/ct8100.h"
#include "host/ct8100_raster.h"
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;
static void check(bool ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static void send(ct8100 &t, const std::string &s) { for (char c : s) t.receive(uint8_t(c)); }
static std::string row(const ct8100 &t, int r) { std::string s; for (int c = 0; c < ct8100::COLS; c++) s += char(t.cell(r, c)); while (!s.empty() && s.back() == ' ') s.pop_back(); return s; }

int main(int argc, char **argv)
{
	ct8100 t;
	std::string sent;
	t.on_key = [&](uint8_t c) { sent += char(c); };

	send(t, "ABC\b\bX");
	check(row(t, 0) == "AXC", "BS moves left without erasing");
	t.clear(); send(t, "\b"); check(t.cursor_col() == 0, "BS at the left margin: no effect");
	t.clear(); send(t, "A\tB"); check(row(t, 0) == "A B", "HT: one non-destructive space right (DIRECT)");
	t.clear(); send(t, "AB\x7f" "C"); check(row(t, 0) == "ABC", "DEL neither writes nor spaces");

	t.clear(); send(t, std::string("\x1c") + char(32 + 18) + char(32 + 10) + "Z");
	check(t.cell(10, 18) == 'Z', "FS X Y: the manual's example, column 18 line 10 ('2', '*')");
	send(t, std::string("\x1c") + char(32 + 90) + char(32 + 3));
	check(t.cursor_row() == 10 && t.cursor_col() == 19, "FS out of range: ignored");

	sent.clear(); send(t, "\x1b]");
	check(sent == std::string("\x1d") + char(32 + 19) + char(32 + 10) + "\r", "ESC ]: read cursor sends GS X Y CR");

	send(t, "\x1b\x0c");
	check(row(t, 10).empty() && t.cursor_row() == 0 && t.cursor_col() == 0, "ESC FF: clear page, cursor home");
	send(t, "\x0c"); check(t.cursor_col() == 0 && row(t, 0).empty(), "FF alone does nothing");

	// fields: US then a FAC (Table 2-1); ESC O blanks unprotected fields only
	t.clear();
	send(t, "\x1f" "BNAME" "\x1f" "@john" "\x1f" "Fkeep");
	check(t.attr(0, 0) & ct8100::A_FAC, "US: the next character is stored as a FAC");
	check(t.text().substr(0, t.text().find('\n')) == " NAME john keep", "FACs show as blanks in text");
	send(t, "\x1bO");
	check(row(t, 0).substr(0, 15) == "BNAME@    Fkeep", "ESC O: unprotected field cleared, protected kept");

	// rulings: SO selects them for 32-63, SI returns
	t.clear(); send(t, "\x0e" "8:" "\x0f" "8");
	check((t.attr(0, 0) & ct8100::A_RULING) && (t.attr(0, 1) & ct8100::A_RULING) && !(t.attr(0, 2) & ct8100::A_RULING), "SO/SI select the rulings set");
	check(t.text().substr(0, 4) == "-|8\n", "rulings as text: - | then a plain 8");

	// scrolling keeps attributes with their cells
	t.clear(); send(t, "\x1f" "H"); for (int i = 0; i < 24; i++) send(t, "\n");
	check(t.attr(0, 0) == 0, "LF past line 24 rolls the screen up (attributes too)");

	// a picture: a ruled box with fields of each look
	t.clear();
	auto at = [&](int x, int y) { send(t, std::string("\x1c") + char(32 + x) + char(32 + y)); };
	at(2, 1); send(t, "\x0e" " 888888888888888888888,");      // ┌ ─ ┐ (32 '8'x 44)
	for (int y = 2; y < 8; y++) { at(2, y); send(t, ":"); at(24, y); send(t, ":"); }
	at(2, 8); send(t, "$8888888888888888888880" "\x0f");    // └ ─ ┘
	at(4, 2); send(t, "\x1f" "@NORMAL"); at(4, 3); send(t, "\x1f" "HINVERSE"); at(4, 4); send(t, "\x1f" "PBLINKING");
	at(4, 5); send(t, "\x1f" "CPROTECTED DIM"); at(4, 6); send(t, "\x1f" "XBLANKED"); at(4, 7); send(t, "\x1f" "@back to normal");
	for (int y = 2; y < 8; y++) { at(22, y); send(t, "\x1f" "@"); }   // each field ends before the box's edge
	at(30, 1); send(t, "\x0e" "!9999999-"); at(30, 2); send(t, ";"); at(38, 2); send(t, ";"); at(30, 3); send(t, "%9999999" "1" "\x0f");
	at(30, 5); send(t, "\x0e" "234567<=>" "\x0f");
	check(t.cell(1, 2) == ' ' && (t.attr(1, 2) & ct8100::A_RULING), "box corner from code 32");
	if (argc > 1) {
		std::vector<uint8_t> dots(size_t(ct8100_raster::W) * ct8100_raster::H);
		ct8100_raster::draw(t, false, true, dots.data());
		if (FILE *f = fopen(argv[1], "wb")) {
			fprintf(f, "P5 %d %d 255\n", ct8100_raster::W, ct8100_raster::H);
			for (uint8_t d : dots) fputc(d == 2 ? 255 : d == 1 ? 110 : 0, f);
			fclose(f);
		}
	}
	printf("%s\n", fails ? "ct8100: FAILURES" : "ct8100: ok");
	return fails != 0;
}
