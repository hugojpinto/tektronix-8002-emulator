# Roadmap

Following FEASIBILITY.md §5.

1. ~~Boot to TEKDOS~~ (2026-11-12).
2. ~~Z80 Assembler Processor~~ (2026-11-12): ASM and LINK run. The Debug module's breakpoints, single cycle and interrupt mask followed in step 3 (unit-tested; TEKDOS's DEBUG needs an emulator board, step 6).
3. ~~System Communications~~ (2026-11-12): printer, modem and paper tape out; the memory map. Open: REMI/PPTR input pacing (`notes/8002.md`).
4. ~~A disc tool, and TEKDOS's own disc tools~~ (2026-11-12): `../tekmato` handles TEKDOS discs; FORMAT, DELETE, COPY, COPYSYS and DUP work on writable images (`tests/disc_tools.sh`).
5. **The disc controller at low level** from its microprogram (070-2587 Appendix C, two 512×8 ROMs), with a hard-sectored FM track model; this also yields FM sector streams for writing real discs.
   - ~~Stage A~~ (2026-11-12): the microcode as ROM images, triple-checked, and a disassembler matching the manual's listing (`notes/fdc.md`, `tools/fdc_dasm.py`).
   - ~~Stage B~~ (2026-11-12): the processor runs the real microprogram: power-on delay, autoload with error recovery, SIO/OTD handshake, seek (`emu/src/machine/tek8002_fdc.*`, `emu/tests/fdc_test.cpp`).
   - ~~Stage C~~ (2026-11-12): bit-cell tracks (`tek8002_fdd`) and reads through the microcode: TEKDOS boots and LDIRs under `-fdc lle`, identical to the high-level model on all 33 discs; CRC errors and the error recovery work.
   - ~~Stage D~~ (2026-11-12): writes and FORMAT through the microcode (FORMAT of a never-formatted disc equals Tek8002_1b); the real controller is the default, `-fdc hle` kept. Open: the factory interleave on real units (`notes/fdc.md` "Still to establish").
6. ~~**6800 emulator board**~~ (2026-11-12): the 6800 runs user programs with SVCs, and TEKDOS's DEBUG works on it: TRACE ALL/JMP/STEP, breakpoints, DSTAT, SET (`notes/emulators.md`, `tests/run6800.sh`). Its dump/restore PROM is modelled at high level. The Z80 board followed the same day (`tests/runz80.sh`), with the status bits worked out (`notes/emulators.md`) and modes 1 and 2 on both boards against a prototype over the 8550 emulator's link (`tests/prototype.sh`). Then control bits 2 and 4 and the 6800's status bit 7 were inferred from TEKDOS, and the prototype's clock was added (link v4) along with the 8002A's Z80 wait states. Open: the boards' service manuals, which would confirm those inferences; the 8080 board (E1); link v4 for the sibling 8550 emulator and FUSE.
7. Options: PROM programmers (2704/2708, 1702A), Real Time Trace / RTPA, front panel in a GUI.

Open questions:
- Port map of the C0–C7 board TEKDOS writes at start-up (Real Time Trace or RTPA?). TEKDOS's debug control reads C2 to see whether it is fitted and can take PC last/next from its C4–C7.
- Whether the controller really continues to the next sector after 128 bytes (the boot PROM's 16K read implies it).
