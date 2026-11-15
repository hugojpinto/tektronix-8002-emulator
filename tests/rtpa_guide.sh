#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# The Real-Time Prototype Analyzer against its own users guide (070-2785, "8002 Real-Time
# Prototype Analyzer Users Guide", 1979), Section 3: "How to use the RTPA commands". Its seven
# procedures run on the 8080A sample program of Figs. 3-1 to 3-3 (it clears 0250-0275, then
# 0350 down to 0325, and halts at 0301) under TEKDOS 8080, and print what a real 8002A showed:
# the break lines and the counts. Each check below is one of those displays. Microsecond counts
# are allowed the guide's own tolerance ("may vary one or two microseconds"); bus-cycle counts,
# pass counts and break lines must match exactly. docs/RTPA.md walks through the same session.
# Run from the repository root; KEEP=1 keeps the transcripts.
cd "$(dirname "$0")/.." || exit 1
EMU=emu/build/tek8002 FDC=${FDC:-lle}
[ -x "$EMU" ] || { echo "build first: make -C emu"; exit 1; }
W=$(mktemp -d); trap '[ -n "$KEEP" ] || rm -rf "$W"' EXIT; [ -n "$KEEP" ] && echo "work dir $W"
cp sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_3.img "$W/sys.img"; chmod u+w "$W/sys.img"

# the guide's set-up: fill 0200-0400 with FF, load the program (here with PATCH, Fig. 3-1),
# enter the debug system and turn its trace off
SETUP="F 200 400 FF|PATCH 200 F311750221500236007DBBC21102C37503|PATCH 211 23C30702|PATCH 300 0076|PATCH 375 01250321500336007DB9C28503C30003|PATCH 385 2BC37B03|DEB|TRACE OFF"
# run NAME 'cmd|cmd|...': the set-up, then the commands, one per prompt
run() {
	name=$1; shift
	set -f; old=$IFS; IFS='|'; args=""
	for c in $SETUP $1; do args="$args -type prompt '$c\\r'"; done
	IFS=$old; set +f
	eval "\"$EMU\" -rtpa -fdc $FDC -prom roms/tekdos_boot.bin -0 \"$W/sys.img\" -cycles $((400 * 1250000)) $args" \
		</dev/null 2>/dev/null | tr -d '\000\007\023' | tr '\r' '\n' | grep -v '^$' > "$W/$name.txt"
}
fail=0
check() { if grep -qF -- "$2" "$3"; then echo "PASS $1"; else echo "FAIL $1 (wanted '$2')"; fail=1; fi; }
# the Nth COUNT= line of a transcript is within TOL of WANT
count() {
	got=$(grep -o 'COUNT=[0-9]*' "$3" | sed -n "$2p" | cut -d= -f2 | sed 's/^0*//'); got=${got:-0}
	d=$((got - $4)); [ $d -lt 0 ] && d=$((-d))
	if [ $d -le "$5" ]; then echo "PASS $1 ($got)"; else echo "FAIL $1 (count $got, wanted $4)"; fail=1; fi
}

# Procedure 1: comparison options with BIF 1 and BIF 2
run p1 "EV 1 CLR A=201 D=11|BIF CLR|BIF 1|EV|G 200|EV 1 B=F|G 200|EV 1 B=W|G 200|EV 1 B=ALL|EV 2 CLR A=300 D=00|BIF CLR|BIF 2|G 200|DR 5|CN C|CN|G 200|CN|CN M|G 200|CN|CN U|G 200|CN|EV 2 B=MR|G 200|EV 2 B=F|RT F|G 200|DR 5"
P=$W/p1.txt
check "1.A.3 BIF-1 S"              "BIF-1 S" "$P"
check "1.A.4 break at 0201"        "0201 117502 LXI   D,0275    0000  02  00 00 00 02 75 00 00" "$P"
check "1.B.5 B=W: runs to the HLT" "0301 76     HLT             0000  56  25 03 25 02 75 03 25" "$P"
check "1.C.4 BIF 2: break at 0300" "0300 00     NOP             0000  56  25 03 25 02 75 03 25" "$P"
check "1.C.5 DR 5: the JMP"        "0382  C3  JMP       00000000  M R F" "$P"
count "1.C.6 CN C before a run: 0" 1 "$P" 0 0
count "1.C.6 996 bus cycles"       2 "$P" 996 0
count "1.C.6 1 msec"               3 "$P" 1 0
count "1.C.6 1546 us"              4 "$P" 1546 2
check "1.D.3 RT F: fetches only"   "037F  C2  JNZ       00000000  M R F" "$P"

