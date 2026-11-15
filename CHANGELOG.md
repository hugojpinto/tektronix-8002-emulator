# Changelog

## 2026-11-15 — The Z80 board's firmware, rebuilt from the diagnostics
- The Z80 Emulator Processor's firmware was never dumped. Millennium's Z80 DEBUG diagnostic carries a table for its PROM test that is the firmware but for a regular pattern: read straight it is not working code; with a byte inserted before every address ending in 0, and the 19 bytes after the table taken as its start, it is coherent and holds TEKDOS's four entries (why the table differs so is open); `tools/rebuild_z80_firmware.py` rebuilds it (`roms/epz80/z80_firmware.bin`, 1K), setting 35 bytes by reasoning: 14 in the code, each forced by the code around it, 21 guessed in data the code never reads.
- It runs on the Z80, like the 6800 and 8080 boards' PROMs (`-epz80 hle` keeps the routines at high level, now matching it). TEKDOS's DEBUG gives the same output with either, and modes 1 and 2 work.
- What it showed: the forced jump is a JP jammed onto the bus (an M1 cycle, which its R compensation counts); a halted Z80 ignores it, which is why TEKDOS reaches a halted board with NMI + jump; the dump adds block byte 1A to SP (TEKDOS sets it to 2 after such a break). DSTAT's R and the SP after a HALT break change accordingly (`tests/runz80.sh`, screenshot 13).
- The emulator boards' PROMs answer the System Processor's program-memory reads while mapped, as the diagnostics read them.
- Z80 DEBUG's instruction decode test (762 single-stepped instructions) passes: a RESET keeps the Z80's registers but PC, I, R and the interrupt state; a halted Z80 fetches at the HALT's own address, which the board latches as PC next; a single-cycle stop on a HALT raises single cycle only. The F900 table of the firmware is not an opcode-indexed decode table under any wiring (`tools/z80_decode`).

## 2026-11-15 — Against the diagnostics: 13 modules pass
- MASTER CPU: the 2650 core changed OVF on rotates with WC clear; the real 2650 does so only through the carry (`emu/src/cpu/SOURCE.txt`).
- TIMER: the interval timer jumper is at 100 ms, as the test expects.
- SYS COMM 1, run with an operator (`SO 02`) and its wrap-back fixture (`-rem loop`, the CTS strap as `-input SEC syscomm:strap1=1`): the MC6850's RTS, CTS, DCD and break, its character time from the control register, and an overrun that shows only after the previous character is read.
- 8080 DEBUG passes, Z80 DEBUG to subtest D: PC last/next follow every instruction while debugging; coinciding breakpoints and single cycle raise one interrupt; the forced interrupt reaches the slave (the Z80's NMI); a reset or interrupt without a jump cancels a jump a stop had armed; SVCs only while enabled; control bit 2 gates the debug signals on the Z80 board only.
- Findings that touch the sibling 8550 emulator are listed in its ROADMAP, "To verify".

## 2026-11-15 — Millennium's internal floppies
- 80 disc images from bitsavers (`bits/Millennium/Millennium_internal_floppies.zip`), Millennium Systems' own discs for the 8002 hardware, in `sources/bitsavers/disks/Millennium_internal_floppies/` with their imaging logs (the `.a2r` flux files left in the zip).
- Every one was booted on the emulator unchanged: TEKDOS 1.6 for the 2650 (the earliest seen, with a source file), 1.9 for the 6800, 2.2 Z80, 3.1 for the Z80, 6800, 6801 and 8048, TEKTIP 1.5, UCSD Pascal's PMON, and the Diagnostic Interpreter Program 4.4 with one test file per module (master CPU, memories, disc, Debug, the 8080/6800/Z80 boards...). The CP/M discs stop where they hand over to a 64K Z80 slave; 14 are data discs with no system, and one is not in FDU geometry. The README lists them all.
- `notes/8002.md`: the front-panel connector P3 pin by pin, from the Universal One manual, confirmed on a real 8002.
- The diagnostics run: `LO xxxx` at DIP's `$` prompt loads and runs test file Dxxxx (read from DIP's code: two-letter commands, arguments in hex byte pairs; `notes/diagnostics.md`). Each failing subtest prints what it found and what it should be. `tests/dip.sh` runs modules; the nine the emulator passes (memories, system tests, Assembler Processor, 8080 chip, probe) form its default check, and the failures are the next work (ROADMAP 13).

