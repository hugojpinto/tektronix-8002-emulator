# The Flexible Disc Controller's microprogram

The 8002A disc unit's controller (070-2587 section 3) is a 16-bit microprogrammed
machine built from TTL around an 8-bit ALU. It does the FM encoding, the sector framing
and the CRC in microcode, bit by bit. Its program is 512 words in two 512 × 8 PROMs,
U1070 (160-0025-00) and U1080 (160-0029-00). This note covers stages A and B of roadmap step 5:
the microcode as ROM images, a disassembler that reproduces the manual's listing, and the processor that runs it.

## The ROM images (`roms/fdc/`)
`fdc_bits15-8.bin` and `fdc_bits7-0.bin` hold the instruction words' upper and lower
bytes: U1080 (160-0029-00) holds bits 15–8 and U1070 (160-0025-00) bits 7–0, as the
schematic shows (070-2587 p. 88; the OCR text does not say).

They come from **three printings in the manual**, extracted and compared:
1. **Appendix C listing, binary column:** 16 characters of 0/1 per instruction, which the OCR
   rarely gets wrong. All 512 addresses were recovered. Addresses follow from the sequence,
   with the OCR'd address column as a check, after repairing split fields (`01 E1`, SEQ
   `01S1`, binary with stray spaces).
2. **Appendix C listing, hex column:** agrees with the binary in every word. Seven differ
   only by OCR confusion (`CFCS` = CFC8, `cco0` = CC08).
