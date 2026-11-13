#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Rebuild the 8080 Emulator Processor's P2 PROM (160-0027-00, FA00-FBFF) from Eelco
# Vriezekolk's read, whose data bit 0 is stuck at 0 (the chip is faulty: notes/emulators.md,
# "The 8080 board's PROMs"). Bits 7-1 are taken from the read as they are; bit 0 is set only
# at the addresses listed below, each with the reason. The output never differs from the
# read in bits 7-1 (checked).
# usage: tools/rebuild_8080_p2.py [READ] [OUT] [--report]   (--report: every byte's grade, as a table)
import sys

ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
READ = ARGS[0] if len(ARGS) > 0 else 'sources/ztpe/8080_board/8080 160-0027-00 P2.bin'
OUT = ARGS[1] if len(ARGS) > 1 else 'roms/ep8080/8080_p2_160-0027-00.bin'

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
s(0xfa0d, 0x01, 'B', 'Read as the source someone wrote: the five templates sit in order (CI, CO, RI, PO, LO), and a table filled in that order is numbered 0-4, which is also the order of the Intel I/O byte (console, reader, punch, list). CO: channel 1 (CI and CO must differ: CI 0, CO 1, RI 2, PO 3, LO 4).')
s(0xfa11, 0x01, 'B', 'CO: buffer length 1 (one character)')
s(0xfa16, 0x01, 'A', 'RI: function 01, read ASCII and wait (00 is no function; P1 sets 04, rewind, on the same block)')
s(0xfa21, 0x03, 'B', 'Read as the source someone wrote: the five templates sit in order (CI, CO, RI, PO, LO), and a table filled in that order is numbered 0-4, which is also the order of the Intel I/O byte (console, reader, punch, list). PO: channel 3')
for a, dev in ((0xfa1b, 'RI'), (0xfa25, 'PO'), (0xfa2f, 'LO')):
    s(a, 0x7f, 'C', '%s: buffer length 7F (7E or 7F; P1 keeps 128-byte line buffers, and a length of N+1 with N = 126 is the manual\'s convention)' % dev)

# --- FA40-FB20: patch routines for Intel's paper-tape "8080 MDS Macro Assembler" (identified:
# the MDS-800 operator manual 98-129A starts it at 0020, where P1's start-up jumps; its PL/M
# source, versions 2.0 and 4.1 on mark-ogden.uk, lists to column 72 and tests IOCHK AND 0C0H;
# no binary is known). They add a line-number column and an END at the end of the source.
# Below, "a RAM-resident 8080 program" is that assembler:
# patch routines for a RAM-resident 8080 program reading source through the
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
# FAC9 CPI 40 is kept as read and is no longer uncertain: column 64 = the assembler's 72
# (its PL/M LIST stops at COLUMN < 72) minus the 8 columns the line number adds.
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
s(0xfafd, 0x21, 'B', 'Read as source, the alternative is an 8085 RIM in 8080 code or stray bytes between routines; LXI H fits the displaced-instruction pattern. FAFD: displaced LXI H,204E, run before FB00 (operand bits 0 unknown). Nothing shows FAFD is entered: if only FB00 is, these three bytes are dead and 20 (as read) is as good')
s(0xfb03, 0xb7, 'B', 'LDA 1EAA; ORA A')
s(0xfb05, 0xaf, 'B', 'XRA A')
s(0xfb0c, 0xc9, 'B', 'RET (RZ, as read, behaves the same: Z is always set after XRA A)')
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
# Mutation testing (each executed byte's bit 0 flipped, tests rerun) shows TEKDOS always
# restores before resuming, so the dump's tail (FB9A-FBA5: SP, HL and A put back) is never
# observed: those bits rest on the code's own symmetry (LXI SP,0007 is the only value that
# pops back what was pushed; LHLD 0000 and LDA 0006 mirror SHLD 0000 and STA 0006), grade B.
TAIL = {0xfb9a, 0xfb9b, 0xfb9d, 0xfb9e, 0xfb9f}
for a, v, why in dump + restore + getput:
    s(a, v, 'B' if a in TAIL else 'A', why)

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
    # kept as read, never observable: TEKDOS writes them (status, data, count of each template)
    # or never executes them (the NOP after each SVC, the padding); 00 is the natural value

    0xfa4d: 'displaced LXI H,1EFA: low byte (1EFA or 1EFB)',
    0xfa5f: 'displaced LXI H,2024: low byte (24 or 25)', 0xfa60: 'displaced LXI H,2024: high byte (20 or 21)',
    0xfa71: 'displaced JMP 14FA: low byte (FA or FB)', 0xfa72: 'displaced JMP 14FA: high byte (14 or 15)',
    0xfa9b: 'CALL 0282: the hex-print routine (0282 or 0283)', 0xfaa2: 'CALL 0282 (0282 or 0283)',
    0xfac6: 'LDA 1E3E (1E3E or 1E3F)',
    0xface: 'LDA 1E40 (1E40 or 1E41)',
    0xfae2: 'CALL 007A (007A or 007B)', 0xfae5: 'JMP 00D8 (00D8 or 00D9)', 0xfae8: 'CALL 008A (008A or 008B)',
    0xfaf6: 'CALL 007A', 0xfafb: 'JMP 007A',
    0xfafe: 'displaced LXI H,204E: low byte (4E or 4F)', 0xfaff: 'displaced LXI H,204E: high byte (20 or 21)',
    0xfb01: 'LDA 1EAA (1EAA or 1EAB)', 0xfb0a: 'STA 006E (006E or 006F)',
    0xfbdf: 'unused table entry JMP 0000 (0000, 0001, 0100 or 0101)', 0xfbe2: 'unused table entry JMP 0000',
}


