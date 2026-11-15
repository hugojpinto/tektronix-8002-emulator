# Integration guide

How other programs and new hardware join the emulated 8002A. Written for people adding:
- **a prototype**: another emulator, or a model of your board, in which the 8002A's emulator board runs your program;
- **something on a serial port**: a host computer, a printer or a paper tape station on the System Communications ports;
- **front-panel lamps and switches**, or new panel artwork;
- **a new emulator processor board**, such as the 8080.

| Surface | Where it attaches | Transport | Reference | Notes |
|---|---|---|---|---|
| [The prototype link](#1-the-prototype-link) | the emulator board's probe (modes 1 and 2) | 4-byte requests, 2-byte replies over TCP or a Unix socket | `tools/proto_target.py`, FUSE | `notes/prototype-link.md` |
| [Serial ports](#2-the-serial-ports) | J101 modem, J102 printer, J103 paper tape | plain bytes: a file, a socket or a wrap-back | — | `notes/8002.md` "System Communications" |
| [Lamps, switches, panels](#3-lamps-switches-and-front-panels) | `tek8002_system` outputs and inputs | named values | the 8002A and disc unit panels | `emu/src/system/outputs.h`, `tools/gen_panels.py` |
| [Emulator boards](#4-adding-an-emulator-board) | the Debug module and program memory | C++, a `tek8002_emboard` | the 6800 and Z80 boards | `notes/emulators.md` |

**One rule runs through all of them: be faithful to 1979.** A link carries only what the real hardware carried: probe pins, RS-232 bytes, board registers. Anything that exists only for emulation (the link's `T` time messages) is kept minimal and marked as such, and additions are versioned so that existing peers keep working.

---

## 1. The prototype link

On a real 8002A, emulation modes 1 and 2 move the emulator processor onto a probe that plugs into the processor socket of the engineer's own board, the prototype. In the emulator the prototype is a separate process. The emulator connects to it once, and for every bus cycle that belongs to the prototype it sends a request and waits for the reply, as the probe drove the board's bus. The protocol is the sibling tektronix-8550-emulator's, so the same targets serve both; the 8002 adds version 4, the prototype's clock.

```
 ┌───────────── tek8002 ──────────────┐                      ┌──── your prototype ────┐
 │ 2650 TEKDOS ── Debug module        │   TCP or Unix socket │ memory, I/O devices,   │
 │                    │               │  4-byte request ───▶ │ interrupt sources,     │
 │  Z80 or 6800 emulator processor ───┼── 2-byte reply ◀──── │ its own clock          │
 └────────────────────────────────────┘                      └────────────────────────┘
       client: -target tcp:HOST:PORT                          server: listens, accepts one
```

### Which cycles reach the prototype

| | Mode 0 | Mode 1 | Mode 2 |
|---|---|---|---|
| Memory in blocks mapped **P** | program memory | program memory | prototype |
| Memory in blocks mapped **U** | program memory | prototype | prototype |
| Z80 I/O (`IN`/`OUT`) | none | prototype | prototype |
| SVC ports (Z80 F0–F7, 6800 FEF0–FEF7) | SVCs | prototype: TEKDOS turns SVCs off in modes 1 and 2 | prototype |
| INT, NMI, RESET, WAIT | none | from the prototype; interrupts only while user code runs | from the prototype |
| Clock | the board's | the prototype's, if it reports one (v4) | the prototype's |
| TEKDOS's DUMP, PATCH, MOVE, the trace's instruction bytes | — | through the firmware's GET/PUT, for U blocks | through GET/PUT |

`EMULATE n` selects the mode, `MAP U lo-hi` / `MAP P lo-hi` the blocks ([OPERATION.md §8](OPERATION.md#8-emulation-modes-and-the-prototype)). The 6800 has no I/O space and no fetch pin, so a 6800 prototype sees `R`, `W`, the interrupt lines and RESET (and `H`, `K`, `T`, `Z`).

### Messages

The emulator connects at start-up. Every exchange is synchronous: one 4-byte request, one 2-byte reply, in order.

```
request  (emulator → target)   op  addr_lo  addr_hi  data          op is an ASCII letter
reply    (target → emulator)   data  lines
```

| op | Meaning | addr | data (request) | data (reply) |
|---|---|---|---|---|
| `H` | hello, once at connect | the emulator's highest version (4) | — | the version the target will speak, ≤ the emulator's |
| `K` | v4: the prototype's clock | 0: low byte, 1: high byte | — | the rate in kHz (0 = not reported) |
| `R` | memory read | address | — | the byte (FF if nothing is there) |
| `F` | opcode fetch, the Z80's M1 (v2+; v1 targets get `R`) | address | — | the byte |
| `W` | memory write | address | the byte | ignored |
| `I` | I/O read | the full 16-bit I/O address: A8–A15 carry A, or B for the `(C)` and block forms | — | the byte |
| `O` | I/O write | the full 16-bit I/O address | the byte | ignored |
| `V` | interrupt acknowledge (INTA) | — | — | the byte on the data bus (IM 0 opcode, IM 2 vector) |
| `T` | time advance: microseconds of the processor's time since the last `T`, about every 256 instructions while it runs | microseconds | — | ignored |
| `Z` | reset | — | — | ignored |

Every reply carries the target's control lines:

| `lines` bit | Since | Meaning |
|---|---|---|
| 0 | v1 | INT asserted. A level: keep it set until your device has been serviced |
| 1 | v1 | NMI asserted; the emulator acts on the rising edge |
| 2 | v3 | RESET asserted by the target (its reset button, power-on). Send it as a pulse, held for at least one `T`. Never raise it in answer to `Z` |
| 3 | v3 | in the `H` reply only: target power present (the probe senses the target's Vcc) |
| 7–4 | v3 | WAIT: T-states the target held /WAIT during this cycle, 0–15. Under v4 with a clock, they are the processor's own T-states; otherwise 4 MHz T-states |

**Versions.** v1 has INT and NMI; v2 adds `F`; v3 adds RESET, WAIT and power; **v4 adds `K`** (first in this emulator). A target answers `H` with any version up to the one offered, and the emulator then speaks exactly that version: FUSE and the sibling 8550 emulator speak v4 too (`notes/prototype-link.md`); a v3 target is served as before, at the board's own clock.

**Version 4: the prototype's clock.** A real probe takes the clock from the prototype's socket (070-3665 §1, §3), so in modes 1 and 2 the board runs at the prototype's rate. After `H`, a v4 emulator asks `K 0` and `K 1`. With a rate reported, the board runs at it in modes 1 and 2, and WAIT counts its own T-states. `T` stays in microseconds whatever the clock: the emulator converts each stretch of cycles at the clock it ran at. After a RESET pulse it reads `K` again, since a target that switches machines (FUSE: 48K at 3500 kHz, 128K at 3547 kHz) may change its clock. `K` replies are not bus cycles; their WAIT bits are ignored.

### What it looks like on the wire

`tools/proto_target.py --log --clock 2`, during [TUTORIALS.md §7](TUTORIALS.md#7-modes-1-and-2-against-a-prototype)'s mode 1 run, which prints "HI" on port 10h:
```
[H 0004 00 -> 04]       hello: the emulator offers v4; the target answers v4
[K 0000 00 -> d0]       clock, low byte...
[K 0001 00 -> 07]       ...high byte: 07D0 = 2000 kHz
[O 4810 48 -> ff]       OUT (10H),A with A = 48 'H': port 10h, and A on A8-A15
[O 4910 49 -> ff]       'I'
[O 0d10 0d -> ff]       CR
```
The program sits in program memory (mode 1, P blocks), so only its `OUT`s reach the target. In mode 2, or with U blocks, the target also sees every `F`, `R` and `W`.

### Writing a target

`tools/proto_target.py` is the reference: 64 KB of RAM, a console UART at ports 10h (data) and 11h (status: bit 0 input ready, bit 1 output ready), and test aids:

| Option | Does |
|---|---|
| `--tcp PORT`, `--unix PATH` | where to listen |
| `--rom FILE@ADDR` | preload a ROM image |
| `--tick MS` | a periodic INT, acknowledged with RST 38h and cleared by `OUT (12h)` |
| `--clock MHZ` | report a clock (v4); the target time seen is printed at disconnect |
| `--io-wait N`, `--reset-at MS` | WAIT states on every I/O cycle; one RESET pulse |
| `--log` | log every request |

Checklist:
- **Answer every request with exactly two bytes, in order.** Requests may arrive in batches: split what arrives into 4-byte requests and reply to each.
- **Answer `H` with a version no higher than offered**, and use only that version's messages and reply bits. If you don't know `K`, answer `H` with 3.
- **Keep INT asserted** until your device is serviced. The emulator takes it when the program has interrupts enabled, and then sends `V`.
- **Drive your timers from `T`**, never from the wall clock. The emulator runs faster or slower than real time (batch runs, turbo). If you stop answering, the processor waits: a stopped target clock, as on a real probe.
- **RESET is a pulse you originate.** Don't echo it back on `Z`.
- **WAIT is optional.** Report it only if your hardware has wait states.

`tests/prototype.sh` runs modes 1 and 2 on both boards against `tools/proto_target.py`, including the clock: a fixed loop takes the target 21,647 µs at the board's 4 MHz and 43,295 µs at a reported 2 MHz.

**FUSE** (the TC3256 EMUL repository, branch `feature/tek8550-probe`, `--tek-probe`) is a ZX Spectrum as a v4 target (it reports 3500 kHz for a 48K, 3547 for a 128K); in mode 2 the Spectrum ROM runs on the 8002's Z80 board ([TUTORIALS.md §8](TUTORIALS.md#8-a-zx-spectrum-as-the-prototype)).

**Not part of the protocol:** bus timing beyond WAIT, DMA from the target, and the copying of mode-1 writes to P blocks that the 8550's board does (nothing says the 8002's does). Each would need a new version.

---

## 2. The serial ports

The System Communications board has three RS-232 ports on MC6850 ACIAs (`machine/tek8002_syscomm.*`), at 2400 baud. Each can be plugged into something with one option:

| Option | Port | TEKDOS | Interrupt |
|---|---|---|---|
| `-rem SPEC` | J101 modem (CA/CB) | REMI, REMO | 6 |
| `-lpt SPEC` | J102 line printer (CC/CD) | LPT1 | 7 |
| `-ppt SPEC` | J103 paper tape reader/punch (CE/CF) | PPTR, PPTP | 8 |

| SPEC | What is plugged in |
|---|---|
| `file:PATH` | a printer: what the port sends is appended to PATH; nothing comes back |
| `tcp:HOST:PORT` | your program, listening |
| `listen:PORT` | waits for your program on 127.0.0.1:PORT, and accepts again after a disconnect |
| `unix:PATH` | your program on a Unix socket |
| `loop` | a wrap-back connector: what the port sends comes back to it |

The bytes are the cable's, with no framing: what TEKDOS writes to REMO is what your program reads. The link never blocks; the machine runs on while nothing is connected. `host/serial_link.*` is the 8550 emulator's, unchanged.

**What a peer must know.** `tests/syscomm.sh` checks output, byte for byte: LDIR to LPT1, COPY of a source file to REMO and to PPTP. Input is open: a line sent to REMI or PPTR arrives intact, but a second line sent without a pause is lost while TEKDOS files the first. On the console TEKDOS sends DC3 (Teletype "reader off") after each line, which suggests that the senders of the time paced themselves; no handshake has been seen on REMO yet (`notes/8002.md`). Until it is understood, send one line, then wait for TEKDOS.

---

## 3. Lamps, switches and front panels

Front ends never look inside the machine. They read **named outputs** and send **named inputs** through `tek8002_system` (`emu/src/system/tek8002_system.h`), as MAME's output manager works.

**Outputs** (1 = lit, each the state over the last 1 ms tick):

| Name | What | |
|---|---|---|
| `8002:pwr` | the mainframe's power | |
| `8002:sys` | the MSTR/SLV flip-flop gives the bus to the System Processor (070-2711 schematics 4A/4B, 9) | that SYS is the legend over the MASTER LED pair is inferred |
| `8002:run` | the bus line RUN (a processor runs) while HOLD is not active (schematics); no bus hold is modelled | dark while SYSTEM RESTART is held: inferred |
| `fdu:pwr` | the disc unit's power (there is no lamp for it on the unit) | |
| `fdu:drive0`…`fdu:drive3` | a drive is busy (an emulator aid, not on the real unit) | |

**Inputs** (switches; each is also an output of the same name, so the artwork can draw its position):

| Name | Kind | Does |
|---|---|---|
| `8002:power` | rocker, 1 on | at power-up the bootstrap PROM runs again |
| `fdu:power` | rocker, 1 on | an unpowered disc unit answers nothing |
| `8002:restart` | momentary, 1 held | SYSTEM RESTART: the 2650 restarts when it is let go |
| `8002:diag` | momentary | DIAG INTRPT: the Debug module's interrupt 28 |

`tek8002_system::input(name, value)` sets one, `press(name)` gives a timed press; `-input SEC NAME=VALUE`, `-press SEC NAME` and the monitor's `p` script them. The GUI handles one more input itself: a click on a drive door (`drive="N"` in the artwork) ejects the disc (`driveN:eject`), and a file dropped there is mounted.

To add a lamp, publish it in `tek8002_system::update_outputs()`, which runs every tick; to add a switch, handle it in `tek8002_system::input()`. Front ends pick up new names with no code change.

**Artwork.** The panels are MAME `.lay` files, `emu/artwork/8002a.lay` and `fdu.lay`, a subset described in `emu/src/frontend/sdl/layout.h`. They are generated: edit `tools/gen_panels.py` and run `tools/gen_panels.py emu/artwork`, never the `.lay` files. In the layout, an element's `name=` binds it to an output (a lamp's state, a switch's position) and `inputtag=` makes it clickable as an input. The generator's docstring lists the bindings, and how the panels were drawn after photos of a surviving pair of units and 070-2701 Fig. 2-6.

---

## 4. Adding an emulator board

Tektronix made emulator boards for many processors; the 6800 (E2) and the Z80 (E3) are emulated. 070-2711 Appendix E names the others' command ports: E1 the 8080, E5 the 8085. A new board has three partners: **TEKDOS**, which drives it through its command port and its firmware; the **Debug module**, which runs it as a slave; and, in modes 1 and 2, the **prototype**.

### What TEKDOS expects of a board

The life cycle, as TEKDOS drives the 6800 and Z80 boards (`notes/emulators.md`):
```
start-up             (TEKDOS 3.3, 6800) probe: board active (90), forced jump to RESTORE, HALT; the board
                     answers with its debug SVC; with no board, interrupt 27 says nobody took the bus
LOAD                 the program goes into program memory (mode 2: through the firmware's PUT)
GO                   register block swapped into 0000-; forced jump to RESTORE -> debug SVC 1
                     control <- run value (92, B2, F2), forced jump to the user's PC; the 2650 halts
... program runs ... the 2650 is halted; the board owns the bus
break                SVC, breakpoint, single cycle, HALT/WAI, or Esc
                     control <- 80, forced jump to DUMP -> debug SVC 1; TEKDOS reads the block
DSTAT, trace line    from the register block, and the board's status byte
```

Find, for the new board:
- **its control port** and status bits, from TEKDOS's writes (`-io`), its emulator table (2032/2033), and the board's manual if it survives;
- **its firmware entries**: TEKDOS's DEB INIT loads them (on the 6800 disc at 195D–196E, on the Z80 disc at 1958–1969), and the kernel derives GET and PUT from the table at 24CB;
- **its register block**: the bytes TEKDOS swaps around each firmware call and DSTAT prints (fill the block with marker bytes and read DSTAT);
- **its SVC mechanism**: I/O ports on the Z80, memory-mapped on the 6800;
- **its halt instruction**, which status bit 4 reports.

### Where it goes in the code

1. **The processor core.** Port it from MAME into `emu/src/cpu/<name>/`, changed as little as possible, each change marked `[tek8002]`, and recorded in `emu/src/cpu/SOURCE.txt`. Prefer generating it with a script, as `tools/port_m6800.py` does, so the device code stays verbatim.
2. **The board class.** Derive from `tek8002_emboard` (`machine/tek8002_emboard.h`), as `tek8002_ep6800` and `tek8002_epz80` do. The base gives it the control port, the status byte's common bits, the firmware window (control bit 1 = 0), the firmware's SVCs, GET/PUT, the modes and the prototype's clock. The board supplies:
   - the processor: `cpu_reset`, `cpu_step`, `cpu_jump` (which also ends a halt), `cpu_pc`, `cpu_halted`, `cpu_cycles`, `board_clock_hz`;
   - its memory and I/O, routed by `proto_mem(a)` / `proto_io()` to `proto_read`/`proto_write`/`target`, or else to `m_bus.slave_read`/`slave_write` (program memory, the memory map and the breakpoint comparators); its SVCs with `svc(port)` while `m_bus.svcs_enabled()`;
   - `firmware(entry)`: the restore, dump, GET and PUT routines at their entries, each ending with `svc(1)` (restore, dump) or through `get_put()`;
   - `board_status()`: its own status bits;
   - `target_lines(reset_edge, nmi_edge, irq)`: what the prototype's RESET, NMI and INT do to it.
3. **The mainframe.** Add the board as a member of `tek8002` and to `m_slaves`. That is all: the Debug module runs whichever slave is active (`tek8002_debug::run_slave`), with the forced reset and jump, SVCs, breakpoints, single cycle, halt, PC last/next and the interrupt mask, the same for every board.
4. **Modes 1 and 2.** The link carries Z80 pin semantics, and the 6800 uses the subset that fits its pins. A board with other pins (the 8085's RST 5.5–7.5, SID/SOD) needs a new protocol version first.
5. **Validate** with TEKDOS for that processor: ASM, LINK, LOAD, GO, the SVCs, then DEBUG's TRACE ALL and STEP, BKPT, DSTAT, SET, and the halt instruction (ERROR 46 outside DEBUG, a break inside). Write a session test like `tests/runz80.sh` and `tests/run6800.sh`, and add modes 1 and 2 to `tests/prototype.sh`. `TEK_EPTRACE=1` logs every firmware call with its block, and every forced jump.
6. **Record** what was established in `notes/emulators.md`, each fact with how it was found; label what is inferred. Add a `CHANGELOG.md` entry and update `ROADMAP.md` and the README.

A new board's firmware PROM will most likely not have been dumped, so its routines start as a high-level model. Say so plainly in the notes. Then look for a copy: the 6800 board's PROMs were dumped in 2026, and the Z80 board's firmware turned up inside a diagnostic's PROM test; each time the real firmware replaced the model, which stays behind `-ep6800 hle` / `-epz80 hle`.

---

## 5. Programs that drive the CT8100

A program on the emulator board can use the CT8100's host features through TEKDOS's console output: cursor addressing (FS, X = column + 32, Y = line + 32), clear page (ESC FF), read cursor (ESC ]; the terminal answers GS, X, Y, CR on the console input), display fields (US and a field attribute code), clear input (ESC O) and the rulings set (SO, SI). The codes are in `notes/ct8100.md` and 070-2359 Table 2-1/2-2. `host/ct8100` interprets them; `-snap` saves the screen as text (rulings as their nearest ASCII, fields' looks dropped); the GUI draws everything. The terminal front end does not translate them for your own terminal.

## 6. Conventions

- **Fidelity:** model the 1979 hardware and nothing more. Where something had to be inferred, say so.
- **Compatibility:** protocol changes are additive and versioned; older peers keep working.
- **Determinism:** the system has no wall clock. Anything that advances emulated time must behave the same in real-time and batch runs. The tests and pictures are batch runs.
- **Regressions:** run the list in `CLAUDE.md` (`make -C emu test` and the session tests) before submitting.
- **Licensing:** new original files carry `SPDX-License-Identifier: GPL-2.0-or-later` and their author's copyright line; ported files keep their own (see [NOTICE.md](../NOTICE.md)).
