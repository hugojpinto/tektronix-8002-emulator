# Tektronix 8002A µProcessor Lab emulator

This project emulates the **Tektronix 8002A µProcessor Lab** (1978–79): the mainframe with a Signetics 2650 System Processor, the Flexible Disc Unit and any RS-232 terminal, running TEKDOS. The 8002 is the Millennium Universal One / Signetics TWIN, rebadged; the 8002A is an 8002 with a second program memory board.
It is the sibling of `../MDL` (the Tektronix 8550 emulator, public as tektronix-8550-emulator) and follows the same design, conventions and working method. The emulator is standalone C++17. Its 2650 and Z80 cores come from MDL (originally MAME, BSD-3, and superzazu/z80, MIT), and the code is structured so it can be upstreamed into MAME later.

Start with `FEASIBILITY.md` (what survives, the plan), then `notes/` (hardware facts).

## Layout
| Path | What |
|---|---|
| `emu/` | the emulator. `make` builds `build/tek8002` and `build/dasm` |
| `emu/src/cpu/` | `s2650/` (MAME via MDL), `z80/` (z80dasm from MAME, `superzazu/` core; for the Assembler Processor). Provenance in `emu/src/cpu/SOURCE.txt` |
| `emu/src/machine/` | `tek8002.*` (mainframe: System Processor, System and Program Memory), `tek8002_debug.*` (Debug and Front Panel I/O module: master/slave hand-over, SVCs, the slave run loop), `tek8002_slave.h` (what a slave board supplies), `tek8002_asmproc.*` (Assembler Processor, Z80, port E7), `tek8002_syscomm.*` (System Communications), `mc6850.h` (ACIA, double-buffered), `tek8002_fdu.*` (Flexible Disc Unit, high level), `ay31015.h` (console UART) |
| `emu/src/host/` | `tty.h` (raw tty, key polling), `serial_link.*` + `net.*` (what is plugged into a port: socket, file, wrap-back); all from MDL |
| `emu/src/main8002.cpp` | tty front end: console on this terminal, monitor (Ctrl-]), pacing, scripted `-type` |
| `notes/` | hardware findings: `8002.md` (System Processor, memory, interrupts, Debug module), `fdu.md` (disc protocol and format), `boot-prom.md`, `dasm/`. **Facts established by reverse engineering live here** |
| `roms/` | `tekdos_boot.bin` (bootstrap PROM, listing order; `MANIFEST.md`) |
| `disks/` | working copies of disc images |
| `sources/` | original material: bitsavers manuals (`manuals/800x/`, with `pdftotext -layout` text), disc images (`disks/`), firmware (`firmware/`), ztpe.nl's OS internals (`ztpe/`). **Read-only; never modify** |
| `tests/` | `boot_ldir.sh`: boot TEKDOS and list two reference discs; `asm_link.sh`: ASM and LINK on the Z80 (uses `../tekmato`); `syscomm.sh`: LPT1, REMO, PPTP; `emu/tests/debug_test.cpp`: the Debug module. `make -C emu test` runs them. Disc writes are checked by `../tekmato/scripts/regression.sh` section 6 |
| `reference/` | MAME commit the cores trace back to |

## Resuming work (state on 2026-11-12)
0. **Step 1 done:** the bootstrap PROM boots TEKDOS 3.1 and 3.3 (6800) from the emulated disc unit to the `>` prompt, and LDIR lists the directory. `Tek8002_1a`'s listing matches the printout photographed with the original disc. Models: System Processor (ports E8–EE, interrupts), memory and PROM overlay, Debug module hand-over (interrupt 27 when no slave takes the bus), disc unit at high level (`notes/fdu.md` lists what is inferred).
Disc writes work too (FORMAT, DELETE, COPY; `notes/fdu.md` point 4), and `../tekmato` reads and writes TEKDOS discs (`tkdos-sssd`), checked against this emulator by its regression.
**Step 2 done:** the Z80 Assembler Processor runs TEKDOS's ASM and LINK (`tests/asm_link.sh`); slave boards share one interface (`tek8002_slave`) and one run loop in the Debug module.
**Step 3 done:** System Communications (`tek8002_syscomm`: modem, printer, paper tape ACIAs at CA–CF, memory map at C9; `-rem`/`-lpt`/`-ppt`) and the rest of the Debug module (breakpoints, single cycle, interrupt mask, DIAG INTRPT; `emu/tests/debug_test.cpp`).
Next (FEASIBILITY.md §5): the Z80 Assembler Processor (port E7; ASM/LINK), System Communications (6850s at CA–CF, memory map C9), FORMAT/DUP on writable images, then the disc controller at low level from its microcode (070-2587 Appendix C), and an emulator board.