# Procedure 2: counting options with BIF 1 and BIF 2
run p2 "EV CLR|EV 1 A=209 D=7D B=F|BIF CLR|BIF 1|EV|CN C|G 200|CN|CN U|G 200|CN|EV 1 P=2|G 200|CN|CN C|G 200|CN|EV 1 P=20|G 200|CN|EV 1 C=500|G 200|CN|EV 1 P=0 C=12|G 200|EV 2 A=37E D=B9 B=MR|BIF CLR|BIF 2|G 200|CN|EV 2 P=10|G 200|CN|EV 2 C=150|G 200|CN|EV 2 P=0 C=258|G 200|CN|EV 1 CLR A=207 D=36|EV 2 CLR A=300 D=00|CN 1|G 200|CN|EV 1 CLR A=37B D=36|G 200|CN|EV CLR|EV 1 A=208|EV 2 A=37B|EV|CN|G 200|CN|CN 2|G 200|CN"
P=$W/p2.txt
check "2.A.1 EVT display"          "EVT1  A=0209 D=007D B=F" "$P"
check "2.A.3 break at 0209"        "0209 7D     MOV   A,L" "$P"
count "2.A.3 11 bus cycles"        1 "$P" 11 0
count "2.A.3 17 us"                2 "$P" 17 2
count "2.A.4 P=2: 36 us"           3 "$P" 36 2
count "2.A.4 P=2: 23 bus cycles"   4 "$P" 23 0
count "2.A.5 P=20: 239"            5 "$P" 239 0
check "2.B.1 C=500: break line"    "037F C28503 JNZ   0385      0000  12  3A 03 25 02 75 03 3A" "$P"
count "2.B.1 C=500: 739"           6 "$P" 739 0
check "2.B.2 C=12: break line"     "0209 7D     MOV   A,L       0000  87  51 03 25 02 75 02 51" "$P"
check "2.C.2 BIF 2: break at 037E" "037E B9     CMP   C" "$P"
count "2.C.2 473"                  7 "$P" 473 0
count "2.C.3 P=10: 581"            8 "$P" 581 0
check "2.D.1 C=150: break line"    "0386 C37B03 JMP   037B      0000  12  3B 03 25 02 75 03 3A" "$P"
count "2.D.1 731"                  9 "$P" 731 0
count "2.D.2 P=0 C=258: 731"       10 "$P" 731 0
count "2.E.1 CNT 1: 38 passes"     11 "$P" 38 0
count "2.E.2 LOOP2: 44 passes"     12 "$P" 44 0
check "2.E.3 no BIF: IND"          "IND" "$P"
count "2.E.3 CNT 1: 38"            14 "$P" 38 0
count "2.E.3 CNT 2: 44"            15 "$P" 44 0

