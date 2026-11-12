# Bootstrap PROM

`roms/tekdos_boot.bin` is a byte-for-byte copy of `sources/bitsavers/firmware/tekdos_boot.bin`
(256 bytes, CRC32 7e5c2e68). That file is the listing printed as Fig. 6-2 "TEKDOS Bootstrap
Loader" in the 8001/8002 Installation Guide 070-2393 (`tekdos_boot.tif` next to it is the
figure). All 256 bytes were compared with the figure by eye.

## Address order
The listing is in PROM order. The PROM (U6010, a 1702A on the System Memory board) sits
behind inverting address buffers U6030/U6040 (070-2711 §4, parts list), so the 2650 at
address *a* reads PROM location FF − *a*. Reversed, the image is clean 2650 code from the
reset vector on. Read in listing order, it is not. The emulator reverses it on loading
(`tek8002::load_boot_prom`). The same conclusion was reached independently in the EEVblog
restoration thread (posts #50–51). There, the restorer noted that the CPU board also
inverts the address bus. Whether the real PROM is programmed in reverse, or the figure was
printed reversed, can only be settled by reading a real PROM (he plans to).

## Layout (CPU order)
| Address | Contents |
|---|---|
| 0000–003F | 32 interrupt entries `1B BE` = `BCTR,UN *+0x40`: entry 2n branches through the word at 0040+2n |
| 0040–007F | pointers: 0080 for reset, 1000+3n for interrupt n |
| 0080–00A7 | start: reset the controller; send Normal Read, drive 0, sector 0, track 1. On error, retry up to 256 times, then switch to drive 1, and so on |
| 00A9–00B1 | command table, read backwards: `88 00 00 01` (drive 0), FF, `88 01 00 01` (drive 1) |
| 00B2–00C6 | handshake subroutines (RST and wait FLAG = 0; strobe and wait FLAG = 1; status read masked with $36) |
| 00C7–00FF | read 16K into 0000–3FFF, checking status every 128 bytes and restarting on error; set 1000 = 10; jump to 0100 |

A disassembly of the code part is in `dasm/boot_prom_0080.lst`
(`emu/build/dasm s2650` on the reversed image). The first sector read (the `MOD` header)
lands at 0000–007F under the PROM and is never seen.
