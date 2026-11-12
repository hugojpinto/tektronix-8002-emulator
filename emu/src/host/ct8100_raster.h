// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The CT8100's screen as dots: characters on a 5x8 dot matrix in an 8x10 dot field, 80x24 ->
// 640 dots x 240 scan lines. The CT8100's character generator is not known: the font is the
// one the sibling tektronix-8550-emulator draws its CT8500 with (host/term_font.inc: the Tek
// 308's upper case and a hand-drawn lower case). Front ends scale and colour it; nothing here
// depends on a graphics library.
#ifndef TEK_HOST_CT8100_RASTER_H
#define TEK_HOST_CT8100_RASTER_H
#include "host/ct8100.h"
#include <cstdint>

namespace ct8100_raster {
constexpr int W = ct8100::COLS * 8, H = ct8100::ROWS * 10;
// dots[W*H]: 0 off, 1 lit. The cursor is a block in inverse video, shown in the cursor_on phase.
void draw(const ct8100 &t, bool cursor_on, uint8_t *dots);
// the glyph of an ASCII character: 8 rows of 5 dots, bit 4 = leftmost
const uint8_t *glyph(uint8_t c);
}
#endif