# Every other byte the patches' operands use is kept as read too, for a stated reason; the
# report grades them so that every byte of P2 is accounted for.
KEPT = {}
def k(addrs, conf, why):
    for a in addrs:
        assert a not in SET and a not in AS_READ and a not in KEPT, hex(a)
        KEPT[a] = (conf, why)
k([0xfa7f, 0xfa80, 0xfa96, 0xfa97, 0xfac3, 0xfac4, 0xfacb, 0xfacc, 0xfad3, 0xfad4, 0xfadb, 0xfadc, 0xfb11],
  'B', 'a jump inside P2: the other value lands mid-instruction, skips a needed INX H or PUSH H, or (high byte FB) jumps into the firmware')
k([0xfad7, 0xfb0e, 0xfb0f], 'B', 'a call into P1: F815 IOCHK and F806 RI (F906 is CSTS, which reads nothing; F807 is mid-entry)')
k([0xfa5b], 'B', 'ACI 00: 01 would add the BCD carry twice')
k([0xfa82, 0xfa87, 0xfa8c, 0xfa91, 0xfaab, 0xfab0, 0xfab5, 0xfaba], 'B', 'MVI C,20: a space for the listing columns (21 would print "!")')
k([0xfad9], 'B', 'ANI C0: the list field of the I/O byte, as the assembler\'s PL/M source tests IOCHK AND 0C0H')
k([0xfac9], 'B', 'CPI 40: column 64 = the assembler\'s 72 (PL/M LIST stops at COLUMN < 72) less the 8 the line number adds')
k([0xfaf9], 'B', 'MVI C,0A: LF after the CR')
k([0xfa03, 0xfa17, 0xfa2b], 'B', 'a channel (CI 0, RI 2, LO 4): '+'Read as the source someone wrote: the five templates sit in order (CI, CO, RI, PO, LO), and a table filled in that order is numbered 0-4, which is also the order of the Intel I/O byte (console, reader, punch, list)')
k([0xfae0], 'B', 'MOV C,M: A holds the I/O-byte list field just stored (40, 80 or C0), which is no character to print; M is the caller\'s character')
k([0xfad1], 'B', 'CPI 20: is the character at 1E40 a space (21 would test for "!")')
k([0xfba8, 0xfbba, 0xfbc6, 0xfbd9], 'B', 'the NOP after an SVC (070-2702 §11: OUT then NOP; the 8080\'s NOP is 00, also Tek\'s fill)')
k([0xfbc7], 'B', 'padding before PUT: 00, Tek\'s fill')
k([0xfb14], 'B', 'CPI 1A: ^Z, end of file under TEKDOS (1B is ESC)')
k([0xfa44, 0xfa4a, 0xfa51, 0xfa63, 0xfa77, 0xfa7a, 0xfa9e, 0xfaa5, 0xfaa8, 0xfabf, 0xfade, 0xfaeb, 0xfaf1, 0xfb07, 0xfb18, 0xfb1d],
  'B', 'the low byte of one of Tek\'s variables (1CD8 last line, 1CDA counter, 1CDC message pointer, 1CDE, 1CE0): only the even values let the three words (INX H walks the counter\'s two bytes) and the flags sit side by side without overlapping')
