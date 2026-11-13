// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// What the 8002's emulator processor boards share (notes/emulators.md): a control port the
// System Processor writes and reads, a firmware PROM with the dump and restore routines
// (070-2711 §1: "a PROM to hold a dump and restore routine"), the forced jump into it, and
// the debug SVC each routine ends with. Each board (tek8002_ep6800, tek8002_epz80) supplies
// its processor, its SVC decoding and its firmware's register block.
//
// - Control byte: bit 7 = active (the Debug module runs the board). Bit 1 = 0 maps the
//   firmware PROM: every firmware call is made with bit 1 clear (TEKDOS 80/C0/90) and every
//   user run with it set (92/96), as on the same boards in the 8550 (notes in the sibling
//   tektronix-8550-emulator, 8301.md "E3 write bits"). Bits 6-5 = emulation mode (00 = 0,
//   01 = 1, 11 = 2). Bit 2: the board's fetch and access signals reach the Debug module's
//   breakpoint and single-cycle logic. TEKDOS sets it whenever it hands breakpoints or single
//   cycle to the Debug module, and clears it when a trace board at C0-C7 does that job (TRACE
//   1A95-1ABE, BKPT 1AD7-1AF8, DEB INIT 197A-1985, the ESC stop 2E46-2E52, ABORT 3818;
//   inferred meaning). Bit 4: set for every user run and clear for every firmware call;
//   modelled as letting the prototype's interrupts through, after the OS/40 Z80 specifics'
//   "Interrupts are detected only when user code is being executed" (inferred).
// - Status (a read of the control port): bit 5 = board present (the 8550's Z80 diagnostic
//   tests it), bit 4 = the processor is halted (TEKDOS's kernel 3C4C-3C55 and 2E3F, its
//   6800, Z80 and 8080 debug control; DOS/50 on the 8550). TEKDOS then resumes at PC last,
//   the halt instruction itself (070-2349 §11: "Execution then continues at the beginning
//   of the WAIT instruction"). The other bits are each board's (board_status;
//   notes/emulators.md). Bits nobody drives read 0 (the open bus).
// - The firmware runs when a forced jump lands on one of its entries while the PROM is
//   mapped. A board with its PROM dumped (the 6800) runs it on its processor; otherwise one
//   step does the routine's work at high level. Either way the routine signals its debug SVC
//   and the board then waits in the PROM for the next forced jump.
// - Emulation modes 1 and 2 (070-2701 §4 EMULATE, §7 "Operating Modes"; TEKDOS's EMULATE
//   builds the control byte as 90 | mode bits 20 or 60, and 02 for runs: 92, B2, F2). In a
//   user run (bit 1 set) the processor's memory is the prototype's in blocks the map marks U
//   (mode 1) or everywhere (mode 2), and its I/O and interrupts are the prototype's. During
//   firmware calls (bit 1 clear) the processor sees program memory, where TEKDOS lends the
//   firmware its blocks (070-2711 §1 step 34's "special MEMSW mode").
// - The firmware's GET and PUT reach the prototype's memory for TEKDOS (DUMP, PATCH, MOVE,
//   LOAD in modes 1 and 2): TEKDOS's kernel (35AC-363B) lends it program memory 0000-0003,
//   [0] data, [1] [2] address (high, low), [3] status, writes C4 to the control port and
//   forces a jump to the firmware's table base [24CB:24CC] minus 8 (GET) or minus 11 (PUT);
//   the routine ends with debug SVC 2 (port F0, interrupt 23, handler 3BF7). Status 00 = done
//   (the same block and entries as the 8550's Z80 firmware: sibling 8301.md).
//   The prototype is reached through prototype_bus (machine/prototype.h, notes/
//   prototype-link.md); with none connected, nothing answers.
// - Clock (Z80 Emulator Processor manual 070-3665 §1-2): mode 0 runs from the board's own
//   clock (a jumper); modes 1 and 2 from the prototype's, which the probe takes from the
//   socket's clock pin. The link reports its rate (v4); without it the board's is used.
#ifndef TEK_TEK8002_EMBOARD_H
#define TEK_TEK8002_EMBOARD_H
#include "machine/tek8002_slave.h"
#include "machine/prototype.h"