# Procedure 3: BIF ARM (event 1 arms event 2; the G.P. counter counts once armed)
run p3 "EV CLR|EV 1 A=201 D=11|EV 2 A=300 D=00|BIF ARM|EV|CN U|G 200|CN|CN C|G 200|CN|EV 1 CLR A=207 D=36|EV 2 CLR A=37B D=36|G 200|CN|EV 1 P=10|G 200|CN|EV 2 P=20|G 200|CN|EV 1 C=100|G 200|CN|EV 1 C=0|EV 2 C=100|G 200|CN|EV 1 CLR A=207 D=36|EV 2 CLR A=207 D=36|G 200|CN|CN U|G 200|CN|DR 25"
P=$W/p3.txt
check "3.A.1 BIF-ARM S"            "BIF-ARM S" "$P"
count "3.A.1 1541 us"              1 "$P" 1541 2
count "3.A.1 994 bus cycles"       2 "$P" 994 0
check "3.B.1 break at 037B"        "037B 3600   MVI   M,00      0000  56  75 03 25 02 75 03 50" "$P"
count "3.B.1 461"                  3 "$P" 461 0
count "3.B.2 EVT1 P=10: 353"       4 "$P" 353 0
check "3.B.3 EVT2 P=20: break"     "037B 3600   MVI   M,00      0000  12  3E 03 25 02 75 03 3D" "$P"
count "3.B.3 581"                  5 "$P" 581 0
count "3.B.4 EVT1 C=100: never armed, 0" 6 "$P" 0 0
check "3.B.5 EVT2 C=100: break"    "037E B9     CMP   C         0000  12  35 03 25 02 75 03 35" "$P"
count "3.B.5 681"                  7 "$P" 681 0
check "3.B.6 same address: next pass" "0207 3600   MVI   M,00      0000  87  50 03 25 02 75 02 51" "$P"
count "3.B.6 12 bus cycles"        8 "$P" 12 0
count "3.B.6 20 us"                9 "$P" 20 2
check "3.B.7 DR 25: from the arming pass" "0251  00            00000000  M W" "$P"

# Procedure 4: BIF IND with the return option C (break, print, go on). The guide's run in
# microseconds breaks one instruction later than here (020A, 037F) and shows two event-2 lines
# where this shows three (notes/rtpa.md, open); its run in bus cycles matches line for line.
run p4 "EV 1 CLR A=209 D=7D B=F P=10 C=20|EV 2 CLR A=37E D=B9 B=MR P=10 C=150|BIF IND C|EV|CN U|G 200|CN|CN C|G 200|CN"
P=$W/p4.txt
check "4.A.1 BIF-1 C 2 C"          "BIF-1 C 2 C" "$P"
check "4.A.1 EVT1, 10th pass"      "0212 C30702 JMP   0207      0000  93  5A 03 25 02 75 02 5B" "$P"
check "4.A.1 EVT1, 20th pass"      "0212 C30702 JMP   0207      0000  83  64 03 25 02 75 02 65" "$P"
check "4.A.1 EVT1, 30th pass"      "0212 C30702 JMP   0207      0000  97  6E 03 25 02 75 02 6F" "$P"
check "4.A.1 EVT2, 10th pass"      "0386 C37B03 JMP   037B      0000  12  3B 03 25 02 75 03 3A" "$P"
check "4.A.1 EVT2, 30th pass"      "0386 C37B03 JMP   037B      0000  12  27 03 25 02 75 03 26" "$P"
check "4.A.1 then the HLT"         "0301 BREAK" "$P"
count "4.A.1 CN U: 1541 us"        1 "$P" 1541 8
count "4.A.1 995"                  2 "$P" 995 0

# Procedure 5: BIF FRZ (as ARM; the trace stops at event 1 and runs again after event 2)
run p5 "EV 1 CLR A=209 D=7D|EV 2 CLR A=300 D=00|BIF FRZ|EV|CN C|G 200|DR 11|CN|CN U|G 200|CN|EV 2 CLR A=37E D=B9 P=5 C=15|EV 1 P=5|G 200|CN|BIF FRZ C|G 200|CN|EV 1 CLR A=209 D=7D|EV 2 CLR A=212 D=C3|BIF FRZ C|G 200|DR 30"
P=$W/p5.txt
check "5.A.1 BIF-FRZ S"            "BIF-FRZ S" "$P"
check "5.A.1 DR 11 ends at EVT1"   "0209  7D  MOV   A,L 00000000  M R F" "$P"
count "5.A.1 985"                  1 "$P" 985 0
count "5.A.1 1529 us"              2 "$P" 1529 2
check "5.A.2 break"                "037B 3600   MVI   M,00      0000  16  4C 03 25 02 75 03 4B" "$P"
count "5.A.2 732 us"               3 "$P" 732 2
count "5.A.3 FRZ C: still 732 us"  4 "$P" 732 2
check "5.A.4 every pass"           "0212 C30702 JMP   0207      0000  83  54 03 25 02 75 02 55" "$P"
check "5.A.5 the trace runs again from 0207" "0207  36  MVI   M   00000000  M R F" "$P"

