#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-3 regression: System Communications. TEKDOS 3.3 lists its directory to the line
# printer (LPT1) and copies a source file to the modem (REMO) and the paper tape punch
# (PPTP); each port is connected to a file. Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
"$EMU" -prom roms/tekdos_boot.bin -0 sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img -1 sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_1a.img \
	-lpt "file:$W/lpt" -rem "file:$W/rem" -ppt "file:$W/ppt" -cycles $((60 * 1250000)) \
	-type 6 'LDIR 0 LPT1\r' -type 25 'COPY V;S/1 REMO\r' -type 40 'COPY V;S/1 PPTP\r' </dev/null 2>/dev/null | tr -d '\000\007' | tr '\r' '\n' > "$W/console"
for f in lpt rem ppt; do tr -d '\000' < "$W/$f" 2>/dev/null | tr '\r' '\n' > "$W/$f.txt"; done
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }
# the whole listing, every character (a lost byte per pair once garbled it to "IENM BK")
check "LPT1 listing"   "FILE NAME  BLKS"           "$W/lpt.txt"
check "LPT1 totals"    "BLOCKS AVAILABLE  101"     "$W/lpt.txt"
check "REMO text"      'STITLE	"MODULE : V680X.   SECTION : V680X    FILE :V680X%."' "$W/rem.txt"
check "REMO end"       "	LIST	TRM,DBG"           "$W/rem.txt"
check "PPTP text"      "	ORG	07FF8H"            "$W/ppt.txt"
grep -q "ERROR [0-9]" "$W/console" && { echo "FAIL TEKDOS reported: $(grep "ERROR [0-9]" "$W/console" | head -1)"; fail=1; }
exit $fail
