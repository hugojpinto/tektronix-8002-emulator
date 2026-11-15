# The emulator processor boards

`emu/src/machine/tek8002_emboard.*` holds what the boards share. Each board is its own class:
`tek8002_ep6800` (port E2) and `tek8002_epz80` (port E3). Both run from program memory as
slaves of the System Processor, under the Debug module. The 6800 board's two PROMs were dumped
in 2026 and run as they are (below, "The 6800 board's PROMs"). The Z80 board's firmware was
never dumped; it is reconstructed from a table Millennium's diagnostics carry (below, "The Z80
board's firmware"), and runs on the Z80 too.
The boards' own service manuals are not on bitsavers (070-2701 lists the 6800's).

Sources:
- 070-2349 (6800 Assembler and Emulator Users Manual) §10–12.
- 070-2711 §1 ("Emulator Processor Module" and the debug-mode walkthrough, steps 1–41), §6,
  and Appendix E (ports: E1 8080, E2 "6800/6802", E3 Z80, E5 8085).
- 070-2701 §4 (EMULATE) and §7 (operating modes, MAP, MOVE).
- ztpe.nl §8, the same Debug card under Signetics' DOS.
- TEKDOS's kernel and its overlays in `.`. These are MOD modules, each starting on a sector
  with its own header and load address: DEB CONTROL at 1500, the others at 1880.
- The sibling tektronix-8550-emulator's `notes/8301.md`, for the same board family in the 8550.
- 070-3665 (Z80 Emulator Processor and Prototype Control Probe installation, 1980/83, in the
  sibling's sources): the same board for the 8001, 8002A, 8540 and 8550 (8002F18/F33). It covers
  jumpers, clock and wait states, but not the registers.

## Shared by the boards
| Item | Behaviour | Established by |
|---|---|---|
| Control byte | Bit 7 = active. Bit 1 = 0 maps the firmware (firmware calls 80, 90, C0, C4, D0; user runs 92, 96, B2, F2). Bits 6–5 = mode (00 = 0, 01 = 1, 11 = 2). Bits 4 and 2 below | TEKDOS writes (`-io`); EMULATE (Z80 EMULATO 18C5–1926: 90 \| mode << 5, then \| 02); the 8550's rule (8301.md "E3 write bits") |
| Control bit 2 | the board's fetch and access signals reach the Debug module's breakpoint and single-cycle logic. TEKDOS sets it in the run value whenever it gives breakpoints or single cycle to the Debug module, and clears it when a trace board does that job: DEB INIT checks port C2 for one (197A–1985). The ESC stop sets it together with F8 ← CF, single cycle on (2E46–2E52), and leaving DEBUG clears it (3818). Modelled: an emulator board's breakpoint and single-cycle events count only while it is set. Every test still passes with the gate, so TEKDOS always sets it where it needs them. **Z80 board only:** the diagnostics run the 6800 and 8080 boards with bit 2 clear (E2/E1 ← 90) and expect breakpoints and single cycle (DE211, DE111), so there it gates nothing; their Z80 test sets it (E3 ← 94) | TRACE 1A95–1ABE, BKPT 1AD7–1AF8, DEB INIT, kernel (*inferred meaning*); the diagnostics |
| Control bits, as Millennium named them | The 9508's emulator control source (S170EMU on `99900821-01`, Millennium Systems' 6800-based product on 8002 boards, which reaches the 8002's ports as memory at 20xx) names the board's command latch: bit 0 CLOL "open loop: a 1 disables BUSREQ in the Z80", 1 CLPD "PROM disable", 2 CLDB "DEBUG", 3 CLWD "wait disable: a high disables user WAIT request for Z80 and 8085", 4 CLIE "INTEN: a low disables interrupts", 5 CL12 "modes 1 and 2", 6 CL2M "mode 2 and memswitch", 7 CLAC "slave active". Its values: 80 mode 0, B6 mode 1, F6 mode 2 ("full ICE"), C0 memswitch, 8F/89 mode 0 for its utilities. This confirms bits 1, 2, 4 and 6–5 as inferred here; TEKDOS never sets bits 0 and 3 (not modelled) | `sources/bitsavers/disks/Millennium_internal_floppies/`, 9508 emulator control, "EMULATOR COMMAND LATCH EQUATES" and its listing L170EMU |
| Control bit 4 | set for every user run (92, 96, B2, F2) and the start-up probe (90), clear for every debug firmware call (80, C0, C4). Modelled as letting the prototype's INT/NMI through, after the OS/40 Z80 specifics' "Interrupts are detected only when user code is being executed" (070-3939). No TEKDOS code tests anything else that fits | TEKDOS writes; sibling 8301.md (*inferred*) |
| Status, bit 5 | board present | the 8550's Z80 diagnostic (configuration probe 4454) |
| Status, bit 4 | the processor is halted (6800 WAI, Z80 HALT). TEKDOS then resumes at PC last, the halt instruction itself, and on the 6800 resets the board first | kernel 3C4C–3C55 and 2E3F; DEB CNTL on 6800, Z80 and 8080; 070-2349 §11 "continues at the beginning of the WAIT instruction" |
| Undriven status bits | read 0 (the open bus, `notes/8002.md`) | — |
| Forced jump | sets the PC; it also ends a halt. TEKDOS adds a forced interrupt for a halted Z80 (DEB INIT 1AC5–1AD4), as DOS/50 does; here the jump alone is enough | 070-2711 §6 |
| Restore | loads the registers from the block TEKDOS lends at 0000, then debug SVC 1 (port F1, interrupt 22) | 070-2711 §1 steps 9–19; TEKDOS forces it before GO and in its start-up probe |
| Dump | stores the registers there, then debug SVC 1 | §1 steps 34–37; TEKDOS forces it after each traced instruction or break |
| GET / PUT | a byte of prototype memory. TEKDOS's kernel (35AC–363B) lends 0000–0003: [0] data, [1] [2] address (high, low), [3] status (00 = done). It writes C4 to the board and forces a jump to its table base minus 8 (GET) or minus 11 (PUT), then waits for debug SVC 2 (port F0, interrupt 23, handler 3BF7) | the kernel routine; `-io` and `TEK_EPTRACE` during MOVE, PATCH and DUMP in modes 1/2 |
| Waiting | after signalling, the firmware waits in the PROM for the next forced jump | — |

The kernel calls GET/PUT whenever TEKDOS's own memory access targets the prototype: in mode 2,
or in mode 1 for a block the map marks U (it reads the map bit at C9 itself). This covers DUMP,
PATCH, the trace line's instruction bytes (070-2711's "UGET"), and MOVE. MOVE sets 2032 to D0
for U and D2 for P to steer that routine (MOVEO 18A3–18C6). The entries come from the board
table at 24CB: F8 0B on the Z80 discs, F9 FF on the 6800's.

