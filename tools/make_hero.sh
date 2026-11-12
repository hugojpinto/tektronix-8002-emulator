#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Render docs/screenshots/hero.png: the graphical front end (CT8100, 8002A and disc unit panels)
# tracing Tektronix's Z80 DEMO under TEKDOS's DEBUG, composed on a backdrop. The DEMO is first
# assembled and linked on a copy of the Z80 system disc by the tty front end (quicker).
# usage: tools/make_hero.sh [WORKDIR]   (needs emu/build/tek8002 and tek8002-gui, python3 + Pillow)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$(mktemp -d)}"; mkdir -p "$W"
cd "$ROOT/emu"
cp ../sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img "$W/hero.img"; chmod u+w "$W/hero.img"
build/tek8002 -rw -0 "$W/hero.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM DEMO;O DEMO;A DEMO%S\r' -type prompt 'LINK DEMO;L DEMO;K DEMO;O\r' </dev/null >/dev/null 2>&1
# the run ends at the prompt after the last command, and the shot is taken then
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software build/tek8002-gui -scale 1 -0 "$W/hero.img" -cycles $((600 * 1250000)) \
	-type prompt 'LOAD DEMO;L\r' -type prompt 'PATCH 500 0102030405\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r' \
	-shot 600 "$W/hero.bmp" </dev/null >/dev/null 2>&1
python3 -I "$ROOT/tools/compose_gui.py" "$W/hero.bmp" "$ROOT/docs/screenshots/hero.png" --width 1280
