#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
"""Render CT8100 screen snapshots (tek8002 -snap) or plain text to PNG.

The raster is the one the GUI draws (emu/src/host/ct8100_raster.*): 80x24 characters, each a
5x8 dot glyph in an 8x10 dot field, so 640 dots x 240 scan lines; every scan line doubled (the
second dimmer) for 640x480, in the GUI's green phosphor. The glyphs are read from
emu/src/host/term_font.inc, the font both front ends use. The CT8100's own character ROM and
phosphor colour are not documented (notes/ct8100.md), so both are the emulator's choice.
A -snap file holds no cursor position, so no cursor is drawn.

--crt: present the raster on a tube face, as the GUI's CRT style does: phosphor glow, overscan
border, rounded face with a thin lip, edge vignette (the raster itself is not rescaled).
--lines A-B: render only text lines A..B (1-based) of a plain text file, as the bottom of the
screen; for transcripts longer than a screen.
usage: render_screens.py [--crt] [--lines A-B] INPUT OUTPUT.png
INPUT is a -snap file (24 lines) or any text file (its last 24 lines).
"""
import argparse, os, re, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
FONT_INC = os.path.join(ROOT, 'emu', 'src', 'host', 'term_font.inc')
COLS, ROWS = 80, 24
BG = (5, 9, 6)                 # the GUI's background and phosphor (main_gui.cpp)
GREEN = (70, 255, 120)

def load_font():
    rows = []
    for line in open(FONT_INC, encoding='utf-8'):
        m = re.match(r'\s*\{([^}]*)\}', line)
        if m: rows.append([int(v, 16) for v in m.group(1).split(',')])
    assert len(rows) == 96, 'term_font.inc: expected 96 glyphs'
    return rows

def load_text(path, lines=None):
    body = [l.rstrip('\r\n') for l in open(path, encoding='latin-1').read().split('\n')]
    if lines:
        a, b = (int(v) for v in lines.split('-'))
        body = body[a - 1:b]
    else:
        while body and body[-1] == '': body.pop()
    body = body[-ROWS:]
    return [l.expandtabs(8).ljust(COLS)[:COLS] for l in body] + [' ' * COLS] * (ROWS - len(body))

def render(text, font):
    W, H = COLS * 8, ROWS * 10
    img = Image.new('RGB', (W, H * 2), BG)
    px = img.load()
    dim = tuple(int(BG[k] + GREEN[k] * 0.55) for k in range(3))
    for y in range(ROWS):
        for x in range(COLS):
            c = ord(text[y][x])
            if c <= 0x20 or c >= 0x80: continue
            g = font[c - 0x20]
            for gy in range(8):
                for gx in range(5):
                    if g[gy] >> (4 - gx) & 1:
                        X, Y = x * 8 + 1 + gx, (y * 10 + 1 + gy) * 2
                        px[X, Y] = GREEN
                        px[X, Y + 1] = dim
    return img

def crt_frame(img, overscan=36, radius=44):
    W, H = img.size
    sw, sh = W + 2 * overscan, H + 2 * overscan
    glass = Image.new('RGB', (sw, sh), BG)
    glass.paste(img, (overscan, overscan))
    bloom = glass.filter(ImageFilter.GaussianBlur(3)).point(lambda v: int(v * 0.5))
    glass = ImageChops.add(glass, bloom)
    halo = glass.filter(ImageFilter.GaussianBlur(10)).point(lambda v: int(v * 0.2))
    glass = ImageChops.add(glass, halo)
    vig = Image.new('L', (sw, sh), 0)
    vd = ImageDraw.Draw(vig)
    steps = 40
    for i in range(steps):
        t = i / (steps - 1)
        ix, iy = int(sw * 0.16 * (1 - t)), int(sh * 0.16 * (1 - t))
        vd.rounded_rectangle([ix, iy, sw - ix, sh - iy], radius=radius, fill=int(150 + 105 * t))
    vig = vig.filter(ImageFilter.GaussianBlur(25))
    glass = Image.composite(glass, Image.new('RGB', (sw, sh), (0, 0, 0)), vig)
    mask = Image.new('L', (sw, sh), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, sw - 1, sh - 1], radius=radius, fill=255)
    lip = 10
    fw, fh = sw + 2 * lip, sh + 2 * lip
    frame = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    fd = ImageDraw.Draw(frame)
    fd.rounded_rectangle([0, 0, fw - 1, fh - 1], radius=radius + lip, fill=(18, 18, 17, 255))
    fd.rounded_rectangle([0, 0, fw - 1, fh - 1], radius=radius + lip, outline=(58, 56, 52, 255), width=2)
    frame.paste(glass, (lip, lip), mask)
    return frame

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--crt', action='store_true')
    ap.add_argument('--lines')
    ap.add_argument('input'); ap.add_argument('output')
    a = ap.parse_args()
    img = render(load_text(a.input, a.lines), load_font())
    if a.crt: img = crt_frame(img)
    img.save(a.output, optimize=True)
    print(a.output, img.size)

if __name__ == '__main__':
    main()
