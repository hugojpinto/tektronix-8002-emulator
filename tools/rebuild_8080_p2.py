#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Rebuild the 8080 Emulator Processor's P2 PROM (160-0027-00, FA00-FBFF) from Eelco
# Vriezekolk's read, whose data bit 0 is stuck at 0 (the chip is faulty: notes/emulators.md,
# "The 8080 board's PROMs"). Bits 7-1 are taken from the read as they are; bit 0 is set only
# at the addresses listed below, each with the reason. The output never differs from the
# read in bits 7-1 (checked).
# usage: tools/rebuild_8080_p2.py [READ] [OUT]
import sys

READ = sys.argv[1] if len(sys.argv) > 1 else 'sources/ztpe/8080_board/8080 160-0027-00 P2.bin'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'roms/ep8080/8080_p2_160-0027-00.bin'

# address: (value, confidence, reason). Confidence: A = forced by the code or by TEKDOS
# (verified in the emulator where TEKDOS uses it), B = the only sensible reading, C = a choice
# between equally working values (says which and why).
SET = {}
def s(addr, value, conf, why):
    assert addr not in SET, hex(addr)
    SET[addr] = (value, conf, why)

# --- FA00-FA31: the SVC request templates P1's monitor calls copy to 0040-0049: the SVC 1
# pointer (0042), then the SRB: function, channel, status, data, count, buffer length,
# buffer 004A (070-2706 §4)
s(0xfa02, 0x41, 'A', 'CI: function 41, read binary and wait (40 is no function)')
s(0xfa07, 0x01, 'B', 'CI: buffer length 1 (one character; 0 reads nothing)')
s(0xfa0d, 0x01, 'C', 'CO: channel 1 (CI and CO must differ: CI 0, CO 1, RI 2, PO 3, LO 4)')
s(0xfa11, 0x01, 'B', 'CO: buffer length 1 (one character)')
s(0xfa16, 0x01, 'A', 'RI: function 01, read ASCII and wait (00 is no function; P1 sets 04, rewind, on the same block)')
s(0xfa21, 0x03, 'C', 'PO: channel 3 (RI and PO must differ)')
for a, dev in ((0xfa1b, 'RI'), (0xfa25, 'PO'), (0xfa2f, 'LO')):
    s(a, 0x7f, 'C', '%s: buffer length 7F (7E or 7F; P1 keeps 128-byte line buffers, and a length of N+1 with N = 126 is the manual\'s convention)' % dev)

# --- FA40-FB20: patch routines for a RAM-resident 8080 program reading source through the
# monitor's RI (an assembler, by its line counter and END injection). Each replaces an
# instruction in that program, does its work and runs the displaced instruction (first if it
# keeps HL, last if not). The program itself is not preserved, so nothing here can be run.
# BCD line counter at 1CDA, last printed line at 1CD8, message pointer at 1CDC; routines in
# the program at 007A and 00BE (print the character in C), 0282 (print C as two hex digits),
# 008A, 00D8.
s(0xfa40, 0x21, 'B', 'FA40 init: LXI H,9999')
s(0xfa41, 0x99, 'B', 'LXI H,9999: the BCD counter starts at 9999, so the first increment gives 0000 (9898, 9899 and 9998 fit no BCD start)')
s(0xfa42, 0x99, 'B', 'LXI H,9999')
s(0xfa46, 0x21, 'A', 'LXI H,FB21: the message')
s(0xfa47, 0x21, 'A', 'LXI H,FB21: low byte (the text starts there)')
s(0xfa48, 0xfb, 'A', 'LXI H,FB21: high byte')
s(0xfa4c, 0x21, 'B', 'displaced LXI H,1EFA (1EFA or 1EFB: as read)')
s(0xfa4f, 0xc9, 'B', 'RET')
s(0xfa50, 0x21, 'A', 'FA50 BCD increment: LXI H,1CDA')
s(0xfa55, 0x01, 'A', 'ADI 01')
s(0xfa56, 0x27, 'A', 'DAA')
s(0xfa57, 0x77, 'A', 'MOV M,A')
s(0xfa58, 0x23, 'A', 'INX H')
s(0xfa5c, 0x27, 'A', 'DAA (after ACI 00)')
s(0xfa5d, 0x77, 'A', 'MOV M,A')
s(0xfa5e, 0x21, 'B', 'displaced LXI H,2024 (its operand bits 0 unknown: 2024, 2025, 2124, 2125)')
s(0xfa61, 0xc9, 'B', 'RET')
s(0xfa62, 0x21, 'A', 'FA62 BCD decrement: LXI H,1CDA')
s(0xfa67, 0x99, 'A', 'ADI 99 (with DAA: minus 1)')
s(0xfa68, 0x27, 'A', 'DAA')
s(0xfa69, 0x77, 'A', 'MOV M,A')
s(0xfa6a, 0x23, 'A', 'INX H')
s(0xfa6d, 0x99, 'A', 'ACI 99')
s(0xfa6e, 0x27, 'A', 'DAA')
s(0xfa6f, 0x77, 'A', 'MOV M,A')
s(0xfa70, 0xc3, 'B', 'displaced JMP 14FA (operand bits 0 unknown: 14FA, 14FB, 15FA, 15FB)')
s(0xfa73, 0xcd, 'B', 'FA73 line number: displaced CALL 00BE')
s(0xfa76, 0x01, 'A', 'LXI B,1CD8: the last line printed')
s(0xfa79, 0x21, 'A', 'LXI H,1CDA')
s(0xfa7d, 0xbe, 'B', 'LDAX B; CMP M (same line as last time?)')
for a in (0xfa83, 0xfa88, 0xfa8d, 0xfa92):
    s(a, 0xcd, 'A', 'MVI C,20; CALL 00BE: a space')
