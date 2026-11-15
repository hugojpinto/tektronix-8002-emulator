# RTPA (Real-Time Prototype Analyzer) on the 8002A

**Status:** emulated (`-rtpa`), ported from the sibling 8550 emulator on 2026-11-15. The same
files serve both (`emu/src/machine/rtpa.{h,cpp}`); the board reference, with every register and
how each rule was established, is the 8550's `notes/rtpa.md`
(`../tektronix-8550-emulator/notes/rtpa.md`). This note covers the 8002 side: TEKDOS's commands,
how each emulator board looks to the analyzer, the evidence the 8002 material added, and what
is still open. Tutorial: [docs/RTPA.md](../docs/RTPA.md).

| Check | Result |
|---|---|
| `tests/rtpa_guide.sh`: the RTPA users guide's seven procedures (070-2785 §3) on its 8080 sample program, against the displays a real 8002A printed | 81 checks pass: every bus-cycle count, pass count and break line; microsecond counts within the guide's "one or two microseconds" |
| `tests/rtpa.sh`: TEKDOS Z80's EVT, BIF, CNT, DRT on a Z80 loop | passes |
| D3A21 "8080 HWA" (Millennium's DIP 4.4 diagnostics, `notes/diagnostics.md`) | 13 of 16 subtests pass; 0F and 10 need the probe (bypassed) |
| D3A30 "6800 HDWR ANA" | 16 of 18 subtests pass; 11 and 12 need the probe (bypassed) |

## Sources
- **070-2785, *8002 Real-Time Prototype Analyzer Users Guide* (1979)**, `sources/bitsavers/manuals/800x/`: the commands (§2), seven procedures with the lab's displays (§3), command interactions (§4), applications in modes 0–2 (§5), and each emulator processor's clock and wait states (Appendix A). Its sample program (Figs. 3-1 to 3-3) is in `tests/rtpa_guide.sh`.
- **070-2356, *8001/8002 Real-Time Prototype Analyzer Service* (1977)**: theory of operation (§2) and the control, data and status bytes (§3).
- **Millennium's 9508 source** (S171RTA on `99900823-01`): the same board's registers, named (`../tektronix-8550-emulator/notes/rtpa.md`, "An independent source").
- **D3A21 and D3A30** on `diag_4.4_5-23-79`: each subtest is a table record (below) with the register set-up, a slave program and the expected results.
- **TEKDOS**: its start-up programs the board (C6/C4/C5 sequences; `notes/8002.md`, ports C0–C7), and its command table names the commands (070-2706 booklet).

## TEKDOS commands
TEKDOS 3 (8080, 6800 and Z80 versions) answers to `EVT` (`EV`), `BIF`, `CNT` (`CN`), `RTT` (`RT`)
and `DRT` (`DR`). `BIF`, `RTT` and `DRT` work only in the debug system: enter `DEBUG` (`DEB`)
first, or they give error 03 ("only available in the debug system", 070-2706). The guide also
turns the debug trace off (`TRACE OFF`) before its procedures.

| Command | Does | Notes |
|---|---|---|
| `EV 1 A=209 D=7D B=F P=20 C=500` | event 1: address (`A=`, `A<`, `A>`; 4 hex digits), data, bus type (`F` fetch, `MR`, `MW`, `IR`, `IW`, `W`, `ALL`...), probe pattern `T=`, pass count `P=`, delay `C=` (in the CNT unit) | `EV 1 CLR`, `EV CLR` (both events, and BIF); `P=0`, `C=0` remove them; `EV` alone displays |
| `BIF 1`, `BIF 2`, `BIF IND`, `BIF ARM`, `BIF FRZ`, `BIF LIM`, `BIF CLR` | which triggers break: event 1, event 2, either, event 1 arms event 2, ARM with the trace frozen at event 1, event 1 within event 2's limits | a return option `S` (stop, the default) or `C` (print the break line and go on) per event: `BIF IND C`, `BIF FRZ C`. Cleared, it shows `IND` and never breaks |
| `CN C` | what the general-purpose counter counts, and the unit of every delay: `F` fetches, `C` bus cycles, `E` emulator clocks, `U` µs, `M` ms, `T` trace stores, `1`/`2` event occurrences | one letter (`CNT FTCH` gives error 32); `CN` alone shows `COUNT=00739 CYCL` |
| `RT F` | what the trace stores: `ALL`, `F`, `MR`, `MW`, `IR`, `IW`, ... | |
| `DR 25`, `DR *` | the last 25 (or all 128) stored transactions, with mnemonics and bus letters | |

## How the boards look to the analyzer
The RTPA sees the bus cycles of the board that runs the user's program (`rtpa_visible()`,
any active board), and each board tells it what its cycles are. Established on 2026-11-15:

