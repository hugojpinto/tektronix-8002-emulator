# The emulator processor boards

`emu/src/machine/tek8002_emboard.*` holds what the boards share. Each board is its own class:
`tek8002_ep6800` (port E2) and `tek8002_epz80` (port E3). Both run from program memory as
slaves of the System Processor, under the Debug module. Their firmware PROMs are not preserved,
so what the firmware does is modelled from the manuals and from TEKDOS 3.1 (Z80) and 3.3 (6800).
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
| Control bit 2 | the board's fetch and access signals reach the Debug module's breakpoint and single-cycle logic. TEKDOS sets it in the run value whenever it gives breakpoints or single cycle to the Debug module, and clears it when a trace board does that job: DEB INIT checks port C2 for one (197A–1985). The ESC stop sets it together with F8 ← CF, single cycle on (2E46–2E52), and leaving DEBUG clears it (3818). Modelled: an emulator board's breakpoint and single-cycle events count only while it is set. Every test still passes with the gate, so TEKDOS always sets it where it needs them | TRACE 1A95–1ABE, BKPT 1AD7–1AF8, DEB INIT, kernel (*inferred meaning*) |
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

## 6800 (`tek8002_ep6800`, E2, 1.00 MHz)
| Item | Behaviour | Established by |
|---|---|---|
| SVCs | a read or write of FEF7–FEF2 is SVC 1–6 (ports F7–F2), while the Debug module takes SVCs. The access does not reach memory, and an instruction makes one SVC. SRB pointers at 0040–004B | 070-2349 §10, Table 10-1 |
| Firmware | F9FA restore, F9FD dump, F9F4 PUT, F9F7 GET | DEB INIT 195D–196E; the kernel's GET/PUT derivation |
| Register block | 0000 A, 0001 B, 0002–0003 X, 0004–0005 SP, 0006 CC (7 bytes, swapped with TEKDOS's buffer at 1538) | DEB CNTL 1733–174B; DSTAT 18E9–1937 |
| Status bit 7 | must read 1 in normal running. When it reads 0, TEKDOS treats the last instruction as incomplete: it resumes at PC last and resets the 6800 first (DEB CNTL 1550–155C, 15A9–15BB, 1602–1607), the same treatment it gives a halt (bit 4). The reading that fits: the 6800 stopped on an instruction boundary (its last cycle was a fetch, so PC last/next are valid). A stop in mid-instruction would need the reset and the re-run. In the model instructions are atomic, so it reads 1. The 8550 diagnostic (6800.TST) reads E2 once and does not test it, and no 6800 board manual is preserved | DEB CNTL (*inferred meaning*) |
| Status bit 6 | a 6802 fitted: DSTAT prints "6802" when set, "6800" when clear (E2 is the "6800/6802 Emulator Processor"; a "6802 probe connects to 6800 emulator") | DSTAT 1951–1957; 070-2711 Appendix E, §1 |

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
- **Firmware:** each routine is one step, with a guessed timing (40 cycles). The model saves the
  true registers. How the real dump captured the 6800's CC unchanged, or the Z80's IM, is
  unknown (the Z80's comes from the status). Where each PROM sits in the address space, beyond
  its entries, is unknown.
- **The prototype's clock:** in modes 1 and 2 the processor runs from it. The probe takes it from
  the socket's clock pin (070-3665 §1: mode 1 "allows the prototype circuit's memory, I/O, and
  clock to be exercised"; §3, Fig. 3-9). The link carries its rate (v4, `K`), and the board then
  runs at that rate: the Debug module schedules it so, and the time sent to the target follows.
  With no rate reported (v1–v3 targets), the board's own clock is used. Checked by
  `tests/prototype.sh`: a fixed loop takes the target 21 647 µs at 4 MHz and 43 295 µs at 2 MHz.
- **Inferred, not established:** the 6800's status bit 7, control bits 4 and 2 (above). The
  boards' service manuals (the Z80's optional service manual, the 6800's) would settle them.
- **The 8080 board (E1)** is not modelled. Its TEKDOS discs boot, and its DEB CNTL tests status
  bit 4 like the others.

## Checked
- `tests/run6800.sh` (18 checks) and `tests/runz80.sh` (17, Tektronix's DEMO%S) cover running,
  the SVCs, TRACE ALL and STEP, breakpoints, DSTAT, SET, and the halt instruction.
- `tests/prototype.sh` (18) covers modes 1 and 2 on both boards, against `tools/proto_target.py`:
  prototype I/O (Z80), the map, MOVE, PATCH, DUMP, and runs and traces from prototype memory.
- All in `make test`.
