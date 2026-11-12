# Tektronix 8002A µProcessor Lab — Emulation Feasibility Study

> **Status (2026-11-12):** steps 1 and 2 of §5 are done (TEKDOS boots; ASM and LINK run on the Z80). TEKDOS 3.1 and 3.3 boot from the real bootstrap PROM and run LDIR (see CHANGELOG.md). Port and interrupt facts found since are in `notes/`.

*2026-11-12. Sources: bitsavers (`pdf/tektronix/800x/`, `bits/Tektronix/8002/`), TekWiki, ztpe.nl/2650, the EEVblog restoration thread "Vintage CPU emulator Tektronix 8002 µProcessor Lab" (143 posts, 2026-10-19 to 2026-11-11). The three key manuals, with `pdftotext -layout` text, are in `sources/bitsavers/manuals/800x/`.*

## Verdict

**Yes, an 8002A emulator is doable, and smaller than the 8550 was.** Everything needed to boot TekDOS and run its editor, assembler, linker and PROM programmer survives: both CPU types (already ported in the sibling 8550 emulator), the boot PROM, complete schematics, the I/O port and interrupt maps, the disc controller's microcode listing, and about 30 bootable disc images.

The gaps are the same kind as on the 8550: the emulator boards' own PROMs (needed only for in-circuit emulation of a target CPU), and no diagnostic disc. The **8001 is out of reach** until someone dumps TEKOPS.

## 1. What the machine is