## 2650 (`tek8002_ep2650`, E0, 1.25 MHz) — added 2026-11-15
No manual, schematic or PROM dump of this board is known. Millennium's 2650 DEBUG and 2650 FETCH tests (DE011, DE021; `notes/diagnostics.md`) are the specification, and both pass. TEKDOS 1.6 for the 2650 (on the Millennium discs) runs programs on it (`GO`; a HALT gives its ERROR 46); its DEBUG needs the board's dump/restore firmware, which is lost (open).

| Item | Behaviour | Established by |
|---|---|---|
| Processor | a Signetics 2650 on MAME's core (the System Processor's), 15-bit addresses, 1.25 MHz (inferred, as the System Processor) | — |
| Fetches | the 2650 has no fetch pin; the board marks the first read of each instruction as its fetch | DE021's single steps |
| RESET | clears the IAR, the interrupt inhibit and an interrupt waiting; the registers, the PSW and the return address stack stay. MAME's reset clears them all; the board uses a `[tek8002]` `reset_pin()` | Signetics 2650 manual (1975), "RESET"; DE021 steps a BCFA on the flags, BDRR on R0, and RETC/RETE through forced RESET + JUMPs |
| Forced jump | loads the IAR (`[tek8002]` `set_pc`) | DE011, DE021 |
| Stops (breakpoint, single cycle) | the board stops the 2650 with PAUSE when it sees the next fetch; PAUSE counts at the start of an instruction's last cycle, so a **two-cycle instruction at PC next runs before the stop** (one of three or four cycles does not). Inferred | the manual's PAUSE ("finishes the instruction in progress ... If PAUSE is turned on then off again before the last cycle of the current instruction begins, program execution continues"); DE021 follows each instruction with a NOP and a HALT, and expects the IORZ a BIRR branches to to have run, and the BDRR a BCFA branches to not to have |
| SVCs | an extended output (WRTE) to F0–F7 while the Debug module takes them (inferred, as the other boards' OUT); DE011's SVC subtests pass | DE011 |
| Firmware | none modelled: the lab's dump/restore PROM is not known | open |

## 6800 (`tek8002_ep6800`, E2, 1.00 MHz)
| Item | Behaviour | Established by |
|---|---|---|
| SVCs | a read or write of FEF7–FEF2 is SVC 1–6 (ports F7–F2), while the Debug module takes SVCs. The access does not reach memory, and an instruction makes one SVC. SRB pointers at 0040–004B | 070-2349 §10, Table 10-1 |
| Firmware | F9FA restore, F9FD dump, F9F4 PUT, F9F7 GET: the SYSTEM PROM at F800–F9FF, run by the 6800 | DEB INIT 195D–196E; the kernel's GET/PUT derivation; confirmed by the PROM (below) |
| Register block | 0000 A, 0001 B, 0002–0003 X, 0004–0005 SP, 0006 CC (7 bytes, swapped with TEKDOS's buffer at 1538) | DEB CNTL 1733–174B; DSTAT 18E9–1937; the PROM's direct addresses 01–06 (A reaches 0000 by the FETCH PROM's steering) |
| Status bit 7 | must read 1 in normal running. When it reads 0, TEKDOS treats the last instruction as incomplete: it resumes at PC last and resets the 6800 first (DEB CNTL 1550–155C, 15A9–15BB, 1602–1607), the same treatment it gives a halt (bit 4). The reading that fits: the 6800 stopped on an instruction boundary (its last cycle was a fetch, so PC last/next are valid). A stop in mid-instruction would need the reset and the re-run. In the model instructions are atomic, so it reads 1. The 8550 diagnostic (6800.TST) reads E2 once and does not test it, and no 6800 board manual is preserved | DEB CNTL (*inferred meaning*) |
| Status bit 6 | a 6802 fitted: DSTAT prints "6802" when set, "6800" when clear (E2 is the "6800/6802 Emulator Processor"; a "6802 probe connects to 6800 emulator") | DSTAT 1951–1957; 070-2711 Appendix E, §1 |
| RESET | sets the I mask and loads the PC (a forced jump: from the jump register); A, B, X, SP and the other CC bits stay | MC6800 data sheet; Millennium's 6800 FETCH test (DE221) sets CC with TAP, then steps a BMI with RESET + JUMP and expects it taken. MAME's reset clears CC; the board model keeps it. (So TEKDOS's DEBUG shows the program's own N bit after a WAI: `tests/run6800.sh`) |
| One step, one instruction | the instruction after TAP or CLI is a step of its own (the IRQ hold-off is kept); MAME ran it inside TAP/CLI | DE221 expects PC next = PC + 1 for both (`[tek8002]` in `tools/port_m6800.py`, `emu/src/cpu/SOURCE.txt`) |
| Unknown opcodes | an opcode the FETCH PROM gives 00 (one the 6800 lacks) **halts** the board: the halt interrupt, PC last at the opcode, PC next FFFF (the idle bus) | Millennium's 6800 DEBUG test (DE211, slave reset) runs CLI; JMP 0064 onto an 04 and expects the halt with PC last 0064; DE221 ends each step with an 04 and expects PC next FFFF after all 58 unknown opcodes. (The PROM gives opcode 00 a 2-cycle count, unlike the other unknown ones) |
| FETCH PROM steering | applies to the instruction's data cycles, not its operand bytes | DE221 single-steps a JSR extended (flagged "to 0000" like PSHA, PULA) and expects it to reach its target |
| I/O page FE00–FEFF | in mode 0 the board turns it into **I/O cycles on the 8002 bus** (not program memory), and the RTPA sees them as I/O; in modes 1 and 2 it is the prototype's memory | 070-2349 §10 ("memory mapped I/O ... FE00 through FEFF is reserved"); Millennium's 6800 hardware analyzer test (D3A30) reads FE00 as 00 with four program memory boards, and traces FE00 as I/O cycles |
| SVC window FEF0–FEF7 | an access takes **one clock more** (inferred) | D3A30 counts one clock more per loop than its instructions take, in loops that each read FE00 and write FEF7 (TRACE I/O 95 µs, TRACE I/O READS 217 clocks): a stretch of the whole I/O page would give two; the SVC decode is the path only the write takes. A stretch of I/O-page reads only, or writes only, fits the test as well |
| Bus cycles to the RTPA | one clock each (1 MHz, no wait states: 070-2785 Appendix A), so its time counts run at cycle resolution | D3A30, all 16 runnable subtests |

## The 6800 board's PROMs
Two 82S115s (512 × 8), "SYSTEM" and "FETCH", dumped by Eelco Vriezekolk ([ztpe.nl](https://ztpe.nl/2650/), the EEVblog
thread's "2650") with his own 82S115 reader, sent 2026-11-13. Originals in `sources/ztpe/6800_board/`,
copies in `roms/ep6800/` (`roms/MANIFEST.md`). `build/dasm m6800 roms/ep6800/6800_system.bin f800`
disassembles the first.

**SYSTEM** holds 74 bytes of 6800 code at offsets 1B6–1FF; the rest is 00. With the PROM at
F800–F9FF, the four entries TEKDOS's DEB INIT derives are a jump table of `BRA; NOP` pairs:
| Entry | Routine | Code |
|---|---|---|
| F9F4 PUT | F9E4 | `LDX $01; LDA $00; NOP; STA 0,X; TAB; NOP; LDA 0,X; CBA` → status 00 if the byte reads back, else the byte read; `STA $03; JMP $FEF0` |
| F9F7 GET | F9D5 | `LDX $01; NOP; LDA 0,X; STA $00; LDA #0; STA $03; JMP $FEF0` |
| F9FA restore | F9C4 | `LDB $01; LDX $02; LDS $04; DES; LDA $06; TAP; PULA; JMP $FEF1` |
| F9FD dump | F9B6 | `PSHA; INS; TPA; STA $06; STB $01; STX $02; STS $04`, then the restore's tail from `LDS $04` |
- **The register block and the GET/PUT block are as inferred from TEKDOS**, and the routines end
  in debug SVC 1 (FEF1) and debug SVC 2 (FEF0), as modelled.
- **The CC is read with TPA** after `PSHA; INS`, which leave the flags alone: that answers how
  the dump kept the CC. TPA must give bits 7–6 as 1: MAME's core lost them after a `TAP` of
  00, which the PROM exposed; fixed in the core (`emu/src/cpu/SOURCE.txt`).
- **A never goes into the block by address.** The dump pushes it (`PSHA`, SP restored by `INS`)
  and the restore pulls it (`DES` … `PULA`). TEKDOS reads and writes A at block byte 0, and
  does not look at the user's stack: with SP = 0FF9 and A = 5A, the pushed 5A sat at 0FF9 while
  DSTAT showed 00, until the steering below was modelled (checked 2026-11-13).
- **The SVC jump stops the board:** the 6800 halts at its fetch of FEF1/FEF0 until TEKDOS's next
  forced jump, so nothing there runs (modelled as a NOP).
- **SVCs in modes 1 and 2:** TEKDOS turns SVCs off for the user's runs there (F9 bit 4). FEF0–FEF7
  reach the Debug module only while SVCs are on, the firmware's debug SVCs included: the
  diagnostics' SVC mask test (DE211 subtest 0E) masks them around a firmware call and expects
  none. (Until 2026-11-15 the firmware's were modelled as always reaching it, *inferred* from GET
  and PUT working in modes 1 and 2; `tests/prototype.sh` passes either way, so TEKDOS has SVCs
  on for the firmware's runs.) The 8080 board's OUT F0–F7 likewise.

**FETCH** is a table indexed by the opcode (offsets 000–0FF; 100–1FF are 00, the address bit
above the opcode presumably unused or a state). Every opcode the 6800 lacks reads 00.
- **Bits 3–0 = 17 − the instruction's cycle count**, for every opcode: 2-cycle immediates `F`,
  3-cycle direct loads `E`, branches `D`, indexed `C`, … BSR `9`, JSR extended `8`, RTI (10) `7`,
  SWI (12) `5`, WAI (9) `8`. The 6800 has no fetch pin; the board counts each instruction's
  cycles from its fetch to know when the next fetch comes (the Debug module's FETCH, PC last
  and single cycle need it). Status bit 7 ("stopped on an instruction boundary", above) is
  likely this counter at its end.
- **Bit 4 = 0** for exactly the instructions with operand bytes.
- **Bits 7–5 are 1 except for a handful:** bit 7 clear for PSHA, PULA and JSR (indexed and
  extended); bit 6 clear for NOP, LDA indexed and STA indexed; bit 5 clear for NOP, PSHA, PULA
  and WAI. PSHB, PULB, LDB and STB are not flagged. These are what the SYSTEM PROM uses: PSHA and
  PULA move A, the indexed LDA/STA reach the prototype in GET/PUT, each after a NOP that does
  nothing else, and WAI is the halt.
- **Modelled (*inferred*):** while the firmware is mapped, a data cycle of an instruction whose
  bit 7 is clear goes to 0000 (so PSHA/PULA move A to and from the block's byte 0), and one
  whose bit 6 is clear goes to the prototype in modes 1 and 2 (GET and PUT, as 070-2711 §1's
  UGET; in mode 0 to program memory). That reproduces everything TEKDOS shows. What the NOPs,
  JSR's bit 7, WAI's bit 5 and the counter drive in hardware is not established; that needs the
  board's schematic (its service manual is not on bitsavers).
- `-ep6800 hle` keeps the earlier high-level routines (the same results; `tests/run6800.sh`).

## 8080A (`tek8002_ep8080`, E1, 3 MHz)
The 8080A/8085A Emulator Processor, as an 8080A, running its two PROMs (070-2702-01, the
8080A/8085A Assembler and Emulator Users Manual, March 1979 revision, §11–12). Checked by
`tests/run8080.sh` (15 checks) and `tests/prototype.sh` (7).
| Item | Behaviour | Established by |
|---|---|---|
| SVCs | `OUT` to F7 … F0 then a `NOP` ("allowing time for the SVC to occur"); SRB pointers at 0040–004B | 070-2702 §11 |
| Halt | `HLT`; GO resumes at it | §12 |
| Firmware | P2's table at FBE4 PUT, FBE7 GET, FBEA restore, FBED dump (TEKDOS's base FBEF, on six discs: Tek8002_3, 4, 5, 6a, 7a and 11) | the PROM; DEB INIT's derivation as for the other boards |
| Register block | 0000 L, 01 H, 02 E, 03 D, 04 C, 05 B, 06 A, 07 F, 08 scratch, 09/0A SP | the firmware; TEKDOS's trace and DSTAT match the manual's example; the 8550's 8085 board (sibling `notes/8301.md`) has the same layout, and its unknown byte 8 is this scratch |
| PROMs visible | F800–FBFF during firmware calls and mode-0 user runs | *inferred*: P1's monitor needs SVCs, which modes 1/2 turn off |
| GET/PUT in modes 1/2 | the firmware's `LDAX D` / `STAX D` reach the prototype | *inferred*: the only instructions GET and PUT use for it (as the 6800's FETCH PROM flags its own) |
| Clock | 3 MHz in mode 0 (2 MHz was inferred until 2026-11-15) | 070-2785 (RTPA users guide) Appendix A, "8080A Emulator Processor: Mode 0 System Clock 3 MHz" (prototype clock up to 2.08 MHz); its worked example measures 2728 µs for 6404 clocks plus 1793 wait states; the guide's sample-program counts (17 and 36 µs) and the diagnostics' D3A21 agree |
| Wait states | one per memory cycle in modes 0 and 1, none from prototype memory in mode 2 | 070-2785 Tables A-1 (one per machine cycle for the 8080A/8085A, two for the Z80, none for the 6800) and A-4 (8197 clocks measured for 6404); none on I/O cycles, inferred from D3A21 subtest 0D (694 clocks, exact) |

### The PROMs
Read by Eelco Vriezekolk ([ztpe.nl](https://ztpe.nl/2650/)) on 2026-11-13: P1, 160-0028-00,
F800–F9FF, and P2, 160-0027-00, FA00–FBFF (`sources/ztpe/8080_board/`; P1 loads FA00, FA0A and
FA14). `build/dasm i8080 FILE F800` disassembles them.

**P1 is an Intel MDS-800 monitor interface.** Its jump table at F800 is the MDS monitor's
vector: CI F803, RI F806, CO F809, PO F80C, LO F80F, CSTS F812, IOCHK F815 and IOSET F818
(the I/O byte at 0003), MEMCK F81B (B = top of memory, A = EF), IODEF F81E and SPCL F821
(both `RET`), F824 start-up and F827 reader rewind. F800 is a `HLT` loop. Each call is made
through SVC 1: it saves 0040–00C9 to the top page, copies a 10-byte template from P2 to 0040
(the SVC 1 pointer, 0042, then the SRB), issues `OUT F7` and restores the page. PO and LO
collect a line in 128-byte buffers in the top pages and write it at CR; RI reads a line and
hands it out a character at a time. The start-up (F824) finds the top of RAM (the first page
that does not keep a complemented byte), puts the stack below it, sets 0003 = 94, the buffer
pointers, 0007 = 0A and 0014 = FA, and jumps to 0020. So 8080 programs written for Intel's
development system run unchanged on the 8002; the 1978 manual does not mention it.

**P2's read lost data bit 0.** No byte of 512 is odd (P1 has 293), and the chip is faulty:
Eelco cleaned the pins and read it again, with the same result. `tools/rebuild_8080_p2.py`
rebuilds it: bits 7–1 as read, and bit 0 set at 130 addresses, each listed with its reason and
a confidence. `--report` grades every one of the 512 bytes: A 152 (verified under TEKDOS, or
forced by the decoding), B 294 (one sensible reading), C 66 (a choice). Only the channels and
the line lengths (8 bytes) would show to a TEKDOS user; 6 more never matter (equivalent entries,
unused table entries); the rest are addresses inside Intel's assembler. How it was attacked (2026-11-13):
- **The empty zones** (FA32–FA3F, FB31–FB7F, FBDA–FBDD, FBF0–FBFF, and the byte before PUT)
  are 00, not 01: every intact PROM of this family that Tektronix programmed fills unused space
  with 00 (the 6800 SYSTEM PROM's 438 unused bytes, its FETCH PROM's 256; not one 01 or FF).
- **The assembler compiled from its source:** version 2.0 in the original PL/M (ASSV1, with Mark
  Ogden's C port of the Fortran PL/M compiler, after repairing a 66-column truncation that had
  cut five comments and hidden a DO) and versions 2.0 and 4.1 in PL/M-80 (ASSV2, ASSV3, with the
  port of ISIS PL/M-80). No build lands on the tape's addresses, which belong to a later, larger
  version ("8080 MDS MACRO ASSEMBLER"; the compiled 2.0 says "8080 MACRO ASSEMBLER, VERSION
  2.0"). But the original-PL/M build has the tape's shape: the I/O procedures and LIST first and
  below 0060, the hex printer next, variables near the top, free memory just above. That
  settles the page of the output routines (00); the PL/M-80 builds, with their DATA tables
  first, are not the tape's lineage.
- **Intel's assembler run on the board (2026-11-13):** version 2.0, compiled for the MDS
  monitor (its seven monitor addresses set to P1's F803 … F81B), loaded with TEKDOS's RVHEX and
  started at F824, runs: banner and "P=" through CO, the pass number through CI, the source
  through RI line by line across passes (a file holding the source three times stands for
  feeding the tape again: this version does not call P1's reader rewind), and the listing
  through LO, with correct code for backward references. So P1 and P2's templates carry real
  Intel software end to end. Two places in the compiled assembler misbehave because the
  compiler port evaluates an embedded assignment before the other operand, where the original
  compiler did not (LOOKUP's binary search, written `LOC+0 <> (LOC:=…)`, had to be rewritten for
  the test; forward references still come out wrong): faults of the rebuilt assembler, not of
  the board.
- **Operands decided by structure:** jumps inside P2 and calls into P1 are forced; Tek's own
  variables (1CD8, 1CDA, 1CDC, 1CDE, 1CE0) can only be at even addresses, or the three words
  and two flags would overlap, but the whole block could be one page up (1D).
- **Mutation testing:** each byte TEKDOS executes (FA00–FA31, FB80–FBEF) had its bit 0 flipped
  and the checks rerun: 117 of 162 mutants fail. The survivors are fields TEKDOS overwrites
  (each template's status, data and count), code never executed (the NOP after each SVC,
  padding, the two unused table entries), entries one byte before their code (a NOP), the line
  lengths, and the dump's tail, which TEKDOS never observes because it always restores before
  resuming. Those are graded B or C, not A. PUT's read-back was invisible until the test
  wrote to ROM in the prototype (`proto_target.py --ro`): TEKDOS then reports
  `*PATCH* ERROR 59`.
- **A blind rebuild** by an independent agent that saw only the two reads and the manuals
  agrees on 508 of 512 bytes. The four others are the line lengths (7E) and FAFD (20), all
  graded C here.
- **No other copy** of any part of P2: 836 files (both emulators' disc images, the DOS/50
  discs' files, every ROM), searched with bit 0 masked.
- **The channels** (CI 0, CO 1, RI 2, PO 3, LO 4) cannot be tested: TEKDOS pre-assigns none,
  so a user assigned whatever the board used. No Tektronix document about the monitor
  interface was found; the 8080A Emulator Processor service supplement (070-3380-00) is not
  online.

P2 holds:
- **FA00–FA31, P1's SVC templates.** Each is 00 42 (the pointer), then function, channel,
  status, data, count, length and buffer 004A. The functions are forced by what is legal and by
  P1 (CI 41 read binary, CO 42 write binary, RI 01 read ASCII, PO and LO 02 write ASCII). The
  channels are CI 0, CO 1, RI 2, PO 3, LO 4: each pair must differ, and the read allows only
  that or a swap within the pairs, so the order is a choice. The line lengths are 7F or 7E (7F
  chosen).
- **FA40–FB20, patch routines for a RAM-resident 8080 program.** Each replaces an instruction
  in it, does its work and runs the displaced instruction. They keep a BCD line counter (1CDA,
  starting at 9999 so the first line is 0000), print it as four hex digits through the
  program's routine at 0282, or four spaces when it has not changed, followed by four spaces;
  test the column and the I/O byte's list field; print a pending CR LF; and when the source
  read through RI ends, or reaches ^Z, feed back " END *MISSING*" CR LF a character at a time.
  The program is Intel's paper-tape **8080 MDS Macro Assembler**: the MDS-800 operator manual
  (98-129A, 1975) loads it from tape and starts it at 0020, where P1's start-up jumps. Its PL/M
  source (versions 2.0 and 4.1, on mark-ogden.uk) lists to column 72 (the patch's column 64 is
  72 less the 8 the line number adds), tests `IOCHK AND 0C0H` for the list device (the patch's
  `ANI C0`), and reads until an END statement, which under TEKDOS would never come. No binary
  is known, and no disc in `sources/` calls into P2 or the monitor, so this part cannot be run.
  Its bytes that are addresses inside the assembler are kept as read although 1 would fit as
  well.
- **FB21–FB30, the text** " END *MISSING*" CR LF.
- **FB80–FBDD, the debug firmware.** All verified under TEKDOS:
  - dump (FB80): `SHLD 0000; STA 0006`; A = the carry; `LXI H,0; DAD SP`; then
    `LXI SP,000B; PUSH H; RAR` (the carry back); `PUSH PSW; DCX SP; PUSH B; PUSH D`. It ends
    by restoring SP, HL and A, then `OUT F1; NOP`;
  - restore (FBA9): `LXI SP,0002; POP D; POP B; INX SP; POP PSW; POP H; SPHL; LHLD 0000;
    LDA 0006; OUT F1; NOP`;
  - GET (FBBB): `LHLD 0001; MOV E,H; MOV D,L; LDAX D; STA 0000; OUT F0; NOP`. It does not
    write the status byte;
  - PUT (FBC8): the same address, `STAX D; LDAX D; XRA B; STA 0003`, so the status is 00 when
    the byte reads back;
  - the table (FBDE–FBEF): two unused `JMP 0000`, then PUT, GET, restore and dump. GET's and
    restore's entries could equally be the NOP before them.

## Z80 (`tek8002_epz80`, E3, 4.00 MHz)
Mode 0 runs from the board's clock: jumper J1 gives 2 or 4 MHz on the Z80A board. The Z80B
board's J3003 gives 4 or 6 MHz, but "only 4 MHz or below" in an 8002A (070-3665 §2). On
8001/8002A systems "two wait states are inserted each time the program memory is accessed, or
when running in emulation Mode 1. No wait states are inserted in Mode 2", except in the debug and
memswitch jump sequences. The model adds 2 T-states to each memory cycle on that rule (the
jump sequences are firmware here).

| Item | Behaviour | Established by |
|---|---|---|
| SVCs | OUT to F7 … F0, while the Debug module takes SVCs | TEKDOS's DEMO%S ("OUT (0F7H),A … EXIT FROM USER PROGRAM") |
| Firmware | F806 restore, F809 dump, F800 PUT, F803 GET: the entries of the 8550's Z80 board | DEB INIT 1958–1969; the kernel |
| Register block | 1C bytes. 1 IM (bits 1–0), IFF1 (bit 6), IFF2 (bit 7); 2 R; 3 I; 4–B L′ H′ E′ D′ C′ B′ A′ F′; C–D IY; E–F IX; 10–17 L H E D C B A F; 18–19 SP (low first). Bytes 0, 1A and 1B are not shown | DEB CONTROL 1702 (swap of 1C bytes); DSTAT 1983–1A62 |
| Status bits 7, 6, 1–0 | IFF2, IFF1 and IM. TEKDOS stores the status byte into the block's byte 1 after each break (DEB INIT 1ADA–1AE0), and that is what DSTAT shows. Software cannot read IM on a Z80, so the board must follow it (how is not known) | DEB INIT; DSTAT; `IM 2`, `EI` shows IM=2 IFF1=1 IFF2=1 |

The 8550's DOS/50 keeps IM in byte 0 and IFF1/IFF2 in bits 7/6 of byte 1. That is the same
entries with a different block, so the two firmware PROMs differ.

## The Z80 board's firmware
Never dumped. Millennium's diagnostic disc (`notes/diagnostics.md`) carries a table it can be
reconstructed from: the Z80
DEBUG test's PROM subtest (DE310 subtest 0E) compares the board's F9FF, F9FE … F800, as the
System Processor reads them through program memory with the firmware mapped (E3 ← 80, bank 3),
with a 512-byte table at 25B9. `tools/rebuild_z80_firmware.py` rebuilds the firmware from it
(`roms/epz80/z80_firmware.bin`, `--report` for every byte it sets).
- **The table is not the firmware byte for byte.** Read as a straight image of F800–F9FF (the
  test compares one table byte per read, linearly: 2574–25AF, pointer step at 296F), its code
  breaks at regular places (JP F860 lands in the middle of an instruction; `LD (0018),SP` is
  followed by no `LD SP`; there is no `PUSH HL'`), and TEKDOS's DEBUG fails on it in the emulator
  (SP 2A00, registers 0). The difference is regular: with one byte inserted before every address
  ending in 0, and the 19 bytes stored just after the table taken as F80F down to F801, the code
  is coherent from F800 to FA27, holds TEKDOS's entries and runs TEKDOS's DEBUG. That is the
  reconstruction used here; it implies a 1K firmware (F800–FBFF; FA28 up FF, as an erased 2708).
- **Open, and not to be assumed either way:** why the table differs so. It may be a faulty copy
  (for instance made from a 16-byte-per-line listing that lost a byte per line), or the System
  Processor's view of the PROM through program memory may differ from the Z80's in a way not
  modelled here (address wiring alone cannot drop exactly the xxx0 bytes, but other board logic
  might), or the test may be an early version that never passed. Whether a real board passes
  subtest E is therefore unknown; the emulator, with the reconstruction, does not. A dump of a
  real board's PROM, or its schematic, would settle it. 35 bytes are set by reasoning: the 14
  in the code are each forced by the code around them (A, B), the 21 in the data after it are
  guesses (C).
- **What it is:** a jump table with TEKDOS's four entries (F800 → F8CA PUT, F803 → F8BD GET,
  F806 → F860 restore, F809 → F80C dump); the dump saves SP, sets SP to 0018 and pushes every
  register into the block, swapping A/F and A′/F′ into the block's order, then ends in
  `OUT (F1)` (debug SVC 1); the restore pops them back; GET and PUT end in `OUT (F0)`. The block
  layout is exactly the one inferred from TEKDOS above, now confirmed. The code ends at F8DC.
- **After the code, fill with bit noise, not data.** F8DD–F8FF is FF with bits cleared (DF, FD,
  DB, FB, and a few with more: 66, 6E, 4E, 24, 08, 20); F900–FA27 is 30 (ASCII "0") with 18
  scattered single- or double-bit flips (31, 32, 34, 38, 35, 36 gain bits, B0 and 70 gain bit 7
  or 6, 20, 10, 00 lose bit 4 or 5). Nothing reads it, and its departures from the fill have no
  structure. In a real 2708 everything past the code would most likely read FF (erased); the
  table's content looks like the memory it was made in. The 21 missing bytes there are set to
  the fill around them (FF, then 30); their exact values cannot be known and do not matter to
  anything but subtest E.
- **Not an opcode-indexed decode table either** (tested 2026-11-15, before the region was read as
  fill; it agrees,
  `tools/z80_decode/`): if the opcode drove A7–A0 in any order, any lines inverted (10,321,920
  wirings), the stored value would tell what each instruction does. `props.c` measures each
  opcode on the Z80 core (length, M1s, data and I/O cycles, T-states, flow, prefix, stack,
  HALT); `search.c` scores every wiring by the mutual information between PROM value and each
  property. The best wiring scores no better than the same values shuffled into random places
  (1.06 bits against 0.92–1.06), in this frame and in the test's own (0.86). Of its 240 known
  entries 222 are 30, and the rest are 30 with bits flipped (above).
- **The diagnostics' instruction decode test (DE310 subtest F) passes** (2026-11-15). It does not
  use this region: it single-steps 762 instructions from 0000, one at a time, each after a forced
  reset and a forced jump, and checks PC next and the interrupt raised. It established three
  things, now modelled:
  - **RESET keeps the registers** other than PC, I, R, the IFFs and the interrupt mode: the test
    sets SP and the stack in earlier steps (`LD SP,0100; LD HL,0001; PUSH HL`) and expects each
    RET to come back to 0001 after the resets in between. (The core's `z80_init` sets AF and SP
    to FFFF, a behaviour reported for some later Z80s; this board's does not, by Tektronix's test.)
    Millennium relied on the same: the 9508's dispatcher resumes a halted board's program, and enters
    its register-dump utility, with a forced *reset* plus jump (SBSFJM in S170EMU: "SET RESET AND
    JUMP" when the last interrupt was a halt), which only works if RESET keeps the registers.
  - **A halted Z80 fetches at the HALT's own address**: PC next after a HALT is the HALT (the core
    keeps its PC past it, so the board reports PC − 1 while halted).
  - **A single-cycle stop on a HALT raises single cycle, not the halt** (the Debug module).
- **What it showed about the board** (each now modelled, `tek8002_epz80.h`):
  - **The forced jump is an M1 cycle:** the Debug module jams a JP onto the bus. The dump's R
    compensation (`SUB 18`) is the 24 M1 cycles from the break to its `LD A,R`, the jammed JP
    included; the restore's (`SUB 9B; AND 7F`, i.e. less 1B) likewise. DSTAT's R follows: 05
    in `tests/runz80.sh` where the high-level model gave 06.
  - **A halted Z80 ignores a forced jump:** it reads no instructions until an interrupt ends the
    halt. TEKDOS knows: for a halted board it sends NMI + jump (F9 ← F9, DEB 1490–14A4). The
    NMI is taken first and pushes the PC; the jump replaces the fetch at 0066.
  - **Block byte 1A:** the dump adds it to SP and clears it. After a break of a halted board
    TEKDOS sets it to 2 in its copy of the block (154F; the copy at 1535–1550 is swapped with
    the block by 1702), so that at the next break the next NMI's push is taken back off: SP stays
    at FFFE (one NMI frame), as `tests/runz80.sh` shows, instead of falling by 2 each time.
  - The restore takes IM from byte 1 bits 1–0 and IFF1 from bit 6 (EI or DI), and only when
    byte 0 is non-zero; the dump writes 00 to byte 0.
- **Modes 1 and 2:** the firmware's data cycles outside the register block (GET's and PUT's (HL)
  accesses, the only ones) reach the prototype (*inferred*: the firmware keeps all its own data at
  0000–001B; nothing in the PROM marks instructions, as the 6800's FETCH PROM does). `tests/prototype.sh` passes.
- **The System Processor reads it:** while a board is active with its firmware mapped, its PROM
  answers the System Processor's program-memory reads at those addresses (so do the 6800's and
  8080's); this is how the diagnostics read it.
- `-epz80 hle` keeps the high-level routines, brought in line with the firmware (the R
  compensation, byte 1A): every session test gives the same output with either.

## Modes 1 and 2
070-2701 §7 describes the modes. Mode 0 uses program memory, system I/O and the system clock.
Mode 1 uses program memory plus the prototype's memory, as MAP assigns 128-byte blocks (U or P,
one bit each at C9), and the prototype's I/O and clock. Mode 2 uses the prototype's memory, I/O
and clock throughout. As modelled:
- **In a user run** (control bit 1 set), the processor's memory is the prototype's in U blocks
  (mode 1) or everywhere (mode 2).
- **I/O and interrupts:** the Z80's I/O, and both processors' interrupts and RESET, are the
  prototype's.
- **SVCs:** TEKDOS's EMULATE turns SVCs off in modes 1 and 2 (F9 bit 4), so FEF0–FEF7 and ports
  F0–F7 belong to the prototype then.
- **Firmware calls** (bit 1 clear) see program memory, where the register block is. This is
  070-2711's "MEMSW" mode.
- **Writes to P blocks are not copied to the prototype in mode 1.** The 8550's manual says its
  board does that; nothing says the 8002's does.
- **Link:** the prototype is reached through `prototype_bus`, over the same link the 8550
  emulator uses (`notes/prototype-link.md`; `-target unix:PATH|tcp:HOST:PORT`).
- **The 6800 has no I/O space and no fetch pin,** so its prototype sees reads and writes only.
- **TEKDOS's halt handler puts it back in mode 0** (2032 ← 92 at 3B7A) after an "emulator
  halted" error (46). Select the mode again before the next run.

## Modelled at high level, or open
- **Z80 firmware:** reconstructed and run on the Z80 (above). Past the code (F8DD on) the table
  holds fill with bit noise, so what a real chip holds there (most likely FF) is not known from it. How the board captures the Z80's IM for its status bits is
  unknown (the model follows the processor).
- **The prototype's clock:** in modes 1 and 2 the processor runs from it. The probe takes it from
  the socket's clock pin (070-3665 §1: mode 1 "allows the prototype circuit's memory, I/O, and
  clock to be exercised"; §3, Fig. 3-9). The link carries its rate (v4, `K`), and the board then
  runs at that rate: the Debug module schedules it so, and the time sent to the target follows.
  With no rate reported (v1–v3 targets), the board's own clock is used. Checked by
  `tests/prototype.sh`: a fixed loop takes the target 21 647 µs at 4 MHz and 43 295 µs at 2 MHz.
- **Inferred, not established:** the 6800's status bit 7, control bits 4 and 2 (above). The
  boards' service manuals (the Z80's optional service manual, the 6800's) would settle them.
- **The 8080 board (E1)** runs its PROMs, P2 rebuilt (above). The 8085A emulator (port E5, in
  the same manual: its trace and DSTAT add RIM and SOD) is not modelled.

## Checked
- `tests/run6800.sh` (18 checks) and `tests/runz80.sh` (17, Tektronix's DEMO%S) cover running,
  the SVCs, TRACE ALL and STEP, breakpoints, DSTAT, SET, and the halt instruction.
- `tests/prototype.sh` (18) covers modes 1 and 2 on both boards, against `tools/proto_target.py`:
  prototype I/O (Z80), the map, MOVE, PATCH, DUMP, and runs and traces from prototype memory.
- All in `make test`.
