#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Memory parity (070-2711 §4, Table 3-28): a byte with bad parity raises interrupt 3 when program
# memory is read and interrupt 1 for System Memory; TEKDOS reports them as errors 66 and 65
# (070-2701's error list). -parity injects the fault; the byte stays bad until it is written.
# FDC=hle uses the high-level disc model. Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
run() { "$EMU" -fdc "$FDC" -0 disks/TekDOS_3.3_6800_cpy1.img "$@" </dev/null 2>/dev/null | tr -d '\000\023' | tr '\r' '\n'; }
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }
# program memory: a fault at user 0003, then DUMP reads it; PATCH rewrites it, and the next DUMP is clean
run -cycles $((60 * 1250000)) -parity 20 U:3 -type 25 'DUMP 0 F\r' -type 30 'PATCH 3 AA\r' -type 35 'DUMP 0 F\r' > "$W/p.txt"
check "program memory parity: ERROR 66"   "*DOS* ERROR 66"     "$W/p.txt"
check "the data is still read"            "0000=00 00 00 00"   "$W/p.txt"
check "rewritten, then clean"             "0000=00 00 00 AA"   "$W/p.txt"
[ "$(grep -c 'ERROR 66' "$W/p.txt")" -eq 1 ] && echo "PASS one error, until the byte is written" || { echo "FAIL ERROR 66 count $(grep -c 'ERROR 66' "$W/p.txt"), wanted 1"; fail=1; }
# System Memory: TEKDOS's console-input interrupt slot (1000 + 3 x 4: the boot PROM's vector
# table branches there on every key), so the next key typed reads the faulty byte
run -cycles $((40 * 1250000)) -parity 25 100C -type 26 'LDIR\r' > "$W/s.txt"
check "System Memory parity: ERROR 65"   "*DOS* ERROR 65"       "$W/s.txt"
# nothing without a fault
run -cycles $((40 * 1250000)) -type 25 'DUMP 0 F\r' > "$W/n.txt"
grep -q "ERROR 6[56]" "$W/n.txt" && { echo "FAIL a parity error with no fault"; fail=1; } || echo "PASS no fault, no error"
exit $fail