s(0xfa95, 0xc3, 'B', 'JMP FAAA (after the four spaces)')
s(0xfa98, 0x23, 'A', 'INX H: the counter\'s high byte first')
s(0xfa9a, 0xcd, 'A', 'MOV C,M; CALL 0282: two digits (0282 or 0283: as read)')
s(0xfa9d, 0x21, 'A', 'LXI H,1CDA')
s(0xfaa1, 0xcd, 'A', 'MOV C,M; CALL 0282')
for a in (0xfaac, 0xfab1, 0xfab6):
    s(a, 0xcd, 'A', 'MVI C,20; CALL 00BE')
s(0xfabb, 0xc3, 'B', 'MVI C,20; JMP 00BE (the fourth space, as a tail call)')
s(0xfac1, 0xb7, 'B', 'LDA 1CDE; ORA A (ORA M would need HL)')
s(0xfad5, 0xcd, 'A', 'CALL F815: the monitor\'s IOCHK (the I/O byte)')
s(0xfad6, 0x15, 'A', 'CALL F815 (F814 is mid-entry)')
s(0xfae4, 0xc3, 'B', 'JMP 00D8 (the displaced jump; every exit above goes here)')
s(0xfae1, 0xcd, 'A', 'CALL 007A')
s(0xfae7, 0xcd, 'A', 'FAE7: CALL 008A (008A or 008B: as read)')
s(0xfaed, 0xb7, 'B', 'LDA 1CE0; ORA A')
s(0xfaef, 0xaf, 'B', 'XRA A: clear the flag')
s(0xfaf4, 0x0d, 'B', 'MVI C,0D: CR, then LF (0C, a form feed, before an LF is unlikely)')
s(0xfaf5, 0xcd, 'A', 'CALL 007A')
s(0xfafa, 0xc3, 'B', 'MVI C,0A; JMP 007A (tail call)')
s(0xfafd, 0x21, 'B', 'FAFD: displaced LXI H,204E (operand bits 0 unknown)')
s(0xfb03, 0xb7, 'B', 'LDA 1EAA; ORA A')
s(0xfb05, 0xaf, 'B', 'XRA A')
s(0xfb0c, 0xc9, 'B', 'RET')
s(0xfb0d, 0xcd, 'A', 'FB0D: CALL F806, the monitor\'s RI (the displaced call)')
s(0xfb12, 0xfb, 'A', 'JC FB16: end of input')
s(0xfb16, 0xe5, 'A', 'PUSH H')
s(0xfb1b, 0x23, 'A', 'LHLD 1CDC; MOV A,M; INX H; SHLD 1CDC: the next message character')
s(0xfb1f, 0xe1, 'A', 'POP H')
s(0xfb20, 0xc9, 'A', 'RET')

# --- FB21-FB30: the message " END *MISSING*" CR LF
s(0xfb22, 0x45, 'B', "text: 'E' of END ('D' as read)")
for a, ch in ((0xfb27, 'M'), (0xfb28, 'I'), (0xfb29, 'S'), (0xfb2a, 'S'), (0xfb2b, 'I'), (0xfb2d, 'G')):
    s(a, ord(ch), 'B', "text: '%s' of MISSING" % ch)
