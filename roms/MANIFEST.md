# ROM manifest

| file | bytes | crc32 | sha1 | what |
|---|---|---|---|---|
| `tekdos_boot.bin` | 256 | 7e5c2e68 | 65634c1ae4a5a65c1b12f56c55406ed89b6217ee | TEKDOS bootstrap loader, 1702A on the System Memory board (U6010), in listing (PROM) order: 070-2393 Fig. 6-2, = bitsavers `800x/firmware/tekdos_boot.bin`. See `notes/boot-prom.md` |
| `fdc/fdc_bits15-8.bin` | 512 | 1a8a7215 | 6185c068692706bb61ced87056699f93131cbbfd | Flexible Disc Controller microprogram, instruction bits 15-8 (U1070 or U1080: 160-0025-00 / 160-0029-00, which one is open). From 070-2587 Appendix C and Fig. 3-5a; see `notes/fdc.md` |
| `fdc/fdc_bits7-0.bin` | 512 | b3837294 | 48725f8c00f892057573c71cb8d47239471530a9 | the same, bits 7-0 (Fig. 3-5b) |
