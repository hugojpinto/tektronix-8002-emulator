// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Flexible Disc Controller (070-2587 section 3): the microprogrammed
// processor that runs the 512-word microprogram (roms/fdc, notes/fdc.md), its host port
// and its drive interface. The drives themselves (index and sector pulses, track 00, the
// FM data) are behind fdc_drive_bus.
//
// The machine, as established from §3, Appendix B and the listing (notes/fdc.md):
// - 200 ns cycle; a taken jump takes two ("400 NS DELAY PER EXECUTED JUMP").
// - 16 working registers, the accumulator, the T-register (the last ALU result, which
//   the bit and zero jump conditions test), the Carry flip-flop, UBANK (the ROM bank).
// - ALU operand A is the WR bus (a register, an input or the immediate byte), B is the
//   accumulator (Table 3-2). In arithmetic mode the carry input is enabled by RIR6 = 0
//   and is forced by RIR9 = 0, or else comes from the Carry flip-flop (ADC, RLC).
// - A jump loads the ROM Address Register with the complement of the WR bus, direct or
//   through a register (return vectors are kept complemented: LOD l,A,WRT1 loads 4B).
// - Inputs reach the WR bus inverted: a bit is 1 when its (active-low) line is asserted.
//   CSW = TRK00, INDEX, C0, SECTOR, C1, S, 0, READY (bits 7-0, Fig. 3-8); with the host's
//   own C1/C0 levels that is bit 5 = !C0, bit 3 = !C1, bit 2 = strobe active, as the
//   service-request decode at 05A needs.
// - The host reads the disc status byte while its C0 is inactive (SIO, RST), and the
//   accumulator otherwise (IND) (§3 "Controller Functional Description").
// - The Sector Buffer is a 1024-bit shift register: SPSH pushes (C7 with SELECT CARRY on,
//   else the read data bit) at the input while enabled by EN STACK; SBH/SBL read the bit
//   at its output; pushing back the bit read recirculates the buffer.
#ifndef TEK_TEK8002_FDC_H
#define TEK_TEK8002_FDC_H
#include "emu.h"
#include <string>
#include <vector>

// what the controller sees of its drives (stage C models them)
class fdc_drive_bus
{
public:
	virtual ~fdc_drive_bus() = default;
	// drive status lines of the selected drive, asserted = true
	virtual bool track00() = 0;
	virtual bool index() = 0;
	virtual bool sector() = 0;
	virtual bool ready() = 0;
	virtual bool write_protect() = 0;
	// the read logic: a separated clock pulse has arrived (sets Data Ready), and the data bit with it
	virtual bool data_ready() = 0;
	virtual bool data_bit() = 0;
	virtual void data_taken() = 0;           // SPSH clears Data Ready
	// outputs
	virtual void select(int drive) = 0;
	virtual void step(bool in) = 0;           // a STEP pulse (leading edge), direction in = towards track 76
	virtual void head_load(bool on) = 0;      // FILE INOP pulse: load the selected drive's head; RAISE HEADS: unload
	virtual void write_gate(bool on) = 0;
	virtual void write_data(bool v) = 0;      // the write flip-flop as PULL loads it
	virtual void elapse(u64 ns) = 0;          // controller time advances (the disc turns)
};

class tek8002_fdc
{
public:
	static constexpr int CYCLE_NS = 200;

	bool load_rom(const std::string &dir, std::string &err);
	void reset();
	void run(u64 ns);                          // execute for this much time
	void step();                               // one instruction

	// host side: the System Processor's parallel interface
	void host_control(u8 c1c0s) { m_host_ctl = c1c0s & 7; }
	void host_data_out(u8 d) { m_host_byte = d; }
	u8 host_data_in() const;                   // EB read
	bool host_flag() const { return m_host_flag; }

	fdc_drive_bus *drive = nullptr;
	u8 interleave = 0x07;                      // S4104 switches 5-1 (bit 4 = switch 5), On = 1: Off Off On On On (070-2587 §1)
	bool trace = false;
	int interleave_step() { return input(7) & 31; }   // what the sector count adds per SECTOR pulse (03F-041)

	// state, for tests and the debugger
	u16 pc() const { return u16((m_ubank << 8) | m_rar); }
	u8 reg(int n) const { return m_r[n & 15]; }
	u8 acc() const { return m_acc; }
	u64 time_ns() const { return m_time; }
	u8 status_latches() const { return m_group1; }

private:
	u16 m_rom[512] = {};
	u8 m_r[16] = {};
	u8 m_acc = 0, m_t = 0, m_rar = 0;
	bool m_carry = false, m_ubank = false, m_ubank_next = false, m_c7 = false;
	u8 m_group1 = 0, m_group2 = 0;             // Type S command latches, bit n = command n (Fig. B-4)
	bool m_host_flag = false;
	u8 m_host_ctl = 7, m_host_byte = 0;
	u8 m_device = 0;
	bool m_buf[1024] = {};
	int m_buf_pos = 0;
	u64 m_time = 0;
	u64 m_owed = 0;

	u8 input(int sel);
	u8 alu(int fn, u8 a, u8 b, bool arith_allowed);
	void command(u16 w);
	void special(int sp);
	bool condition(int c) const;
};
#endif
