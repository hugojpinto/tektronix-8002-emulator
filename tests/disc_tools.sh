#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-4 regression: TEKDOS's own disc tools on writable images. FORMAT a blank in drive
# 1 (it must equal the real formatted disc Tek8002_1b), COPYSYS the system onto it and
# boot from it, and DUP the system disc onto another formatted blank; ../tekmato compares
# the files with the original's. Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 TEKMATO=../tekmato/tekmato
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
[ -x "$TEKMATO" ] || { echo "SKIP: needs ../tekmato (make -C ../tekmato) to compare the discs"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
SYS=sources/bitsavers/disks/TekDOS_3.3_6800_cpy1.img
run() { "$EMU" -prom roms/tekdos_boot.bin -rw "$@" </dev/null 2>/dev/null | tr -d '\000\007' | tr '\r' '\n'; }
fail=0
pass() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fail=1; }
cp "$SYS" "$W/sys.img"; chmod u+w "$W/sys.img"
head -c 315392 /dev/zero > "$W/a.img"; head -c 315392 /dev/zero > "$W/b.img"

run -0 "$W/sys.img" -1 "$W/a.img" -cycles $((1500 * 1250000)) -type 6 'FORMAT 1 FORMATTED\r' -type 340 'COPYSYS 0 1\r' > "$W/copysys.log"
grep -q "COPYSYS COMPLETED" "$W/copysys.log" && pass "COPYSYS completed" || bad "COPYSYS did not complete"
"$TEKMATO" -open "$SYS" -export "$W/x0" >/dev/null 2>&1
"$TEKMATO" -open "$W/a.img" -export "$W/x1" >/dev/null 2>&1
n=0; for f in "$W"/x1/*; do cmp -s "$f" "$W/x0/$(basename "$f")" || { bad "COPYSYS: $(basename "$f") differs"; n=1; }; done
[ "$(ls "$W/x1" | tr '\n' ' ')" = "COPYSYS NEWDISC SYSTEM.SOF TEKDOS " ] && [ $n -eq 0 ] && pass "COPYSYS: TEKDOS, '.', COPYSYS and NEWDISC, identical" || bad "COPYSYS: wrong file set: $(ls "$W/x1" | tr '\n' ' ')"
run -0 "$W/a.img" -cycles $((25 * 1250000)) -type 6 'LDIR\r' | grep -q "TOTAL FILES         4" && pass "the COPYSYS disc boots" || bad "the COPYSYS disc does not boot"

run -0 "$W/sys.img" -1 "$W/b.img" -cycles $((3000 * 1250000)) -type 6 'FORMAT 1 X\r' -type 340 'DUP 0 1 DUPLICATED\r' > "$W/dup.log"
grep -q "\*DUP\* EOJ" "$W/dup.log" && ! grep -q "ERROR [0-9]" "$W/dup.log" && pass "DUP completed" || bad "DUP: $(grep ERROR "$W/dup.log" | head -1)"
"$TEKMATO" -open "$W/b.img" -export "$W/x2" -check -info > "$W/dup.chk" 2>&1
grep -q "volume is consistent" "$W/dup.chk" && grep -q "Disc name      DUPLICATED" "$W/dup.chk" && pass "DUP: consistent, named" || bad "DUP: volume"
diff -r "$W/x0" "$W/x2" >/dev/null && pass "DUP: every file identical" || bad "DUP: files differ"
exit $fail