class tek8002_emboard : public tek8002_slave
{
public:
	tek8002_emboard(slave_bus &bus, u8 port) : tek8002_slave(bus), m_port(port) {}

	bool decodes(u8 port) const override { return port == m_port; }
	void command(u8, u8 d) override { m_cmd = d; }
	u8 status(u8) const override { return u8(0x20 | (cpu_halted() ? 0x10 : 0) | board_status()); }
	bool active() const override { return m_cmd & 0x80; }
	bool debug_signals() const override { return m_cmd & 0x04; }
	// its clock: the prototype's in a mode 1/2 run when the link reports it (v4), else its own
	double clock_hz() const override { return user_run() && emulation_mode() != 0 && target->clock_hz() > 0 ? target->clock_hz() : board_clock_hz(); }
	void reset() override { m_in_prom = false; m_svc = -1; cpu_reset(); }
	void jump(u16 target) override { m_in_prom = false; cpu_jump(target); }
	void step() override;
	u16 pc() const override { return m_in_prom ? m_prom_pc : cpu_pc(); }
	bool halted() const override { return !m_in_prom && cpu_halted(); }
	u64 cycles() const override { return cpu_cycles() + m_prom_cycles; }

	prototype_bus *target = &m_no_target;     // what the probe is plugged into
	u8 command_byte() const { return m_cmd; }
	bool firmware_mapped() const { return (m_cmd & 0x82) == 0x80; }
	int emulation_mode() const { return (m_cmd & 0x60) == 0x60 ? 2 : (m_cmd & 0x60) ? 1 : 0; }
	bool trace = false;

protected:
	// the processor
	virtual void cpu_reset() = 0;
	virtual void cpu_step() = 0;
	virtual void cpu_jump(u16 target) = 0;   // also ends a halt
	virtual u16 cpu_pc() const = 0;
	virtual bool cpu_halted() const = 0;
	virtual u64 cpu_cycles() const = 0;
	// the firmware: the routine at this entry, or false if there is none; sets the SVC port
	virtual bool firmware(u16 entry) = 0;
	virtual double board_clock_hz() const = 0;   // mode 0 (and modes 1/2 with no clock reported)
	// the board's own status bits (7, 6, 3-0; bit 5 present and bit 4 halted are common)
	virtual u8 board_status() const = 0;

	void svc(int port) { if (m_svc < 0) m_svc = port & 7; }   // the board's SVC port, F0-F7 (one per instruction)
	void get_put(bool put_entry);              // the firmware's GET or PUT on the block at 0000-0003
	bool user_run() const { return (m_cmd & 0x82) == 0x82; }
	bool proto_mem(u16 a) const { return user_run() && (emulation_mode() == 2 || (emulation_mode() == 1 && m_bus.user_block(a))); }
	bool proto_io() const { return user_run() && emulation_mode() != 0; }
	u8 proto_read(u16 a, bool fetch) { m_bus.slave_watch(a, false); return fetch ? target->mem_fetch(a) : target->mem_read(a); }
	void proto_write(u16 a, u8 d) { m_bus.slave_watch(a, true); target->mem_write(a, d); }
	// before each instruction in modes 1/2: time to the target (in 4 MHz units, as the link
	// counts), its RESET and its interrupt lines
	virtual void target_lines(bool reset_edge, bool nmi_edge, bool irq) = 0;
	u8 mem(u16 a) { return m_bus.slave_read(a, false); }
	void put(u16 a, u8 d) { m_bus.slave_write(a, d); }
	void dump_block(const char *what, u16 entry, int size);

	u64 m_prom_cycles = 0;

private:
	u8 m_port, m_cmd = 0;
	bool m_in_prom = false;                // waiting in the firmware for a forced jump
	u16 m_prom_pc = 0;
	int m_svc = -1;
	prototype_bus m_no_target;
	bool m_trst = false, m_tnmi = false;
	u64 m_link_last = 0;                  // cpu_cycles() at the last link poll
	double m_link_cyc = 0;                // link time so far, in 4 MHz T-states
};
#endif
