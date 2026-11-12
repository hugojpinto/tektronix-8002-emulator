# Flexible Disc Unit: host protocol and disc format

Modelled at high level in `emu/src/machine/tek8002_fdu.*`. Sources: FDU service manual
070-2587 (Figs. 1-8, 1-9a/b, Tables 1-1 to 1-3), service manual 070-2711 Table 3-6,
ztpe.nl Annex B (ports EA/EB), the bootstrap PROM and TekDOS 3.3.

## Disc format (070-2587 Table 1-1)
77 tracks × 32 hard sectors × 128 bytes, FM, 360 rpm, single sided.
- Track 0: sector 0 header (disc identifier, bad-block and master bit maps), sectors 1–5
  directory (8-byte names, at most 78 files), sectors 6–31 file bit maps.
- Tracks 1–4: the operating system, loaded by the bootstrap PROM to 0000–3FFF.
- Tracks 5–76: files, in 304 blocks of 8 sectors.
- Physical interleave 0, 16, 1, 17 … (switch S4104). The images are in logical order
  (315,392 bytes = 77 × 32 × 128): the first sector of track 1 sits at offset 0x1000 and
  starts with TekDOS's `MOD` header, as the boot needs.

## Host protocol
The host writes C1 C0 /S to EA (bits 2–0); /S is active low.

| C1 C0 /S | Name | Controller |
|---|---|---|
| 1 1 0 | SIO | start an I/O sequence; presents the disc status byte on EB |
| 1 0 0 | OTD | takes the byte on EB (command, drive, sector, track, data) |
| 0 0 0 | IND | presents the next byte on EB |
| 1 1 1 | RST | end of strobe / reset; presents the status byte |

Each strobe is answered by FLAG (EA bit 7) going high. The host then writes RST and the
controller drops FLAG. Commands: 00 read diagnostic registers (R1–R15, then R0 sense),
0D write unformatted, 0E write formatted, 88 normal read, BE copy write. Status byte:
1 F.D. failure, 2 drive fault (not ready / no disc), 3 write protect, 4 error recovery,
5 illegal operation.

## What the model adds to the manual (inferred from the software)
1. **Completion FLAG.** After the track byte of a read (and after the 128th data byte of
   a write), the controller raises FLAG by itself once the seek and transfer are done.
   TekDOS 3.3 sends the address bytes, writes EA ← 37 (disc interrupt on, RST) and waits
   for interrupt 11 (handler 25AF). The bootstrap PROM polls instead, so it works either
   way. Modelled with a 1 ms delay.
2. **SIO keeps the sector.** In its handler TekDOS issues SIO (reads the status), then
   reads the 128 data bytes with IND, without a new command. So SIO must not discard a
   sector that has been read.
3. **Multi-sector reads.** The bootstrap PROM issues one Normal Read for track 1 sector 0
   and then reads 16K with IND, checking the status byte on EB after every 128 bytes. The
   model therefore continues with the next logical sector (and track) after each sector.
   The manual describes only one-sector transfers. This is consistent with "interleaving
   increases system performance during multiple sector record transfers", but not
   confirmed.

None of these timings is real: seek, rotation and interleave are not modelled. The
low-level path (the controller's microprogram, 070-2587 Appendix C) will replace this
model.

## Drive status
An empty drive reports DRIVE FAULT. A disc mounted without `-rw` reports WRITE PROTECT.
TekDOS ignores WRITE PROTECT when reading, as does the boot PROM (it masks status with $36).
