// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100's screen as dots -- see ct8100_raster.h.
#include "host/ct8100_raster.h"
#include <cstring>

namespace {
const uint8_t font[96][8] = {
#include "host/term_font.inc"
};

// A rulings character in an 8x10 cell, from Table 2-3 (code = 32 + 6P + 2S + T) and Fig. 2-2.
// Simple joins (P 0-2): P is the horizontal element (0 the right half, 1 across, 2 the left
// half), S the vertical (0 the lower half, 1 across, 2 the upper half), T double. Complex joins
// (P 3, codes 50-55): a double superior element across, horizontal (T 0) or vertical (T 1), and
// a single inferior one by S's join type. Single elements (P 4, 56-59): S 0 horizontal, 1
// vertical; T double. 60-63: the miscellaneous elements (upper block, full block, lower block,
// blank; their exact extents are read from Fig. 2-2).
void ruling(uint8_t c, uint8_t cell[10][8])
{
	const int n = c - 32;
	auto hline = [&](int y, int x0, int x1) { for (int x = x0; x <= x1; x++) cell[y][x] = 1; };
	auto vline = [&](int x, int y0, int y1) { for (int y = y0; y <= y1; y++) cell[y][x] = 1; };
	auto h = [&](int part, bool dbl) {            // part 0: right half, 1: across, 2: left half
		const int x0 = part == 0 ? 3 : 0, x1 = part == 2 ? 4 : 7;
		if (dbl) { hline(3, x0, x1); hline(6, x0, x1); } else hline(4, x0, x1);
	};
	auto v = [&](int part, bool dbl) {            // part 0: lower half, 1: across, 2: upper half
		const int y0 = part == 0 ? 4 : 0, y1 = part == 2 ? 5 : 9;
		if (dbl) { vline(2, y0, y1); vline(5, y0, y1); } else vline(3, y0, y1);
	};
	if (n < 0 || n > 31) return;
	if (n >= 28) {                                 // miscellaneous
		const int y0 = n == 30 ? 5 : 0, y1 = n == 28 ? 4 : 9;
		if (n != 31) for (int y = y0; y <= y1; y++) hline(y, 0, 7);
		return;
	}
	const int p = n / 6, s = (n % 6) / 2, t = n & 1;
	if (p <= 2) { h(p, t); v(s, t); }
	else if (p == 3) { if (t) { v(1, true); h(s, false); } else { h(1, true); v(s, false); } }
	else { if (s == 0) h(1, t); else v(1, t); }
}
}

const uint8_t *ct8100_raster::glyph(uint8_t c) { return font[(c >= 0x20 && c < 0x80) ? c - 0x20 : '?' - 0x20]; }

void ct8100_raster::draw(const ct8100 &t, bool cursor_on, bool blink_on, uint8_t *dots)
{
	memset(dots, 0, size_t(W) * H);
	for (int y = 0; y < ct8100::ROWS; y++) {
		uint8_t fac = 0;                         // the field's attribute code (0: none, normal)
		for (int x = 0; x < ct8100::COLS; x++) {
			const uint8_t ch = t.cell(y, x), at = t.attr(y, x);
			uint8_t cell[10][8] = {};
			if (at & ct8100::A_FAC) fac = ch;    // the FAC's own position shows blank
			else if (at & ct8100::A_RULING) ruling(ch, cell);
			else if (ch > 0x20 && ch < 0x80) {
				const uint8_t *g = font[ch - 0x20];
				for (int gy = 0; gy < 8; gy++)
					for (int gx = 0; gx < 5; gx++)
						if (g[gy] >> (4 - gx) & 1) cell[1 + gy][1 + gx] = 1;
			}
			// Table 2-1: rows of 8 codes from @: white on black, black on white, blinking, blanked;
			// the fourth and eighth of each row (protected dim) are dim
			const int look = fac >= 0x40 && fac < 0x60 ? (fac >> 3) & 3 : 0;
			const bool inverse = look == 1, hidden = look == 3 || (look == 2 && !blink_on);
			const uint8_t on = fac >= 0x40 && fac < 0x60 && (fac & 3) == 3 ? 1 : 2;
			const bool cur = cursor_on && y == t.cursor_row() && x == t.cursor_col();
			uint8_t *row0 = dots + size_t(y * 10) * W + x * 8;
			for (int cy = 0; cy < 10; cy++)
				for (int cx = 0; cx < 8; cx++) {
					bool lit = !hidden && cell[cy][cx];
					if (inverse) lit = !lit;
					if (cur) lit = !lit;
					row0[size_t(cy) * W + cx] = lit ? on : 0;
				}
		}
	}
}
