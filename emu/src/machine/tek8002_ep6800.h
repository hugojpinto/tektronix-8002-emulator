// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 6800 Emulator Processor: a 6800 at 1 MHz (1978-81 catalogues) that runs the
// user's program out of program memory as a slave of the System Processor, under the
// Debug module (notes/6800.md).
//
// - Control: the System Processor writes the board's command byte to port E2 (TEKDOS's
//   emulator table, 2032/2033). Bit 7 selects (activates) the board. TEKDOS writes 90 to
//   probe it at start-up, 92 after, 80 or C0 when it starts a program (DEB CNTL 1611-1619),
//   00 to turn it off. The other bits are recorded, not decoded (modes 1 and 2 need a
//   prototype; mode 0 is program memory only).
// - SVCs are memory mapped (070-2349 §10): a read or write of FEF0-FEF7 is the Debug
//   module's SVC port F0-F7 (FEF7 = SVC 1). The access does not reach memory, and one
//   instruction makes one SVC however many times it touches the address.
// - WAI ("WAIT", 070-2349 §11) is the halt the Debug module detects (slave halted).
// - A forced jump replaces the next instruction fetch with a jump (070-2711 §6), here by
//   setting the PC; it also ends a WAI.
// - The board's SYSTEM PROM (82S115, not preserved) holds two routines TEKDOS forces
//   jumps to (DEB INIT 195D-196E). Modelled at high level, from what TEKDOS does around
//   them (DEB CNTL 15DE-160B and 1733, DSTAT 18E9-1937):
//     F9FA  restore: load A, B, X, SP, CC from 0000-0006, then "debug SVC 1" (port F1,
//           interrupt 22)
//     F9FD  dump: store A, B, X, SP, CC at 0000-0006, then debug SVC 1 likewise
//   (the roles and the SVC: 070-2711 §1 "Debug mode" walkthrough, steps 9-19 and 34-37;
//   TEKDOS restores before GO and dumps after each traced instruction)
//   TEKDOS swaps 0000-0006 with its own buffer around each call, so the user's bytes
//   there are kept. After signalling, the routine waits in the PROM for the forced jump
//   the master sends next (to the user's PC, or back into the PROM).
#ifndef TEK_TEK8002_EP6800_H
#define TEK_TEK8002_EP6800_H
#include "machine/tek8002_slave.h"
#include "cpu/m6800/m6800.h"

class tek8002_ep6800 : public tek8002_slave, private m6800_cpu_device::bus_interface
{
public:
	static constexpr u16 PROM_RESTORE = 0xf9fa, PROM_DUMP = 0xf9fd, SVC_BASE = 0xfef0, REGS = 0x0000;

	explicit tek8002_ep6800(slave_bus &bus) : tek8002_slave(bus), cpu(*this) {}
	const char *name() const override { return "6800 Emulator Processor"; }
	double clock_hz() const override { return 1.0e6; }
	bool decodes(u8 port) const override { return port == 0xe2; }
	void command(u8 port, u8 d) override { m_cmd = d; }
	// Its status, read at E2: bit 7 high and bit 4 low in normal running. TEKDOS's debug
	// control takes bit 7 low, or bit 4 high, as an instruction that did not complete: it
	// resumes at PC last instead of PC next, and then also resets the board (DEB CNTL
	// 1550-155C, 15A9-15BB, 1602-1607; TMI is "equal" when the masked bits are all 1).
	// What the bits mean on the board is not known (inferred; the other bits read 1).
	u8 status(u8) const override { return 0xef; }
	bool active() const override { return m_cmd & 0x80; }
	void reset() override { m_in_prom = false; m_svc = -1; cpu.reset(); }
	void jump(u16 target) override { m_in_prom = false; cpu.clear_wait(); cpu.set_pc(target); }
	void step() override;
	u16 pc() const override { return m_in_prom ? m_prom_pc : cpu.pc(); }
	bool halted() const override { return !m_in_prom && cpu.waiting(); }
	u64 cycles() const override { return cpu.total_cycles() + m_prom_cycles; }

	u8 command_byte() const { return m_cmd; }
	m6800_cpu_device cpu;

private:
	u8 m_cmd = 0;
	bool m_in_prom = false;                // waiting in the SYSTEM PROM for a forced jump
	u16 m_prom_pc = 0;
	u64 m_prom_cycles = 0;
	int m_svc = -1;                        // the SVC port this instruction touched

	u8 read(u16 a, bool opcode) override;
	void write(u16 a, u8 d) override;
	void prom(u16 entry);
};
#endif
