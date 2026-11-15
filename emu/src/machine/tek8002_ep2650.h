// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 2650 Emulator Processor, port E0 (ztpe.nl Annex B; Millennium's diagnostics number
// their modules after the ports: E0 2650, E1 8080, E2 6800, E3 Z80; notes/diagnostics.md). It runs
// the user's program out of program memory as a slave of the System Processor, like the other
// emulator boards (tek8002_emboard). No manual, schematic or PROM dump of this board is known:
// what is modelled comes from Millennium's 2650 DEBUG and 2650 FETCH tests (DE011, DE021) and from
// the 2650 itself (Signetics 2650 manual, 1975), and is noted in notes/emulators.md, "The 2650 board".
// - The processor: a Signetics 2650 on MAME's core (the System Processor's), 15-bit addresses.
//   The 2650 has no fetch pin; the board marks the first read of each instruction as its fetch.
// - Clock: 1.25 MHz, as the System Processor's (inferred).
// - SVCs: an extended output (WRTE) to F0-F7 while the Debug module takes SVCs (inferred, as the
//   other boards' OUT F0-F7).
#ifndef TEK_TEK8002_EP2650_H
#define TEK_TEK8002_EP2650_H
#include "machine/tek8002_emboard.h"
#include "cpu/s2650/s2650.h"

class tek8002_ep2650 : public tek8002_emboard, private s2650_device::bus_interface
{
public:
	explicit tek8002_ep2650(slave_bus &bus);
	const char *name() const override { return "2650 Emulator Processor"; }
	double board_clock_hz() const override { return 1.25e6; }
	s2650_device cpu;

protected:
	void cpu_reset() override;
	void cpu_step() override { m_cyc += u64(cpu.run(1)); }
	void cpu_jump(u16 target) override { cpu.set_pc(target); }
	u16 cpu_pc() const override { return cpu.pc(); }
	bool cpu_halted() const override { return cpu.halted(); }
	u64 cpu_cycles() const override { return m_cyc; }
	bool firmware(u16) override { return false; }   // no firmware known yet
	bool debug_signals() const override { return true; }
	u8 board_status() const override { return 0x00; }
	void target_lines(bool reset_edge, bool nmi_edge, bool irq) override;
	// The board stops the 2650 with PAUSE when it sees the next fetch. "When PAUSE is driven low,
	// the 2650 finishes the instruction in progress and then enters the WAIT state. ... If PAUSE
	// is turned on then off again before the last cycle of the current instruction begins,
	// program execution continues" (Signetics 2650 manual): PAUSE counts at the start of an
	// instruction's last cycle. Raised after the next fetch, it is in time for an instruction of
	// three or four cycles, and too late for one of two, whose last cycle follows the fetch at
	// once: such an instruction at PC next runs before a breakpoint or single-cycle stop takes
	// effect (inferred). Millennium's 2650 FETCH test (DE021) follows each instruction with a NOP
	// and a HALT; it expects the IORZ a BIRR branches to to have set the condition code, and the
	// BDRR a BCFA branches to not to have run
	void finish_fetched() override;

private:
	u8 read(offs_t a) override;
	void write(offs_t a, u8 d) override;
	u8 io_read(offs_t port) override;
	void io_write(offs_t port, u8 d) override;
	int intack() override { return 0; }
	static const u8 two_cycle[32];            // opcodes of 2 processor cycles (6 clocks)
	int m_fetch_at = -1;                      // the address of the next instruction's opcode
	u64 m_cyc = 0;
};
#endif
