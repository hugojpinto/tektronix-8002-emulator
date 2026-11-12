# The Flexible Disc Controller's microprogram

The 8002A disc unit's controller (070-2587 section 3) is a 16-bit microprogrammed
machine built from TTL around an 8-bit ALU. It does the FM encoding, the sector framing
and the CRC in microcode, bit by bit. Its program is 512 words in two 512 × 8 PROMs,
U1070 (160-0025-00) and U1080 (160-0029-00). This note is stage A of roadmap step 5:
the microcode as ROM images, and a disassembler that reproduces the manual's listing.

## The ROM images (`roms/fdc/`)
`fdc_bits15-8.bin` and `fdc_bits7-0.bin` hold the instruction words' upper and lower
bytes. Which of U1070/U1080 holds which half is not in the OCR text; only the
schematic shows it (open).

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
  lower or upper 256 words (§3 "Read-Only Memory"). SH8/SL8 set and reset it, and the next jump
  lands in the new bank (`054 SH8; 055 JMP WRT` reaches 1AA).
- **Established:** `tools/fdc_dasm.py --check` against the listing's OCR text. All 512 op codes
  match the listing's (OCR spellings like LOO for LOD mapped). 154 of the 155 jump targets
  whose label the symbol table gives match its decimal column; the exception is E9B, where
  that column is garbled and the hex column agrees with the decode.

## Still to establish (stage B)
- The semantics behind the decode: T-register and jump-condition timing, the carry rules per
  function, the Sector Buffer (a 1024-bit shift register, SPSH/EPSH/PULL), the read logic
  (separated clock/data, Data Ready), write logic, and the 400 ns machine cycle (§3
  "Instruction Execution Time").
- Which PROM is which half (schematic).
