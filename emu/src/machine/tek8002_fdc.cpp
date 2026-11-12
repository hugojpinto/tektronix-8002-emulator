// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Flexible Disc Controller -- see tek8002_fdc.h and notes/fdc.md.
#include "machine/tek8002_fdc.h"
#include <cstdio>

bool tek8002_fdc::load_rom(const std::string &dir, std::string &err)
{
	auto rd = [&](const char *name, std::vector<u8> &v) {
		FILE *f = fopen((dir + "/" + name).c_str(), "rb");
		if (!f) return false;
		v.resize(512);
		const size_t n = fread(v.data(), 1, 512, f);
		fclose(f);
		return n == 512;
	};
	std::vector<u8> hi, lo;
	if (!rd("fdc_bits15-8.bin", hi) || !rd("fdc_bits7-0.bin", lo)) { err = "missing disc controller microprogram (fdc_bits15-8.bin, fdc_bits7-0.bin) in " + dir; return false; }
	for (int a = 0; a < 512; a++) m_rom[a] = u16(hi[a] << 8 | lo[a]);
	return true;
}

void tek8002_fdc::reset()
{
	// power up: the RAR is reset, and both command groups (Fig. B-4 remark 1)
	m_rar = 0; m_ubank = m_ubank_next = false;
	m_group1 = m_group2 = 0;
	m_host_flag = true;                    // HOST FLG is active while its latch is reset (see command())
	m_carry = false; m_t = 0; m_acc = 0;
	m_buf_pos = 0;
}

u8 tek8002_fdc::host_data_in() const
{
	if (m_host_ctl & 2) {                  // C0 inactive (SIO, RST): the disc status byte (Fig. 1-9a, Table 1-3)
		u8 s = 0;
		if (m_group1 & (1 << 3)) s |= 0x02;                       // UNIT CHECK = F.D. FAILURE
		if (drive && !drive->ready()) s |= 0x04;                  // DRIVE FAULT
		if (drive && drive->write_protect()) s |= 0x08;
		if (m_group1 & (1 << 2)) s |= 0x10;                       // ERROR RECOVERY
		if (m_group1 & (1 << 1)) s |= 0x20;                       // ILLEGAL OPERATION
		return s;
	}
	return m_acc;                          // C0 active (IND): the accumulator
}

// The WR bus inputs (Fig. 3-8), as logical values. The bus is active low (WR̅, Fig. B-2), and
// Fig. 3-8 prints physical levels ("1 = high level = logic false"): its 1s are logical 0s (the
// unused bits), its 0s logical 1s (INL bit 0). Asserted lines read 1.
u8 tek8002_fdc::input(int sel)
{
	switch (sel) {
	case 1: return 0x00;                   // TST: test panel, nothing pressed
	case 2: {                              // CSW
		u8 v = 0;
		if (drive && drive->track00()) v |= 0x80;
		if (drive && drive->index()) v |= 0x40;
		if (!(m_host_ctl & 2)) v |= 0x20;  // !C0
		if (drive && drive->sector()) v |= 0x10;
		if (!(m_host_ctl & 4)) v |= 0x08;  // !C1
		if (!(m_host_ctl & 1)) v |= 0x04;  // strobe active (/S low)
		if (drive && drive->ready()) v |= 0x01;
		return v;
	}
	case 3: return m_buf[m_buf_pos] ? 0x80 : 0x00;               // SBH
	case 4: return m_buf[m_buf_pos] ? 0x01 : 0x00;               // SBL
	case 5: return drive && drive->data_bit() ? 0x80 : 0x00;     // DBT
	case 6: return m_host_byte;            // BYT
	case 7: return u8(((interleave & 0x1f) << 1) | 1);          // INL: switches 5-1 on bits 5-1 (On = 1), bit 0 = 1
	default: return 0xff;
	}
}

