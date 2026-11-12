#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
"""Disassembler for the 8002A Flexible Disc Controller's microprogram.

The controller (070-2587 section 3) is a 16-bit microprogrammed machine; its instruction
formats are Appendix B, its ALU map Table 3-2, and its microprogram (512 words in two
512 x 8 PROMs) Appendix C. The mnemonics are the manual's.

  tools/fdc_dasm.py [roms/fdc] [--check LISTING_TEXT]

--check compares every decoded word with the manual's own listing (the OCR text of
070-2587): op code, operands and jump target, and reports where they differ.
"""
import os, re, sys

# Table 3-2: RIR bits 11-6 (S3 S2 S1 S0 M CI) of Type A instructions
ALU = {0b101011: 'ADD', 0b101010: 'ADC', 0b010110: 'SUB', 0b110010: 'INC', 0b001111: 'DEC',
       0b111111: 'RLL', 0b111110: 'RLC', 0b000011: 'LMN', 0b000000: 'LON', 0b111100: 'LRZ',
       0b101000: 'XOR', 0b110000: 'LOD', 0b001100: 'CMD', 0b010100: 'NXR', 0b011100: 'NOR',
       0b001000: 'NAN', 0b110100: 'AND', 0b100000: 'OR', 0b100100: 'LDB'}
# Fig. B-5: Type L Immediate Group, RIR bits 11-8 (logic mode forced)
IMM = {0b1000: 'OR', 0b1010: 'XOR', 0b1101: 'AND', 0b0010: 'NAN', 0b0111: 'NOR', 0b0101: 'NXR',
       0b1100: 'LOD', 0b1111: 'NOP'}
# Figs. B-6/B-7: jump conditions, RIR bits 11-8
JMP = {0: 'JNZ', 1: 'JZR', 2: 'J6N', 3: 'J6Z', 4: 'J4N', 5: 'J4Z', 6: 'JNC', 7: 'JZC',
       8: 'J2N', 9: 'J2Z', 10: 'JLB', 11: 'JLZ', 12: 'JDA', 13: 'JNR', 14: 'JMP', 15: 'JNV'}
# Fig. B-2: Type A Input Group sources, RIR bits 5-2
INP = {0: 'NONE', 1: 'TST', 2: 'CSW', 3: 'SBH', 4: 'SBL', 5: 'DBT', 6: 'BYT', 7: 'INL'}
DST = {0: 'L', 1: 'DR', 2: 'A', 3: 'M'}
SPECIAL = {0: '', 1: 'SPSH', 2: 'PULL', 3: 'EPSH'}
# Fig. B-4: Type S commands, by RIR bits 11-0
CMDS = {0x0000: 'NOP', 0x0200: 'CDA', 0x0C00: 'RST',
        0x0400: 'RBY', 0x0600: 'SBY', 0x0440: 'RIP', 0x0640: 'SIP', 0x0480: 'RER', 0x0680: 'SER',
        0x04C0: 'RCK', 0x06C0: 'SCK', 0x0500: 'RFI', 0x0700: 'SFI', 0x0540: 'RWR', 0x0740: 'SWR',
        0x0580: 'RPT', 0x0780: 'SPT', 0x05C0: 'SFG', 0x07C0: 'RFG',
        0x0800: 'RHD', 0x0A00: 'SHD', 0x0840: 'DRO', 0x0A40: 'DRI', 0x0880: 'RSP', 0x0A80: 'SSP',
        0x08C0: 'DSK', 0x0AC0: 'ESK', 0x0900: 'EDY', 0x0B00: 'DDY', 0x0940: 'ROD', 0x0B40: 'RDC',
        0x0980: 'TKL', 0x0B80: 'TKH', 0x09C0: 'SL8', 0x0BC0: 'SH8'}
# Fig. B-3: Type U unary instructions, as (source/destination nibble, ALU field)
UNARY = {(6, 0b110000): 'LAR', (5, 0b100100): 'LRA', (5, 0b111100): 'LRZ', (5, 0b000011): 'LRM',
         (6, 0b111100): 'LAZ', (6, 0b000011): 'LAM'}
# what the OCR makes of some op codes, and the listing's alternative spellings of one encoding
SAME = {'LOO': 'LOD', 'SHB': 'SH8', 'SLB': 'SL8', 'RHO': 'RHD', 'RIL': 'RLL', 'OSK': 'DSK', 'JL8': 'JLB',
        'JOA': 'JDA', 'SFG': 'SFG'}
ALIASES = {('LAR', 'LOD')}      # LAR Rn is LOD SR,A,Rn


