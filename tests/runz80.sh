#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-6 regression: the Z80 Emulator Processor, with Tektronix's own Z80 demonstration
# (DEMO%S on the TEKDOS 3.1 Z80 system disc, Tek8002_10). It is assembled, linked and
# loaded; five table bytes are patched in; the Z80 adds them and exits by SVC (OUT F7).
# Then TEKDOS's debugger: TRACE ALL, a read breakpoint, DSTAT (the whole Z80 register
# block), SET, TRACE STEP, and HALT (error 46 outside DEBUG, a break inside, resuming at
# the HALT: status bit 4). FDC=hle uses the high-level disc model. Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img "$W/sys.img"; chmod u+w "$W/sys.img"
run() { "$EMU" -fdc "$FDC" -prom roms/tekdos_boot.bin -rw -0 "$W/sys.img" -cycles $((1200 * 1250000)) "$@" </dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n'; }
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }

run -type prompt 'ASM DEMO;O DEMO;A DEMO%S\r' -type prompt 'LINK DEMO;L DEMO;K DEMO;O\r' -type prompt 'LOAD DEMO;L\r' \
	-type prompt 'PATCH 500 0102030405\r' -type prompt 'GO 100\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' -type prompt 'GO 100\r' > "$W/trace.txt"
check "ASM"                  "No assembly errors detected" "$W/trace.txt"
check "banner"               "TEKDOS  Z80     VERSION 3.1"  "$W/trace.txt"
check "trace header"         "LOC  INST     MNEM OPER        SP  RF RA RB RC RD RE RH RL  IX  IY" "$W/trace.txt"
check "trace: XOR A"         "0105 AF       XOR  A          0000 44 00 05 00 00 00 05 00 0000 0000" "$W/trace.txt"
check "trace: last ADD"      "0106 86       ADD  A,(HL)     0000 08 0F 01 00 00 00 05 04 0000 0000" "$W/trace.txt"
check "trace: DEC B to 0"    "0108 05       DEC  B          0000 42 0F 00 00 00 00 05 05 0000 0000" "$W/trace.txt"
n=$(grep -c "^0106 86" "$W/trace.txt")
[ "$n" -eq 5 ] && echo "PASS trace: 5 passes" || { echo "FAIL trace: $n passes, wanted 5"; fail=1; }
[ "$(grep -c '\*DOS\* EOJ' "$W/trace.txt")" -ge 2 ] && echo "PASS SVC exit, with and without DEBUG" || { echo "FAIL SVC exit"; fail=1; }

run -type prompt 'LOAD DEMO;L\r' -type prompt 'PATCH 500 0102030405\r' -type prompt 'DEBUG\r' -type prompt 'BKPT 0108 R\r' -type prompt 'GO 100\r' \
	-type prompt 'DSTAT\r' -type prompt 'SET RA 77\r' -type prompt 'TRACE ALL STEP\r' -type prompt 'GO\r' -type prompt 'GO\r' \
	-type prompt 'ABORT DEBUG\r' -type prompt 'PATCH 10C 76\r' -type prompt 'GO 100\r' -type prompt 'DEBUG\r' -type prompt 'GO 100\r' -type prompt 'DSTAT\r' > "$W/debug.txt"
check "breakpoint"           "0108 BREAK"                  "$W/debug.txt"
check "DSTAT, main set"      "PC=0109 BP=0108 R          M=0 SP=0000 RF=02 RA=01 04 00 00 00 05 01" "$W/debug.txt"
check "DSTAT, IM/IFF/ALT"    "IM=0  IFF1=0  IFF2=0    ALT SET:" "$W/debug.txt"
check "DSTAT, IX IY I R"     "IX=0000  IY=0000  I=00  R=06" "$W/debug.txt"
check "SET, then STEP"       "0109 C20601   JP   NZ,0106    0000 02 77 04" "$W/debug.txt"
check "SET carried on"       "0106 86       ADD  A,(HL)     0000 28 79 04" "$W/debug.txt"
check "HALT: emulator halted" "*DOS* ERROR 46"             "$W/debug.txt"
check "HALT under DEBUG"     "010C 76       HALT            0000 42 0F 00" "$W/debug.txt"
check "HALT: resume at HALT" "PC=010C"                     "$W/debug.txt"
grep -q "ERROR [0-9]" "$W/trace.txt" && { echo "FAIL TEKDOS reported $(grep "ERROR [0-9]" "$W/trace.txt" | head -1)"; fail=1; }
exit $fail
