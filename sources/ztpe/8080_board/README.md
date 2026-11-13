# 8080 Emulator Processor PROMs

The two 512-byte PROMs of an 8001/8002 8080 Emulator Processor board, as received: P1,
Tektronix part 160-0028-00, and P2, 160-0027-00. Read by Eelco Vriezekolk
([ztpe.nl](https://ztpe.nl/2650/)) and sent on 2026-11-13. © Tektronix, Inc.; kept for
preservation and research.

| File | SHA-256 |
|---|---|
| `8080 160-0028-00 P1.bin` | 7313aaef2966e38937931e83520f36625cb74f212d38cb8a79f75e7d994ce07a |
| `8080 160-0027-00 P2.bin` | 26506cfd53665d902fbe7960e9119c9f744d02526f41db0aceae65a5b56083d8 |

**P2 needs a second read:** its data bit 0 is 0 in all 512 bytes, while every other line of
both PROMs, and P1's bit 0 (293 ones), toggle normally. Its jump table has `C2` (JNZ) where
`C3` (JMP) belongs, and its message reads " DND *LHRRHNF*". Not yet in `roms/`: the 8080
board is not emulated. What they contain: `notes/emulators.md`, "The 8080 board's PROMs".

**Rebuilt:** `roms/ep8080/8080_p2_160-0027-00.bin` is P2 with bit 0 restored by
`tools/rebuild_8080_p2.py` (130 bytes, each with its reason and a confidence; 23 more kept as
read but uncertain). Everything TEKDOS uses is verified in the emulator (`tests/run8080.sh`).