def decode(a, w, bank=None):
    """(mnemonic, operands) of word w at address a. A jump lands in the bank UBANK selects:
    a's own, unless the instruction before was SH8/SL8 (bank 1/0)."""
    if bank is None:
        bank = a >> 8
    top, s, d = w >> 12, w >> 14, (w >> 12) & 3
    f6, f4, reg, sp = (w >> 6) & 63, (w >> 8) & 15, (w >> 2) & 15, w & 3
    spc = ',' + SPECIAL[sp] if sp else ''
    if top == 0:
        return CMDS.get(w & 0x0FFF, 'S?%03X' % (w & 0xFFF)), ''
    if top == 1:
        return 'LDL', ''
    if s == 1 and d == 3:                      # through-register jump: register number complemented
        return JMP[f4], 'SR,M,R%d' % (15 - reg)
    if s == 2 and d == 3:                      # direct jump: 1s complement of the target, in the current bank
        return JMP[f4], 'I,M,%03X' % ((bank << 8) | (~w & 0xFF))
    if s == 2:
        return IMM.get(f4, 'I?%X' % f4), 'I,%s,%02X' % (DST[d], w & 0xFF)
    if s == 1:
        if (top, f6) in UNARY:
            return UNARY[(top, f6)], 'R%d%s' % (reg, spc)
        return ALU.get(f6, 'A?%02X' % f6), 'SR,%s,R%d%s' % (DST[d], reg, spc)
    return ALU.get(f6, 'A?%02X' % f6), 'IN,%s,%s%s' % (DST[d], INP[reg], spc)   # s == 3


def bank_after(words, a):
    """The bank a jump at a lands in (UBANK as the instruction before a left it)."""
    prev = words[(a - 1) & 0x1FF]
    return 1 if prev == 0x0BC0 else 0 if prev == 0x09C0 else a >> 8


def load(dirname):
    hi = open(os.path.join(dirname, 'fdc_bits15-8.bin'), 'rb').read()
    lo = open(os.path.join(dirname, 'fdc_bits7-0.bin'), 'rb').read()
    return [(hi[a] << 8) | lo[a] for a in range(512)]


def symbols(path):
    """Label -> value from Appendix C's symbol table (its DECIMAL column)."""
    t = open(path, encoding='utf-8', errors='replace').read().split('\n')
    out = {}
    for l in t:
        m = re.match(r'^\s*([A-Z][A-Z0-9]{0,5})\s+\S{4}\s+(\d{5})\s+\d{6}\s+[01 ]{16,}', l)
        if m:
            out[m.group(1)] = int(m.group(2)) & 0x1FF
    return out


def listing_ops(path):
    """Address -> (op, operands) from the OCR text of Appendix C."""
    t = open(path, encoding='utf-8', errors='replace').read().split('\n')
    lo = next(i for i, l in enumerate(t) if 'FLOPPY DISC MICROCODE' in l)
    hi = next(i for i, l in enumerate(t) if 'SYMBOL' in l and 'VAL/ADRS' in l)
    rx = re.compile(r'^\s*[0-9S]{4}\s+\S{2}\s?\S{2}\s+(?:[01OolIi]\s?){16}\s*\S{4}\s+(.*)$')
    out, a = {}, 0
    for n in range(lo, hi):
        m = rx.match(t[n])
        if not m:
            continue
        out[a] = m.group(1).split()
        a += 1
    return out


def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    rom = args[0] if args else os.path.join(os.path.dirname(__file__), '..', 'roms', 'fdc')
    words = load(rom)
    if '--check' in sys.argv:
        path = sys.argv[sys.argv.index('--check') + 1]
        lst, sym = listing_ops(path), symbols(path)
        sym.update({k.replace('O', '0'): v for k, v in list(sym.items())})
        mism = jumps = jbad = 0
        for a in range(512):
            op, opr = decode(a, words[a], bank_after(words, a))
            toks = [SAME.get(x, x) for x in lst.get(a, [])]
            if op not in toks[:2] and not any((op, x) in ALIASES for x in toks[:2]):
                mism += 1
                print('op  %03X %04X %-4s %-18s | %s' % (a, words[a], op, opr, ' '.join(toks[:4])))
            if op in JMP.values() and opr.startswith('I,M,'):
                target = int(opr[4:], 16)
                i = toks.index(op) if op in toks else 1
                ref = toks[i + 1].split(',')[-1] if i + 1 < len(toks) else ''
                m = re.fullmatch(r'\*([+-]\d+)?', ref)
                want = (a + int(m.group(1) or 0)) & 0x1FF if m else sym.get(ref.replace('O', '0'), sym.get(ref))
                if want is None:
                    continue
                jumps += 1
                if want != target:
                    jbad += 1
                    print('jmp %03X %04X %s -> %03X, listing says %s = %03X' % (a, words[a], op, target, ref, want))
        print('%d of 512 op codes differ from the listing; %d of %d checkable jump targets differ' % (mism, jbad, jumps))
        return
    for a in range(512):
        op, opr = decode(a, words[a], bank_after(words, a))
        print('%03X  %04X  %-4s %s' % (a, words[a], op, opr))


if __name__ == '__main__':
    main()
