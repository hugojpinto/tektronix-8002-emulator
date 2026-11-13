# How the Tektronix 8002A works, and how the emulator mirrors it

This explains the 8002A in about twenty minutes, without the service manuals, and shows where each part lives in the code. The facts come from `notes/`, which say how each one was established. Follow the pointers there for register-level detail.

## 1. What the machine is for

The **8002A µProcessor Lab** (Tektronix, 1978–79) is a workstation for writing software for *another* microprocessor: the 6800, Z80, 8080 or others in a product you are designing. On it you:
1. edit source files;
2. assemble and link them for the target processor (ASM, LINK);
3. run and debug the result on a real target processor on an **emulator board** inside the 8002A, with breakpoints, single steps, tracing and register display;
4. move that processor into your own hardware, the **prototype**, through a probe in its processor socket.

The operating system is **TEKDOS**. The collection has TEKDOS 3.3 for the 6800 and 3.1 for the Z80 (`sources/bitsavers/disks/`).

The 8002 is the Millennium Universal One, also sold as the Signetics TWIN, under Tektronix's name. The 8002A is the 8002 with a second program memory board. Eelco Vriezekolk's notes on the TWIN (ztpe.nl, kept in `sources/ztpe/`) cover the same hardware under Signetics' DOS.

## 2. Three boxes

```
 ┌──────────┐  RS-232   ┌─────────────────────────────────────┐   parallel    ┌───────────────────────┐
 │  CT8100  │◀────────▶│  8002A mainframe                     │◀────────────▶│  Flexible Disc Unit    │
 │ terminal │ (any RS-  │  2650 System Processor: TEKDOS       │  EA/EB, FLAG  │  microprogrammed       │
 │ 24 × 80  │  232 term)│  Assembler Processor (Z80): ASM, LINK│               │  controller, two 8"    │
 └──────────┘           │  emulator boards (6800, Z80): yours  │               │  drives (four with a   │
                        │  System Communications: 3 RS-232     │               │  second unit)          │
                        └──────────────────┬──────────────────┘               └───────────────────────┘
                                           │ probe cable (modes 1 and 2)
                                           ▼
                                    your prototype board
```

