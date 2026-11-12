# Roadmap

Following FEASIBILITY.md §5.

1. ~~Boot to TEKDOS~~ (2026-11-12).
2. ~~Z80 Assembler Processor~~ (2026-11-12): ASM and LINK run. Left for the Debug module: breakpoints, single cycle and the interrupt mask, needed by DEBUG and the emulator boards (step 6).
3. **System Communications** (3× 6850 at CA–CF, memory map at C9; §5): printer, remote and paper-tape ports.
4. ~~A disc tool~~: `../tekmato` handles TEKDOS discs (2026-11-12). FORMAT, DELETE and COPY work on writable images; DUP and COPYSYS still to try.
5. **The disc controller at low level** from its microprogram (070-2587 Appendix C, two 512×8 ROMs), with a hard-sectored FM track model; this also yields FM sector streams for writing real discs.
6. **6800 emulator board** (high level: its PROMs are lost), debug features (breakpoints, single cycle, TRACE).
7. Options: PROM programmers (2704/2708, 1702A), Real Time Trace / RTPA, front panel in a GUI.

Open questions:
- Port map of the C0–C7 board TEKDOS writes at start-up (Real Time Trace or RTPA?).
- Whether the controller really continues to the next sector after 128 bytes (the boot PROM's 16K read implies it).
