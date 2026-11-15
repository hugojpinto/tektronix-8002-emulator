// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Measures what each of the 256 Z80 opcodes does, on the project's Z80 core (superzazu), for
// tools/z80_decode/search.c: one line per opcode, the properties a board's instruction decoding
// could care about. Each opcode runs twice, from 1000 with operand bytes 00 00 00 (a prefix
// takes 00 as its second byte), once with every flag clear and once with every flag set, so a
// conditional instruction shows both ways.
// Columns: op length m1 dreads dwrites io tstates_clear tstates_set flow halt prefix stack
//   length   bytes read from the instruction stream (1000-1003)
//   m1       opcode fetches (M1 cycles)
//   dreads / dwrites / io   data cycles outside the instruction stream, I/O cycles
//   flow     0 PC goes on, 1 always elsewhere, 2 depends on the flags
//   stack    SP changed (either run)
#include <stdio.h>
#include <string.h>
#include "z80.h"

static unsigned char mem[65536];
static int len, m1, dr, dw, io;
static unsigned char rd(void *u, uint16_t a)
{
	z80 *z = u;
	if (a >= 0x1000 && a <= 0x1003) { len++; if (z->m1) m1++; }
	else dr++;
	return mem[a];
}
static void wr(void *u, uint16_t a, uint8_t d) { (void)u; dw++; mem[a] = d; }
static uint8_t in(z80 *z, uint16_t p) { (void)z; (void)p; io++; return 0xff; }
static void out(z80 *z, uint16_t p, uint8_t d) { (void)z; (void)p; (void)d; io++; }

struct run { int len, m1, dr, dw, io; unsigned long t; uint16_t pc, sp; int halted; };

static struct run go(int op, int flags)
{
	static z80 z;
	memset(mem, 0, sizeof mem);
	mem[0x1000] = (unsigned char)op;
	z80_init(&z);
	z.read_byte = rd; z.write_byte = wr; z.port_in = in; z.port_out = out; z.userdata = &z;
	z.pc = 0x1000; z.sp = 0x8000; z.b = 2; z.c = 3; z.d = 0x40; z.e = 5; z.h = 0x20; z.l = 0;
	z.ix = 0x3000; z.iy = 0x3100;
	z.sf = z.zf = z.hf = z.pf = z.nf = z.cf = flags;
	len = m1 = dr = dw = io = 0;
	z.cyc = 0;
	z80_step(&z);
	struct run r = { len, m1, dr, dw, io, z.cyc, z.pc, z.sp, z.halted };
	return r;
}

int main(void)
{
	for (int op = 0; op < 256; op++) {
		struct run a = go(op, 0), b = go(op, 1);
		const uint16_t next = (uint16_t)(0x1000 + a.len);
		int flow = (a.pc != b.pc) ? 2 : (a.pc != next ? 1 : 0);
		int prefix = op == 0xcb || op == 0xdd || op == 0xed || op == 0xfd;
		printf("%02X %d %d %d %d %d %lu %lu %d %d %d %d\n", op, a.len, a.m1, a.dr, a.dw, a.io, a.t, b.t,
		       flow, a.halted, prefix, a.sp != 0x8000 || b.sp != 0x8000);
	}
	return 0;
}
