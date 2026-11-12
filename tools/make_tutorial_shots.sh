#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Render docs/screenshots/tutorials/*.png: the graphical front end (CT8100 and both front
# panels) during the first boot of docs/TUTORIALS.md §1. Batch runs of the GUI advance 1/60 s of
# machine time per frame, so the pictures come out the same on every run.
# usage: tools/make_tutorial_shots.sh [WORKDIR]   (needs emu/build/tek8002-gui, python3 + Pillow)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$(mktemp -d)}"; mkdir -p "$W"
OUT="$ROOT/docs/screenshots/tutorials"; mkdir -p "$OUT"
cd "$ROOT/emu"
compose() { python3 -I "$ROOT/tools/compose_gui.py" "$W/$1.bmp" "$OUT/$1.png" --width 1100; }
# boot from the TEKDOS 6800 system disc: the bootstrap PROM reading tracks 1-4, the banner, LDIR
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software build/tek8002-gui -scale 1 -0 ../sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img \
	-cycles $((60 * 1250000)) -type prompt 'LDIR\r' -shot 4 "$W/t1_boot.bmp" -shot 60 "$W/t1_ldir.bmp" </dev/null >/dev/null 2>&1
compose t1_boot; compose t1_ldir