// The 74181 in its active-low data convention, with S0 and S1 inverted on the way (Table
// 3-2 and its footnote). fn = RIR bits 11-6 (S3 S2 S1 S0 M CI). Sets C7 (m_c7).
u8 tek8002_fdc::alu(int fn, u8 a, u8 b, bool arith_allowed)
{
	const int s = ((fn >> 2) & 0xc) | (((fn >> 2) & 3) ^ 3);     // S3 S2 ~S1 ~S0
	const bool arith = arith_allowed && (fn & 2);                 // RIR7 = 1: arithmetic (M inverted)
	const u8 x = u8(~a), y = u8(~b);                               // the physical (active-low) inputs
	if (!arith) {
		// active-high 74181 logic on the physical levels, output inverted back
		u8 f;
		switch (s) {
		case 0x0: f = u8(~x); break;          case 0x1: f = u8(~(x | y)); break;
		case 0x2: f = u8(~x & y); break;      case 0x3: f = 0; break;
		case 0x4: f = u8(~(x & y)); break;    case 0x5: f = u8(~y); break;
		case 0x6: f = u8(x ^ y); break;       case 0x7: f = u8(x & ~y); break;
		case 0x8: f = u8(~x | y); break;      case 0x9: f = u8(~(x ^ y)); break;
		case 0xa: f = y; break;               case 0xb: f = u8(x & y); break;
		case 0xc: f = 0xff; break;            case 0xd: f = u8(x | ~y); break;
		case 0xe: f = u8(x | y); break;       default: f = x; break;
		}
		m_c7 = false;
		return u8(~f);
	}
	// arithmetic: P + G + carry on the physical levels (the 74181's sum), inverted back
	const unsigned p = x | ((s & 1) ? y : 0) | ((s & 2) ? u8(~y) : 0);
	const unsigned g = ((s & 4) ? (x & ~y) : 0) | ((s & 8) ? (x & y) : 0);
	bool cin = false;                                              // a carry into the true-sense sum
	if (!(fn & 1)) cin = !(fn & 8) ? true : m_carry;               // RIR6 = 0 enables; RIR9 = 0 forces, else the Carry flip-flop
	const unsigned sum = (p & 0xff) + (g & 0xff) + (cin ? 0 : 1);
	m_c7 = sum < 0x100;                                          // the true carry is the physical one inverted
	return u8(~sum);
}

bool tek8002_fdc::condition(int c) const
{
	bool v;
	switch (c >> 1) {
	case 0: v = m_t != 0; break;
	case 1: v = m_t & 0x40; break;
	case 2: v = m_t & 0x10; break;
	case 3: v = m_carry; break;
	case 4: v = m_t & 0x04; break;
	case 5: v = m_t & 0x01; break;
	case 6: v = drive && drive->data_ready(); break;
	default: v = true; break;
	}
	return (c & 1) ? !v : v;
}

// Type S commands (Fig. B-4). Group 1 latch addresses (bits 8-6): 0 BUSY, 1 ILLEGAL OP,
// 2 ERROR RECOVERY, 3 UNIT CHECK, 4 FILE INOP, 5 WRITE GATE, 6 PRINT STB, 7 HOST FLG;
// bit 9 is the value. HOST FLG is "set" by writing 0 (SFG = 05C0, RFG = 07C0): the flag is
// active while its latch is low, which is also how RST (resetting group 1) acknowledges
// an SIO (the executive answers SIO with RST alone, 05D). Group 2: 0 RAISE HEADS,
// 1 DIRECTION, 2 STEP, 3 EN STACK, 4 EN DISPLAY, 5 SELECT CARRY, 6 >TRK43, 7 UBANK.
void tek8002_fdc::command(u16 w)
{
	const int type = (w >> 10) & 3, addr = (w >> 6) & 7;
	const bool val = w & 0x200;
	if (type == 0) {
		if (w & 0x200) { m_device = m_t & 7; if (drive) drive->select(m_device); }   // CDA: the device address latch from the TB bus
		return;                                                    // NOP
	}
	if (type == 3) { m_group1 = 0; }                               // RST
	else if (type == 1) { if (val) m_group1 |= u8(1 << addr); else m_group1 &= u8(~(1 << addr)); }
	else {
		const bool was = m_group2 & (1 << addr);
		if (val) m_group2 |= u8(1 << addr); else m_group2 &= u8(~(1 << addr));
		if (drive) {
			if (addr == 2 && val && !was) drive->step(m_group2 & 2);   // a STEP pulse, DIRECTION (IN) as set
			if (addr == 0) drive->head_load(!val);                     // RAISE HEADS
		}
		if (addr == 7) m_ubank_next = val;   // takes effect after the next fetch (see step())
	}
	m_host_flag = !(m_group1 & 0x80);
	if (drive) drive->write_gate(m_group1 & 0x20);
}

