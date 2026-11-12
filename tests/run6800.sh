#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-6 regression: the 6800 Emulator Processor. tests/data/run68.s (put on a copy of the
# TEKDOS 3.3 system disc by ../tekmato) is assembled, linked and loaded; the 6800 sums its
# message bytes, prints the message by SVC 1 and exits by SVC 2. Then TEKDOS's debugger:
# TRACE ALL (070-2349 §11's trace line), a read breakpoint, DSTAT, SET and TRACE STEP, and
# WAI (error 46 outside DEBUG, a break inside). FDC=hle uses the high-level disc model.
# Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 TEKMATO=../tekmato/tekmato FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
[ -x "$TEKMATO" ] || { echo "SKIP: needs ../tekmato (make -C ../tekmato) to put the source on the disc"; exit 0; }
W=$(mktemp -d); trap '[ -n "$KEEP" ] || rm -rf "$W"' EXIT; [ -n "$KEEP" ] && echo "work dir $W"
cp sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img "$W/sys.img"; chmod u+w "$W/sys.img"
"$TEKMATO" -open "$W/sys.img" -write tests/data/run68.s 'RUN68;S' text -save >/dev/null 2>&1
run() { "$EMU" -fdc "$FDC" -prom roms/tekdos_boot.bin -rw -0 "$W/sys.img" -cycles $((1200 * 1250000)) "$@" </dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n'; }
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }

run -type prompt 'ASM RUN68;O RUN68;A RUN68;S\r' -type prompt 'LINK RUN68;L RUN68;K RUN68;O\r' \
	-type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'GO 300\r' -type prompt 'DUMP 32E 32F\r' > "$W/go.txt"
check "ASM"                 "No assembly errors detected"  "$W/go.txt"
check "SVC 1 output"        "HELLO FROM THE 6800"          "$W/go.txt"
check "SVC 2 exit"          "*DOS* EOJ"                    "$W/go.txt"
check "the 6800's sum"      "0320=00 00 14 14 03 30 1A 00 00 00 00 00 00 00 C4 04" "$W/go.txt"

run -type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 300\r' > "$W/trace.txt"
check "trace header"        "LOC  INST   MNEM R  OPER X/PC EADD   RA RB XREG SP   CC" "$W/trace.txt"
check "trace: LDX"          "0300 CE0330 LDX    #0330             00 00 0330 0000 C0" "$W/trace.txt"
check "trace: ADD, indexed" "0305 AB00   ADD  A X00  +0330=0330   48 00 0330 0000 C0" "$W/trace.txt"
check "trace: last ADD"     "0305 AB00   ADD  A X00  +0343=0343   C4 04 0343 0000 E8" "$W/trace.txt"
check "trace: CPX, Z set"   "030A 8C0344 CPX    #0344             C4 04 0344 0000 C4" "$W/trace.txt"
check "trace: the SVC"      "0315 B7FEF7 STA  A  FEF7     =FEF7   C4 04 0344 0000 C8" "$W/trace.txt"
n=$(grep -c "^0305 AB00" "$W/trace.txt")
[ "$n" -eq 20 ] && echo "PASS trace: 20 loop iterations" || { echo "FAIL trace: $n loop iterations, wanted 20"; fail=1; }

run -type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'DEBUG\r' -type prompt 'BKPT 030F R\r' -type prompt 'GO 300\r' \
	-type prompt 'DSTAT\r' -type prompt 'SET RA 11 22\r' -type prompt 'TRACE ALL STEP\r' -type prompt 'GO\r' -type prompt 'ABORT DEBUG\r' -type prompt 'DUMP 32E 32F\r' > "$W/bkpt.txt"
check "breakpoint"          "030F BREAK"                   "$W/bkpt.txt"
check "DSTAT"               "PC=0312 BP=030F R          RA=C4 04 RX=0344 SP=0000 CC=C8" "$W/bkpt.txt"
check "SET, then STEP"      "0312 F7032F STA  B  032F     =032F   11 22 0344 0000 C0" "$W/bkpt.txt"
check "SET reached memory"  "0320=00 00 14 14 03 30 1A 00 00 00 00 00 00 00 C4 22" "$W/bkpt.txt"

run -type prompt 'ASSIGN 1 CONO\r' -type prompt 'LOAD RUN68;L\r' -type prompt 'PATCH 319 01010101\r' -type prompt 'GO 300\r' \
	-type prompt 'DEBUG\r' -type prompt 'GO 300\r' -type prompt 'DSTAT\r' > "$W/wai.txt"
check "WAI: emulator halted" "*DOS* ERROR 46"              "$W/wai.txt"
check "WAI under DEBUG"     "031D 3E     WAI                      C4 04 0344 FFF9 D0" "$W/wai.txt"
check "WAI: resume at WAI"  "PC=031D"                      "$W/wai.txt"
for f in go trace bkpt; do grep -q "ERROR [0-9]" "$W/$f.txt" && { echo "FAIL $f: TEKDOS reported $(grep "ERROR [0-9]" "$W/$f.txt" | head -1)"; fail=1; }; done
exit $fail