k([0xfa45, 0xfa4b, 0xfa52, 0xfa64, 0xfa78, 0xfa7b, 0xfa9f, 0xfaa6, 0xfaa9, 0xfac0, 0xfadf, 0xfaec, 0xfaf2, 0xfb08, 0xfb19, 0xfb1e],
  'C', 'the page of Tek\'s variables, 1C as read: the whole block could equally be in page 1D (one free area of the assembler\'s RAM or the next); all these bytes go together')
# 2026-11-13: the assembler's surviving source was compiled (ASSV1, version 2.0, original PL/M,
# with the c-ports Fortran-PL/M compiler; ASSV2/ASSV3 with the PL/M-80 port). No build lands on
# the tape's addresses (the tape is a later, larger version), but the original-PL/M build has the
# tape's shape: the I/O procedures and LIST come first and lie below 0060, the hex printer next
# (0139), the variables near the top (1810-1CB9), free memory just above (1D00). ASSV2/ASSV3 put
# their DATA tables first and are not the tape's lineage.
k([0xfa75, 0xfa85, 0xfa8a, 0xfa8f, 0xfa94, 0xfaae, 0xfab3, 0xfab8, 0xfabd, 0xfae3, 0xfae6, 0xfae9, 0xfaf7, 0xfafc],
  'B', 'the page of an output routine of Intel\'s assembler (007A, 008A, 00BE, 00D8): page 00, as read. Compiled, the assembler\'s I/O procedures and LIST come first and end below 0060, so even its larger tape version keeps them in page 00')
k([0xfa9c, 0xfaa3], 'C', 'the page of the assembler\'s hex-print routine (0282 or 0382): compiled version 2.0 has it at 0139; the tape\'s later version adds code before it, by an unknown amount')
k([0xfa74, 0xfa84, 0xfa89, 0xfa8e, 0xfa93, 0xfaad, 0xfab2, 0xfab7, 0xfabc],
  'C', 'the low byte of the assembler\'s character-output routine 00BE (BE or BF; the same in all nine)')
k([0xfa4e, 0xfac7, 0xfacf, 0xfb02, 0xfb0b], 'C', 'the page of an assembler variable or displaced operand (1EFA, 1E3E, 1E40, 1EAA: 1E or 1F; 006E: 00 or 01)')

# The empty zones: every intact PROM of this family that Tektronix programmed fills unused space
# with 00 (the 6800 SYSTEM PROM's 438 unused bytes, its FETCH PROM's unused 256; not one FF or
# 01 in empty space), so these read-as-00 bytes are 00. The 8080's NOP is 00 as well.
EMPTY = [(0xfa32, 0xfa3f), (0xfb31, 0xfb7f), (0xfbda, 0xfbdd), (0xfbf0, 0xfbff)]


