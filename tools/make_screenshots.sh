#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Regenerate docs/screenshots/*.png: the CT8100 screen in the sessions docs/TUTORIALS.md and
# docs/OPERATION.md walk through (the command sequences of tests/*.sh), drawn by
# tools/render_screens.py on a CRT face.
#
# Each session runs twice on fresh copies of its discs. The first run finds where it ends (batch
# runs are deterministic, and a run typed with -type prompt ends at the prompt after the last
# command). The second takes a -snap every 0.05 s, and tools/pick_snap.py picks, by content, the
# frame where the wanted output stands complete at a bare prompt. So the pictures do not depend
# on hand-tuned times, and a session that stops giving the expected output stops the script.
# usage: tools/make_screenshots.sh [WORKDIR]   (needs emu/build/tek8002, ../tekmato, python3 + Pillow)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$(mktemp -d)}"; mkdir -p "$W"
OUT="$ROOT/docs/screenshots"; mkdir -p "$OUT"
EMU="$ROOT/emu/build/tek8002" TEKMATO="$ROOT/../tekmato/tekmato"
SYS68="$ROOT/sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img"
SYSZ80="$ROOT/sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img"
DATA1A="$ROOT/sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_1a.img"
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
[ -x "$TEKMATO" ] || { echo "needs ../tekmato (make -C ../tekmato) to put sources on the discs"; exit 1; }
cd "$ROOT"

