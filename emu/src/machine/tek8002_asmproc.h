// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002A Assembler Processor (670-5220; 070-2711 section 9): a Z80 at 2.5 MHz
// (10 MHz / 4) that runs TEKDOS's assembler, linker and macro processor out of program
// memory, as a slave of the System Processor.
//
// Control (§9 "Control Byte Decoder and Latch"): the System Processor writes a command
// byte to port E7. Bit 7 (Active) selects the board; on becoming active the Z80 is reset
// and starts at 0000. TEKDOS writes 90 to start it and 00 to stop it (ASM, TEKDOS 3.3
// 3991). Its address and data buffers invert both ways, which software cannot see.
#ifndef TEK_TEK8002_ASMPROC_H
#define TEK_TEK8002_ASMPROC_H
#include "machine/tek8002_slave.h"
extern "C" {
#include "cpu/z80/superzazu/z80.h"
}

class tek8002_asmproc : public tek8002_slave
{
public:
	explicit tek8002_asmproc(slave_bus &bus);
	const char *name() const override { return "Assembler Processor"; }
	double clock_hz() const override { return 2.5e6; }
	bool decodes(u8 port) const override { return port == 0xe7; }
	void command(u8 port, u8 d) override;
	bool active() const override { return m_cmd & 0x80; }
	void reset() override;
	void jump(u16 target) override { cpu.halted = false; cpu.pc = target; }
	void step() override { z80_step(&cpu); }
	u16 pc() const override { return cpu.pc; }
	bool halted() const override { return cpu.halted; }
	u64 cycles() const override { return cpu.cyc; }

	z80 cpu{};

private:
	u8 m_cmd = 0;
	static u8 mem_rd(void *u, u16 a);
	static void mem_wr(void *u, u16 a, u8 d);
	static u8 port_in(z80 *z, u16 port);
	static void port_out(z80 *z, u16 port, u8 d);
};
#endif