The 8002 (1978) is the Millennium Information Systems "Universal One" (1976), also sold by Signetics as the TWIN, re-badged by Tektronix (history: ztpe.nl/2650/hardware/signetics-twin/). The **8002A** is the same machine shipped with a second 16K program memory board. It is not a hardware revision (users manual; confirmed in the EEVblog thread, post #33). The **8001** uses the same mainframe without the Z80 board or disc unit, with the TEKOPS monitor in 7 PROMs on its System Memory board (service manual 070-2711, §1).

A complete 8002A is two boxes plus any RS-232 terminal:

| Unit | Contents |
|---|---|
| **Mainframe** (20-slot backplane) | System Processor (2650, 670-5300), System Memory (16K + boot PROM, 670-5297), Assembler Processor (Z80, 670-5220), Program Memory ×2 (16K each, 670-5298, 9-bit with parity), Debug & Front Panel I/O (670-5299), System Communications (3× 6850, 670-5219); options: emulator boards, Real-Time Prototype Analyzer, 1702A and 2704/2708 PROM programmers |
| **Flexible Disc Unit** | two CalComp 140/142 8" drives (40-pin, not Shugart), a microprogrammed disc controller, its own power supply |
| **Terminal** | anything RS-232. Only RxD/TxD are used; the CPU board ties every control line to +5 V (schematic, verified on a real board in the thread, post #35) |

### Facts established so far

| Topic | Fact | Source |
|---|---|---|
| Clocks | 2650 at 10 MHz ÷ 8 = 1.25 MHz; 10 MHz bussed to the Z80 and emulator boards. A separate 9.984 MHz oscillator gives the baud clocks (38.4 kHz = 16 × 2400) and a 10 ms / 100 ms interval timer (jumper) | 070-2711 §3, Fig. 3-2 |
| Console | AY-5-1013 UART on the CPU board, 110–2400 baud from a thumbwheel, 8 data bits (bit 8 ignored), 1 stop bit | 070-2711 §3, Table 3-7 |
| Memory | The 2650 sees system memory at 0000–3FFF; program (slave) memory is bank-switched in 16K blocks into 4000–7FFF, one bank at a time. The slave cannot see system memory | 070-2711 §3 "Program Memory Bank Select Logic", Table 3-8; ztpe.nl |
| Interrupts | 32 vectors at 0000–003E (Table 3-1): reset, parity errors, console in/out, comms 1–3, timer, disc, PROM programmers, 8 emulator SVCs, 2 breakpoints, single cycle, emulator halted, diagnostic, RTPA, debug 30/31 | 070-2711 Table 3-1 |
| Disc interface | Port **EA**: write = Parallel Control Register (PNTR/DISC INT EN, C1, C0, /STRB); read = status (PBZY, PFLT, TAPE, FLG). Port **EB**: data in/out. Service requests on C1 C0 /S: RST, SIO, OTD, IND | 070-2711 Table 3-6; 070-2587 Fig. 1-8, Tables 1-2, 1-3 |
| Boot PROM | 256 bytes in a 1702A on the System Memory board. Fig. 6-2 of 070-2393 lists it in reversed address order: CPU address = FF − listed offset. Reversed, it is clean 2650 code. It loads tracks 1–4 (16K) of drive 0, else drive 1, into 0000–3FFF with command `$88` (Normal Read) and jumps to 0100 | `bitsavers 800x/firmware/tekdos_boot.bin` = Fig. 6-2; disassembled here and, independently, in the EEVblog thread (posts #50–51) |
| Slaves | TekDOS needs the Z80 Assembler Processor. Each DOS disc supports one emulator type; the selected slave is stored at 0166 | ztpe.nl (EEVblog post #135); checked on two images (`'6'` = 6800) |
| Disc format | 77 tracks × 32 hard sectors × 128 bytes, FM, 360 rpm. Track 0 = directory (sector 0 header and bit maps, sectors 1–5 directory, 78 files max); tracks 1–4 = operating system; 5–76 = files. Physical interleave 0, 16, 1, 17 … (set by switch S4104) | 070-2587 Table 1-1, "Sector Interleaving" |
| Images | 315,392 bytes = 77 × 32 × 128, in logical order. Offset 0x1000–0x4FFF is what the boot PROM loads; it starts with a `MOD` module header (entry 0100); 0x4FFE–F holds the DOS version in ASCII (`31`, `33`) | checked on `TekDOS_3.3_6800_cpy1.img` and `Tek8002_1a.img`; ztpe.nl |
| Disc controller | A dedicated 16-bit microprogrammed controller built from TTL around an ALU; microprogram in two 512×8 ROMs. **Appendix C lists all of it, each line with binary and hex**, so the OCR can be checked against itself; Appendix B gives the instruction formats, Table 3-2 the ALU function map | 070-2587 §3, Apps. B–C |

## 2. What survives

| Item | Status | Where |
|---|---|---|
| Boot PROM | **have**, verified against the manual figure | bitsavers `800x/firmware/` |
| TekDOS 3.1 / 3.3 (6800, 6500/1) and about 30 more discs, several with labels and listings photographed | **have** | bitsavers `bits/Tektronix/8002/` (Applesauce `.img`, some `.a2r` flux) |
| Service manual with 64 pages of schematics, users manual, FDU service manual, installation guide, TekDOS reference, editor, assembler, RTPA service and users manuals, emulator specifics (6500, 6800, 8080, 9900), Universal One manuals | **have** | bitsavers `pdf/tektronix/800x/` |
| TekDOS internals, partly reverse-engineered DOS sources, Perl disc tools | **have** (incomplete, actively worked on) | ztpe.nl/2650 |
| Disc controller microcode | **have**, as a listing | 070-2587 App. C |
| Emulator-board PROMs (e.g. the 6800 board's 82S115 "SYSTEM" and "FETCH") | **missing** | — |
| Diagnostic disc | **missing** | — |
| TEKOPS (8001) | **missing** | — |

## 3. Building blocks

From `../MDL` (the 8550 emulator), with no new porting:

| Part | Reuse |
|---|---|
| Signetics 2650 | `emu/src/cpu/s2650` (MAME), with its disassembler |
| Z80 (Assembler Processor) | `emu/src/cpu/z80/superzazu` (zexall cycle-exact) |
| 6850 ACIA | `emu/src/machine/mc6850.h` |
| Slave around a Z80, bank mapping | the pattern of `z80_slave.*` and the 8301 Language Processor |
| System layer with no wall-clock notion, scripted `-type`/`-snap`, tty front end, pacing | `emu/src/system/`, `main8550.cpp` |
| PROM programmer | `prom_programmer.*` (2704/2708 module needed) |
| Byte-cell track model, image ↔ track conversion | `flexdrive.*` (needs hard sectors and FM without ID fields) |
| Emulator boards for later | MAME cores for the 6800, 8080/8085, 9900, F8/3870, 1802, 6502, 8048 |

New code would be:

| Part | Effort | Notes |
|---|---|---|
| System Processor board: decoding, 32-vector interrupt logic, AY-5-1013, baud and interval timer, bank select | moderate | fully in schematics and §3 of 070-2711 |
| System Memory with the reversed boot PROM; Program Memory with parity | small | |
| System Communications (3× 6850, jumpered baud rates) | small | |
| Debug & Front Panel I/O: SYS/RUN/PWR lamps, DIAG INTRPT, SYSTEM RESTART, breakpoints, single cycle | moderate | the debug logic is only needed once a slave runs |
| Assembler Processor (Z80 slave) | moderate | |
| Disc interface, **high level** (EA/EB protocol, commands from Table 1-2) | small | enough to boot |
| Disc controller, **low level** (run the App. C microcode on a model of the controller) | moderate–large | optional; the same route as the 8501 FDC |
| Disc tool (list/extract/write TekDOS discs), as `../tekmato` does for DOS/50 | small–moderate | ztpe.nl's Perl scripts are a reference |
| 2704/2708 PROM programmer | small | |
| Emulator boards (6800 first) | moderate each, high level | their PROMs are not dumped |
| RTPA ("Real Time Trace", 2× 8253) | moderate | manuals 070-2356, 070-2785 |

## 4. Risks

| Risk | Severity | Mitigation |
|---|---|---|
| No diagnostic disc: the 8550's main reverse-engineering tool is missing | medium | TekDOS itself is the test (boot, LDIR, EDIT, ASM, LINK, DUPE, FORMAT); ztpe.nl's sources; the three working machines at ztpe.nl and Vince's machine for comparison |
| Debug/emulator interaction learned only from schematics | medium | start with the Z80 slave, whose software (the assembler) runs from disc |
| Emulator-board PROMs not dumped | medium, for emulation only | high-level boards, as with the 8550's Z80 and 8085 boards; ask Vince (6800 board) for dumps once he can read 82S115s |
| Disc images in a different sector order than the controller expects | low | images are logical; the high-level path ignores interleave; the low-level path applies the 0/16/1/17 order when building tracks |
| OCR errors in the microcode listing | low | every line carries binary and hex; cross-check |

## 5. Plan

1. **Boot to TekDOS:** 2650 board, System Memory and boot PROM, console, timer, and a high-level disc interface. Success: the TekDOS prompt from `TekDOS_3.3_6800_cpy1.img`. This is the go/no-go step.
2. **Z80 Assembler Processor and bank switching:** the assembler and linker run.
3. **Disc tool, then FORMAT/DUPE** on writable images.
4. **Low-level disc controller** from the App. C microcode, with a hard-sectored FM track model. Its track builder also gives the FM sector streams that real-hardware restorers need to write discs.
5. **6800 emulator board** (high level), then the debug features: breakpoints, single cycle, TRACE.
6. Options: 2704/2708 programmer, RTPA, front panel in the SDL GUI.

## 6. Project layout

A sibling project, as this directory. It copies the CPU cores and shared helpers from `../MDL` (the 8550 emulator, commit 86caf61; `emu/src/cpu/SOURCE.txt`), so it builds on its own; fixes to a shared core go to both. Machine code follows the same rules as there: one class per board, mirroring MAME devices so both can be upstreamed later; hardware facts go into `notes/` with how each was established; original material goes into `sources/`, read-only.

## 7. Who this helps

- **Vince (EEVblog):** an image known to boot before he writes his NOS hard-sectored discs, and FM sector streams for a disc writer.
- **ztpe.nl:** a place to run and check the reverse-engineered DOS sources.
- **Preservation:** no software emulator exists for any 8001/8002, Universal One or TWIN; MAME has none.
