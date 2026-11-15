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
  Checked: `DU 0100 011F` dumps memory 0100–011F in hex and ASCII. **`SO xx yy`** sets the
  option bytes 1102/1103, shown as the last word of every status line: bit 1 (`SO 02`) means an
  operator is present, so subtests that need one (fitting a fixture, moving a strap) run and ask
  ("TYPE C TO CONTINUE") instead of being bypassed; bit 2 silences messages; bit 4 halts at the
  module's start, bit 5 at each subtest. The others are not yet read (by their names: FI fill,
  GO go, LP loop, PA patch, RH/WH read and write hex, SY system).
- **Ports.** DIP keeps a device table at 1150 (six bytes each: device, flags, two ports, an
  interrupt vector, a count): 0A console E8/E9, 16 disc EB/EA, 12 timer EC, CA/CC/CE the System
  Communications ACIAs, E0–E3 the emulator boards ... `LO` copies the ports of each device a
  module names into the module's own table (DIP 02FB), and the module patches them into its I/O
  routines. `DF n`/`IF n` set and clear flag bit 5 of entry n (device disabled).
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
| DE111 | 8080 DEBUG | pass |
| DE310 | Z80 DEBUG | subtests 1–D pass. E compares the board's PROM with a table the test carries: that table is the firmware, damaged (every 16th byte lost), and is where the emulator's Z80 firmware comes from (`notes/emulators.md`); the real firmware therefore fails E, as a real board would. F (instruction decode) is open |
| DE211 | 6800 DEBUG | subtests 02 (slave reset: the 6800 starts at FEF1, not at the test's program), 0E (with SVCs masked the program runs away from FEF1), 0F and 10 fail; the others pass |
| DE221 | 6800 FETCH | **fail**, see below |
| DCA11 | SYS COMM 1 | pass, with the test fixture (below), then chains (CAL) to DCC11 |
| DCC11 | SYS COMM 2 | fails: needs a cable between ports 1 and 2 ("SET BAUD RATE THE SAME FOR BOTH PORTS"), not modelled |
| D1212 | TIMER REV 2C | pass, since the timer jumper is at 100 ms (it measured 10.0 where 100.0 was expected) |
| D0141 | MASTER CPU | pass. It checks the 2650's instructions, with a HALT after each check as a trap; it used to reach one ("UIN 0036", the slave-halted interrupt that followed): a rotate with WC = 0 that flips bit 7 set OVF in the core, which the real 2650 does only when WC = 1 (`emu/src/cpu/SOURCE.txt`) |
| DE011, DE021, DA110 | 2650 DEBUG and FETCH, 1802 DEBUG | fail: those boards are not emulated |
| D0411, D0811–D0831, D0C11–D0C31, D1411, D1A11, D1A22, D1C11, D3A21, D3A30 | paper tape, TTY, GPIO, RS-232, printer, PROM programmers (1702A, 2708/04, 82S), 8080 and 6800 hardware analysers | fail or bypass: not emulated |
| D1613 | DISK1 | stops at once: "ALL DISKS TO BE TESTED MUST HAVE SCRATCH FORMATED DISKETTES" (to be run with formatted blanks in the drives) |

The Debug tests, as they first reported them (before the fixes listed in `notes/8002.md`, Debug module) (each sets the jump register, breakpoints, the command
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

**SYS COMM 1 and its fixture.** Without an operator (`SO 02`) it bypasses subtest 1, which is
where it patches its ports in, so every later subtest talked to port 00: the first failures
above said nothing about the ACIAs. Run properly it asks for a cable with a test fixture on
port 1 and the port's strap at A, then at B. The fixture is a wrap-back connector (`-rem loop`):
data back to the receiver, RTS back to CTS and DCD. What the six subtests established, all in
`machine/mc6850.h` now (from the MC6850 data sheet, confirmed by the test):
- RTS is high only with control bits 6–5 = 10; CTS high inhibits TDRE (status 0C with strap A);
  DCD high latches status bit 2 and holds the receiver in reset;
- the character time follows the control register: frame length and the ÷1/16/64 counter, on
  the board's 38.4 kHz (16 × 2400 baud) clock;
- an overrun shows in the status only after the character before it has been read (03, read,
  then 23);
- a held break comes back as one null with a framing error (13), not one per character time.
The strap is inferred: with A the fixture's RTS reaches CTS (0C), with B it does not (06), so B
holds CTS active; the test ends in B, the operating position. In the emulator it is the input
`syscomm:strap1`–`3` (1 = A; default B), moved with `-input SEC syscomm:strap1=...` when the test
asks (`tests/dip.sh` does so).

These are the next things to fix: they test exactly the Debug module and board behaviour that
`notes/8002.md` and `notes/emulators.md` inferred from TEKDOS.
