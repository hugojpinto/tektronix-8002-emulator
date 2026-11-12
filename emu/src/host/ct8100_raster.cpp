// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100's screen as dots -- see ct8100_raster.h.
#include "host/ct8100_raster.h"
#include <cstring>

namespace {
const uint8_t font[96][8] = {
#include "host/term_font.inc"
};
}

const uint8_t *ct8100_raster::glyph(uint8_t c) { return font[(c >= 0x20 && c < 0x80) ? c - 0x20 : '?' - 0x20]; }

void ct8100_raster::draw(const ct8100 &t, bool cursor_on, uint8_t *dots)
{
	memset(dots, 0, size_t(W) * H);
	for (int y = 0; y < ct8100::ROWS; y++)
		for (int x = 0; x < ct8100::COLS; x++) {
			const uint8_t ch = t.cell(y, x);
			uint8_t cell[10][8] = {};
			if (ch > 0x20 && ch < 0x80) {
				const uint8_t *g = font[ch - 0x20];
				for (int gy = 0; gy < 8; gy++)
					for (int gx = 0; gx < 5; gx++)
						if (g[gy] >> (4 - gx) & 1) cell[1 + gy][1 + gx] = 1;
			}
			const bool inv = cursor_on && y == t.cursor_row() && x == t.cursor_col();
			uint8_t *row0 = dots + size_t(y * 10) * W + x * 8;
			for (int cy = 0; cy < 10; cy++)
				for (int cx = 0; cx < 8; cx++) row0[size_t(cy) * W + cx] = inv ? !cell[cy][cx] : cell[cy][cx];
		}
}
