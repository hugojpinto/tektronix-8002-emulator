#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# The 8080A Emulator Processor (TEKDOS 8080 3.0, Tek8002_3), running its two PROMs: P1 as
# read, P2 rebuilt by tools/rebuild_8080_p2.py (notes/emulators.md, "The 8080 board").
# - The 8080A/8085A users manual's own trace (070-2702-01 §12, TRACE example): its first
#   lines, value for value. That runs P2's dump and restore after every instruction.
# - Every register through the dump (the carry across its DAD SP), SET and the restore, and a
#   STEP that computes with the restored values.
# - P1's Intel MDS-800 monitor calls through P2's SVC templates: CO and CI on the console
#   (channels 1 and 0), PO to a disc file and LO to the console (3 and 4), RI reading the file
#   back (2). P1's start-up (F824) sizes memory and jumps to the program at 0020.
# Modes 1 and 2 (GET and PUT) are in tests/prototype.sh. Run from the repository root.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 TEKMATO=../tekmato/tekmato FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap '[ -n "$KEEP" ] || rm -rf "$W"' EXIT; [ -n "$KEEP" ] && echo "work dir $W"
cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_3.img "$W/sys.img"; chmod u+w "$W/sys.img"
run() { "$EMU" -fdc "$FDC" -prom roms/tekdos_boot.bin -rw -0 "$W/sys.img" -cycles $((600 * 1250000)) "$@" </dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n'; }
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }

# the manual's average program, its first instructions (13FF holds 12)
run -type prompt 'PATCH 0 AF476711FF131A1B76\r' -type prompt 'PATCH 13FF 12\r' -type prompt 'DEBUG\r' \
	-type prompt 'TRACE ALL\r' -type prompt 'GO 0\r' -type prompt 'DSTAT\r' > "$W/manual.txt"
check "manual: XRA A"      "0000 AF     XRA   A         0000  46  00 00 00 00 00 00 00" "$W/manual.txt"
check "manual: LXI D"      "0003 11FF13 LXI   D,13FF    0000  46  00 00 00 13 FF 00 00" "$W/manual.txt"
check "manual: LDAX D"     "0006 1A     LDAX  D         0000  46  12 00 00 13 FF 00 00" "$W/manual.txt"
check "manual: DCX D"      "0007 1B     DCX   D         0000  46  12 00 00 13 FE 00 00" "$W/manual.txt"
check "HLT breaks"         "0008 BREAK"                                                  "$W/manual.txt"
check "DSTAT"              "PC=0008                       M=0 SP=0000 RF=46 RA=12 00 00 13 FE 00 00" "$W/manual.txt"

# every register; ADD A of 80 leaves A 00 with Z, P and the carry (RF 47)
run -type prompt 'PATCH 100 3100200122111144332166553E80873E5A000076\r' -type prompt 'DEBUG\r' -type prompt 'TRACE ALL\r' \
	-type prompt 'GO 100\r' -type prompt 'TRACE ALL STEP\r' -type prompt 'SET SP 1FF0 13 A1 B2 C3 D4 E5 F6 07\r' \
	-type prompt 'DSTAT\r' -type prompt 'GO 10E\r' > "$W/regs.txt"
check "registers"          "0109 216655 LXI   H,5566    2000  02  00 11 22 33 44 55 66" "$W/regs.txt"
check "the carry kept"     "010F 3E5A   MVI   A,5A      2000  47  5A 11 22 33 44 55 66" "$W/regs.txt"
check "SET"                "SP=1FF0 RF=13 RA=A1 B2 C3 D4 E5 F6 07"                        "$W/regs.txt"
check "STEP after SET"     "010E 87     ADD   A         1FF0  07  42 B2 C3 D4 E5 F6 07" "$W/regs.txt"

# the monitor: CO prints HI; PO writes POK to a file, LO prints LO
run -type prompt 'ASSIGN 1 CONO\r' -type prompt 'PATCH 20 0E48CD09F80E49CD09F80E0DCD09F876\r' -type prompt 'GO F824\r' > "$W/co.txt"
check "monitor CO"         "HI"                                                          "$W/co.txt"
run -type prompt 'ASSIGN 3 PUNCHED\r' -type prompt 'ASSIGN 4 CONO\r' \
	-type prompt 'PATCH 20 0E50CD0CF80E4FCD0CF80E4BCD0CF80E0DCD0CF8\r' -type prompt 'PATCH 34 0E4CCD0FF80E4FCD0FF80E0DCD0FF876\r' \
	-type prompt 'GO F824\r' -type prompt 'CLOSE 3\r' > "$W/pl.txt"
check "monitor LO"         "LO"                                                          "$W/pl.txt"
if [ -x "$TEKMATO" ]; then
	"$TEKMATO" -open "$W/sys.img" -type PUNCHED > "$W/punched.txt" 2>&1
	check "monitor PO"     "POK"                                                         "$W/punched.txt"
else echo "SKIP monitor PO (needs ../tekmato)"; fi
# RI reads the file back, CO echoes it
run -type prompt 'ASSIGN 2 PUNCHED\r' -type prompt 'ASSIGN 1 CONO\r' -type prompt 'PATCH 20 CD06F84FCD09F8CD06F84FCD09F8CD06F84FCD09F876\r' \
	-type prompt 'GO F824\r' > "$W/ri.txt"
check "monitor RI"         "POK"                                                         "$W/ri.txt"
# CI reads a key (typed while the program waits), CO echoes it in upper case
run -type prompt 'ASSIGN 0 CONI\r' -type prompt 'ASSIGN 1 CONO\r' -type prompt 'PATCH 20 CD03F8EE204FCD09F876\r' \
	-type prompt 'GO F824\r' -type 100 'q' > "$W/ci.txt"
check "monitor CI"         "Q*DOS* ERROR 46"                                             "$W/ci.txt"

for f in manual regs co pl ri; do grep -q "ERROR [0-9]" "$W/$f.txt" && ! grep -q "ERROR 46" "$W/$f.txt" && { echo "FAIL $f: TEKDOS reported $(grep "ERROR [0-9]" "$W/$f.txt" | head -1)"; fail=1; }; done
exit $fail
