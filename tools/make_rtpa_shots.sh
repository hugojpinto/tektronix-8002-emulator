#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Render docs/screenshots/rtpa/*.png: the CT8100 pictures in docs/RTPA.md. One TEKDOS 8080
# session with the RTPA fitted types the tutorial's commands in order (the sample program, then
# sections 3 to 12), as tools/make_screenshots.sh does: the first run finds where it ends, the
# second snaps every 0.05 s, and tools/pick_snap.py picks by content the frame where each
# display stands complete at a bare prompt. Batch runs are deterministic, so the pictures come
# out the same on every run, and a session that stops giving the tutorial's output stops the
# script.
# usage: tools/make_rtpa_shots.sh [WORKDIR]   (needs emu/build/tek8002, python3 + Pillow)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$(mktemp -d)}"; mkdir -p "$W"
OUT="$ROOT/docs/screenshots/rtpa"; mkdir -p "$OUT"
EMU="$ROOT/emu/build/tek8002"
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
cp "$ROOT/sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_3.img" "$W/tek8080.img"; chmod u+w "$W/tek8080.img"
cd "$ROOT"

A=()
c() { for l in "$@"; do A+=(-type prompt "$l\r"); done; }
# 2. the sample program
c 'F 200 400 FF' 'PATCH 200 F311750221500236007DBBC21102C37503' 'PATCH 211 23C30702' 'PATCH 300 0076' \
  'PATCH 375 01250321500336007DB9C28503C30003' 'PATCH 385 2BC37B03' 'DEB' 'TRACE OFF'
# 3. stop on an event
c 'EV 1 CLR A=201 D=11' 'BIF CLR' 'BIF 1' 'EV' 'G 200'
c 'EV 1 B=W' 'G 200'
c 'EV 1 B=ALL' 'EV 2 CLR A=300 D=00' 'BIF CLR' 'BIF 2' 'G 200'
# 4. the trace
c 'DR 5'
c 'EV 2 B=F' 'RT F' 'G 200' 'DR 5'
# 5. count and time
c 'RT ALL' 'EV CLR' 'EV 1 A=209 D=7D B=F' 'BIF 1' 'CN C' 'G 200' 'CN' 'CN U' 'G 200' 'CN'
# 6. pass counts and delays
c 'EV 1 P=2' 'CN C' 'G 200' 'CN' 'EV 1 P=20' 'G 200' 'CN'
c 'EV 1 C=500' 'G 200' 'CN'
c 'EV 2 A=37E D=B9 B=MR' 'BIF CLR' 'BIF 2' 'G 200' 'CN' 'EV 2 P=10' 'G 200' 'CN'
# 7. count loop passes
c 'EV 1 CLR A=207 D=36' 'EV 2 CLR A=300 D=00' 'CN 1' 'G 200' 'CN'
# 8. ARM
c 'EV 1 CLR A=207 D=36' 'EV 2 CLR A=37B D=36' 'BIF ARM' 'CN C' 'G 200' 'CN'
c 'EV 1 CLR A=207 D=36' 'EV 2 CLR A=207 D=36' 'G 200' 'CN' 'DR 25'
# 9. watch without stopping
c 'EV 1 CLR A=209 D=7D B=F P=10 C=20' 'EV 2 CLR A=37E D=B9 B=MR P=10 C=150' 'BIF IND C' 'EV' 'CN C' 'G 200' 'CN'
# 10. FRZ
c 'EV 1 CLR A=209 D=7D' 'EV 2 CLR A=300 D=00' 'BIF FRZ' 'CN C' 'G 200' 'DR 11' 'CN'
c 'EV 1 CLR A=209 D=7D' 'EV 2 CLR A=212 D=C3' 'BIF FRZ C' 'G 200' 'DR 30'
# 11. LIM
c 'EV 1 CLR A>200 D=C3' 'EV 2 CLR A<300' 'BIF LIM' 'G 200' 'EV 2 A>300' 'G 200'
# 12. before, around or after the event
c 'EV 1 CLR A=200 C=128' 'CN C' 'BIF CLR' 'BIF 1' 'G 200' 'DR *'
ARGS=(-rtpa -0 "$W/tek8080.img" -cycles $((900 * 1250000)) "${A[@]}")

# first run: where the session ends (a run typed with -type prompt ends at the prompt after the
# last command)
end=$("$EMU" "${ARGS[@]}" </dev/null 2>&1 >/dev/null | sed -n 's/.*cyc=\([0-9]*\).*/\1/p')
[ -n "$end" ] || { echo "rtpa: no end"; exit 1; }
# second run: a snapshot every 0.05 s
mkdir -p "$W/rtpa"; rm -f "$W/rtpa"/*.scr; snaps=()
while read -r t f; do snaps+=(-snap "$t" "$W/rtpa/$f.scr"); done < <(python3 -I -c "
import sys
e = int(sys.argv[1]) / 1250000; t = 0.0
while t <= e: print('%.2f s%08d' % (t, round(t * 100))); t += 0.05" "$end")
"$EMU" "${ARGS[@]}" "${snaps[@]}" </dev/null >/dev/null 2>&1
echo "rtpa: ends at $(python3 -I -c "print(round($end/1250000, 1))") s"

# shot PNG NEEDLE...: render the first snapshot holding every NEEDLE at a prompt
shot() {
	png=$1; shift
	f=$(python3 -I tools/pick_snap.py "$W/rtpa" "$@")
	cp "$f" "$W/$png.scr"
	python3 -I tools/render_screens.py --crt "$f" "$OUT/$png.png"
}
shot r1_break "BIF-1 S" "0201 BREAK"
shot r2_drt "> DR 5" "0300  00  NOP       00000000  M R F"
shot r3_count "COUNT=00011 CYCL" "USEC"
shot r4_delay "COUNT=00239 CYCL" "COUNT=00739 CYCL"
shot r6_ind_c "BIF-1 C 2 C" "COUNT=00995 CYCL"
shot r5_arm "COUNT=00461 CYCL"
shot r7_frz "> DR 11" "COUNT=00985 CYCL"
shot r8_lim "0386 C37B03 JMP   037B      0000  06  50 03 25 02 75 03 4F"
echo "done: $OUT"
