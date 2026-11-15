// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// A processor the 8002's Debug module runs as its "slave" while the System Processor is
// halted: the Assembler Processor (Z80, tek8002_asmproc) and, later, the emulator boards.
// What every slave shares (forced reset and jump, SVCs, halt detection, PC last/next,
// pausing for the master's interrupts) is the Debug module's and lives once, in
// tek8002_debug::run_slave. A slave supplies what differs: its processor, its clock and
// its slave command port. It reaches memory and I/O through slave_bus, which the
// mainframe implements (program memory; SVC ports to the Debug module).
#ifndef TEK_TEK8002_SLAVE_H
#define TEK_TEK8002_SLAVE_H
#include "emu.h"

class slave_bus
{
public:
	virtual ~slave_bus() = default;
	virtual u8 slave_read(u16 a, bool fetch) = 0;
	virtual void slave_write(u16 a, u8 d) = 0;
	virtual u8 slave_in(u16 port) = 0;
	virtual void slave_out(u16 port, u8 d) = 0;
	// what an emulator board needs besides (notes/emulators.md)
	virtual bool user_block(u16 a) const = 0;      // the memory map: this 128-byte block is the prototype's (U)
	virtual bool svcs_enabled() const = 0;         // the Debug module takes SVC ports (F9 bit 4)
	virtual void slave_watch(u16 a, bool write) = 0;   // a cycle on the probe: the breakpoint comparators see it
	// a bus cycle of the user's program that does not go through slave_read/write/in/out (the
	// prototype's, in modes 1 and 2), for the Real-Time Prototype Analyzer
	virtual void slave_cycle(u16 a, u8 d, bool fetch, bool write, bool io) { (void)a; (void)d; (void)fetch; (void)write; (void)io; }
};

class tek8002_slave
{
public:
	explicit tek8002_slave(slave_bus &bus) : m_bus(bus) {}
	virtual ~tek8002_slave() = default;
	virtual const char *name() const = 0;
	virtual double clock_hz() const = 0;

	// its slave command port, as the System Processor writes it
	virtual bool decodes(u8 port) const = 0;
	virtual void command(u8 port, u8 d) = 0;
	virtual u8 status(u8 port) const { return 0xff; }   // a read of its command port (FF: none)
	// whether its fetch and access signals reach the Debug module's breakpoint and single-cycle
	// logic (an emulator board's control bit 2)
	virtual bool debug_signals() const { return true; }
	virtual bool active() const = 0;             // selected: the one the Debug module runs
	// whether the RTPA sees its cycles: it watches the slave bus, whoever drives it (software
	// decides what it records: C3's run enable). A board's own PROM fetches stay on the board.
	virtual bool rtpa_visible() const { return active(); }

	virtual void reset() = 0;                    // forced reset (and activation)
	virtual void jump(u16 target) = 0;           // forced jump
	// forced interrupt: the Debug module's pulse on the slave's interrupt level zero line
	// (Universal One Hardware Reference §4.2.3), "stored and processed on the Slave CPU card"
	virtual void forced_interrupt() {}
	// a System Processor read of program memory at a (the full 64K address): an active board
	// whose firmware is mapped answers for its PROM there (the diagnostics' PROM tests read
	// the boards' PROMs this way, DE310 subtest 0E)
	virtual bool master_read(u16 a, u8 &d) const { (void)a; (void)d; return false; }
	virtual void step() = 0;                     // one instruction
	virtual u16 pc() const = 0;
	// the address its next fetch puts on the bus, which the Debug module latches as PC next
	virtual u16 fetch_address() const { return pc(); }
	virtual bool halted() const = 0;
	virtual u64 cycles() const = 0;              // its own clock cycles so far

protected:
	slave_bus &m_bus;
};
#endif
