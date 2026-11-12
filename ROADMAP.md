# Roadmap

Following FEASIBILITY.md §5.

1. ~~Boot to TEKDOS~~ (2026-11-12).
2. ~~Z80 Assembler Processor~~ (2026-11-12): ASM and LINK run. The Debug module's breakpoints, single cycle and interrupt mask followed in step 3 (unit-tested; TEKDOS's DEBUG needs an emulator board, step 6).
3. ~~System Communications~~ (2026-11-12): printer, modem and paper tape out; the memory map. Open: REMI/PPTR input pacing (`notes/8002.md`).
4. ~~A disc tool, and TEKDOS's own disc tools~~ (2026-11-12): `../tekmato` handles TEKDOS discs; FORMAT, DELETE, COPY, COPYSYS and DUP work on writable images (`tests/disc_tools.sh`).
5. **The disc controller at low level** from its microprogram (070-2587 Appendix C, two 512×8 ROMs), with a hard-sectored FM track model; this also yields FM sector streams for writing real discs.
   - ~~Stage A~~ (2026-11-12): the microcode as ROM images, triple-checked, and a disassembler matching the manual's listing (`notes/fdc.md`, `tools/fdc_dasm.py`).
   - Stages B–D: the processor, the track model and reads, writes and FORMAT (`notes/fdc.md` "Still to establish").
6. **6800 emulator board** (high level: its PROMs are lost), debug features (breakpoints, single cycle, TRACE).
7. Options: PROM programmers (2704/2708, 1702A), Real Time Trace / RTPA, front panel in a GUI.

Open questions:
- Port map of the C0–C7 board TEKDOS writes at start-up (Real Time Trace or RTPA?).
- Whether the controller really continues to the next sector after 128 bytes (the boot PROM's 16K read implies it).
