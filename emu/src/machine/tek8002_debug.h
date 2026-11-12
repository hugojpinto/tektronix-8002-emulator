// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Debug and Front Panel I/O module (670-5299; 070-2711 section 6):
// master/slave control, the debug registers on ports F8-FF, and the debug
// interrupts 16-31 it vectors to the System Processor.
//
// Master/slave hand-over (070-2711 §6 "Master/Slave Status Logic", "Halt Detection";
// ztpe.nl "OS Reference and Internals" §8.2): the System Processor gives the bus to the
// slave by executing HALT, and the active slave (tek8002_slave) runs. The slave is paused
// whenever an interrupt for the master is pending. The module raises:
//   16-23  SVC: the slave wrote port F7 ... F0 (F7 = SVC 1), while F9 bit 4 is high
//   24/25  breakpoint 1/2: the slave read (fetches included) or wrote the breakpoint
//          address, as enabled by F8, while debugging is enabled (F8 bit 5 low)
//   26     single cycle: after each slave instruction, while debugging is enabled and
//          F8 bit 4 is low (TRACE ALL)
//   27     SLAVE HALTED: the slave executed HALT, or no slave took the bus within one
//          38.4 kHz I/O clock after the master halted
//   28     DIAGNOSTIC INTERRUPT: the front panel switch, master or slave running
// An interrupt the slave causes takes effect after its instruction. It latches PC last
// (that instruction) and PC next, and pauses the slave. After an SVC the slave continues
// where it stopped; the others arm a forced jump, so it continues at the forced jump
// address, which the master sets (TEKDOS's SVC handler leaves it alone, notes/8002.md).
// With the interrupt mask on (F9 bit 3 low), debug interrupts are held ("allowed to
// stack") and raised when the mask is released (§6 "Interrupt Mask Bit").
//
// Registers (070-2711 Tables 6-1A/B, Figs. 6-1, 6-2; ztpe.nl Annex B). Active low, and
// addresses are written and read inverted:
//   F8 write  debug control: 0 BP1 read, 1 BP1 write, 2 BP2 read, 3 BP2 write,
//             4 single cycle, 5 debug (sequencer) enable
//   F9 write  command: 0 forced reset, 1 forced interrupt, 2 forced jump (each on a
//             1 -> 0 transition), 3 interrupt mask, 4 SVCs disabled when low
//   FA/FB write  forced jump address low/high
//   FC/FD write  breakpoint 1 low/high; read: PC next low/high (true, not inverted)
//   FE/FF write  breakpoint 2 low/high; read: PC last low/high (likewise; tek8002_debug::read)
#ifndef TEK_TEK8002_DEBUG_H
#define TEK_TEK8002_DEBUG_H
#include "emu.h"
#include "machine/tek8002_slave.h"

// what the Debug module needs of the mainframe
class debug_host
{
public:
	virtual ~debug_host() = default;
	virtual void debug_interrupt(int n) = 0;                  // raise interrupt n (16-31) to the master
	virtual bool master_interrupt_pending() const = 0;        // the slave is paused while one is
};

class tek8002_debug
{
public:
	static constexpr int INT_SVC1 = 16, INT_BP1 = 24, INT_BP2 = 25, INT_SINGLE_CYCLE = 26,
	                     INT_SLAVE_HALTED = 27, INT_DIAG = 28;

	explicit tek8002_debug(debug_host &h) : m_host(h) {}
	void reset();
	bool decodes(u8 port) const { return port >= 0xf8; }
	void write(u8 port, u8 d);
	u8 read(u8 port) const;

	// The master has halted with no active slave: count I/O clocks while nothing holds the bus
	void master_halted(int cycles);
	void master_running() { m_halt_cycles = 0; m_halt_reported = false; }
	// the master is halted and slave s is active: run it for this much master time
	void run_slave(tek8002_slave &s, int cycles2650, double master_hz);
	// what the slave does on the bus, as the module sees it
	void slave_access(u16 a, bool write);  // breakpoint comparators
	bool slave_out(u16 port);              // true if the module takes it (an SVC port)
	bool svcs_enabled() const { return m_command & 0x10; }
	void diag_switch();                    // the front panel's DIAG INTRPT switch

	int io_clock_cycles = 33;              // one 38.4 kHz I/O clock in 1.25 MHz 2650 clocks
	bool trace = false;

	u8 control() const { return m_control; }
	u8 command() const { return m_command; }
	u16 jump_address() const { return u16(~(m_jump_hi << 8 | m_jump_lo)); }
	u16 breakpoint(int n) const { return u16(~(m_bp[2 * n + 1] << 8 | m_bp[2 * n])); }
	u16 pc_last() const { return m_pc_last; }
	u16 pc_next() const { return m_pc_next; }

private:
	debug_host &m_host;
	u8 m_control = 0xff, m_command = 0xff;
	u8 m_jump_lo = 0xff, m_jump_hi = 0xff;
	u8 m_bp[4] = { 0xff, 0xff, 0xff, 0xff };
	u16 m_pc_last = 0, m_pc_next = 0;
	int m_halt_cycles = 0;
	bool m_halt_reported = false;
	bool m_reset_pending = false, m_jump_pending = false;   // forced reset / jump, taken when the slave next runs
	u32 m_events = 0;                      // debug interrupts (bit n = interrupt n) the current slave instruction caused
	u32 m_held = 0;                        // raised while masked
	double m_frac = 0;                     // slave clocks owed

	bool debugging() const { return !(m_control & 0x20); }
	bool masked() const { return !(m_command & 0x08); }
	void raise(u32 events);                // to the master, or held while masked
};
#endif
