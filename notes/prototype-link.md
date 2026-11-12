# Connecting a target system ("prototype") to the emulated 8002

In emulation modes 1 and 2 (070-2701 §7) the emulator processor runs with its probe plugged
into your own board, the "prototype". In the emulator that board is an **external process**
reached over a socket, with the same protocol as the sibling tektronix-8550-emulator, whose Z80
board works the same way. The 8002's Z80 board uses all of it. The 6800 board has no I/O space
and no fetch pin, so it uses the subset that fits 6800 pins: `R`, `W`, the interrupt lines and
RESET (`H`, `T`, `Z` too).

```
 ┌───────────── tek8002 ──────────────┐                      ┌──── target process ────┐
 │ 2650 TEKDOS ── Debug module        │   socket (TCP/Unix)  │ memory, I/O devices,   │
 │                    │               │  4-byte requests ──▶ │ interrupt sources,     │
 │  Z80 or 6800 emulator processor ───┼── 2-byte replies ◀── │ its own time base      │
 │  (memory map: per 128-byte block   │                      │ e.g. tools/            │
 │   P program memory or U prototype) │                      │   proto_target.py      │
 └────────────────────────────────────┘                      └────────────────────────┘
```

## Which accesses go to the target (`notes/emulators.md` "Modes 1 and 2")
| | Mode 0 | Mode 1 | Mode 2 |
|---|---|---|---|
| Memory in blocks mapped **P** | program memory | program memory | target |
| Memory in blocks mapped **U** | program memory | **target** | target |
| Z80 I/O (`IN`/`OUT`) | none (reads 00) | **target** | target |
| SVC ports F0–F7 / the 6800's FEF0–FEF7 | SVCs | target (EMULATE turns SVCs off) | target |
| Interrupts, RESET | none | **from target** | from target |
| TEKDOS's access to prototype memory (`DUMP`, `PATCH`, `MOVE`, the trace's instruction bytes) | — | through the firmware's GET/PUT, for U blocks | GET/PUT for all |

- **TEKDOS's control bytes:** `EMULATE 0/1/2` makes the run value 92 / B2 / F2. Firmware
  calls use 80 (mode 0) or C0, and C4 for GET/PUT.
- **The memory map:** `MAP U lo-hi` and `MAP P lo-hi` set it, `MAP R` and `MAP M` show it. One
  bit per 128-byte block, loaded through port C9 (`notes/8002.md`).
- **MOVE** names source and destination together: `MOVE UP 8000 80FF 300` copies prototype
  memory to program memory, `MOVE PU …` the other way.
- **After error 46** ("emulator halted"), TEKDOS is back in mode 0: select the mode again.

## Protocol (version 3; version 1 and 2 targets still work)

- **Version:** the emulator sends `H` with its highest version (3). The target answers the version it will speak, which must not exceed the client's. A v1 target answers 1, and the emulator then uses `R` for fetches too. A v2 target answers 2; the emulator then ignores reply bits 2–7.
- **Transport:** a stream socket. The target listens; the emulator connects (`-target tcp:HOST:PORT` or `-target unix:PATH`). It is synchronous: every request gets exactly one reply.
- **Request**, 4 bytes: `op, addr_lo, addr_hi, data`.
- **Reply**, 2 bytes: `data, lines`. Every reply refreshes the line state.

| `lines` bit | Since | Meaning |
|---|---|---|
| 0 | v1 | INT asserted (level) |
| 1 | v1 | NMI asserted (the emulator detects the rising edge) |
| 2 | v3 | RESET asserted by the target (reset button, power-on). The emulator resets the emulator processor on the rising edge. It does **not** send `Z` back, and a target must not raise RESET in answer to `Z`, so the two can't ping-pong. Send it as a pulse, held until at least one `T` has gone by |
| 3 | v3 | in the `H` reply only: target power/clock present (the probe senses target Vcc). Ignored in other replies |
| 7–4 | v3 | WAIT: T-states of the emulator processor (counted at 4 MHz; the 6800 board's 1 MHz cycles are converted) that the target held /WAIT for during this cycle, 0–15. They are added to the Z80's T-state count, so they slow the program and show in the `T` messages. A target with a different clock converts (e.g. a 3.5 MHz Spectrum's contention × 8/7, keeping the fraction) |

| op | meaning | reply data |
|---|---|---|
| `H` | hello; addr = client protocol version (1) | protocol version (1) |
| `R` | memory read at addr | byte (FF = nothing there) |
| `F` | opcode fetch (M1 read) at addr. v2 only; prefix bytes and the opcode after CB/ED/DD/FD are M1, the opcode of DD CB d op is not | byte |
| `W` | memory write data → addr | ignored |
| `I` | I/O read: addr = the full 16-bit I/O address (addr_hi = A8–A15 = A for `IN A,(n)`, B for the `(C)` forms and block I/O) | byte |
| `O` | I/O write data → I/O address (as `I`; for OUTI/OTIR B is already decremented, as on a real Z80) | ignored |
| `V` | interrupt acknowledge (Z80 INTA cycle) | byte on the data bus (IM 0 opcode / IM 2 vector low byte) |
| `T` | time advance, addr = microseconds of elapsed Z80 time, computed from the real T-state count at 4 MHz with the fraction carried over. Sent about every 256 instructions, and only while the emulator processor runs | ignored |
| `Z` | reset | ignored |

Reference target: `tools/proto_target.py`. It has 64 KB RAM, `--rom FILE@ADDR`, and a console UART at port 10h (data) / 11h (status: bit 0 input ready, bit 1 output ready). `--tick MS` adds a periodic INT that is acknowledged with RST 38h and cleared by `OUT (12h)`. `--log` logs every access. v3 test aids: `--io-wait N` (WAIT states on every I/O cycle) and `--reset-at MS` (one RESET pulse).

## Try it
```
python3 tools/proto_target.py --tcp 18002 --log          # terminal 1: the "board"
emu/build/tek8002 -target tcp:127.0.0.1:18002 -0 sources/bitsavers/disks/Tek8002_Belgium_Amusements/Tek8002_10.img
> EMULATE 1
> PATCH 100 2120017EB72805D3102318F776                   (LD HL,120 / loop: OUT (10H),A ... / HALT)
> PATCH 120 48490D00                                     ("HI", CR)
> GO 100                                                 -> HI appears in terminal 1
```
`tests/prototype.sh` runs modes 1 and 2 on both boards this way. FUSE's `--tek-probe` target
(see the sibling project) speaks the same protocol, so a ZX Spectrum can be the prototype of
the 8002's Z80 board as well.

**Not modelled:** the prototype's clock (the boards keep their own), bus timing beyond WAIT,
DMA from the target, and the copying of mode-1 writes to P blocks that the 8550's board does
(nothing says the 8002's does).
