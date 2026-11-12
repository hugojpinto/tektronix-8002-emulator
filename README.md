# Tektronix 8002A µProcessor Lab emulator

![The graphical front end: TEKDOS Z80 stopped at a breakpoint in Tektronix's Z80 DEMO, with DSTAT showing the Z80's registers, beside the 8002A and disc unit front panels](docs/screenshots/hero.png)

An emulator of the **Tektronix 8002A µProcessor Lab** (1978–79), the development system on which Z80 programs were written, assembled, linked, run and debugged at the bench. The emulator runs the original bootstrap PROM and boots TEKDOS from emulated 8" disc images.

The 8002A is built around a Signetics 2650 (the Millennium Universal One / Signetics TWIN, sold under the Tektronix name). On the Z80 TEKDOS discs, Z80 work happens on two boards:
- the **Z80 Assembler Processor**, which runs TEKDOS's Z80 assembler and linker;
- the **Z80 Emulator Processor**, which runs your program in program memory (mode 0) or in a target system through its probe (modes 1 and 2), under TEKDOS's DEBUG.

A sibling of the [Tektronix 8550 MDL emulator](https://github.com/hugojpinto/tektronix-8550-emulator), sharing its CPU cores and design.

## Status
- **TEKDOS Z80 3.1** boots from the real bootstrap PROM (070-2393 Fig. 6-2) to the `>` prompt. TEKDOS 6800 3.1 and 3.3 boot too.
- **The Z80 Assembler Processor** runs ASM and LINK: Tektronix's own `DEMO%S` assembles and links unchanged.
- **The Z80 Emulator Processor** runs programs with their SVCs, and TEKDOS's DEBUG works on it: TRACE ALL/JMP/STEP, breakpoints, DSTAT with the whole Z80 register block (alternate set, IX, IY, I, R, IM, IFF1/2), SET, HALT.
- **Emulation modes 1 and 2** reach a target system over a socket (`-target`, `notes/prototype-link.md`). The target can be the reference target `tools/proto_target.py`, or a ZX Spectrum in FUSE: in mode 2, `GO 0` runs the Spectrum's ROM on the 8002's Z80 board, up to its copyright screen. Link v4 carries the target's clock.
- **Also:**
  - The 6800 Emulator Processor, with the same DEBUG support (`notes/emulators.md`).
  - The disc controller running its real microprogram (070-2587) against flux-level tracks; FORMAT, COPYSYS and DUP work.
  - System Communications: the printer, modem and paper-tape ports.
  - Memory parity.
  - The CT8100 terminal from its manual.
- **Two front ends on one system layer:** the terminal (`tek8002`), and a window with the CT8100 screen and the mainframe and disc unit front panels (`tek8002-gui`, SDL3).
- **Not yet:** the 8080, 8085 and 6500/1 boards, the PROM programmers and the Real-Time Prototype Analyzer. See [ROADMAP.md](ROADMAP.md).

## Build and run
**Binaries:** the [releases](https://github.com/hugojpinto/tektronix-8002-emulator/releases) have `tek8002`, `tek8002-gui` and `dasm` for macOS (universal), Linux (x86-64) and Windows (x64), with the ROMs, the TEKDOS discs and the panel artwork beside them: unpack and see `QUICKSTART.txt`. Every program answers `-version`.

**From source:**
```
cd emu && make
cp ../disks/TekDOS_3.1_Z80.img /tmp/z80.img       # a copy: -rw saves changes to it
build/tek8002 -rw -0 /tmp/z80.img
```
The console is your terminal. Ctrl-] opens the monitor (`q` quits).
- **Discs:** `-1`…`-3` mount more drives. Without `-rw` a disc is write-protected, as with its tab on, so ASM cannot write its output. `-1 new:FILE` mounts a never-formatted disc, for FORMAT.
- **Disc model:** the disc controller runs its real microprogram; `-fdc hle` uses the high-level model instead (faster discs, same files).
- **Case:** TEKDOS takes commands in upper case only. The CT8100's TTY LOCK key is on, so whatever you type goes up as upper case.

A Z80 session, with Tektronix's own demonstration program: assemble it, link it, load it, give it five bytes to add, run it, then stop it at a breakpoint and look at the Z80:
```
 TEKDOS  Z80     VERSION 3.1

> ASM DEMO;O DEMO;A DEMO%S
Tektronix        Z80 ASM V3.3
**** Pass 2
   15 Source Lines      15 Assembled Lines   12921 Bytes available
            >>> No assembly errors detected <<<
*ASM* EOJ

> LINK DEMO;L DEMO;K DEMO;O
    NO ERRORS     NO UNDEFINED SYMBOLS
    1  MODULE     1  SECTION
    TRANSFER ADDRESS IS 0100
*LINK* EOJ

> LOAD DEMO;L
TRANSFER ADDRESS: 0100
*LOAD* EOJ

> PATCH 500 0102030405

> GO 100
*DOS* EOJ

> DEBUG

> BKPT 0108 R

> GO 100
LOC  INST     MNEM OPER        SP  RF RA RB RC RD RE RH RL  IX  IY
0108 05       DEC  B          0000 02 01 04 00 00 00 05 01 0000 0000
0108 BREAK

> DSTAT
PC=0109 BP=0108 R          M=0 SP=0000 RF=02 RA=01 04 00 00 00 05 01
  IM=0  IFF1=0  IFF2=0    ALT SET:     AF=00 AA=00 00 00 00 00 00 00
  IX=0000  IY=0000  I=00  R=06
```
[docs/TUTORIALS.md](docs/TUTORIALS.md) goes on from there: TRACE ALL, SET, single steps, and the program running on a target system in modes 1 and 2. `tests/runz80.sh` is this session's regression test.

### Options
```
tek8002 [-prom FILE] [-0..-3 IMG|new:FILE] [-rw] [-fdc hle|lle] [-cycles N] [-type SEC|prompt TEXT]
        [-snap SEC FILE] [-input SEC NAME=VALUE] [-press SEC NAME] [-mount SEC N=FILE] [-eject SEC N]
        [-target unix:PATH|tcp:HOST:PORT] [-rem SPEC] [-lpt SPEC] [-ppt SPEC] [-trace FILE] [-io] [-fdutrace] [-log FILE]
```
- `-cycles N`: stop after N 2650 clocks (1,250,000 a second). With `-cycles`, or stdin not a terminal, the run is flat out and deterministic.
- `-type SEC TEXT` types at an emulated second. `-type prompt TEXT` types at the next TEKDOS `> ` prompt; after the last one, a batch run ends at the following prompt.
- `-snap SEC FILE` saves the CT8100 screen as text.
- `-ttylock on|off`: the CT8100's TTY LOCK key, on by default. Lower-case keys go to the 8002 as upper case, because TEKDOS takes commands in upper case only. Scripted `-type` text is sent as written.
- `-input SEC NAME=VALUE` sets a switch; `-press SEC NAME` presses and releases one: `8002:power`, `fdu:power` (rockers), `8002:restart` (SYSTEM RESTART), `8002:diag` (DIAG INTRPT).
- `-mount SEC N=FILE` and `-eject SEC N` change discs while the system runs.
- `-parity SEC ADDR` (System Memory) or `-parity SEC U:ADDR` (program memory, the user address) makes one byte hold bad parity until it is written, as a failing RAM would. TEKDOS reports ERROR 65 or 66.
- `-target`: the prototype for emulation modes 1 and 2. `-rem`, `-lpt`, `-ppt`: the modem, printer and paper tape ports (a socket, a file, or `loop`).

The monitor (Ctrl-]) also presses switches (`p NAME`, `R`, `D`), changes discs (`m N FILE`, `e N`), toggles TTY LOCK (`T`) and injects parity faults (`P [U] ADDR`).