s(0xfb2f, 0x0d, 'B', 'text: CR before the LF')

# --- FB80-FBDD: the debug firmware TEKDOS calls (dump, restore, GET, PUT). Register block
# 0000 L, 01 H, 02 E, 03 D, 04 C, 05 B, 06 A, 07 F, 08 scratch, 09/0A SP: the 8550's 8085
# board uses the same layout. Each routine ends with its SVC, OUT F1 or OUT F0, and a NOP
# (070-2702 §11: "a no-operation instruction, allowing time for the SVC to occur").
dump = [  # FB80 dump: SHLD 0000; STA 0006; A = CY (MVI A,0; JNC FB8D; MVI A,1);
    # LXI H,0; DAD SP; LXI SP,000B; PUSH H; RAR (CY back); PUSH PSW; DCX SP; PUSH B; PUSH D;
    # LXI SP,0007; POP PSW; POP H; SPHL; LHLD 0000; LDA 0006; OUT F1; NOP
    (0xfb89, 0x8d, 'JNC FB8D: low byte'), (0xfb8a, 0xfb, 'JNC FB8D: high byte'),
    (0xfb8c, 0x01, 'MVI A,01: A = 1 when CY'), (0xfb8d, 0x21, 'LXI H,0000'),
    (0xfb90, 0x39, 'DAD SP: HL = SP'), (0xfb91, 0x31, 'LXI SP,000B'), (0xfb92, 0x0b, 'LXI SP,000B: the block top'),
    (0xfb94, 0xe5, 'PUSH H: SP to 0009/0A'), (0xfb95, 0x1f, 'RAR: CY back from A (DAD changed it)'),
    (0xfb96, 0xf5, 'PUSH PSW: F to 0007'), (0xfb97, 0x3b, 'DCX SP: skip 0006 (A, stored first)'),
    (0xfb98, 0xc5, 'PUSH B: C B to 0004/05'), (0xfb99, 0xd5, 'PUSH D: E D to 0002/03'),
    (0xfb9a, 0x31, 'LXI SP,0007'), (0xfb9b, 0x07, 'LXI SP,0007: back to F'),
    (0xfb9d, 0xf1, 'POP PSW'), (0xfb9e, 0xe1, 'POP H'), (0xfb9f, 0xf9, 'SPHL: the user SP again'),
    (0xfba6, 0xd3, 'OUT F1: debug SVC 1'), (0xfba7, 0xf1, 'OUT F1: the port'),
]
restore = [  # FBA9 restore: LXI SP,0002; POP D; POP B; INX SP; POP PSW; POP H; SPHL;
    # LHLD 0000; LDA 0006; OUT F1; NOP
    (0xfba9, 0x31, 'LXI SP,0002'), (0xfbac, 0xd1, 'POP D'), (0xfbad, 0xc1, 'POP B'),
    (0xfbae, 0x33, 'INX SP: skip 0006'), (0xfbaf, 0xf1, 'POP PSW'), (0xfbb0, 0xe1, 'POP H'),
    (0xfbb1, 0xf9, 'SPHL'), (0xfbb8, 0xd3, 'OUT F1'), (0xfbb9, 0xf1, 'OUT F1: the port'),
]
getput = [  # FBBB GET: LHLD 0001; MOV E,H; MOV D,L; LDAX D; STA 0000; OUT F0; NOP
    # FBC8 PUT: LHLD 0001; MOV E,H; MOV D,L; LDA 0000; MOV B,A; STAX D; LDAX D; XRA B;
    # STA 0003; OUT F0; NOP (status 00 when the byte reads back)
    (0xfbbc, 0x01, 'GET: LHLD 0001 (L = address high, H = low)'), (0xfbbf, 0x55, 'GET: MOV D,L'),
    (0xfbc4, 0xd3, 'GET: OUT F0, debug SVC 2'),
    (0xfbc9, 0x01, 'PUT: LHLD 0001'), (0xfbcc, 0x55, 'PUT: MOV D,L'), (0xfbd0, 0x47, 'PUT: MOV B,A'),
    (0xfbd5, 0x03, 'PUT: STA 0003, the status'), (0xfbd7, 0xd3, 'PUT: OUT F0'),
]
for a, v, why in dump + restore + getput:
    s(a, v, 'A', why)

