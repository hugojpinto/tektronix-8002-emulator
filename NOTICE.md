# Licensing notice

## Emulator code and tools
Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>.

The original code in this repository is licensed under the **GNU General Public License, version 2 or (at your option) any later version** (`GPL-2.0-or-later`, full text in [LICENSE](LICENSE)). It covers the emulator's machines, system, host and front-end code, the tests, the tools and the front-panel artwork. Each such file starts with an `SPDX-License-Identifier: GPL-2.0-or-later` line. If you distribute this software or a work based on it, you must make its source available under the same terms.

## Third-party code included
These files keep their own licences, which are compatible with GPL-2.0-or-later:

| Files | Origin | Licence |
|---|---|---|
| `emu/src/cpu/s2650`, `emu/src/cpu/z80/z80dasm.*` | [MAME](https://www.mamedev.org/), via the sibling tektronix-8550-emulator; commit in `reference/MAME-SOURCE.txt` | BSD-3-Clause (per-file headers) |
| `emu/src/cpu/m6800`, `reference/mame/cpu/m6800` | [MAME](https://www.mamedev.org/) `src/devices/cpu/m6800`; the core is generated from it by `tools/port_m6800.py`; commit in `reference/MAME-SOURCE.txt` | BSD-3-Clause (per-file headers) |
| `emu/src/machine/prototype.h`, `emu/src/host/ipc_target.*`, `tools/proto_target.py` | the sibling tektronix-8550-emulator (same author), the prototype link | GPL-2.0-or-later |
| `emu/src/cpu/z80/superzazu/` | [superzazu/z80](https://github.com/superzazu/z80) | MIT (`LICENSE` in that directory) |

Local changes to the ported cores are marked `[tek8550]` (made in the 8550 emulator) or `[tek8002]`.

## Tektronix material
The ROM image (`roms/`), disc images (`disks/`, `sources/`), manuals and their OCR text (`sources/bitsavers/`) are © Tektronix, Inc. or their owners. They come from public archives ([bitsavers](https://bitsavers.org/pdf/tektronix/800x/), [TekWiki](https://w140.com/tekwiki/wiki/8001)) and are included for preservation and research. `sources/ztpe/` is Eelco Vriezekolk's "OS Reference and Internals" ([ztpe.nl](https://ztpe.nl/2650/)), kept as a reference. None of this is covered by the GPL.