### Graphical front end
```
cd emu && make gui            # needs SDL3: brew install sdl3 / apt install libsdl3-dev
build/tek8002-gui -rw -0 /tmp/z80.img
```
It takes the same options, plus `-cursor steady|blink` (the CT8100's cursor is steady; a strap made it blink), `-style crt|sharp`, `-scale F` (by default the windows fit the screen, centred), `-panels on|off`, `-artwork DIR` and `-shot SEC FILE.bmp`. The shot saves every window (`FILE_8002a.bmp`, `FILE_fdu.bmp`); if the run ends sooner, it is taken at the end.

- **The terminal** is a 24×80 CT8100 in green phosphor, with the control codes of its manual (070-2359, a version of the Tektronix 4023; `notes/ct8100.md`): BS and HT move the cursor without erasing. It also has the terminal's cursor addressing, clear page, read cursor, display fields (inverse, blinking, dim, blanked) and rulings set, for programs that use them. The phosphor colour and the character ROM are not documented, so the font is the one the 8550 emulator uses.
- **The front panels** are drawn after photos of a surviving pair of units (`sources/photos/`) and 070-2701 Fig. 2-6 (`tools/gen_panels.py` writes `emu/artwork/*.lay`):
  - **Mainframe:** POWER, SYSTEM RESTART and DIAG INTRPT work, and SYS, RUN and PWR light. The PROM sockets are decoration.
  - **Disc unit:** POWER works, and each drive has a busy light. Drop a disc image on a drive door to insert it; click the door to take the disc out.
- **Hotkeys** (Cmd, or Ctrl+Alt): V paste, C copy the screen, U TTY LOCK, T turbo, F full screen, S sharp/CRT, P show the panels, L fit the windows to the screen again, R SYSTEM RESTART, Q quit.

The lamps follow the service manual's schematics (`notes/8002.md`, "Front panel lamps"): SYS is the master/slave flip-flop giving the bus to the System Processor, RUN the bus line RUN (a processor runs) gated by HOLD, PWR the +5.2 V supply. That the SYS legend sits over the MASTER LED pair, and that SYS and RUN go dark while SYSTEM RESTART is held, are inferred.

A headless check: `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software build/tek8002-gui -0 IMG -cycles $((40*1250000)) -type prompt 'LDIR\r' -shot 35 x.bmp`.

## Documentation
- [docs/TUTORIALS.md](docs/TUTORIALS.md): short recipes with pictures: boot, the Z80 and 6800 programs run and debugged, ASM and LINK, the printer and paper tape ports, FORMAT and DUP, modes 1 and 2, a ZX Spectrum as the prototype.
- [docs/OPERATION.md](docs/OPERATION.md): the long tour: both front ends, every option, the monitor, discs, TEKDOS and its pitfalls (upper case and TTY LOCK, BACK SPACE, DEBUG ending with the program).
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): how the 8002A works, and how the code mirrors it.
- [docs/INTEGRATION.md](docs/INTEGRATION.md): for implementers: the prototype link (v1–v4), the serial ports, panel bindings, adding an emulator board.
- `notes/`: the hardware facts, each with how it was established.

The pictures are regenerated by `tools/make_screenshots.sh` (the CT8100, rendered by `tools/render_screens.py`), `tools/make_tutorial_shots.sh` and `tools/make_hero.sh` (the GUI, composed by `tools/compose_gui.py`).

## Wanted
- Dumps of any 8001/8002 emulator board PROMs (e.g. the 6800 board's 82S115 "SYSTEM" and "FETCH"), and of the 8001's TEKOPS PROMs.
- An 8002 diagnostic disc.

## Sources and thanks
Manuals and disc images from [bitsavers](https://bitsavers.org/pdf/tektronix/800x/) and [TekWiki](https://w140.com/tekwiki/wiki/8001). TWIN hardware and OS internals from Eelco Vriezekolk's [2650 Restoration Project](https://ztpe.nl/2650/). The EEVblog thread "Vintage CPU emulator Tektronix 8002 µProcessor Lab" documents a real machine's restoration.

Licence: GPL-2.0-or-later for the emulator code; see [NOTICE.md](NOTICE.md).