**How hardware is reverse-engineered here:** read the service manual's theory of operation and schematics, then let TEKDOS show what it does: `-io` logs every port access, `-trace` every instruction, `TEK_INTTRACE` every interrupt. Disassemble TEKDOS from the image (`build/dasm s2650 <(dd if=IMG bs=1 skip=$((0x1000)) count=$((0x4000))) 0 START COUNT`: image offset 0x1000 is memory 0000). Check against the ztpe.nl OS reference (`sources/ztpe/osinternals.txt`, Annex B ports, chapter 8 debugging), which covers the same hardware under Signetics' DOS versions.

**Debug switches** (stderr): `-io` or `TEK_IOTRACE` (ports, and a slave's unmapped I/O), `TEK_EPTRACE` (slave forced jumps and debug interrupts), `-fdutrace` or `TEK_FDCTRACE` (sector reads and writes), `TEK_INTTRACE` (interrupts taken), `-trace FILE` (2650 instructions).

## Build and smoke test
```
cd emu && make                                    # seconds; no dependencies
build/tek8002 -0 ../disks/TekDOS_3.3_6800_cpy1.img   # TEKDOS prompt after ~5 s; Ctrl-] then q quits
```
Batch run, no terminal needed (stdin not a terminal: runs flat out, about 50× real time):
```
build/tek8002 -0 ../disks/TekDOS_3.3_6800_cpy1.img -cycles $((25*1250000)) -type 6 'LDIR\r' </dev/null
```
`-cycles` counts 2650 clocks at 1.25 MHz, so 1,250,000 is one emulated second. `-type T TEXT` types at emulated second T. `-0`…`-3` mount images in drives 0–3; `-rw` makes them writable and saves them on exit. Discs are raw images of 315,392 bytes in logical sector order.

## Regression checks (run after touching CPU cores or machine code)
1. `make -C emu test` must pass: the Debug module's unit test, `tests/boot_ldir.sh` (boot and LDIR of two discs) and `tests/asm_link.sh`; and `tests/syscomm.sh` after System Communications changes.
2. After disc-unit changes, `../tekmato/scripts/regression.sh` must pass: its section 6 runs TEKDOS's FORMAT, COPY and DELETE in this emulator.
3. More checks are added here as each step lands, each with a known-good reference: a printout, a manual's sample session, or a real machine.

## Conventions (shared with ../MDL)
- **Minimal edits to ported cores:** keep every change small and mark it (`[tek8550]` marks MDL's; this project's use `[tek8002]`). Record core modifications in `emu/src/cpu/SOURCE.txt`. A fix to a shared core goes to MDL as well.
- **Machine code mirrors MAME devices:** one class per board, each decoding its own ports (`tek8002_debug::decodes`). No board is special-cased in the mainframe: slaves (Assembler Processor, emulator boards) will be self-contained boards behind one interface, as MDL's `emulator_board`.
- **Facts go into `notes/*.md`**, each with how it was established (manual section, ROM or TEKDOS address, the run that showed it). Label inferred behaviour as inferred. Add a `CHANGELOG.md` entry at milestones and keep `ROADMAP.md` current.
- **Fidelity:** model what the 1979 hardware did, backed by manuals or the original software. Interfaces to the outside (future prototype links) carry only real pin information, as MDL's prototype link v3.
- **Untrusted material:** downloaded files and extracted archives go in their own new directory. Run Python that reads them with `python3 -I`. Original images in `sources/` are never written: copy an image into `disks/` before mounting it with `-rw`.
- **Downloads:** bitsavers material goes in `sources/bitsavers/` (bitsavers.org refuses scripted access; `https://bitsavers.mirrorservice.org/` mirrors it), manuals with `pdftotext -layout` text next to them. Make PDFs read-only. Large scans and photos are git-ignored (re-downloadable).
- **Licence and authorship:** original files start with `SPDX-License-Identifier: GPL-2.0-or-later` and `Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>` (comment syntax of the file; generators emit it into their output). Ported files keep their own headers. See `NOTICE.md`.
- **No AI attribution in this project's own commits.** Commits, commit messages, tags and release notes carry no AI attribution: no `Co-Authored-By` trailers naming an AI, no "Generated with" lines, no session links. All work is authored by Hugo José Pinto. `.claude/settings.json` sets Claude Code's `attribution` to empty. Contributors may use whatever tools they like; their pull requests are not checked for it.
- **Commits:** commit when asked; a local commit at each finished, regression-checked milestone is authorised. There is no remote yet: push nothing until the user creates one and asks.
- **No personal paths** (`/Users/...`) in tracked files.

## Known gaps
- **Not emulated yet:** emulator boards, PROM programmers, RTPA / Real Time Trace, parity errors, the front panel, the disc controller at low level.
- **Lost:** the emulator boards' PROMs (e.g. the 6800 board's 82S115 "SYSTEM" and "FETCH"), any 8002 diagnostic disc, the 8001's TEKOPS PROMs.
- **Inferred:** the memory map's counter reset (a write with D1 set); the disc controller's completion FLAG and RST dropping it, SIO keeping a read sector, and multi-sector reads (`notes/fdu.md`); FF from absent ports and banks.