# --- FBDE-FBEF: the entry table (TEKDOS's base FBEF: PUT = base - 11, GET = base - 8)
for a in (0xfbde, 0xfbe1, 0xfbe4, 0xfbe7, 0xfbea, 0xfbed):
    s(a, 0xc3, 'A', 'JMP (C2, JNZ, would depend on Z)')
s(0xfbe6, 0xfb, 'A', 'PUT -> FBC8')
s(0xfbe8, 0xbb, 'C', 'GET -> FBBB (FBBA is the restore SVC NOP: FBBA would work too)')
s(0xfbe9, 0xfb, 'A', 'GET -> FBxx')
s(0xfbeb, 0xa9, 'C', 'restore -> FBA9 (FBA8 is the dump SVC NOP: FBA8 would work too)')
s(0xfbec, 0xfb, 'A', 'restore -> FBxx')
s(0xfbef, 0xfb, 'A', 'dump -> FB80')


# Bytes kept as read (bit 0 = 0) although 1 would also make sense: addresses inside the RAM
# program the FA40-FB20 routines serve, which is not preserved. The even values are kept;
# the program's variables found so far are all at even addresses (1CD8, 1CDA, 1CDC, 1CDE,
# 1CE0), which supports that for the variables but not for the entry points.
AS_READ = {
    0xfa4d: 'displaced LXI H,1EFA: low byte (1EFA or 1EFB)',
    0xfa5f: 'displaced LXI H,2024: low byte (24 or 25)', 0xfa60: 'displaced LXI H,2024: high byte (20 or 21)',
    0xfa71: 'displaced JMP 14FA: low byte (FA or FB)', 0xfa72: 'displaced JMP 14FA: high byte (14 or 15)',
    0xfa9b: 'CALL 0282: the hex-print routine (0282 or 0283)', 0xfaa2: 'CALL 0282 (0282 or 0283)',
    0xfac6: 'LDA 1E3E (1E3E or 1E3F)', 0xfac9: 'CPI 40 (40 or 41: column 64)',
    0xface: 'LDA 1E40 (1E40 or 1E41)', 0xfad1: 'CPI 20 (20 or 21: a space)',
    0xfae0: 'MOV C,M (4E) or MOV C,A (4F): what is printed before JMP 00D8',
    0xfae2: 'CALL 007A (007A or 007B)', 0xfae5: 'JMP 00D8 (00D8 or 00D9)', 0xfae8: 'CALL 008A (008A or 008B)',
    0xfaf6: 'CALL 007A', 0xfafb: 'JMP 007A',
    0xfafe: 'displaced LXI H,204E: low byte (4E or 4F)', 0xfaff: 'displaced LXI H,204E: high byte (20 or 21)',
    0xfb01: 'LDA 1EAA (1EAA or 1EAB)', 0xfb0a: 'STA 006E (006E or 006F)',
    0xfbdf: 'unused table entry JMP 0000 (0000, 0001, 0100 or 0101)', 0xfbe2: 'unused table entry JMP 0000',
}


def report(out):
    """A table of every byte that differs from the read, and every byte kept as read but uncertain."""
    lines = ['| Address | Read | Rebuilt | Confidence | Why |', '|---|---|---|---|---|']
    for addr in sorted(set(SET) | set(AS_READ)):
        i = addr - 0xfa00
        if addr in SET:
            v, c, why = SET[addr]
        else:
            v, c, why = out[i], 'C', 'kept as read: ' + AS_READ[addr]
        lines.append('| %04X | %02X | %02X | %s | %s |' % (addr, out[i] & 0xfe, v, c, why))
    return '\n'.join(lines) + '\n'


def build(read):
    out = bytearray(read)
    for addr, (v, _, _) in SET.items():
        i = addr - 0xfa00
        assert (v & 0xfe) == read[i], 'bits 7-1 differ at %04X: read %02X, rebuilt %02X' % (addr, read[i], v)
        out[i] = v
    assert all((a & 0xfe) == b for a, b in zip(out, read))
    return bytes(out)


if __name__ == '__main__':
    read = open(READ, 'rb').read()
    assert len(read) == 512 and not any(b & 1 for b in read), 'expects the bit-0-stuck read'
    out = build(read)
    open(OUT, 'wb').write(out)
    n = {c: sum(1 for v in SET.values() if v[1] == c) for c in 'ABC'}
    print('%s: %d bytes with bit 0 set (A %d, B %d, C %d); %d kept as read but uncertain'
          % (OUT, len(SET), n['A'], n['B'], n['C'], len(AS_READ)))
    if '--report' in sys.argv:
        sys.stdout.write(report(out))