3. **Fig. 3-5a/b, ROM contents in hex:** 492 of 512 MSB and 498 of 512 LSB bytes are
   readable, and every one agrees (allowing B↔8, D↔0, S↔5/8). The 01D0 row of 3-5a lost its
   label in the OCR and was placed by hand. Where the figure looked different at first
   (`BB 1B` at 000A), the listing's binary and operand (`LOD l,A,'H88`) settle it.

## Instruction set (Appendix B, Table 3-2), as `tools/fdc_dasm.py` decodes it
Bits 15–14 select the WR bus source, bits 13–12 the destination:

| 15–12 | Format | Decoding |
|---|---|---|
| 0000 | Type S command | bits 11–0 name the command (Fig. B-4: RST, CDA, set/reset BUSY, ILLEGAL OP, ERROR RECOVERY, UNIT CHECK, FILE INOP, WRITE GATE, PRINT STB, HOST FLAG, RAISE HEADS, DIRECTION, STEP, EN STACK, EN DISPLAY, SELECT CARRY, >TRK43, UBANK) |
| 0001 | LDL | load latches |
| 01dd | Type A, registers | ALU function bits 11–6 (S3 S2 S1 S0 M CI, Table 3-2), register bits 5–2, special bits 1–0 (SPSH 01, PULL 10, EPSH 11); dd = L / DR / A |
| 0111 | jump through a register | condition bits 11–8, register = 15 − bits 5–2 |
| 10dd | Type L immediate | logic function bits 11–8, data byte 7–0; dd = L / A |
| 1011 | direct jump | condition bits 11–8; target = ~bits 7–0, in the bank UBANK selects |
| 11dd | Type A, inputs | ALU function bits 11–6, input bits 5–2 (TST, CSW, SBH, SBL, DBT, BYT, INL), special 1–0 |

- **ALU (Table 3-2):** operand A is the WR bus (a register, an input or the data byte), B is the
  accumulator. Bits 6–9 are inverted on the way to the ALU. The carry input, when bit 6 enables
  it, is forced by bit 9 or comes from the Carry flip-flop (ADC, RLC). Unary forms are fixed
  combinations: LAR Rn = LOD SR,A,Rn, LRA = 5/100100, LRZ, LRM, …
- **Jump conditions** (bits 11–8): JNZ, JZR, J6N, J6Z, J4N, J4Z, JNC, JZC, J2N, J2Z, JLB, JLZ,
  JDA (data ready), JNR (not ready), JMP; the other two are not used.
- **Banks:** the ROM Address Register is an 8-bit counter, and the UBANK flip-flop selects the
  lower or upper 256 words (§3 "Read-Only Memory"). SH8/SL8 set and reset it. The bank changes
  one fetch late, because UBANK reaches the ROM through the ROM Instruction Register (U2090 Q1).
  So the instruction after SH8/SL8 still comes from the old bank, and it is the jump that lands
  in the new one (`054 SH8; 055 JMP WRT` reaches 1AA, while the word at 155 is unrelated read code).
- **Established:** `tools/fdc_dasm.py --check` against the listing's OCR text. All 512 op codes
  match the listing's (OCR spellings like LOO for LOD mapped). 154 of the 155 jump targets
  whose label the symbol table gives match its decimal column; the exception is E9B, where
  that column is garbled and the hex column agrees with the decode.

## The machine, from §3 (for stage B, the emulation)
- **Cycle:** 20 MHz ÷ 4 gives a 200 ns machine cycle (T0 latches the instruction, T1 clocks the
  command latches, RAR CLK advances). The listing counts 400 ns per executed jump (`JMP *+1`
  as a delay), so a taken jump costs two cycles (JMP FRZ holds it for a cycle).
- **Registers:** R0–R15 (8-bit), the accumulator, the T-register (the last ALU result, kept
  through a jump: four jump conditions are its bits, TB0/2/4/6, plus zero), and the Carry
  flip-flop (C7 in arithmetic mode).
- **Register roles (Fig. 3-7b):** R0 sense byte, R3 program-state flags (seek state, error
  recovery, end of file, GTCRC, GTID1/2), R4 drive, R5 head delay, R6 command, R7 current
  sector, R8 the input CSW, R9 target sector, R10/R11 current/target track, R13 host byte
  count, R14/R15 CRC.
- **Inputs (Fig. 3-8)**, where "1 = high level = logic false":
  - CSW (TRK00, INDEX, C0, SECTOR, C1, S, READY);
  - SBH/SBL, the Sector Buffer's output bit on WR7/WR0;
  - DBT, the separated read data bit on WR7;
  - BYT, the host's output byte;
  - INL, the interleave switch S4104;
  - TST, the test panel.
- **Host port (§3 "Controller Functional Description"):** with C0 inactive (SIO, RST) the host
  reads the disc status byte (the Group 1 latches and the drive status); with C0 active (IND) it
  reads the accumulator. FLAG is the HOST FLG latch (SFG/RFG).
- **Sector Buffer:** a 1024-bit shift register. SPSH starts a push of C7 (or of the separated
  data while reading, SELECT CARRY off) and EPSH finishes it. SBH/SBL read its output bit.
  Pushing the bit read recirculates it (`RLC IN,L,SBH,SPSH`).
- **Read logic:** the separated clock pulse sets Data Ready (JDA/JNR); SPSH clears it and
  clocks the separated data bit into the buffer.
- **Write logic:** PULL loads the write flip-flop from C7. The microprogram times the FM clock
  and data pulses itself by counting cycles (`NEED 10 CYCLES IN THIS SEQUENCE`, `400 NS DELAY
  PER EXECUTED JUMP`), so a model must take the bit cells from when each PULL happens.

## Stage B: the processor (`emu/src/machine/tek8002_fdc.*`, `emu/tests/fdc_test.cpp`)
The microprogram now runs on a model of the processor. Facts settled while doing so:
- **The WR bus is active low** (WR̅ in Fig. B-2), and Fig. 3-8 prints physical levels
  ("1 = high level = logic false"). Its 1s are logical 0s, so the unused input bits read 0.
  Its 0s are logical 1s: INL bit 0 = 1, and INL = switches 5–1 on bits 5–1. Reading the figure
  the other way gives an even INL, and the sector arithmetic then misses every other sector.
- **Interleave switch S4104:** the factory setting is Off Off On On On (switches 5–1, §1), so
  INL = 0x0F. How that maps to the "0, 16, 1, 17" sector order is left for FORMAT in stage D.
- **The ALU** is a 74181 used in active-low convention, with S0/S1 inverted on the way (Table 3-2
  footnote). Arithmetic is the chip's P + G + carry on the physical levels, and the true carry
  (C7) is the physical carry inverted. Logic functions are the active-high table on the inverted
  operands.
- **CSW** as the executive's service decode (05A–05C) needs it: bit 5 = C0 asserted, bit 3 = C1
  asserted, bit 2 = strobe active.
- **HOST FLG is active while its latch is reset.** SFG writes 0 and RFG writes 1, so RST, which
  resets group 1, raises FLAG. That is how the executive acknowledges SIO (05D RST).
- **OTD raises FLAG before it stores the byte:** 066 reads BYT, 067 SFG, and the byte reaches its
  register a few instructions later (the track byte at 08E LRA R11).
- **Checked by `fdc_test`** against a drive that only turns (no data):
  - The power-on delay ends in the executive after 1.05 s.
  - The autoload (drive 0, track 0, sector 0) finds no sync and runs error recovery, and FLAG
    drops 3.3 s later with UNIT CHECK and sense 08 (SYNC ERROR).
  - SIO and four OTD bytes (88, 00, 05, 10) land in R6/R4/R9/R11, and the drive is selected.
  - The seek steps the head to track 16. The read there fails the same way, and FLAG drops once.

## Still to establish
- Stage C: a bit-level, hard-sectored FM track (sector and index pulses, separated clock/data,
  32 sectors at 360 rpm = 5.2 ms each) and reads through the real microcode.
- Stage D: writes and FORMAT (write flip-flop timing), then replacing `tek8002_fdu`'s high-level
  path, keeping it as an option (`-fdc hle`, as in the 8550 emulator).
