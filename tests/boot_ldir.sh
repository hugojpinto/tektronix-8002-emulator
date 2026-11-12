#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Step-1 regression: boot TekDOS from the bootstrap PROM and list the directory (LDIR)
# of each reference disc; compare the console text with tests/expected/<disc>.txt. Both
# with the high-level disc unit and with the real controller (-fdc lle, step 5), whose
# boot takes longer: consecutive sectors are about half a turn apart.
# Tek8002_1a.txt was checked against the printout photographed with that disc
# (sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_1a.jpg).
# Run from the repository root:  tests/boot_ldir.sh [--update]
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
fail=0
for mode in hle lle; do
if [ $mode = hle ]; then at=6; secs=25; else at=20; secs=40; fi
for disc in TekDOS_3.3_6800_cpy1 Tek8002_1a; do
	img=sources/bitsavers/disks/$disc.img
	[ -f "$img" ] || img=sources/bitsavers/disks/Tek8002_Belgium_Amusements/$disc.img
	out=$("$EMU" -fdc $mode -0 "$img" -prom roms/tekdos_boot.bin -cycles $((secs * 1250000)) -type $at 'LDIR\r' </dev/null 2>/dev/null | tr -d '\000\007' | tr -d '\r')
	case "$out" in *"TOTAL FILES"*) ;; *) echo "FAIL $disc ($mode): no directory listing"; fail=1; continue;; esac
	if [ "$1" = --update ] && [ $mode = hle ]; then printf '%s\n' "$out" > tests/expected/$disc.txt; echo "updated $disc"; continue; fi
	if printf '%s\n' "$out" | cmp -s - tests/expected/$disc.txt; then echo "PASS $disc ($mode)"
	else echo "FAIL $disc ($mode)"; printf '%s\n' "$out" | diff tests/expected/$disc.txt - | head -20; fail=1; fi
done
done
exit $fail