| Board | Bus as the RTPA sees it | Evidence |
|---|---|---|
| **8080A** | **3 MHz** in mode 0, **one wait state per memory cycle** (none on I/O cycles; none from prototype memory in mode 2); an I/O cycle carries the port on A15–A8 as well as A7–A0 (OUT F7 is address F7F7); a conditional jump or call that is not taken still reads its two address bytes | 070-2785 Appendix A ("Mode 0 System Clock 3 MHz"; Table A-1 one wait per machine cycle; Table A-4 measures 8197 clocks for 6404); the guide's counts (17 µs, 36 µs, 19 µs a loop); D3A21 subtest 0D (694 clocks, exact), "TRACE I/O WRITES" (F7F7), the traced JZ operand reads. The core change is `[tek8002]` in `cpu/i8085` (`emu/src/cpu/SOURCE.txt`) |
| **Z80** | 4 MHz, two wait states per memory cycle except from prototype memory in mode 2 (unchanged; now also given to the RTPA as cycle lengths) | 070-2785 Table A-1; `notes/emulators.md` |
| **6800** | 1 MHz, no wait states; its SVC page **FE00–FEFF counts as I/O** (the board's MNIO line), and an SVC store (STAA FEF7) is a traced write with its data | 070-2349 §10 ("the 6800 microprocessor uses memory mapped I/O, the memory region FE00 through FEFF is reserved as addresses for SVC's"); D3A30's trace (FE00 as I/O cycles) and its "TRACE I/O TEST", which arms on the write of 10 to FEF7 |
| **SVCs** (all boards) | the SVC cycle reaches the RTPA with its real address and data (the 8080's OUT F7 with the accumulator), and a 6800 SVC load is traced once, as the load it is | D3A21 "TRACE I/O WRITES ONLY" (data 80, 40, ... 01); D3A30 subtest 0D |

With the cycle lengths known (`bus_states()`), a time count (clocks, µs, ms) starts after the
cycle that arms and stops with the cycle that breaks, instead of counting whole instructions.

## What the 8002 material settled
The rules are in the board reference (`../tektronix-8550-emulator/notes/rtpa.md`, "Documentation
of the board"); in short:
- **LIMit** is event 1's whole comparison within event 2's address and bus-type requirements; event 2's data, probe, pass and delay take no part (070-2356 §2 step 4b; guide 6.C, 6.D).
- **Data compare modes** are the address codes (≥, 8-bit =, 16-bit =, ≤) applied to the data (070-2356 §3).
- **Status event bits** are the events matching on the breaking cycle (D3A21/D3A30).
- **Delays count in the CNT unit**, share the G.P. counter's gate (so an event-1 delay never runs out under ARM), and restart at each occurrence unless running (guide 2.B, 3.B.4, 4.A.1).
- **ARM** takes event 2 only from the cycle after the arming one, and event 2's trigger disarms; under **FRZ** the trace then runs again until event 1 (070-2356 §2; guide 3.B.6, 5.A.3–5.A.5).
- **Counting stops at the break**, for the bus-driven sources (guide 2.B.1; D3A21).
- **The PC latches follow every instruction**, so TEKDOS's halt line reads right with the board fitted (guide 3.B.4).
- **Status bit 3** (slave clock missing) stays clear after a break here (`stop_status_08 = false`), as the diagnostics expect.

## The diagnostics' hardware analyzer tests (D3A21, D3A30)
Run with `-rtpa` from the diagnostic disc: `emu/build/tek8002 -rtpa -fdc lle -prom roms/tekdos_boot.bin -0 COPY_OF_diag_4.4.img -cycles $((1500*1250000)) -type 15 "LO 3A21\r"` (or `3A30`). Each failing subtest prints its event set-up, the counters (PC last, PC next, G.P.) and status as "WAS / S/B", and the trace both ways.

**Reading the tables.** Each subtest is a record before its name: `81 04 00 xx <ptr>`, the 8253 initialisation (`34 ff ff 72 ff 00 34 00 ff 72 00 00 b4 03 de b0`: counter 0 mode 2, counter 1 mode 1, the G.P. counter loaded FFFF, the msec prescaler 03DE = 990, counter 2 mode 0 or 2), then `00 nn`, the expected status, C1, C2, C3, event 1's record (bus value, mask, flags, C7 mode, data mode, ...), event 2's, and `ptr ptr 07 len` with the name. The slave program follows in the file (D3A21's is `06 01 0E 80 DB 00 78 D3 F7 B9 CA 15 00 07 47 32 18 00 C3 04 00 C3 04 00`: MVI B,01; MVI C,80; IN 00; MOV A,B; OUT F7; CMP C; JZ 0015; RLC; MOV B,A; STA 0018; JMP 0004). Reading them the way the Z80 board's subtest F was read (`notes/emulators.md`) gave each rule above a concrete case.

| Subtest (both tests) | 8080 (D3A21) | 6800 (D3A30) |
|---|---|---|
| break on any event, 8-bit address, < and > address, delays, pass counts, trace addressing | pass | pass |
| TRACE ALL (LIMIT) | **fails**: msec G.P. 0, S/B 1; and the IN cycle's address | pass |
| TRACE FETCHES | pass | pass |
| TRACE I/O (ARM, µs) | **fails**: 27 µs, S/B 29 (the guide allows ±2); and the IN address | **fails**: 97 µs, S/B 95 |
| TRACE I/O WRITES ONLY | pass | pass |
| TRACE I/O READS (clocks) | **fails** on the IN address only (694 clocks: exact) | **fails**: 211 clocks, S/B 217 |
| TRACE WRITES | pass | pass |
| 16-bit data and T-clip, T-clip mask | bypassed (probe) | bypassed (probe) |

**Open:**
- **The 8080's IN cycle address.** IN 00 is expected in the trace as address 2000, not 0000 (the 8080 puts the port on both halves; OUT F7 does show as F7F7). Bit 13 set on an input cycle is unexplained.
- **The msec count.** D3A21 expects 1 ms counted over a 0.3 ms run in one subtest and 0 over another programmed the same way; the prescaler is a mode-1 one-shot whose trigger is unknown.
- **The 6800's timing**: 2 µs and 6 clocks off in runs that take an SVC every loop and access the FE page; whether those cycles are stretched is unknown (the guide gives the 6800 no wait states).
- **The µs time base's phase**: guide 4.A.1 run in µs breaks one instruction later on the real lab, and shows two event-2 lines where the emulator shows three; the same run in bus cycles matches line for line.
- **A parked firmware's fetch.** Before the program, the guide's trace holds the board firmware's NOP fetch at FBA8 (where it waits for the forced jump); here the last entry before the program is the firmware's SVC cycle.

## Found on the way
- **The MAME 8080's conditional jumps and calls** skip their operand reads when not taken; the 8080 reads both bytes (3 machine cycles). Fixed in 8080 mode (`[tek8002]`, `emu/src/cpu/SOURCE.txt`).
- **The MAME 6800 port** evaluated a bus-reading argument of its illegal-opcode log message even with logging off, adding a phantom read that the trace showed. `tools/port_m6800.py` now makes `logerror` a macro that evaluates nothing unless logging, and carries the TAP/RTI `[tek8002]` edits, which regenerating had dropped.
- **The 8080 board's clock** was inferred as 2 MHz; Appendix A gives 3 MHz.