| Real | Emulator |
|---|---|
| the mainframe | `emu/src/machine/tek8002.*` and one class per board |
| the disc unit | `tek8002_fdu.*` (host port, high-level model), `tek8002_fdc.*` (the controller's microprogram), `tek8002_fdd.*` (the drives) |
| the CT8100 | `emu/src/host/ct8100.*` (the screen), `ct8100_raster.*` (its dots) |
| the probe and prototype | `machine/prototype.h`, `host/ipc_target.*`: another program over a socket |
| the RS-232 cables | `host/serial_link.*`: a file, a socket or a wrap-back plug |
| all of it, switched on | `emu/src/system/tek8002_system.*` |

## 3. The mainframe

The mainframe is a card cage on one bus (070-2711; `notes/8002.md`).

| Board | What | Code |
|---|---|---|
| **System Processor** (670-5300) | a Signetics **2650** at 10 MHz ÷ 8 = 1.25 MHz. The interrupt flip-flops, priority encoder and vector generator. Ports E8–EE: the console UART (an AY-5-1013, E8/E9), the disc unit's parallel port (EA/EB), the interval timer (EC), the program memory bank (EE) | `tek8002` |
| **System Memory** (670-5297) | 16K RAM at 0000–3FFF, and the 256-byte bootstrap PROM over 0000–00FF | `tek8002` |
| **Program Memory** (670-5298) | 16K per board, two in the 8002A, up to four. The 2650 sees one bank at a time at 4000–7FFF; the slaves see them all as one space | `tek8002` |
| **Debug and Front Panel I/O** (670-5299) | ports F8–FF: the master/slave hand-over, breakpoints, single cycle, SVCs, forced jumps, PC last/next | `tek8002_debug` |
| **Assembler Processor** (670-5220) | a **Z80** at 2.5 MHz that runs ASM, LINK and the macro processor; port E7 | `tek8002_asmproc` |
| **6800 Emulator Processor** | a **6800** at 1 MHz that runs your program; port E2 | `tek8002_ep6800` |
| **Z80 Emulator Processor** | a **Z80** at 4 MHz; port E3 | `tek8002_epz80` |
| **System Communications** (670-5219) | three MC6850 ACIAs (modem, printer, paper tape) and the memory map; ports C9–CF | `tek8002_syscomm` |

Not emulated yet: the PROM programmers, the Real-Time Prototype Analyzer, and the 8080 board (TEKDOS's 8080 discs boot, but cannot run programs). Memory parity is modelled: a byte with bad parity raises interrupt 1 or 3, and `-parity` injects one (`notes/8002.md`).

**The bootstrap PROM** (070-2393 Fig. 6-2, `notes/boot-prom.md`) sits behind inverting address buffers, so the 2650 at address *a* reads PROM location FF − *a*; `tek8002::load_boot_prom` reverses the listing. It reads 16K from track 1 of drive 0 into 0000–3FFF with one disc command, retrying, and then jumping to 0100.

**Interrupts** (Table 3-1): interrupt *n* has vector 2*n* and priority *n*. The PROM's page zero branches through a table to a 3-byte slot at 1000+3*n* in RAM, which TEKDOS fills in. The disc is interrupt 11 (Table 3-2A's OCR says 31; TEKDOS's jump table settles it). 1 and 3 are System and program memory parity errors, 16–23 the SVCs, 24/25 breakpoints, 26 single cycle, 27 "slave halted", 28 the DIAG INTRPT switch.

**The open bus reads 00.** The boards drive the backplane through inverting buffers, so a line nobody drives reads as 0. TEKDOS relies on it: port C2 reading 0 means "no trace board" (`notes/8002.md`).

## 4. Master and slaves: the Debug module

TEKDOS runs on the 2650, but the assembler and your program run on other processors. They take turns on one bus:

1. The 2650, the **master**, sets up program memory, selects a **slave** board through its command port (E7 for the Assembler Processor, E2 or E3 for an emulator board), loads the Debug module's forced jump address and command, and **executes HALT**.
2. The halt hands the bus to the active slave, which runs from program memory. If no slave takes it within one 38.4 kHz I/O clock, the module raises interrupt 27. TEKDOS 3.3 uses that at start-up to see whether a 6800 board is fitted.
3. The slave runs until it needs the master. The Debug module then raises one of interrupts 16–27, pauses the slave, and latches **PC last** (the instruction just run) and **PC next** (FE/FF and FC/FD, read true).
4. The 2650 wakes, does the work and halts again. The slave goes on.

| Event | How | Interrupt |
|---|---|---|
| **SVC** | the slave writes port F7…F0 (Z80), or touches FEF7…FEF0 (6800) | 16–23 (F7 is SVC 1). The slave continues where it stopped |
| **breakpoint** | a read (fetches included) or write of a breakpoint address, while debugging is enabled | 24, 25 |
| **single cycle** | after each instruction, while F8 bits 4 and 5 are low (TRACE) | 26 |
| **halt** | the slave executes HALT (Z80) or WAI (6800) | 27 |
| **DIAG INTRPT** | the front panel switch | 28 |

All but an SVC arm a **forced jump**: the slave continues at the address the master writes to FA/FB, as each write of F9 with bit 2 low arms it for the next hand-over. A forced reset (bit 0) works the same way. With the interrupt mask on (F9 bit 3 low), debug interrupts are held and raised when it is released.

**An SVC asks TEKDOS for a service.** A service request block in program memory names the function (read a line, write to channel 1, open a file, exit…), and its address sits at 0040 + 2(*n*−1) for SVC *n*. Tektronix's DEMO ends with `OUT (0F7H),A`, SVC 1, whose block at 0040 holds function 1A, "exit". This is how ASM, running on the Z80, reads and writes files that live on the disc unit, and how your program prints.

**In the code**, this logic is written once, in `tek8002_debug::run_slave`, for every slave. A slave is a `tek8002_slave` (`machine/tek8002_slave.h`): it supplies its processor (`step`, `pc`, `halted`, `cycles`, `clock_hz`), its command port (`decodes`, `command`, `status`), and what the Debug module's forced reset and jump mean for it. It reaches memory and I/O through `slave_bus`, which the mainframe implements: program memory as one linear space (board = A15–A14), and SVC ports that go to the Debug module. No slave is special-cased in the mainframe: `tek8002` keeps a list of slave boards, and the one whose command byte has its active bit set is the one that runs. Every slave instruction is run inside the 2650's own time slices, at the slave's clock rate, so the whole machine stays deterministic.

The facts here, and how each was established, are in `notes/8002.md` ("Debug and Front Panel I/O module").

## 5. The emulator boards

`tek8002_emboard` holds what the 6800 and Z80 boards share (`notes/emulators.md`); `tek8002_ep6800` and `tek8002_epz80` add each processor.

**The control byte** (written to E2 or E3): bit 7 active; bit 1 = 0 maps the board's **firmware PROM** (firmware calls 80, 90, C0, C4, D0) and 1 runs your program (92, B2, F2); bits 6–5 the emulation mode; bit 2 lets the board's fetches reach the breakpoint and single-cycle logic; bit 4 lets the prototype's interrupts through. The meanings of bits 2 and 4 are inferred from what TEKDOS writes.

**The status byte** (read): bit 5 present, bit 4 halted. On the Z80, bits 7/6 are IFF2/IFF1 and 1–0 the interrupt mode, which software cannot read on a Z80, so the board must follow them; TEKDOS copies the byte into the register block for DSTAT. On the 6800, bit 6 means a 6802 and bit 7 must read 1.

**The firmware.** The 6800 board's SYSTEM PROM was dumped in 2026 and runs on the 6800 as it is, with its FETCH PROM steering two of its data cycles (`notes/emulators.md`, "The 6800 board's PROMs"). The Z80 board's PROM is lost, so its routines are modelled at high level, as one step each, from what TEKDOS does around them and from 070-2711 §1's step-by-step walkthrough; the 6800's high-level version is kept behind `-ep6800 hle`. The routines:

| Routine | 6800 entry | Z80 entry | Does |
|---|---|---|---|
| restore | F9FA | F806 | load the registers from the block TEKDOS lends at 0000, then debug SVC 1 |
| dump | F9FD | F809 | store them there, then debug SVC 1 |
| GET / PUT | F9F7 / F9F4 | F803 / F800 | read or write one byte of prototype memory for TEKDOS, through 0000–0003, then debug SVC 2 |

So a GO under DEBUG is: TEKDOS swaps the register block into 0000, forces a jump to *restore*, which ends in an SVC; then it writes the run value (92), forces a jump to your PC, and halts. After each traced instruction or break it forces a jump to *dump* and reads the block back for the trace line or DSTAT. The register blocks' layouts were read from TEKDOS's DSTAT: 7 bytes on the 6800, 1C on the Z80.

**Emulation modes** (070-2701 §7):

| Mode | Memory | I/O, interrupts, RESET | Clock |
|---|---|---|---|
| 0 | program memory | none | the board's |
| 1 | per 128-byte block: program memory (P) or the prototype (U), from the memory map | the prototype's | the prototype's |
| 2 | the prototype's | the prototype's | the prototype's |

The **memory map** is 512 bits on the System Communications board, one per 128-byte block, loaded through port C9 one bit at a time (`notes/8002.md`). Firmware calls always see program memory, where the register block is ("MEMSW" mode). The Z80 board adds the 8002A's two wait states to every program memory access, and to every access in mode 1 (070-3665 §2: none in mode 2).

## 6. The Flexible Disc Unit

**The host side** (070-2587, `notes/fdu.md`): the 2650 writes a 3-bit code to EA, SIO (start), OTD (output a byte), IND (input a byte) and RST, and the controller answers each strobe by raising FLAG (EA bit 7). A command is a command byte (88 read, 0E write formatted, 0D write unformatted, 00 diagnostic registers), drive, sector and track, then 128 data bytes. FLAG stays up through the seek and transfer, and **FLAG going low** is the disc interrupt (11). That edge was found by running the real microcode (`notes/fdc.md`).

**The controller** is a 16-bit microprogrammed machine built from TTL around a 74181 ALU, running 512 words from two PROMs, at a 200 ns cycle. It does FM encoding, the sector framing and the CRC bit by bit, in microcode. The microprogram was recovered from three printings in the service manual (Appendix C's binary and hex columns, and Fig. 3-5), all of which agree (`roms/fdc/`, `tools/fdc_dasm.py --check`). `tek8002_fdc` runs it: registers R0–R15, the accumulator, the T-register, UBANK one fetch late, the Sector Buffer that shifts at EPSH.

**The drives** (`tek8002_fdd`): 77 tracks of 32 hard sectors, single density, 360 rpm. A track is held as flux pulses on a 2 µs grid, built from the image when it is mounted and decoded back when it is saved. A sector is 128 zero cells, the sync byte FB, track, sector, 128 data bytes and a CCITT CRC. Physical sector *k* holds logical sector ((*k*+1) × 3) mod 32: the interleave switch S4104 at its factory setting gives 3, read from the schematic (the interleave on shipped units is still to be confirmed).

| `-fdc` | What runs | |
|---|---|---|
| `lle` (default) | the real microprogram on flux-level tracks | real timing; FORMAT of a never-formatted disc equals a real formatted disc |
| `hle` | `tek8002_fdu`'s high-level model of the host protocol, on sector images | faster; same files; kept for comparison, and every session test runs in both |

## 7. The CT8100 and the front panels

**The CT8100** (070-2359, `notes/ct8100.md`) is Tektronix's version of its 4023 terminal: 24 lines of 80. It is modelled in DIRECT mode with the codes TEKDOS uses. BS and HT move the cursor without erasing, which matters because TEKDOS's BACK SPACE echoes a bare BS. Its TTY LOCK key turns lower case into upper case, and is down by default because TEKDOS takes upper case only (a choice: its state is not documented). Its LF DOES CR strap is set, as TEKDOS's echo of Return (LF with no CR) shows the lab's terminal had. The host features a program could use are modelled too: cursor addressing, clear page, read cursor, display fields, clear input and the rulings set, with `ct8100_raster` drawing the fields' looks (inverse, dim, blinking, blanked) for the GUI; `emu/tests/ct8100_test.cpp` checks them. BUFFER mode is documented but not modelled. The character ROM and the phosphor colour are unknown; the font is the 8550 emulator's (`host/term_font.inc`).

**The front panels** (070-2701 Fig. 2-6, photos of a surviving pair) are MAME `.lay` files generated by `tools/gen_panels.py`. They read and drive the system's named outputs and inputs (INTEGRATION.md §3). Their lamps follow the service manual's schematics (`notes/8002.md`, "Front panel lamps"): PWR is lit from +5.2 V; SYS is the MASTER LED pair, lit while the master/slave flip-flop gives the bus to the 2650; RUN is lit while a processor runs and the bus is not held. That the SYS legend sits over the MASTER pair is inferred, as is what holding SYSTEM RESTART does to SYS and RUN (here both go dark).

## 8. The system layer and the front ends

```
 main8002.cpp (tty)         frontend/sdl/main_gui.cpp (SDL3)
        │  keys, pacing, monitor        │  windows, keys, mouse, paste, -shot, pacing
        └──────────────┬────────────────┘
              system/tek8002_system         options.* (one command line)
        tick() = 1 ms of machine time; no wall clock
        CT8100 · links · scripted -type/-snap/-input/-press/-mount/-eject
        named outputs (lamps) and inputs (switches)
                       │
              machine/tek8002 (+ fdu, debug, slaves, syscomm)
```

`tek8002_system` is the whole lab with no notion of wall-clock time: a front end calls `tick()` as often as it likes. Each tick runs 1250 2650 cycles in 50-cycle steps; the devices (UART, disc unit, ACIAs, timer, the active slave) advance in step with them. So a batch run gives the same result as an interactive one, and the tests, the screenshots and the tutorial pictures are made with batch runs.

- **`main8002.cpp`** makes your terminal the CT8100, paces interactive runs to real time, and opens the monitor on Ctrl-].
- **`main_gui.cpp`** draws the CT8100 from `ct8100_raster` (640 × 240 dots, each scan line doubled, a phosphor bloom in the CRT style) and the panels from `layout.*`. In batch runs (`-cycles`) it advances 1/60 s of machine time per frame, so `-shot` captures are deterministic.

## 9. Fidelity at a glance

| Real hardware | Emulator | Fidelity |
|---|---|---|
| 2650 | `cpu/s2650` (MAME core, via the 8550 emulator) | instruction level, with cycle counts |
| Z80s | `cpu/z80` (superzazu core) | passes zexall in the 8550 emulator |
| 6800 | `cpu/m6800` (MAME, generated by `tools/port_m6800.py`) | instruction level |
| mainframe boards | `machine/tek8002*.cpp` | register level, from the service manual and TEKDOS |
| disc controller | `tek8002_fdc`, `tek8002_fdd` | its real microprogram, on flux-level tracks |
| emulator board firmware | 6800: its dumped PROMs, run by the 6800; Z80: `firmware()` | 6800 real (two data-cycle steerings inferred); Z80 **high level**: its PROM was never dumped |
| probe and prototype | `host/ipc_target` | another program over a socket (INTEGRATION.md §1) |
| CT8100 | `host/ct8100` | DIRECT mode, TEKDOS's codes |

## 10. Where to go next

- [OPERATION.md](OPERATION.md): using the lab, from boot to debugging.
- [INTEGRATION.md](INTEGRATION.md): writing a prototype, connecting the serial ports, binding panels, adding an emulator board.
- `notes/`: every fact, with how it was found. `sources/bitsavers/manuals/800x/*.txt`: the manuals' OCR, the ground truth when the notes are silent.
