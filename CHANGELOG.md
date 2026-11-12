# Changelog

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
