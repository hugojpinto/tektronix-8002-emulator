# Millennium Systems internal floppies

From bitsavers, `bits/Millennium/Millennium_internal_floppies.zip` (imaged with Applesauce in
June 2023; one imaging log per disc, the label photos are kept locally but not committed). Millennium
Systems designed the Universal One that became the 8001/8002, and later built its own 9508/9516
products on the same hardware; these are its in-house discs. Copied as received, read-only,
without the two `.a2r` flux captures (34 MB, in the zip).

All but one are 8002 FDU images (77 × 32 × 128 bytes, `tekmato` format `tkdos-sssd`). `tekmato
-check` is clean on 77 of them. `TekDOS/master_cmd_files_for_slave_adapter` has blocks shared
between files and an outdated master map, `TekDOS/hal_gillette_z80_v3.1_system` a directory
longer than its count in sector 0, and `CPM/word_star_work_disk` (311,296 bytes) is not in
FDU geometry.

## What the emulator boots (checked 2026-11-15)
Every disc was booted from drive 0 (`-fdc lle`, the TEKDOS boot PROM); the banner it printed:

| Disc | Comes up as |
|---|---|
| `TekDOS/tekdos_v1.6_2650` | `TEKDOS VER 1.6 REV A 2650` (system file UDOS, 1976–77 commands, ASM50 2.1S, and a 47-block `SOURCE` file) |
| `TekDOS/99900761-01-A_tekdos_1.9`, `tekdos_1.9_bkup_8-7` | `TEKDOS 6800 VERSION 1.9` |
| `TekDOS/99900862-01`, `99902585-01`, `99901253-01`, `99901254-01` | `TEKDOS Z80 VERSION 2.2` |
| the 9508 discs (`99900809`, `99900817`–`99900825`, `99900924`) | `TEKDOS 6800 VERSION 3.1` |
| `TekDOS/6801_tekdos_v3.1_f001`, `f002` | `TEKDOS 6801 VERSION 3.1` |
| `TekDOS/chuck_tekdos_ice` | `TEKDOS 8048 VERSION 3.1` |
| `TekDOS/tektip_1.5` | `TEKTIP - VERSION 1.5` |
| 22 others in `TekDOS/` (`0517-0143-01` to `0149`, the slave adapter and 8086 discs, ...) | `TEKDOS Z80 VERSION 3.1` |
| `diag_4.4_5-23-79` | `DIP 0114 00 0000 0000` and a `$` prompt: the Diagnostic Interpreter Program 4.4 (3/22/79), disc 88002110-4.3 |
| `UCSD_Pascal/pascal_I.4b` | `>` (PMON, "TOPS 2.1 18-DEC-78") |
| `CPM/*`, `TekDOS/88000141_8088_thor_of_ops` | halt: TEKDOS loads `CPMDRI` (80–1FFF) and `CPM64K` (E380–FFFF, start FA00) for a 64K Z80 slave not yet modelled |
| `hazel_6809`, `_8080-85`, `_6800`, `_8048`, `files`, `supvhis2`, `supvhis3`, `supv_edit_disk_slave_adapter_drive_1` | not bootable: a `TEKDOS` entry, but the system area (image offset 1000) is all zeros |
| `h2lasm`, `hazel_z80`, the three `com-ram-05` discs, `99901441-01` | not bootable: data discs with no system file |

## Of most interest here
- **The 9508 sources** (`99900817`-`99900825`, `99900924`: S and L files for executive, I/O,
  subroutines, emulator control, command line, real-time analyzer, memory, comm): Millennium's
  9508, a 6800 system on 8002 boards, addressing their ports as memory at 20xx. They name the
  Debug module's ports, the emulator boards' command latch bits and a real-time analyzer's
  registers (`notes/emulators.md`, `notes/8002.md`).
- `tekdos_1.9_bkup_8-7`: `FETCH`, the documented structure of the 6809 emulator's fetch predictor
  PROM (2048 x 8, 8 pages, opcode on A0-A7, pages chosen through A8-A10), the same idea as the
  6800 board's FETCH PROM; `99902585-01` holds its generator and maps (FPMAP, FPMICRO, FPLST).
- `tekdos_v1.6_2650`: `SOURCE`, the "TOPS equates and data base definition" of the 2650 system.
- `99901441-01`: MON68, a 6800 monitor, source and listing; `99901253`/`99901254`: the 9516 boot,
  listing and source; the "Hazel" discs: Millennium's HZL assemblers and tables per processor.
- **`diag_4.4_5-23-79`**: the 8002's hardware diagnostics, one file per module: MASTER CPU,
  SYS COMM 1/2, CMEM and MMEM (system and program memory), DISK1, TIMER, PRINTER, PROM 1702A,
  2708/04 and 82S, PROBE TEST, the 2650 DEBUG and FETCH tests, 8080 DEBUG/CHIP/HWA, 6800
  DEBUG/FETCH/HDWR ANA, Z80 and 1802 debug hardware, and the system tests (8080, 6800, ASM).
  `DIAGBLD` and `CPYDIP` are the command files that build the disc. How DIP is driven is not
  yet known (it answers each line typed at `$` with a new prompt).
- **TEKDOS 1.6 for the 2650** and **1.9 for the 6800**, the earliest TEKDOS seen so far.
- **`99902585-01`**: volume "6809/09E FETCH PRED CODE-PROMS".
- **"Hazel" discs** (`99900827`–`99900831`) for the 6809, 8080/85, 6800, Z80 and 8048, and
  `99900826` (H2LASM).
- `CPM/*`: CP/M on the 8002, among them WordStar, Whitesmiths C and a 6809 assembler source.
