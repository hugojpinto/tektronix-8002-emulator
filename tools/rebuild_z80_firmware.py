#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Rebuild the 8002 Z80 Emulator Processor's firmware (F800-FBFF; never dumped) from the table
# Millennium's diagnostics carry (notes/emulators.md, "The Z80 board's firmware").
#
# The diagnostic disc's Z80 DEBUG test (file DE310, DIP 4.4) holds a table at 25B9 that its
# PROM subtest compares, one byte per read, with the board's F9FF, F9FE, ... F800. Read as a
# straight image the table is not working code. Reversed, with a byte inserted before every
# address ending in 0, and with the 19 bytes stored after the table taken as F80F down to
# F801, it is coherent Z80 code from F800, with the four entries TEKDOS calls (F800 PUT,
# F803 GET, F806 restore, F809 dump), running to FA27 (so a 1K PROM). Why the table differs
# from the code in that way is not known (a faulty copy, or a view of the PROM through the
# System Processor's path that the emulator does not model): notes/emulators.md.
# The missing bytes are set below, each with its reason and a confidence; FA28-FBFF are FF.
# usage: tools/rebuild_z80_firmware.py [DIAG_DISC] [OUT] [--report]
import os, subprocess, sys, tempfile

ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
DISC = ARGS[0] if len(ARGS) > 0 else 'sources/bitsavers/disks/Millennium_internal_floppies/diag_4.4_5-23-79/diag_4.4_5-23-79.img'
OUT = ARGS[1] if len(ARGS) > 1 else 'roms/epz80/z80_firmware.bin'
TEKMATO = os.environ.get('TEKMATO', '../tekmato/tekmato')

# address: (value, confidence, reason). A = forced by the code and exercised by TEKDOS's DEBUG
# in the emulator (tests/runz80.sh), B = the only sensible reading, C = a guess in data the
# firmware itself never reads
SET = {}
def s(addr, value, conf, why):
    assert addr % 16 == 0 and addr not in SET, hex(addr)
    SET[addr] = (value, conf, why)

s(0xF800, 0xC3, 'A', 'JP F8CA: the PUT entry; F803, F806, F809 are JP too (TEKDOS: table base F80B less 11)')
s(0xF810, 0x31, 'A', 'LD SP,0018 after the dump\'s LD (0018),SP: the pushes that follow fill the register block downwards from 0018')
s(0xF820, 0xE5, 'A', 'PUSH HL\' after PUSH AF\', BC\', DE\': puts L\' H\' at 04-05, the block\'s layout (DSTAT\'s ALT SET)')
s(0xF830, 0x7F, 'A', 'R compensation AND 7F after SUB 18 (the restore does AND 7F; SUB 9B; AND 7F): 18 = the 24 M1 cycles from the break to LD A,R')
s(0xF840, 0x44, 'A', 'LD B,H of the H/L swap that puts A\' F\' in the block\'s order, as at F837 for A F')
s(0xF850, 0x00, 'B', 'LD B,00; LD HL,(0018); ADD HL,BC: SP plus block byte 1A (TEKDOS writes 02 after an NMI break)')
s(0xF860, 0x3A, 'A', 'restore entry: LD A,(0001); AND 03 selects the interrupt mode (byte 1 bits 1-0)')
s(0xF870, 0x18, 'B', 'JR +2 over IM 1 after IM 0 (IM 0 is not exercised)')
s(0xF880, 0x07, 'B', 'RLC A twice, AND 01: IFF1 from byte 1 bit 6, then EI or DI')
s(0xF890, 0x80, 'A', 'AND 80 keeps R\'s bit 7, which LD R,A then restores with the compensated bits 6-0')
s(0xF8A0, 0xD1, 'A', 'POP DE\' between POP HL\' and POP BC\' (the block\'s 04-09)')
s(0xF8B0, 0xC1, 'A', 'POP BC between POP DE and the AF pop (the block\'s 10-17)')
s(0xF8C0, 0x5C, 'B', 'GET: LD E,H of the address swap (block bytes 1, 2 are high, low), as PUT does at F8CD')
s(0xF8D0, 0x3A, 'B', 'PUT: LD A,(0000), then LD (HL),A; LD A,(HL); XOR B; LD (0003),A: 00 when the byte reads back')
# past the code (F8DC) the table holds fill with bit noise, read by nothing: FF with bits cleared
# to F8FF, then 30 with single or double bits flipped; a missing byte there takes the fill
for a in (0xF8E0, 0xF8F0):
    s(a, 0xFF, 'C', 'fill after the code: FF with scattered cleared bits around it; FF')
for a in range(0xF900, 0xFA28, 16):
    s(a, 0x30, 'C', 'fill: 30 with scattered flipped bits around it, read by nothing; 30')


def extract_de310():
    d = tempfile.mkdtemp()
    out = os.path.join(d, 'DE310')
    r = subprocess.run([TEKMATO, '-open', DISC, '-read', 'DE310', out], capture_output=True)
    if r.returncode or not os.path.exists(out):
        sys.exit('cannot read DE310 from %s with %s: %s' % (DISC, TEKMATO, (r.stdout + r.stderr).decode('latin-1')))
    return open(out, 'rb').read()


def rebuild(de310):
    at = lambda a: a - 0x2000 + 0x80      # module address -> file offset (128-byte MOD header)
    stream = de310[at(0x25B9):at(0x27CB) + 1][::-1]   # ascending addresses; [14] is F801
    assert stream[14:17] == bytes([0xCA, 0xF8, 0xC3]), 'unexpected table'
    s_ = stream[14:]
    img, k, a = bytearray(), 0, 0xF800
    while k < len(s_):
        if a % 16 == 0: img.append(SET[a][0])
        else: img.append(s_[k]); k += 1
        a += 1
    assert len(img) == 0x228 and all(0xF800 + i in SET for i in range(0, len(img), 16))
    return bytes(img) + b'\xff' * (1024 - len(img))


img = rebuild(extract_de310())
os.makedirs(os.path.dirname(OUT) or '.', exist_ok=True)
open(OUT, 'wb').write(img)
grades = [c for _, c, _ in SET.values()]
print('%s: 1024 bytes (F800-FA27 from the diagnostic, %d of them set here: A %d, B %d, C %d; FA28-FBFF FF)'
      % (OUT, len(SET), grades.count('A'), grades.count('B'), grades.count('C')))
if '--report' in sys.argv:
    for a in sorted(SET):
        v, c, why = SET[a]
        print('%04X  %02X  %s  %s' % (a, v, c, why))
