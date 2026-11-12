// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A µProcessor Lab mainframe (8001/8002A service manual 070-2711).
// Boards modelled:
//   System Processor (670-5300): 2650 at 1.25 MHz, interrupt flip-flops, priority
//       encoder and vector generator, I/O decoder for ports E8-EE: AY-5-1013 system
//       terminal UART (E8/E9), parallel interface to the Flexible Disc Unit (EA/EB),
//       interval timer (EC), program memory bank switch (EE)
//   System Memory (670-5297): 16K RAM with the 256-byte bootstrap PROM over 0000-00FF
//   Program Memory (670-5298): up to four 16K boards, one bank at a time at 4000-7FFF
//   Debug & Front Panel I/O (670-5299): ports F8-FF, master/slave hand-over, debug
//       interrupts (tek8002_debug)
//   Assembler Processor (670-5220): the Z80 slave, control port E7 (tek8002_asmproc)
//   6800 Emulator Processor: the 6800 slave, control port E2 (tek8002_ep6800)
//   System Communications (670-5219): three ACIAs and the memory map, ports C9-CF
//       (tek8002_syscomm)
// Not yet: emulator boards, PROM programmers, RTPA, parity.
#ifndef TEK_TEK8002_H
#define TEK_TEK8002_H
#include "emu.h"
#include "cpu/s2650/s2650.h"
#include "machine/ay31015.h"
#include "machine/tek8002_fdu.h"
#include "machine/tek8002_debug.h"
#include "machine/tek8002_asmproc.h"
#include "machine/tek8002_ep6800.h"
#include "machine/tek8002_syscomm.h"
#include <string>
#include <vector>

class tek8002 : public s2650_device::bus_interface, public slave_bus, public debug_host
{
public:
	// What a read finds where no board answers. The boards drive the backplane's data bus
	// through inverting buffers (070-2711 §9 "Address and Data Bus Buffers": "the data
	// transferred through these buffers is also inverted"), so a line nobody drives floats
	// high and reads as 0. TEKDOS relies on it: its debug control takes port C2 reading 0
	// as no trace board, and then uses the Debug module's PC registers (DEB CNTL 157E).
	static constexpr u8 OPEN_BUS = 0x00;
	static constexpr double CPU_HZ = 1'250'000.0;   // 10 MHz / 8 (070-2711 §3)
	// interrupt numbers (070-2711 Table 3-1): vector address = 2n, priority = n (0 highest)
	enum { INT_RESET = 0, INT_SYSMEM_PARITY = 1, INT_TAPE = 2, INT_PROGMEM_PARITY = 3,
	       INT_CONSOLE_IN = 4, INT_CONSOLE_OUT = 5, INT_COMM1 = 6, INT_COMM2 = 7, INT_COMM3 = 8,
	       INT_TIMER = 9, INT_DISC = 11, INT_PROM1 = 13, INT_PROM2 = 14, INT_RTPA = 29 };
	// Table 3-2A's OCR text puts the disc and printer flip-flops at "INT 31"/"INT 30"; Table 3-1
	// and TekDOS 3.3's jump table (a real handler at vector 11, a bare RETE at 30 and 31) put the
	// disc at 11. The printer interrupt is unused by the 8002A (070-2587 §1).


	tek8002();
	bool load_boot_prom(const std::string &path, std::string &err);
	void power_on();
	void restart();                 // SYSTEM RESTART switch
	void run_slice(int cycles);

	s2650_device cpu;
	ay31015 term;                   // system terminal
	tek8002_fdu fdu;                // Flexible Disc Unit (drives 0-3)
	tek8002_debug debug{*this};     // Debug and Front Panel I/O module
	tek8002_syscomm syscomm;        // System Communications: modem, printer, paper tape ACIAs; memory map
	tek8002_asmproc asmproc{*this}; // Assembler Processor (Z80)
	tek8002_ep6800 ep6800{*this};   // 6800 Emulator Processor
	tek8002_slave *active_slave() const;   // the slave the Debug module runs, if any
	int program_boards = 2;         // 8002A: two 16K program memory boards standard (up to 4)
	bool timer_100ms = false;       // interval timer jumper: 10 ms (default) or 100 ms
	bool trace_io = false;

	u64 cycles() const { return m_cycles; }
	u8 peek(offs_t a) { return read(a & 0x7fff); }
	void poke(offs_t a, u8 d) { write(a & 0x7fff, d); }
	u32 pending() const { return m_pending; }

	// s2650 bus
	u8 read(offs_t a) override;
	void write(offs_t a, u8 d) override;
	u8 io_read(offs_t port) override;
	void io_write(offs_t port, u8 d) override;
	int intack() override;

	// slave_bus: what a slave sees. Program memory as one linear space (board = A15-A14);
	// its OUTs to F0-F7 are SVCs for the Debug module
	u8 slave_read(u16 a, bool fetch) override;
	void slave_write(u16 a, u8 d) override;
	u8 slave_in(u16 port) override;
	void slave_out(u16 port, u8 d) override;
	// debug_host
	void debug_interrupt(int n) override { raise(n); update_irq(); }
	bool master_interrupt_pending() const override { return m_pending != 0; }

private:
	std::vector<tek8002_slave *> m_slaves{ &asmproc, &ep6800 };   // every slave board, each decoding its own command port
	std::vector<u8> m_prom;                 // bootstrap PROM, in CPU address order
	std::vector<u8> m_sysram, m_progmem;
	u8 m_bank = 0;                          // EE bits 1-0: program memory bank at 4000-7FFF
	u8 m_serial_ctl = 0;                    // E9 write: bit 2 TTY interrupts, bit 1 even parity, bit 0 reader on
	u8 m_par_ctl = 0x07;                    // EA write: bit 5 printer int enable, bit 4 disc int enable, bits 2-0 C1 C0 /S
	u8 m_timer_ctl = 0;                     // EC write bit 0
	u32 m_pending = 0;                      // interrupt flip-flops (bit n = interrupt n)
	bool m_da_was = false, m_tbmt_was = true, m_flag_was = false;
	u64 m_cycles = 0, m_next_tick = 0;

	void raise(int n) { m_pending |= 1u << n; }
	void update_irq() { cpu.set_irq(m_pending ? 1 : 0); }
	void devices_tick(int cycles);
};
#endif