def report(out):
    """A table of every byte that differs from the read, and every byte kept as read but uncertain."""
    lines = ['| Address | Read | Rebuilt | Confidence | Why |', '|---|---|---|---|---|']
    for addr in sorted(set(SET) | set(AS_READ) | set(KEPT)):
        i = addr - 0xfa00
        if addr in SET:
            v, c, why = SET[addr]
        elif addr in KEPT:
            v, (c, why) = out[i], KEPT[addr]
            why = 'kept as read: ' + why
        else:
            v, c, why = out[i], 'C', 'kept as read: ' + AS_READ[addr]
        lines.append('| %04X | %02X | %02X | %s | %s |' % (addr, out[i] & 0xfe, v, c, why))
    for addr in unaccounted(out):
        c, why = rest_grade(addr)
        lines.append('| %04X | %02X | %02X | %s | kept as read: %s |' % (addr, out[addr - 0xfa00] & 0xfe, out[addr - 0xfa00], c, why))
    for lo, hi in EMPTY:
        lines.append('| %04X-%04X | 00 | 00 | B | empty: Tektronix fills unused space with 00 in every intact PROM of this family |' % (lo, hi))
    lines.sort(key=lambda l: l[2:6] if l[2:3] in '0123456789ABCDEF' and l[:2] == '| ' else '')
    return '\n'.join(lines) + '\n'


def summary(out):
    """Grade counts over all 512 bytes."""
    from collections import Counter
    g = Counter(v[1] for v in SET.values())
    g.update(v[0] for v in KEPT.values())
    g.update('C' for _ in AS_READ)
    g.update(rest_grade(a)[0] for a in unaccounted(out))
    g['B'] += sum(hi - lo + 1 for lo, hi in EMPTY)
    return g


# The mutation run of 2026-11-13: each byte of FA00-FA31 and FB80-FBEF with bit 0 flipped, under
# the checks of tests/run8080.sh and tests/prototype.sh (PUT into ROM included). These survived:
# flipping them changes nothing TEKDOS shows.
SURVIVORS = {0xfa04, 0xfa05, 0xfa06, 0xfa0e, 0xfa0f, 0xfa10, 0xfa18, 0xfa19, 0xfa1a, 0xfa1b, 0xfa22, 0xfa23,
             0xfa24, 0xfa25, 0xfa2c, 0xfa2d, 0xfa2e, 0xfa2f, 0xfb9a, 0xfb9b, 0xfb9c, 0xfb9f, 0xfba0, 0xfba1,
             0xfba2, 0xfba3, 0xfba4, 0xfba5, 0xfba8, 0xfbba, 0xfbc6, 0xfbc7, 0xfbd9, 0xfbda, 0xfbdb,
             0xfbdc, 0xfbdd, 0xfbde, 0xfbdf, 0xfbe0, 0xfbe1, 0xfbe2, 0xfbe3, 0xfbe8, 0xfbeb}


def rest_grade(addr):
    """The grade of a byte kept as read that no table above lists."""
    if 0xfa00 <= addr <= 0xfa31:
        if (addr - 0xfa00) % 10 in (4, 5, 6):
            return 'B', 'a template\'s status, data or count: TEKDOS writes it (00 is the natural start)'
        return ('A' if addr not in SURVIVORS else 'B'), 'a template\'s pointer (0042) or buffer (004A), or a field P1 sets: verified (mutation caught)'
    if 0xfb21 <= addr <= 0xfb30:
        return 'B', 'a character of " END *MISSING*" CR LF (the other value is another letter)'
    if 0xfa40 <= addr <= 0xfb20:
        return 'B', 'an opcode of the patches: the other value is a different instruction that breaks the routine'
    if addr in (0xfbe0, 0xfbe3):
        return 'C', 'an unused table entry\'s JMP 0000, high byte (00 or 01)'
    if addr in SURVIVORS:
        return 'B', 'the dump\'s tail (SP, HL, A put back), which TEKDOS never observes: by symmetry with the entry and the restore'
    return 'A', 'firmware: verified under TEKDOS (flipping it fails a check)'


def unaccounted(out):
    """Bytes the report does not cover: they must be opcodes or operands whose bit 0 is forced
    by the decoding (the other value is another instruction or breaks the flow)."""
    covered = set(SET) | set(AS_READ) | set(KEPT)
    for lo, hi in EMPTY:
        covered |= set(range(lo, hi + 1))
    return [a for a in range(0xfa00, 0xfc00) if a not in covered]


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
    g = summary(out)
    print('all 512 bytes graded: A %d (verified under TEKDOS or forced), B %d (one sensible reading), C %d (a choice)' % (g['A'], g['B'], g['C']))
    if '--report' in sys.argv:
        sys.stdout.write(report(out))