void tek8002_fdc::special(int sp)
{
	switch (sp) {
	case 1:                                // SPSH: clear Data Ready; push C7 (SELECT CARRY) or the read data bit
		if (m_group2 & 0x08) {
			const bool bit = (m_group2 & 0x20) ? m_c7 : (drive && drive->data_bit());
			m_buf[m_buf_pos] = bit;
			m_buf_pos = (m_buf_pos + 1) & 1023;
		}
		if (drive) drive->data_taken();
		break;
	case 2:                                // PULL: the write flip-flop from C7
		if (drive) drive->write_data(m_c7);
		break;
	default:                               // EPSH: finish the push (nothing left to do in the model)
		break;
	}
}

void tek8002_fdc::step()
{
	const u16 w = m_rom[(m_ubank << 8) | m_rar];
	// UBANK switches the bank one fetch late: the instruction after SH8/SL8 still comes from
	// the old bank, and is the jump that lands in the new one (054 SH8; 055 JMP WRT reaches
	// 1AA; the upper-bank word at 155 is unrelated read code). Established by the listing's
	// jump targets (tools/fdc_dasm.py --check) and by running it.
	m_ubank = m_ubank_next;
	if (trace) fprintf(stderr, "[fdc %03X %04X acc=%02X t=%02X c=%d]\n", (m_ubank << 8) | m_rar, w, m_acc, m_t, int(m_carry));
	m_rar = u8(m_rar + 1);
	int cycles = 1;
	const int src = w >> 14, dst = (w >> 12) & 3, fn = (w >> 6) & 63, sel = (w >> 2) & 15, sp = w & 3;
	if (src == 0) {
		if (dst == 0) command(w);          // Type S
		// LDL (0001): loading the latches from the accumulator is how the latches showed it; no use found
	} else if (dst == 3) {                 // jumps: the RAR takes the complement of the WR bus
		const int c = (w >> 8) & 15;
		const u8 wr = src == 2 ? u8(w) : m_r[15 - sel];
		if (condition(c)) { m_rar = u8(~wr); cycles = 2; }
	} else {
		u8 a; bool arith = true;
		if (src == 2) { a = u8(w); arith = false; }                 // Type L immediate: logic forced, function bits 11-8
		else if (src == 1) a = m_r[sel];
		else a = input(sel);
		const u8 r = alu(src == 2 ? ((w >> 6) & 0x3c) : fn, a, m_acc, arith);
		m_t = r;
		if (arith && (fn & 2)) m_carry = m_c7;
		if (dst == 1) m_r[sel] = r;
		else if (dst == 2) m_acc = r;
		if (src != 2 && sp) special(sp);
	}
	m_time += u64(cycles) * CYCLE_NS;
	if (drive) drive->elapse(u64(cycles) * CYCLE_NS);
}

void tek8002_fdc::run(u64 ns)
{
	m_owed += ns;
	while (m_owed >= CYCLE_NS) {
		const u64 t0 = m_time;
		step();
		const u64 used = m_time - t0;
		m_owed = m_owed > used ? m_owed - used : 0;
	}
}
