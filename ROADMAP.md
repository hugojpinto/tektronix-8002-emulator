# Roadmap

Following FEASIBILITY.md §5.

1. ~~Boot to TEKDOS~~ (2026-11-12).
2. ~~Z80 Assembler Processor~~ (2026-11-12): ASM and LINK run. The Debug module's breakpoints, single cycle and interrupt mask followed in step 3 (unit-tested; TEKDOS's DEBUG needs an emulator board, step 6).
3. ~~System Communications~~ (2026-11-12): printer, modem and paper tape out; the memory map. Open: REMI/PPTR input pacing (`notes/8002.md`).
4. ~~A disc tool, and TEKDOS's own disc tools~~ (2026-11-12): `../tekmato` handles TEKDOS discs; FORMAT, DELETE, COPY, COPYSYS and DUP work on writable images (`tests/disc_tools.sh`).
5. ~~**The disc controller at low level**~~ (2026-11-12) from its microprogram (070-2587 Appendix C, two 512×8 ROMs), with a hard-sectored FM track model; this also yields FM sector streams for writing real discs.
   - ~~Stage A~~ (2026-11-12): the microcode as ROM images, triple-checked, and a disassembler matching the manual's listing (`notes/fdc.md`, `tools/fdc_dasm.py`).
   - ~~Stage B~~ (2026-11-12): the processor runs the real microprogram: power-on delay, autoload with error recovery, SIO/OTD handshake, seek (`emu/src/machine/tek8002_fdc.*`, `emu/tests/fdc_test.cpp`).
   - ~~Stage C~~ (2026-11-12): bit-cell tracks (`tek8002_fdd`) and reads through the microcode: TEKDOS boots and LDIRs under `-fdc lle`, identical to the high-level model on all 33 discs; CRC errors and the error recovery work.
   - ~~Stage D~~ (2026-11-12): writes and FORMAT through the microcode (FORMAT of a never-formatted disc equals Tek8002_1b); the real controller is the default, `-fdc hle` kept. Open: the factory interleave on real units (`notes/fdc.md` "Still to establish").
6. ~~**6800 emulator board**~~ (2026-11-12): the 6800 runs user programs with SVCs, and TEKDOS's DEBUG works on it: TRACE ALL/JMP/STEP, breakpoints, DSTAT, SET (`notes/emulators.md`, `tests/run6800.sh`). Its dump/restore PROM was modelled at high level until its two PROMs were dumped (2026-11-13, Eelco Vriezekolk); they now run as they are (`-ep6800 hle` keeps the model). The Z80 board followed the same day (`tests/runz80.sh`), with the status bits worked out (`notes/emulators.md`) and modes 1 and 2 on both boards against a prototype over the 8550 emulator's link (`tests/prototype.sh`). Then control bits 2 and 4 and the 6800's status bit 7 were inferred from TEKDOS, and the prototype's clock was added (link v4) along with the 8002A's Z80 wait states. Open: the boards' service manuals, which would confirm those inferences; the 8080 board (E1); link v4 for the sibling 8550 emulator and FUSE.
7. Options: PROM programmers (2704/2708, 1702A), Real Time Trace / RTPA. ~~Front ends~~ (2026-11-12): a system layer (`emu/src/system/`) under a tty and an SDL3 front end with the CT8100 and both front panels. Open: a run on real Windows; the CT8100's own character set and phosphor colour.
8. ~~Smaller items~~ (2026-11-12): memory parity (ERROR 65/66), the front-panel lamps from the schematics, the CT8100's host features and LF DOES CR strap, prototype link v4 agreed with FUSE and adopted by the 8550 emulator.
9. **More emulator boards**, for the TEKDOS discs in `sources/` that boot to processors with no board here: 8085 (2 discs). ~~8080~~ (2026-11-13, port E1): its PROMs run, P2 rebuilt from a read that lost bit 0 (`notes/emulators.md`, `tests/run8080.sh`), whose cores and much board logic exist in the sibling 8550 emulator; the 6500/1 (1 disc), with MAME's 6502 family.
10. **Real-Time Prototype Analyzer**: both its manuals are here (070-2356 service, 070-2785 users); probably the C0–C7 board TEKDOS probes at start-up.
11. **PROM programmer** (1702, 2704/2708 modules; the panel's sockets and PROM POWER switch): needs its manual first.
12. **Maintenance Front Panel**: its interface is known from the A5 schematics (`notes/8002.md`), but its own manual (070-2725-00) is lost; waits for it or a photo.
13. **The diagnostics** (`sources/bitsavers/disks/Millennium_internal_floppies/diag_4.4_5-23-79`): DIP 4.4 runs each module with `LO` and reports what it expects (`notes/diagnostics.md`, `tests/dip.sh`). The memories, system tests, Assembler Processor and 8080 chip test pass; to fix next, in the order the reports make easy: the Debug tests of the 8080, 6800 and Z80 boards (PC last/next after reset, forced interrupt and jumps, single cycle, breakpoint priority, SVCs), 6800 FETCH, the System Communications status bytes, the timer (10 ms where 100 are expected), and MASTER CPU's unexpected interrupt at 0036; then DISK1 with formatted blanks. **CP/M** (the `CPM/` discs there): needs the 64K Z80 slave that `CPM64K` (E380–FFFF) loads into.

Open questions:
- Port map of the C0–C7 board TEKDOS writes at start-up (Real Time Trace or RTPA?). TEKDOS's debug control reads C2 to see whether it is fitted and can take PC last/next from its C4–C7.
- Whether the controller really continues to the next sector after 128 bytes (the boot PROM's 16K read implies it).
