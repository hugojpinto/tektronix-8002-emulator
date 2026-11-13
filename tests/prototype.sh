#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Emulation modes 1 and 2, with tools/proto_target.py as the prototype (notes/prototype-link.md).
#   Z80 board (TEKDOS 3.1 Z80): mode 1, a program in program memory writes to the prototype's
#   UART (port 10h) and, through the memory map, to its RAM; MOVE UP reads it back (the
#   firmware's GET). Mode 2: PATCH puts a program into the prototype (PUT), it runs from there,
#   DUMP reads it (GET), and TRACE ALL traces it.
#   6800 board (TEKDOS 3.3 6800): mode 1 through the map, and mode 2, traced to its WAI.
#   8080 board (TEKDOS 8080 3.0): mode 1 through the map, MOVE UP (GET); mode 2, PATCH (PUT),
#   DUMP (GET), the program run from the prototype and traced (its rebuilt P2 firmware).
# An "emulator halted" error (46) puts TEKDOS back in mode 0 (its halt handler, 3B7A), so
# each run here selects its mode again. Run from the repository root; needs python3.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d)
TPID=
trap '[ -n "$TPID" ] && kill $TPID 2>/dev/null; rm -rf "$W"' EXIT
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }
# start a fresh prototype on a free port; its console output goes to $W/$1.out
target() {
	name=$1; shift
	[ -n "$TPID" ] && kill $TPID 2>/dev/null && wait $TPID 2>/dev/null
	PORT=$(python3 -I -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
	python3 -I tools/proto_target.py --tcp "$PORT" --log "$@" > "$W/$name.out" 2> "$W/$name.log" &
	TPID=$!
	for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listening "$W/$name.log" 2>/dev/null && break; sleep 0.2; done
}
run() { img=$1; shift; "$EMU" -fdc hle -prom roms/tekdos_boot.bin -target "tcp:127.0.0.1:$PORT" -rw -0 "$img" -cycles $((600 * 1250000)) "$@" </dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n'; }

cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img "$W/z80.img"; chmod u+w "$W/z80.img"
target z1
run "$W/z80.img" -type prompt 'EMULATE 1\r' -type prompt 'PATCH 100 2120017EB72805D3102318F776\r' -type prompt 'PATCH 120 48490D00\r' -type prompt 'GO 100\r' \
	-type prompt 'EMULATE 1\r' -type prompt 'MAP U 8000-80FF\r' -type prompt 'MAP R\r' -type prompt 'PATCH 200 3E5A32008076\r' -type prompt 'GO 200\r' \
	-type prompt 'MOVE UP 8000 8003 300\r' -type prompt 'DUMP 300 30F\r' > "$W/z1.txt"
check "Z80 mode 1: prototype I/O"     "HI"                                  "$W/z1.out"
check "Z80 mode 1: OUT (10H),A cycle" "[O 4810 48 -> ff]"                   "$W/z1.log"
check "Z80 mode 1: HALT"              "*DOS* ERROR 46"                      "$W/z1.txt"
check "MAP"                           "0000-7FFF=P  8000-80FF=U"            "$W/z1.txt"
check "Z80 mode 1: mapped write"      "[W 8000 5a -> ff]"                   "$W/z1.log"
check "MOVE UP (GET)"                 "0300=5A FF FF FF"                    "$W/z1.txt"
target z2
run "$W/z80.img" -type prompt 'EMULATE 2\r' -type prompt 'PATCH 100 2120017EB72805D3102318F776\r' -type prompt 'PATCH 120 4D4F444520320D00\r' \
	-type prompt 'DUMP 100 12F\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r' > "$W/z2.txt"
check "Z80 mode 2: PATCH (PUT)"       "[W 0100 21 -> ff]"                   "$W/z2.log"
check "Z80 mode 2: DUMP (GET)"        "0100=21 20 01 7E B7 28 05 D3 10 23 18 F7 76 FF FF FF" "$W/z2.txt"
check "Z80 mode 2: runs from it"      "MODE 2"                              "$W/z2.out"
check "Z80 mode 2: fetches (M1)"      "[F 0100 00 -> 21]"                   "$W/z2.log"
check "Z80 mode 2: traced"            "0107 D310     OUT  (10),A     0000 0C 4D 00 00 00 00 01 20 0000 0000" "$W/z2.txt"

cp sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img "$W/m68.img"; chmod u+w "$W/m68.img"
target m1
run "$W/m68.img" -type prompt 'EMULATE 1\r' -type prompt 'MAP U 8000-80FF\r' -type prompt 'PATCH 400 865AB780003E\r' -type prompt 'GO 400\r' \
	-type prompt 'EMULATE 2\r' -type prompt 'PATCH 400 86A5B704804C3E\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 400\r' \
	-type prompt 'DUMP 480 48F\r' -type prompt 'DSTAT\r' > "$W/m1.txt"
check "6800 mode 1: mapped write"     "[W 8000 5a -> ff]"                   "$W/m1.log"
check "6800 mode 1: WAI"              "*DOS* ERROR 46"                      "$W/m1.txt"
check "6800 mode 2: PATCH (PUT)"      "[W 0400 86 -> ff]"                   "$W/m1.log"
check "6800 mode 2: traced"           "0402 B70480 STA  A  0480     =0480   A5 00 0000 0000 C8" "$W/m1.txt"
check "6800 mode 2: WAI break"        "0406 BREAK"                          "$W/m1.txt"

cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_3.img "$W/i80.img"; chmod u+w "$W/i80.img"
target i1
run "$W/i80.img" -type prompt 'EMULATE 1\r' -type prompt 'MAP U 8000-80FF\r' -type prompt 'PATCH 200 3E5A32008076\r' -type prompt 'GO 200\r' \
	-type prompt 'EMULATE 1\r' -type prompt 'MOVE UP 8000 8003 300\r' -type prompt 'DUMP 300 30F\r' > "$W/i1.txt"
check "8080 mode 1: mapped write"     "[W 8000 5a -> ff]"                   "$W/i1.log"
check "8080 mode 1: HLT"              "*DOS* ERROR 46"                      "$W/i1.txt"
check "8080 mode 1: MOVE UP (GET)"    "0300=5A FF FF FF"                    "$W/i1.txt"
target i2
run "$W/i80.img" -type prompt 'EMULATE 2\r' -type prompt 'PATCH 100 2120017EB7CA0E01D31023C3030176\r' -type prompt 'PATCH 120 4D4F444520320D00\r' \
	-type prompt 'DUMP 100 12F\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r' > "$W/i2.txt"
check "8080 mode 2: PATCH (PUT)"      "[W 0100 21 -> ff]"                   "$W/i2.log"
check "8080 mode 2: DUMP (GET)"       "0100=21 20 01 7E B7 CA 0E 01 D3 10 23 C3 03 01 76 FF" "$W/i2.txt"
check "8080 mode 2: runs from it"     "MODE 2"                              "$W/i2.out"
check "8080 mode 2: traced"           "0108 D310   OUT   10        0000  06  4D 00 00 00 00 01 20" "$W/i2.txt"
check "6800 mode 2: DUMP (GET)"       "0480=A5 FF"                          "$W/m1.txt"
check "6800 mode 2: DSTAT"            "M=2  6800"                           "$W/m1.txt"
# the prototype's clock (link v4): a fixed loop (20 x 256 DJNZ, then HALT) in mode 1 takes the
# target 21647 us at the board's own 4 MHz and twice that at a reported 2 MHz; the count
# includes the 8002A's two wait states per memory access in mode 1 (070-3665 §2)
loop_us() {
	target "$1" ${2:+--clock $2}
	run "$W/z80.img" -type prompt 'EMULATE 1\r' -type prompt 'PATCH 100 1614060010FE1520F976\r' -type prompt 'GO 100\r' > "$W/$1.txt"
	kill $TPID 2>/dev/null; wait $TPID 2>/dev/null; TPID=
	sed -n 's/.*after \([0-9]*\) us of target time.*/\1/p' "$W/$1.log"
}
t4=$(loop_us c4); t2=$(loop_us c2 2)
[ "$t4" = 21647 ] && echo "PASS clock: the board's 4 MHz (21647 us)" || { echo "FAIL clock: $t4 us at 4 MHz, wanted 21647"; fail=1; }
[ "$t2" = 43295 ] && echo "PASS clock: the prototype's 2 MHz (43295 us)" || { echo "FAIL clock: $t2 us at 2 MHz, wanted 43295"; fail=1; }
exit $fail
