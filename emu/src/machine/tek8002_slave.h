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
	virtual bool active() const = 0;             // selected: the one the Debug module runs

	virtual void reset() = 0;                    // forced reset (and activation)
	virtual void jump(u16 target) = 0;           // forced jump
	virtual void step() = 0;                     // one instruction
	virtual u16 pc() const = 0;
	virtual bool halted() const = 0;
	virtual u64 cycles() const = 0;              // its own clock cycles so far

protected:
	slave_bus &m_bus;
};
#endif