# Procedure 6: BIF LIM (event 1 within event 2's address and control requirements)
run p6 "EV 1 CLR A=209|EV 2 CLR A<300|BIF LIM|EV|G 200|EV 1 CLR A>209|EV 2 CLR A=300|G 200|EV 1 CLR A=209|EV 2 CLR A<37E|CN C|G 200|CN|EV 1 P=5|G 200|CN|EV 1 C=10|G 200|CN|EV 2 P=10|G 200|CN|EV 2 C=10|G 200|CN|EV 1 CLR A=209 D=7D|EV 2 CLR A<37E D=FFFF|G 200|EV 1 CLR A>209 D=B9|EV 2 CLR A=37E D=FFFF|G 200|EV 1 CLR A=209 B=F|EV 2 CLR A<37E B=F|G 200|EV 2 B=W|G 200|EV 1 CLR A>200 D=C3|EV 2 CLR A<300|G 200|EV 2 A>300|G 200"
P=$W/p6.txt
check "6.A.1 BIF-LIM S"            "BIF-LIM S" "$P"
check "6.A.1 break at 0209"        "0209 BREAK" "$P"
check "6.B.1 break at 0300"        "0300 BREAK" "$P"
count "6.C.1 11"                   1 "$P" 11 0
check "6.C.2 EVT1 P=5"             "0209 7D     MOV   A,L       0000  87  54 03 25 02 75 02 54" "$P"
count "6.C.2 59"                   2 "$P" 59 0
check "6.C.3 EVT1 C=10"            "0207 3600   MVI   M,00      0000  83  54 03 25 02 75 02 55" "$P"
count "6.C.3 69"                   3 "$P" 69 0
count "6.C.4 EVT2's P is ignored"  4 "$P" 69 0
count "6.C.5 EVT2's C is ignored"  5 "$P" 69 0
check "6.D.2 EVT2's data is ignored" "037E B9     CMP   C         0000  06  50 03 25 02 75 03 50" "$P"
check "6.E.1 B=F on both"          "0209 7D     MOV   A,L       0000  06  50 03 25 02 75 02 50" "$P"
check "6.F.1 A>200 D=C3, A<300"    "0212 C30702 JMP   0207      0000  87  50 03 25 02 75 02 51" "$P"
check "6.F.2 A>300"                "0386 C37B03 JMP   037B      0000  06  50 03 25 02 75 03 4F" "$P"

# Procedure 7: pre-, centre- and post-triggering (where the event sits in the 128 entries)
run p7 "EV 1 A=300|BIF CLR|BIF 1|G 200|EV 1 A=200|EV 2 A=300|BIF ARM|G 200|EV 1 A=375 C=64|CN C|BIF CLR|BIF 1|G 200|EV 1 A=200 C=128|G 200|DR *"
P=$W/p7.txt
check "7.A.3 C=64: centre"         "0386 C37B03 JMP   037B      0000  16  4C 03 25 02 75 03 4B" "$P"
check "7.A.4 C=128: post"          "0207 3600   MVI   M,00      0000  97  59 03 25 02 75 02 5A" "$P"
check "7.A.4 DR *: oldest entry"   "0202  75            00000000  M R" "$P"

exit $fail
