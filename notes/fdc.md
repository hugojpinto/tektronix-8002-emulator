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
| 0001 | LDL | a register from the latches, the last TB result (`LOD IN,L,BYT` "to LTCHS", then `LDL R15`) |
| 01dd | Type A, registers | ALU function bits 11–6 (S3 S2 S1 S0 M CI, Table 3-2), register bits 5–2, special bits 1–0 (SPSH 01, PULL 10, EPSH 11); dd = L / DR / A |
| 0111 | jump through a register | condition bits 11–8, register bits 5–2 (the listing's R12, R13, R5; not complemented, whatever Fig. 3-15 says) |
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
  match the listing's (OCR spellings like LOO for LOD mapped). 154 of the 155 direct jump targets
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
  Its 0s are logical 1s: INL bit 0 = 1. Reading the figure the other way gives an even INL, and
  the sector arithmetic then misses every other sector. The schematic confirms this (stage D).
- **Interleave switch S4104:** see stage D. The schematic gives INL = 03 at the factory setting.
  Stage B took 0x0F from Fig. 3-8, which turned out to be wrong.
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

## Stage C: tracks, and reads through the microcode (`emu/src/machine/tek8002_fdd.*`, `-fdc lle`)
The drives hold hard-sectored tracks of FM bit cells, built from the images. TEKDOS boots
through the real microcode, including the boot PROM's 16K read from one command, and LDIR gives
the same listing as the high-level model on all 33 discs in the collection. Established:
- **Sector layout (Fig. 1-5 and the read routine):** the sector pulse starts the sector. Then come
  128 cells of 0, the sync byte FB (RD1 at 14B finds it by its bit pattern, within 256 bits),
  track, sector, 128 data bytes, and CRC high and low. The rest is 0s up to the next pulse. Bits
  go MSB first (RD1–RD3 shift them in at the LSB), one Data Ready per 4 µs cell.
- **CRC:** CCITT (x^16 + x^12 + x^5 + 1, §3 "CRC Generation"). The read routine seeds R14/R15
  with the track and sector bytes (173–176), then shifts data and CRC through them (CRC2 at 182).
  That is the standard CRC from 0 over track, sector and data, with a zero remainder over the CRC
  bytes too. MSK1/MSK2 = 10/21 are the polynomial.
- **Where the sectors are:** the executive zeroes its sector count R7 at INDEX (02B) and adds INL
  at every SECTOR pulse (03F–042). It reads or writes when R7 equals the sector asked for (04F),
  and the header's sector byte must match (16B). So physical sector k after the index holds
  logical sector ((k + 1) × INL) mod 32. No constant step mod 32 gives the manual's "sector 00,
  sector 16, sector 01, sector 17" (§1 "Sector Interleaving") exactly. The schematic settles the
  step (stage D).
- **Head load:** FILE INOP pulsed (SFI, RFI at 018; Fig. B-4 "Reset FILE INOP RST/LD HD") loads
  the selected drive's head. RAISE HEADS pulsed (031–032) unloads it.
- **FLAG through an operation:** after the track byte of a read (count R13 = 128, 08E–098) the
  executive skips RFG (020–026), so FLAG stays up through the seek and the read. It drops when
  the sector is in the buffer. The boot PROM's "RST, wait FLAG 0" after each byte is this wait.
  **The System Processor's disc interrupt is therefore clocked by FLAG going low:** TEKDOS sends
  the track byte, waits for FLAG 1 (26B1), and only then enables the interrupt (EA ← 37 at 2620),
  with FLAG already up. The high-level model's rising "completion FLAG" and its RST-drops-FLAG
  rule (`notes/fdu.md` points 1 and 4) were two halves of that one wrong edge. Both are now
  replaced by this behaviour, and every TEKDOS write test still passes.
- **IND data is read during the strobe:** EB shows the accumulator only while C0 is active. The
  boot PROM reads EB before its RST (00D3).
- **Multi-sector reads are real:** after a sector, X3 (10F) advances the target sector, and the
  track at sector 31. The boot PROM's 128-sector read and its 16K come from one command, as the
  high-level model had inferred (`notes/fdu.md` point 3).
- **Error recovery (§3, X at 100):** after a CRC error the controller retries. Then it wipes:
  seek out to track 00 (X1 sets track X to 0), in to 76 (`LOD l,L,76; LDL R11` at 128), and back
  to the target (`LOD SR,L,R14; LDL R11`). It reads once more, and then gives up: FLAG drops with
  UNIT CHECK and the sense byte (CRC 20, ID 10, SYNC 08). The sector as read stays in the buffer.
  The drive byte's bit 6 skips the recovery (ERTN at 19C, J6N on R4).
- **Two decoding errors from stage A, found here:**
  - Through-register jumps name the register directly. The listing's 7A30 is `JLB SR,M,R12`, the
    return vector 172 stores. Fig. 3-15's "1s complement of register number" does not describe
    the field. With the complement, the read routine returned through R3, landed on X2 (success)
    and never checked the CRC. `tools/fdc_dasm.py --check` now compares these operands: all 3
    match.
  - LDL (0001) loads the register from the latches, the last TB result (0A8 "INPUT HOST BYTE TO
    LTCHS", 0A9 "LTCHS TO R15"). It is not a no-op.
- **Checked by `fdc_test <roms> <image>`:**
  - The tracks decode back to the image.
  - The autoload succeeds.
  - Sectors across the disc read back equal to the image, with a zero CRC remainder.
  - A flipped cell gives CRC ERROR after the full recovery, and the sector as read.
- **Not modelled:** sector and index pulse widths are not in the manual (0.5 ms here; any width
  under a sector's trailer behaves the same).

## Stage D: writes, FORMAT, and the real controller as the default
TEKDOS's FORMAT, COPY, DELETE, ASM/LINK, COPYSYS and DUP run through the microcode
(`tests/disc_tools.sh`, `tests/asm_link.sh`, `tests/syscomm.sh`, and tekmato's regression).
FORMAT of a never-formatted disc (`-1 new:FILE`, no flux at all) leaves an image byte-identical
to the real formatted disc Tek8002_1b. `-fdc lle` is now the default; `-fdc hle` keeps the
high-level model, and every test runs in both modes. Established:
- **Write timing (DOUT at 1C1):** a PULL of the data bit (R1's MSB), then 2 µs later a PULL of the
  clock bit (R12 = FF gives eight 1s). Each takes exactly 10 cycles ("NEED 10 CYCLES IN THIS
  SEQUENCE", "400 NS DELAY PER EXECUTED JUMP"). The sector data path (WRT5–5B, 1D9–1ED) keeps the
  same spacing. A PULL with C7 = 1 is a flux pulse.
- **The drive model holds flux pulses on a 2 µs grid** (`tek8002_fdd`). Writes are placed relative
  to the start of WRITE GATE, which erases what it passes over. Reading separates clock from data
  by the format's own rule: FM here never omits a clock, so an empty slot is a data slot. That sets
  the separator's phase, so a write in any phase reads back. The sync byte is found by its bits.
- **The Sector Buffer shifts at EPSH, not SPSH:** SPSH loads the Push flip-flop, and EPSH ("finish
  push") clocks it into the buffer (§3 "Sector Buffer"). The write loop pushes at 1E1 (recirculate)
  and must still send that same bit at 1E2 (`RLC IN,L,SBH,PULL`). Every SPSH in the listing has
  its EPSH later. Shifting at SPSH wrote every sector one bit late.
- **Write commands:**
  - 0E (Write Formatted): after stepping, the seek reads a sector ID to verify the track (GTID2,
    0F6–0F8).
  - 0D (Write Unformatted, what FORMAT sends, 2,464 times): the seek recalibrates at track 00
    (0D9–0DA) and reads no ID. So FORMAT works on a disc with no flux, while 0E there waits for
    a clock that never comes.
  - The host sends command, drive, sector, track, then 128 data bytes by OTD. FLAG stays up after
    the 128th until the sector is written.
- **S4104, from the schematic** (070-2587 p. 90, "Interleave Switch"):
  - Each section grounds one input of a 7403 open-collector NAND whose other input is the
    INTERLEAVE IN enable. An open switch's input floats high; there are no pull-ups.
  - So a closed (On) switch leaves its WR line high, which is logic 0 on the active-low bus.
  - WR0's NAND has both inputs on the enable, so bit 0 is always 1.
  - Switch 4 (pins 4–7) drives WR1, 3 drives WR2, 2 drives WR3, and 1 (pins 1–10) drives WR4.
    Switch 5 (pins 5–6) is not connected.
  - At the factory setting (5–1 = Off Off On On On), **INL = 03**: consecutive logical sectors are
    11 holes (≈ 57 ms) apart (3 × 11 ≡ 1 mod 32), about a third of a turn.
  - Fig. 3-8 draws SW5–SW1 on bits 5–1, which disagrees with the schematic. The schematic is the
    wiring. Read Fig. 3-8's way, the setting would give step 17.
  - Neither step gives the §1 text's "00, 16, 01, 17" exactly. That text may describe another
    setting, or only loosely the idea of interleaving. A real disc's flux capture would show the
    order directly: every header carries its sector number. (Schematic read for this project from
    the bitsavers scan; switch numbering = the DIP's pin pairs n and 11 − n.)
- **Saving:** under `-rw`, what the tracks decode to is written back. A sector that does not decode
  (never written, or damaged) is saved as zeros, with a warning, since a raw image cannot hold it.
- **Speed:** with the controller and the drive's flux, batch runs go about 40× real time. The
  high-level model is faster (`-fdc hle`).

## Still to establish
- **The interleave step: 3 or 17.** The schematic's wiring (section 4 → WR1 … section 1 → WR4)
  gives 3 at the factory setting. The manual's other sources all point the other way:
  - the set-up step (§1, 5–1 = Off Off On On On);
  - Fig. 3-8 (SW5–SW1 on bits 5–1, giving 0x31, step 17);
  - the §1 text ("sector 00, sector 16, sector 01, sector 17", a half-turn alternation that step 17
    reproduces and step 3 does not).
  Vince's board (EEVblog #151, 2026-11-12) has a 5-position S4104 at that setting, and he reads
  the interleave as "16 sectors, half a disc rotation". The microcode boots, FORMATs and runs the
  disc tools with INL 03, 11 or 31 alike (checked 2026-11-13), so the emulator cannot decide. What
  would decide it: a continuity check on a real board (S4104 pin 10, section 1, to U2040A pin 1 =
  WR4 as drawn, or to the WR1 gate), or a flux capture of a real disc. It matters for writing real
  discs: the controller checks each header's sector number. The model keeps 3 until then.
- Sector and index pulse widths (the drive's manual, 118-0195-00, is not on bitsavers).