fresh() { cp "$1" "$W/$2"; chmod u+w "$W/$2"; }
TPID=
trap '[ -n "$TPID" ] && kill $TPID 2>/dev/null; true' EXIT
target() {   # a fresh tools/proto_target.py on a free port: $PORT
	if [ -n "$TPID" ]; then kill $TPID 2>/dev/null || true; wait $TPID 2>/dev/null || true; fi
	PORT=$(python3 -I -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
	python3 -I tools/proto_target.py --tcp "$PORT" "$@" > "$W/target.out" 2> "$W/target.log" &
	TPID=$!
	for i in $(seq 50); do grep -q listening "$W/target.log" 2>/dev/null && break; sleep 0.1; done
}

# session NAME PREP [FROM] -- ARGS...: run ARGS twice, PREP before each (fresh discs, a fresh
# target); the second run snaps every 0.05 s from FROM seconds (default 0; -N: N seconds before
# the end) to the end
session() {
	name=$1 prep=$2; shift 2
	from=0; [ "$1" != -- ] && { from=$1; shift; }
	shift
	$prep
	end=$("$EMU" "$@" </dev/null 2>&1 >/dev/null | sed -n 's/.*cyc=\([0-9]*\).*/\1/p')
	[ -n "$end" ] || { echo "$name: no end"; exit 1; }
	mkdir -p "$W/$name"; rm -f "$W/$name"/*.scr
	snaps=()
	while read -r t f; do snaps+=(-snap "$t" "$W/$name/$f.scr"); done < <(python3 -I -c "
import sys
f, e = float(sys.argv[1]), int(sys.argv[2]) / 1250000
t = max(0.0, e + f if f < 0 else f)
while t <= e: print('%.2f s%08d' % (t, round(t * 100))); t += 0.05" "$from" "$end")
	$prep
	"$EMU" "$@" "${snaps[@]}" </dev/null >/dev/null 2>&1
	echo "$name: ends at $(python3 -I -c "print(round($end/1250000, 1))") s, ${#snaps[@]} snap args"
}
# shot SESSION PNG NEEDLE...: render the first snapshot of SESSION holding every NEEDLE at a prompt
shot() {
	s=$1 png=$2; shift 2
	f=$(python3 -I tools/pick_snap.py "$W/$s" "$@")
	cp "$f" "$W/$png.scr"
	python3 -I tools/render_screens.py --crt "$f" "$OUT/$png.png"
}

# --- boot and LDIR; the console's pitfalls (read-only discs) -----------------------------------
none() { :; }
session boot none -- -0 "$SYS68" -cycles $((300 * 1250000)) -type prompt 'LDIR\r' -type prompt 'ldir\r'
shot boot 01_ldir "TOTAL BAD BLOCKS    8"
shot boot 02_lower_case "> ldir" "ERROR 03"
# BACK SPACE and RUB OUT, typed a key at a time once TEKDOS is up
# (TEKDOS drops keys typed while it loads a command from the disc: the second line waits)
keys=(); t=14; for c in D U M Q '\x08' P ' ' 0 ' ' F '\r' 24 D U M Q '\x7f' P ' ' 0 ' ' F '\r'; do
	case "$c" in [0-9][0-9]) t=$c; continue;; esac
	keys+=(-type "$t" "$c"); t=$(python3 -I -c "print(round($t + 0.3, 1))"); done
session keys none 10 -- -0 "$SYS68" -cycles $((34 * 1250000)) "${keys[@]}"
shot keys 03_backspace_rubout "DUMQQP 0 F"
z80ro() { fresh "$SYSZ80" ro.img; }
session ro z80ro -- -0 "$W/ro.img" -cycles $((300 * 1250000)) -type prompt 'ASM DEMO;O DEMO;A DEMO%S\r'
shot ro 04_write_protected "I/O ERROR=0F" "*ASM* EOJ"

# --- the Z80 DEMO: build, run, trace, debug (tests/runz80.sh) ------------------------------------
z80() { fresh "$SYSZ80" z80.img; }
session z80run z80 -- -rw -0 "$W/z80.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM DEMO;O DEMO;A DEMO%S\r' -type prompt 'LINK DEMO;L DEMO;K DEMO;O\r' -type prompt 'LOAD DEMO;L\r' \
	-type prompt 'PATCH 500 0102030405\r' -type prompt 'GO 100\r' -type prompt 'DUMP 500 50F\r' \
	-type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r' \
	-type prompt 'BKPT 0108 R\r'
shot z80run 10_z80_build "0500=01 02 03 04 05"
shot z80run 11_z80_trace "0108 05       DEC  B          0000 42 0F 00"
shot z80run 12_z80_debug_ended "> BKPT 0108 R" "ERROR 03"
z80built() { fresh "$SYSZ80" z80.img; "$EMU" -rw -0 "$W/z80.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM DEMO;O DEMO;A DEMO%S\r' -type prompt 'LINK DEMO;L DEMO;K DEMO;O\r' </dev/null >/dev/null 2>&1; }
session z80dbg z80built -- -rw -0 "$W/z80.img" -cycles $((1200 * 1250000)) \
	-type prompt 'LOAD DEMO;L\r' -type prompt 'PATCH 500 0102030405\r' -type prompt 'DEBUG\r' -type prompt 'BKPT 0108 R\r' -type prompt 'GO 100\r' \
	-type prompt 'DSTAT\r' -type prompt 'SET RA 77\r' -type prompt 'TRACE ALL STEP\r' -type prompt 'GO\r' -type prompt 'GO\r' \
	-type prompt 'ABORT DEBUG\r' -type prompt 'PATCH 10C 76\r' -type prompt 'GO 100\r' -type prompt 'DEBUG\r' -type prompt 'GO 100\r' -type prompt 'DSTAT\r'
shot z80dbg 13_z80_debug "0106 86       ADD  A,(HL)     0000 28 79"
shot z80dbg 14_z80_halt "R=18"

# --- the 6800 (tests/run6800.sh) ----------------------------------------------------------------
m68() { fresh "$SYS68" m68.img; "$TEKMATO" -open "$W/m68.img" -write tests/data/run68.s 'RUN68;S' text -save >/dev/null 2>&1; }
session m68run m68 -- -rw -0 "$W/m68.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM RUN68;O RUN68;A RUN68;S\r' -type prompt 'LINK RUN68;L RUN68;K RUN68;O\r' \
	-type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'GO 300\r' -type prompt 'DUMP 32E 32F\r' \
	-type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 300\r'
shot m68run 20_m68_run "HELLO FROM THE 6800" "0320=00 00 14 14"
shot m68run 21_m68_trace "0315 B7FEF7 STA  A  FEF7"
m68built() { m68; "$EMU" -rw -0 "$W/m68.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM RUN68;O RUN68;A RUN68;S\r' -type prompt 'LINK RUN68;L RUN68;K RUN68;O\r' </dev/null >/dev/null 2>&1; }
session m68dbg m68built -- -rw -0 "$W/m68.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'DEBUG\r' -type prompt 'BKPT 030F R\r' -type prompt 'GO 300\r' \
	-type prompt 'DSTAT\r' -type prompt 'SET RA 11 22\r' -type prompt 'TRACE ALL STEP\r' -type prompt 'GO\r' -type prompt 'ABORT DEBUG\r' -type prompt 'DUMP 32E 32F\r'
shot m68dbg 22_m68_debug "C4 22"

# --- ASM and LINK on the Assembler Processor (tests/asm_link.sh) ----------------------------------
al() { fresh "$SYS68" al.img; "$TEKMATO" -open "$W/al.img" -write tests/data/hello.s 'HELLO;S' text -save >/dev/null 2>&1; }
session asm al -- -rw -0 "$W/al.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM HELLO;O HELLO;A HELLO;S\r' -type prompt 'LINK HELLO;L HELLO;K HELLO;O\r' -type prompt 'COPY HELLO;A CONO\r'
shot asm 30_asm_link "TRANSFER ADDRESS IS 0100" "*LINK* EOJ"
shot asm 31_asm_listing "Symbol Table" "*COPY* EOJ"

# --- printer, modem and paper tape (tests/syscomm.sh) ---------------------------------------------
sc() { rm -f "$W/lpt" "$W/rem" "$W/ppt"; }
session comm sc -- -0 "$SYS68" -1 "$DATA1A" -lpt "file:$W/lpt" -rem "file:$W/rem" -ppt "file:$W/ppt" -cycles $((600 * 1250000)) \
	-type prompt 'LDIR 0 LPT1\r' -type prompt 'COPY V;S/1 REMO\r' -type prompt 'COPY V;S/1 PPTP\r'
shot comm 40_syscomm "COPY V;S/1 PPTP"

# --- FORMAT and DUP (tests/disc_tools.sh): only the last 30 s are snapped ----------------------
disc() { fresh "$SYS68" disc.img; rm -f "$W/blank.img"; }
session disc disc -30 -- -rw -0 "$W/disc.img" -1 "new:$W/blank.img" -cycles $((20000 * 1250000)) \
	-type prompt 'FORMAT 1 SCRATCH\r' -type prompt 'DUP 0 1 BACKUP\r' -type prompt 'LDIR 1\r'
shot disc 50_format_dup "BACKUP" "TOTAL BAD BLOCKS    0"

# --- modes 1 and 2 against tools/proto_target.py (tests/prototype.sh) ------------------------------
# proto NAME [TARGET OPTIONS] -- ARGS: as session, with a fresh target before each run (its port
# is only known then)
proto() {
	name=$1; shift; targs=(); while [ "$1" != -- ]; do targs+=("$1"); shift; done; shift
	fresh "$SYSZ80" p.img; target "${targs[@]}"
	end=$("$EMU" -target "tcp:127.0.0.1:$PORT" -rw -0 "$W/p.img" "$@" </dev/null 2>&1 >/dev/null | sed -n 's/.*cyc=\([0-9]*\).*/\1/p')
	mkdir -p "$W/$name"; rm -f "$W/$name"/*.scr; snaps=()
	while read -r t f; do snaps+=(-snap "$t" "$W/$name/$f.scr"); done < <(python3 -I -c "
import sys
e = int(sys.argv[1]) / 1250000; t = 0.0
while t <= e: print('%.2f s%08d' % (t, round(t * 100))); t += 0.05" "$end")
	fresh "$SYSZ80" p.img; target "${targs[@]}"
	"$EMU" -target "tcp:127.0.0.1:$PORT" -rw -0 "$W/p.img" "$@" "${snaps[@]}" </dev/null >/dev/null 2>&1
	kill $TPID 2>/dev/null || true; wait $TPID 2>/dev/null || true; TPID=
	cp "$W/target.out" "$W/$name.target.txt"
}
proto mode1 -- -cycles $((600 * 1250000)) -type prompt 'EMULATE 1\r' -type prompt 'PATCH 100 2120017EB72805D3102318F776\r' -type prompt 'PATCH 120 48490D00\r' -type prompt 'GO 100\r' \
	-type prompt 'EMULATE 1\r' -type prompt 'MAP U 8000-80FF\r' -type prompt 'MAP R\r' -type prompt 'PATCH 200 3E5A32008076\r' -type prompt 'GO 200\r' \
	-type prompt 'MOVE UP 8000 8003 300\r' -type prompt 'DUMP 300 30F\r'
grep -q HI "$W/mode1.target.txt" || { echo "mode 1: the target printed no HI"; exit 1; }
shot mode1 60_mode1 "0300=5A FF FF FF"
proto mode2 -- -cycles $((600 * 1250000)) -type prompt 'EMULATE 2\r' -type prompt 'PATCH 100 2120017EB72805D3102318F776\r' -type prompt 'PATCH 120 4D4F444520320D00\r' \
	-type prompt 'DUMP 100 12F\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r'
grep -q "MODE 2" "$W/mode2.target.txt" || { echo "mode 2: the target printed no MODE 2"; exit 1; }
shot mode2 61_mode2_trace "010C BREAK"
echo "done: $OUT"
