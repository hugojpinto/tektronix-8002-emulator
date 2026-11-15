#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# The Real-Time Prototype Analyzer (-rtpa) under TEKDOS Z80 3.1's own commands (070-2785; the
# 070-2706 booklet: BIF, RTT and DRT are "only available in the debug system"). A loop of NOPs
# at 0100-0107 and a JP 0100 at 0108, nine fetches a turn; event 1 is a fetch at 0105.
# - EVT, BIF 1, GO: the RTPA breaks after the fetch at 0105; EVT shows the settings.
# - P=3 and CNT F (count fetches): the break comes on the third occurrence, after 6 + 9 + 9 = 24.
# - DRT: the trace holds the fetches up to the break.
# Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img "$W/sys.img"; chmod u+w "$W/sys.img"
run() { "$EMU" -rtpa -fdc "$FDC" -prom roms/tekdos_boot.bin -rw -0 "$W/sys.img" -cycles $((300 * 1250000)) \
	-type prompt 'PATCH 100 0000000000000000C30001\r' -type prompt 'DEBUG\r' -type prompt 'EVT 1 A=0105 B=F\r' "$@" \
	</dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n'; }
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }

run -type prompt 'BIF 1\r' -type prompt 'GO 100\r' -type prompt 'DSTAT\r' -type prompt 'EVT\r' -type prompt 'DRT\r' > "$W/break.txt"
check "break at the event"   "0105 BREAK"                    "$W/break.txt"
check "resumes after it"     "PC=0106"                       "$W/break.txt"
check "EVT shows event 1"    "EVT1  A=0105 B=F"              "$W/break.txt"
check "EVT shows BIF"        "BIF-1 S"                       "$W/break.txt"
check "DRT: the trace"       "0105  00  NOP          00000000  M R F" "$W/break.txt"
run -type prompt 'EVT 1 P=3\r' -type prompt 'BIF 1\r' -type prompt 'CNT F\r' -type prompt 'GO 100\r' -type prompt 'CNT\r' > "$W/pass.txt"
check "pass count 3"         "0105 BREAK"                    "$W/pass.txt"
check "CNT: 24 fetches"      "COUNT=00024 FTCH"              "$W/pass.txt"
exit $fail
