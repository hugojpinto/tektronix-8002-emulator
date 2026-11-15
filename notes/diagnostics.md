# The 8002 diagnostics (DIP 4.4)

Millennium's diagnostic disc (`sources/bitsavers/disks/Millennium_internal_floppies/diag_4.4_5-23-79`,
volume "DIAGNOSTIC DISK", part 88002110-4.3) boots in place of TEKDOS: its system file DIP4
("VERSION 4.4 3/22/79", 0080–3FFF, start 0100) is the Diagnostic Interpreter Program. No manual
for it has been found; what follows was read from its code and checked by running it.
`tests/dip.sh` runs modules on the emulator.

## Using it
At power-up it prints `DIP 0114 00 0000 0000` and the prompt `$`. A line is a two-letter
command, then arguments as **pairs of hex digits** separated by spaces or commas (0488–04B6;
the digits go through 0A50, and an odd count or a non-hex character is an error). Leading
spaces are skipped, and so is a `:` (a label). An unknown command, or a bad argument, answers
`IER` (0867).

- **`LO xxxx`** loads the test file `Dxxxx` from the disc and runs it: `LO 9012` runs D9012,
  CMEM TEST. A shorter prefix finds the first match (`LO 90`). This is the way to run a module.
- The command table (0A91, five bytes each: two letters, a flag, the handler):
  AE 0BBF, BS 0956, DF 0C07, **DU** 0C32, FI 0DDD, GM 0E02, **GO** 0553, IF 0C12, **LO** 13F8,
  LP 0540, OC 0AF0, PA 0D21, RI 0E15, SO 09C4, SY 011F, WI 0E33, AS 0B74, RH 16D2, WH 162D.
  Checked: `DU 0100 011F` dumps memory 0100–011F in hex and ASCII. The others are not yet read
  (by their names: FI fill, GO go, LP loop, PA patch, RH/WH read and write hex, SY system).
- **Status lines**: a code, the test file's id, the subtest number and two data words, e.g.
  `END 9012 08 0002 0000`. The codes, from the table at 06FC: MSG, ERR, UIN, STS, END, BYP, UDT,
  OSR, ORT, PNF, LER, CAL, HLT, IER, DIP, STR. Seen so far: STS (module started), STR (subtest
  started), END (module finished), ERR (subtest failed, followed by the module's own report of
  what it found and what it should be), BYP (subtest bypassed, the hardware it needs being
  absent), UIN (unexpected interrupt, with its vector), HLT, MSG.

## The modules
The file name is D, a module number, and version digits. The module numbers follow the 8002's
port and board numbering: E0 the 2650 emulator board, E1 8080, E2 6800, E3 Z80, A1 1802, CA/CC
System Communications 1 and 2, 90/91 system and program memory.

## The emulator against it (2026-11-15, each module run for 400 emulated seconds)
| File | Module | Result |
|---|---|---|
| D9012, D9112 | CMEM, MMEM TEST (system and program memory) | pass |
| D9912, D9921, D9931, D9981 | system tests: SYS, 8080, 6800, ASM | pass (some subtests bypassed) |
| DE710 | ASSEM. PROC. TEST (Z80 Assembler Processor) | pass |
| DE141 | 8080 CHIP REV 1.C | pass (reports itself as E140) |
| D1E10 | PROBE TEST | pass (bypassed) |
| DE111, DE211, DE310 | 8080, 6800, Z80 DEBUG | **fail**, see below |
| DE221 | 6800 FETCH | **fail**, see below |
| DCA11, DCC11 | SYS COMM 1, 2 | **fail**: "STATUS BYTE ERROR AFTER SENDING HEX 9E COMMAND TO PORT IS 00 S/B 02"; after interrupt-driven output IS 00 S/B 03; break and framing IS 00 S/B 13 |
| D1212 | TIMER REV 2C | **fail** subtest 03: "TIMER ACCURACY: PERIOD IS= 10.0 S/B= 100.0 MS." |
| D0141 | MASTER CPU | ends, with two unexpected interrupts (UIN, vector 0036) |
| DE011, DE021, DA110 | 2650 DEBUG and FETCH, 1802 DEBUG | fail: those boards are not emulated |
| D0411, D0811–D0831, D0C11–D0C31, D1411, D1A11, D1A22, D1C11, D3A21, D3A30 | paper tape, TTY, GPIO, RS-232, printer, PROM programmers (1702A, 2708/04, 82S), 8080 and 6800 hardware analysers | fail or bypass: not emulated |
| D1613 | DISK1 | stops at once: "ALL DISKS TO BE TESTED MUST HAVE SCRATCH FORMATED DISKETTES" (to be run with formatted blanks in the drives) |

The Debug tests, as they report them (each sets the jump register, breakpoints, the command
byte F9 and the control byte F8, runs the board, and checks PC last, PC next and the
interrupts raised):
- **Slave reset** (CMD FE, CONTROL DF): PC last should be 0064. It is 0000 (6800), 03E8 (Z80).
  The 6800 also raises no HALT interrupt.
- **Forced interrupt** (CMD FD): PC last should be 0064 (8080) or 0066 (Z80); it stays at the
  jump address 01F4.
- **Jump, single cycle** (CMD FB, CONTROL CF), for jump addresses 0001, 0002, 0004 … : PC last
  should be the jump address and PC next one more. The 8080 board raises no single-cycle
  interrupt and keeps 01F4; the 6800 shows 0000.
- **Interrupt priority** (Z80; both breakpoints at 0040 with single cycle): BP1 is expected,
  single cycle is raised.
- **SVC** (Z80): SVC2, SVC3 … expected after each OUT, with PC last 0003, 0006 …; none are raised.
- **6800 FETCH**: single-cycle steps from 260C report SVC1, SVC2, SVC4 … instead, with PC last
  268D.

These are the next things to fix: they test exactly the Debug module and board behaviour that
`notes/8002.md` and `notes/emulators.md` inferred from TEKDOS.
