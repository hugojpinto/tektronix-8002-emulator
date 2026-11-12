# Tektronix 8002A µProcessor Lab emulator

An emulator of the **Tektronix 8002A µProcessor Lab** (1978–79), a microprocessor development system built around a Signetics 2650. The 8002 is the Millennium Universal One / Signetics TWIN, sold under the Tektronix name. It runs the original bootstrap PROM and boots TEKDOS from emulated 8" disc images.

A sibling of the [Tektronix 8550 MDL emulator](https://github.com/hugojpinto/tektronix-8550-emulator), sharing its CPU cores and design.

## Status
- The real bootstrap PROM (070-2393 Fig. 6-2) boots **TEKDOS 3.1 and 3.3** (6800) to the `>` prompt.
- `LDIR` lists the disc directory. On `Tek8002_1a` it matches the printout photographed with the original disc.
- The Z80 Assembler Processor runs TEKDOS's ASM and LINK; System Communications drive the printer, modem and paper tape ports; FORMAT, COPYSYS and DUP work.
- The disc controller runs its real microprogram (070-2587) against flux-level tracks.
- The **6800 Emulator Processor** runs user programs, and TEKDOS's DEBUG works on it: TRACE, breakpoints, DSTAT, SET. Its lost dump/restore PROM is modelled at high level (`notes/6800.md`).
- Not yet: PROM programmers, the Real-Time Prototype Analyzer, other emulator boards, prototype (mode 1/2) emulation. See [ROADMAP.md](ROADMAP.md).

## Build and run
```
cd emu && make
build/tek8002 -0 ../disks/TekDOS_3.3_6800_cpy1.img
```
The console is your terminal. Ctrl-] opens the monitor (`q` quits). `-1`…`-3` mount more drives, `-rw` makes them writable. The disc controller runs its real microprogram against flux-level tracks; `-fdc hle` uses the high-level model instead (faster discs, same files). `-1 new:FILE` mounts a never-formatted disc, for FORMAT, saved to FILE with `-rw`. `tests/boot_ldir.sh` is the regression test.

```
 TEKDOS  6800    VERSION 3.3
> LDIR

FILE NAME  BLKS

TEKDOS      16
.          176
COPYSYS      1
NEWDISC      1
CHANGE       0
CONCO        1

TOTAL FILES         6
TOTAL BLOCKS USED 203
BLOCKS AVAILABLE  101
TOTAL BAD BLOCKS    8
```

## Wanted
- Dumps of any 8001/8002 emulator board PROMs (e.g. the 6800 board's 82S115 "SYSTEM" and "FETCH"), and of the 8001's TEKOPS PROMs.
- An 8002 diagnostic disc.

## Sources and thanks
Manuals and disc images from [bitsavers](https://bitsavers.org/pdf/tektronix/800x/) and [TekWiki](https://w140.com/tekwiki/wiki/8001). TWIN hardware and OS internals from Eelco Vriezekolk's [2650 Restoration Project](https://ztpe.nl/2650/). The EEVblog thread "Vintage CPU emulator Tektronix 8002 µProcessor Lab" documents a real machine's restoration.

Licence: GPL-2.0-or-later for the emulator code; see [NOTICE.md](NOTICE.md).
