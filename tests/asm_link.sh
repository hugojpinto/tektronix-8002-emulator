#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-2 regression: the Assembler Processor (Z80 slave). On a copy of the TEKDOS 3.3
# system disc with tests/data/hello.s added (by ../tekmato), TEKDOS's ASM and LINK must
# run on the Z80 and produce the expected 6800 code. Run from the repository root.
# FDC=hle runs them on the high-level disc model instead of the real controller (the
# default, step 5); commands are typed at TEKDOS's prompt.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 TEKMATO=../tekmato/tekmato FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
[ -x "$TEKMATO" ] || { echo "SKIP: needs ../tekmato (make -C ../tekmato) to put the source on the disc"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img "$W/sys.img"; chmod u+w "$W/sys.img"
"$TEKMATO" -open "$W/sys.img" -write tests/data/hello.s 'HELLO;S' text -save >/dev/null 2>&1
"$EMU" -fdc "$FDC" -prom roms/tekdos_boot.bin -rw -0 "$W/sys.img" -cycles $((1200 * 1250000)) \
	-type prompt 'ASM HELLO;O HELLO;A HELLO;S\r' -type prompt 'LINK HELLO;L HELLO;K HELLO;O\r' </dev/null 2>/dev/null | tr -d '\000\007' | tr '\r' '\n' > "$W/console.txt"
"$TEKMATO" -open "$W/sys.img" -type 'HELLO;A' > "$W/listing.txt" 2>/dev/null
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }
check "ASM ran"            "No assembly errors detected" "$W/console.txt"
check "LINK ran"           "TRANSFER ADDRESS IS 0100"    "$W/console.txt"
check "6800 code: LDA A #" "0100 8641       START  LDA A   #41H" "$W/listing.txt"
check "6800 code: BRA"     "0106 20F8              BRA     START" "$W/listing.txt"
grep -q "ERROR [0-9]" "$W/console.txt" && { echo "FAIL TEKDOS reported: $(grep "ERROR [0-9]" "$W/console.txt" | head -1)"; fail=1; }
exit $fail
