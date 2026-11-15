#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Runs modules of the 8002 diagnostics (DIP 4.4, Millennium's diagnostic disc; notes/diagnostics.md)
# and prints each one's status lines. Usage: tests/dip.sh [-v] [ID ...], ID being the test file's
# name without its D (9012 = CMEM TEST); with no ID, the modules the emulator passes, as a check.
# -v prints every line the module printed. Run from the repository root; SEC=seconds per module.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 SEC=${SEC:-400}
DISC=sources/bitsavers/disks/Millennium_internal_floppies/diag_4.4_5-23-79/diag_4.4_5-23-79.img
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
V=; [ "$1" = -v ] && { V=1; shift; }
PASSING="9012 9112 9912 9921 9931 9981 E141 E710 1E10 1212 CA11 E111"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
fail=0
for m in ${*:-$PASSING}; do
	cp "$DISC" "$W/d.img"; chmod u+w "$W/d.img"
	# what a module needs fitted, and the operator's answers (SO 02: an operator is present)
	case $m in
	CA11) set -- -rem loop -input 1 syscomm:strap1=1 -type 12 'SO 02 00\r' -type 40 'C\r' -input 60 syscomm:strap1=0 -type 70 'C\r' ;;
	*) set -- ;;
	esac
	"$EMU" -fdc lle -prom roms/tekdos_boot.bin -0 "$W/d.img" -cycles $((SEC * 1250000)) "$@" -type 15 "LO $m\r" </dev/null 2>/dev/null |
		LC_ALL=C tr -cd '\12\15\40-\176' | tr '\r' '\n' | grep -v '^ *$' > "$W/$m.txt"
	name=$(grep -A1 "LO $m" "$W/$m.txt" | sed -n 2p)
	if [ -n "$V" ]; then cat "$W/$m.txt"; fi
	if grep -q "^\(END\|CAL\) ${m%?}" "$W/$m.txt" && ! grep -q "^\(ERR\|UIN\|HLT\) ${m%?}" "$W/$m.txt"; then echo "PASS $m $name"
	else echo "FAIL $m $name: $(grep -E '^(ERR|UIN|HLT|MSG) ' "$W/$m.txt" | awk '{print $1, $3}' | sort -u | tr '\n' ' ')"; fail=1; fi
done
exit $fail