## 2026-11-13 — The 8080A Emulator Processor, with its PROMs (one rebuilt)
- The 8080 board (port E1) runs its two PROMs, read by Eelco Vriezekolk ([ztpe.nl](https://ztpe.nl/2650/)), on MAME's i8085 core as an 8080 (copied from the sibling 8550 emulator; a constructor flag selects MAME's i8080 variant).
- P1 (160-0028-00) is an Intel MDS-800 monitor interface at F800: CI, RI, CO, PO, LO, CSTS, IOCHK, IOSET, MEMCK..., each through a TEKDOS SVC. So programs written for Intel's development system run on the 8002.
- P2 (160-0027-00) lost data bit 0 (the chip is faulty). `tools/rebuild_8080_p2.py` rebuilds it from the code's own logic, TEKDOS and the 8080 manual (070-2702-01, now in `sources/`), with a confidence for each restored bit. It holds P1's SVC templates, patch routines for an Intel-style assembler (a BCD line counter, listing columns, " END *MISSING*" fed in at the end of the source), and the debug firmware.
- Verified under TEKDOS 8080 3.0 (`tests/run8080.sh`, 15 checks; `tests/prototype.sh`, 7 more). The manual's own trace example comes out value for value. All registers go through dump, SET, restore and STEP, the carry included, which the dump saves around its `DAD SP` with `RAR`. GET and PUT work in modes 1 and 2, and so do the monitor calls CO, CI, RI, PO and LO. The register block is the one the 8550's 8085 board uses, whose byte 8 is this firmware's scratch.
- `dasm` disassembles 8080 code (`build/dasm i8080`).
- Checked: `make test` (113 checks), the screenshots byte-identical.

## 2026-11-13 — The 6800 board's PROMs, dumped and running
- Eelco Vriezekolk ([ztpe.nl](https://ztpe.nl/2650/)) dumped the 6800 Emulator Processor's two 82S115s, "SYSTEM" and "FETCH" (`roms/ep6800/`, originals in `sources/ztpe/6800_board/`). The board now runs its real firmware: SYSTEM at F800–F9FF executes on the 6800, and its routines end in their debug SVCs as before. `-ep6800 hle` keeps the high-level routines.
- What they confirmed: the four entries (F9F4 PUT, F9F7 GET, F9FA restore, F9FD dump), both register blocks and both debug SVCs, all as inferred from TEKDOS. What they settled: the dump keeps the CC by reading it with TPA after `PSHA; INS`.
- What they showed: A is moved by PSHA/PULA, never stored by address. The FETCH PROM is a per-opcode table (cycle count in its low nibble, how the board finds fetches without a fetch pin) that flags exactly PSHA/PULA and the indexed LDA/STA, the firmware's A and GET/PUT cycles. Modelled (*inferred*): while the firmware is mapped, the first go to 0000 (the block's A), the second to the prototype in modes 1 and 2. A test with SP = 0FF9 showed TEKDOS needs exactly that (`notes/emulators.md`, "The 6800 board's PROMs").
- 6800 core: TAP and RTI keep CC bits 7–6 at 1, as on the chip (MAME stored the byte as is; `emu/src/cpu/SOURCE.txt`).
- `dasm` disassembles 6800 code (`build/dasm m6800 FILE BASE`).
- Checked: `make test` (91 checks), `tests/run6800.sh` with both disc models, the screenshots byte-identical.


## 2026-11-12 — Version 0.9.0; binary releases
- `VERSION` (0.9.0) is compiled into every program, and each answers `-version`. Nothing is printed at start-up.
- The programs find `roms/` and `artwork/` beside themselves when the repository's relative defaults are missing, so an unpacked release runs from anywhere (`host/paths.*`, the same file as the 8550 emulator's).
- A tag `v0.9.0` builds the releases on GitHub: macOS universal, Linux x86-64 and Windows x64, with the GUI (SDL3 static, or its DLL on Windows), the ROMs, the TEKDOS discs and the panel artwork (`.github/workflows/release.yml`, `tools/package_release.sh`).

## 2026-11-12 — Memory parity; link v4 agreed; the lamps from the schematics
- The CT8100's host features from its manual: FS cursor addressing, ESC FF clear page, ESC ] read cursor, US FAC display fields (inverse, blinking, dim, blanked), ESC O clear input, and the SO/SI rulings set drawn from its formula. The cursor is steady, as on the standard terminal (`-cursor blink` for the strap). `emu/tests/ct8100_test.cpp`.
- Memory parity (070-2711 §4, Table 3-28): bad parity raises interrupt 1 (System Memory) or 3 (Program Memory), and TEKDOS reports ERROR 65 or 66. Faults are injected with `-parity SEC [U:]ADDR` or the monitor's `P` (`tests/parity.sh`, in `make test`). `peek()` no longer goes through the bus.
- Link v4 agreed with FUSE (EMUL dfbfd98) and adopted by the 8550 emulator: `K` is read again after each RESET pulse (a machine switch, checked jointly: 3.500 → 3.547 MHz), `T` stays in microseconds, and link time is converted per stretch at the clock it ran at (it used to rescale every past cycle when the board switched clocks).
- The front-panel lamps from the schematics (`notes/8002.md`): SYS is the master/slave flip-flop, RUN is the bus RUN line gated by HOLD, PWR is +5.2 V.

## 2026-11-12 — Front ends: a system layer, the tty and the GUI
- The CT8100 from its own manual (070-2359-00, fetched from bitsavers; the CT8100 is a version of the 4023): BS is not destructive (as modelled), HT in DIRECT mode is a one-position space (it was a tab stop every 8), DEL neither writes nor spaces (`notes/ct8100.md`).
- The CT8100's TTY LOCK key (070-2359 p. 1-14): lower case typed as upper case, on by default, since TEKDOS takes commands in upper case only (`-ttylock off`, GUI Cmd+U, monitor `T`).
- Verified with FUSE: a ZX Spectrum 48K (`fuse --tek-probe`) as the Z80 board's prototype. In mode 2, `GO 0` runs the Spectrum ROM on the 8002's board to its copyright screen (`notes/prototype-link.md`).
- `system/tek8002_system`: the lab as one class with no notion of wall-clock time. It holds the mainframe and disc unit, the CT8100 on the terminal port, the links, and scripted events. Switches and lamps are named outputs and inputs (MAME style). `system/options` is the command line both front ends share.
- New scripted options: `-snap`, `-input`, `-press`, `-mount` and `-eject`. The monitor gains `p`, `R`, `D`, `m` and `e`.
- The switches: POWER (the bootstrap runs again at power-up), SYSTEM RESTART (the 2650 starts when it is let go), DIAG INTRPT, and the disc unit's POWER (an unpowered unit answers nothing).
- `host/ct8100`: the terminal as a 24×80 glass screen. `host/ct8100_raster`: its dots, in the 8550 emulator's font.
- `tek8002-gui` (`make gui`, SDL3): the CT8100 window and the mainframe's and disc unit's front panels (`emu/artwork`, generated by `tools/gen_panels.py`). It supports disc drop, paste, turbo and `-shot`. The layout reader comes from the 8550 emulator.
- The panels redrawn after photos of a surviving pair (`sources/photos/`, kept local): the cabinets' lateral-V sides (knee at 3/4 of the height), the PROM bezel with its two-line caution, the three mainframe boxes on one top and bottom, the stickers at their measured size. No power lamp on the disc unit, as on the real one. The windows fit the screen, centred (Cmd+L re-fits). Text dots are never drawn smaller than one output pixel (`layout.cpp`; the same change is due in the 8550 emulator's copy).

## 2026-11-12 — Control bits 2 and 4, the 6800's status bit 7, the prototype clock
- Control bit 2: the board's signals reach the Debug module's breakpoint and single-cycle logic. TEKDOS sets it when it hands them to the Debug module, and clears it for a trace board. Modelled as a gate on those events. Control bit 4 (user runs) gates the prototype's interrupts. Both meanings are inferred from TEKDOS (`notes/emulators.md`).
- The 6800's status bit 7: most likely "stopped on an instruction boundary", since TEKDOS resets the board and re-runs the instruction when it is 0. It reads 1 here.
- The prototype's clock: in modes 1 and 2 the boards run at the rate the target reports over link v4 (`K`), backward compatible with v1–v3. The Z80 board adds the 8002A's two wait states per memory cycle (Z80 Emulator Processor manual 070-3665). `tools/proto_target.py --clock`.

## 2026-11-12 — The Z80 Emulator Processor; status bits; emulation modes 1 and 2
- `tek8002_epz80` (E3, 4 MHz): the Z80 TEKDOS discs run programs on it, and their DEBUG works (`tests/runz80.sh`, with Tektronix's DEMO%S). Its firmware entries are the 8550 board's (F806 restore, F809 dump); the register block is TEKDOS's own.
- What the boards share moved into `tek8002_emboard`: control and status ports, the firmware window (control bit 1 = 0), firmware calls, modes.
- Status bits, from TEKDOS: bit 5 present and bit 4 halted on both. On the 6800, bit 6 = 6802 and bit 7 must be 1. On the Z80, bits 7/6 = IFF2/IFF1 and 1–0 = IM, which TEKDOS copies into the register block (`notes/emulators.md`).
- Modes 1 and 2 on both boards: memory by the map, I/O and interrupts from the prototype over the 8550 emulator's link (`-target`, `notes/prototype-link.md`). The firmware's GET/PUT (Z80 F803/F800, 6800 F9F7/F9F4) serve TEKDOS's DUMP, PATCH, MOVE and trace (`tests/prototype.sh`).

## 2026-11-12 — Step 6: the 6800 Emulator Processor
- `tek8002_ep6800`: a 6800 at 1 MHz on program memory, control port E2, memory-mapped SVCs at FEF0–FEF7, WAI as its halt. The core is MAME's, generated by `tools/port_m6800.py`.
- The board's lost dump/restore PROM is modelled from TEKDOS and the service manual's walkthrough: F9FA restores and F9FD dumps A, B, X, SP, CC through 0000–0006, each ending in the debug SVC 1.
- TEKDOS runs 6800 programs (SVC output, exit), and its DEBUG works: TRACE ALL/STEP, breakpoints, DSTAT, SET (`tests/run6800.sh`, in `make test`).
- Corrected in the Debug module and mainframe on the way: a forced jump/reset is armed by each command write; FC/FD read PC next and FE/FF PC last, true, and only follow the slave while debugging; reads where no board answers give 00, as the inverting bus buffers make them (TEKDOS tests port C2 for zero).

## 2026-11-12 — Step 5 stage D: writes and FORMAT through the microcode; the real controller is the default
- The drives hold flux pulses on a 2 µs grid. Writes are placed as the microprogram's PULLs time them, and reads separate clock from data by the format's rule (an empty slot is a data slot).
- TEKDOS's FORMAT, COPY, DELETE, ASM/LINK, COPYSYS and DUP run on the real controller. FORMAT of a never-formatted disc (`-1 new:FILE`) equals the real formatted disc Tek8002_1b byte for byte.
- `-fdc lle` is the default; `-fdc hle` keeps the high-level model. The session tests type at TEKDOS's prompt (`-type prompt TEXT`) and run in both modes.
- Fixed: the Sector Buffer shifts at EPSH, not at SPSH (written sectors were one bit late).
- S4104 from the schematic: a closed switch reads 0, and switches 4–1 drive WR1–WR4, so the factory interleave is INL = 03. This corrects stage B's 0x0F, which came from Fig. 3-8.

## 2026-11-12 — Step 5 stage C: reads through the disc controller's microcode
- `tek8002_fdd`: the drives, with hard-sectored tracks of FM bit cells built from the images and decoded back (sync FB, track, sector, data, CCITT CRC; sector k holds logical sector (k+1)·INL mod 32).
- `-fdc lle` runs the real controller: TEKDOS boots, including the boot PROM's 16K read from one command, and LDIR gives the same listing as the high-level model on all 33 discs. Discs are write protected in this mode until stage D.
- Found: the controller holds FLAG through an operation, so the disc interrupt is clocked by FLAG going low. The high-level model now does the same; its inferred completion pulse and RST-drops-FLAG rule are gone, and every write test still passes.
- Fixed in the microcode decoding: through-register jumps name the register directly, and LDL loads a register from the latches. Before, reads returned without checking the CRC.

## 2026-11-12 — Step 5 stage B: the disc controller's processor
- `tek8002_fdc` runs the real 512-word microprogram: the 74181 ALU in its active-low convention, the jump conditions, the command latches, UBANK (delayed one fetch), the Sector Buffer, and the host port.
- `emu/tests/fdc_test.cpp` (in `make test`) checks the power-on delay, the autoload's error recovery on a blank disc, the SIO/OTD handshake into R6/R4/R9/R11, and the seek to track 16.
- Established: the WR bus is active low (Fig. 3-8 prints physical levels), INL odd (its value corrected in stage D), HOST FLG is active while its latch is reset, and U1080/U1070 hold bits 15–8/7–0 (`notes/fdc.md`).

## 2026-11-12 — Step 4 completed: TEKDOS's disc tools
- COPYSYS and DUP work, besides FORMAT, DELETE and COPY: a COPYSYS'd disc boots and its system files are identical to the original's; a DUP'd disc has every file identical (`tests/disc_tools.sh`).

## 2026-11-12 — Step 3: System Communications; the rest of the Debug module
- `tek8002_syscomm`: three MC6850s (modem, printer, paper tape) at CA–CF on interrupts 6–8, and the memory map at C9. The front end connects them with `-rem`, `-lpt` and `-ppt` (`host/serial_link`, copied from the 8550 emulator: socket, printer file or wrap-back).
- Debug module: breakpoints, single cycle, the interrupt mask and DIAG INTRPT (monitor `D`), with a unit test (`emu/tests/debug_test.cpp`, `make test`).
- Fixed while doing it: the ACIAs' interrupts are levels, and the MC6850's transmitter is double-buffered. Before, TEKDOS's printer output lost every other character.
- TEKDOS 3.3 prints to LPT1 and copies to REMO and PPTP (`tests/syscomm.sh`).

## 2026-11-12 — Step 2: the Assembler Processor
- `tek8002_slave`: what a slave board supplies. `tek8002_debug::run_slave` runs whichever slave is active, once for all: forced reset/jump on F9 edges, SVCs (OUT F7–F0 → interrupts 16–23), slave HALT → 27, pausing for the master's interrupts, PC last/next.
- `tek8002_asmproc`: the Z80 Assembler Processor, control port E7, on program memory.
- TEKDOS 3.3's ASM and LINK run (`tests/asm_link.sh`): correct 6800 code in the listing and the load file.
- Found while doing it: an SVC must not arm a forced jump (TEKDOS's handler leaves the jump address alone); SVCs are enabled with F9 bit 4 high (`notes/8002.md`).

## 2026-11-12 — Disc writes work
- The disc unit drops a completion FLAG when the host writes RST (`notes/fdu.md` point 4). TEKDOS's FORMAT, DELETE, COPY to a file, and LDIR of data discs now work. FORMAT leaves a disc identical to a real formatted one.
- `-fdutrace` also logs every command (code, drive, sector, track).
- The sibling tool `../tekmato` now reads and writes TEKDOS discs (`tkdos-sssd`), checked against this emulator.

## 2026-11-12 — Step 1: TEKDOS boots
- Repository set up from the feasibility study. CPU cores, the MAME shim, the tty helpers, the disassembler tool, the Makefile and the conventions are copied from the 8550 emulator (commit 86caf61).
- Mainframe (`tek8002`): 2650 at 1.25 MHz, interrupt priority and vectors (Table 3-1), console UART on E8/E9, disc interface on EA/EB, interval timer (EC), program memory banks (EE), the bootstrap PROM over 0000–00FF, read in reversed address order (`notes/boot-prom.md`).
- Debug module (`tek8002_debug`): ports F8–FF; a master HALT with no slave to take the bus raises interrupt 27, which TEKDOS 3.3 relies on at start-up.
- Flexible Disc Unit (`tek8002_fdu`), high level: SIO/OTD/IND/RST handshake, Normal Read and writes, the completion FLAG on interrupt 11 (`notes/fdu.md` lists what is inferred).
- TEKDOS 3.1 and 3.3 boot and run LDIR. `tests/boot_ldir.sh` checks two discs; `Tek8002_1a`'s listing matches its photographed printout.
- Found while doing it: the disc interrupt is 11, not Table 3-2A's OCR "31"; the bank switch is port EE (Table 3-8), not ED (Table 3-4).
