# Tektronix 8002A µProcessor Lab emulator

An emulator of the **Tektronix 8002A µProcessor Lab** (1978–79), a microprocessor development system built around a Signetics 2650. The 8002 is the Millennium Universal One / Signetics TWIN, sold under the Tektronix name. It runs the original bootstrap PROM and boots TEKDOS from emulated 8" disc images.

A sibling of the [Tektronix 8550 MDL emulator](https://github.com/hugojpinto/tektronix-8550-emulator), sharing its CPU cores and design.

## Status
- The real bootstrap PROM (070-2393 Fig. 6-2) boots **TEKDOS 3.1 and 3.3** (6800) to the `>` prompt.
- `LDIR` lists the disc directory. On `Tek8002_1a` it matches the printout photographed with the original disc.
- Not yet: the Z80 Assembler Processor (ASM, LINK), System Communications, emulator boards, PROM programmers, the disc controller at low level. See [FEASIBILITY.md](FEASIBILITY.md) and [ROADMAP.md](ROADMAP.md).

## Build and run
```
cd emu && make
build/tek8002 -0 ../disks/TekDOS_3.3_6800_cpy1.img
```
The console is your terminal. Ctrl-] opens the monitor (`q` quits). `-1`…`-3` mount more drives, `-rw` makes them writable. `-fdc lle` runs the disc controller's real microprogram against bit-cell tracks (reads only so far; the default `-fdc hle` is the high-level model). `tests/boot_ldir.sh` is the regression test.

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
