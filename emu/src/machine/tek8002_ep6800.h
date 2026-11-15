// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 6800 Emulator Processor: a 6800 at 1 MHz (1978-81 catalogues) that runs the
// user's program out of program memory as a slave of the System Processor, under the
// Debug module (notes/emulators.md). Control port E2; what all boards share is in
// tek8002_emboard.
//
// - SVCs are memory mapped (070-2349 §10): a read or write of FEF0-FEF7 is the Debug
//   module's SVC port F0-F7 (FEF7 = SVC 1). The access does not reach memory, and one
//   instruction makes one SVC however many times it touches the address.
// - WAI ("WAIT", 070-2349 §11) is the halt the Debug module detects (slave halted).
// - A forced jump sets the PC; it also ends a WAI.
// - Modes 1 and 2: memory through the probe as tek8002_emboard routes it; IRQ, NMI and RESET
//   from the prototype. The 6800 has no I/O space and no fetch signal.
// - Firmware: the SYSTEM PROM (82S115, 512 bytes; dumped by Eelco Vriezekolk, ztpe.nl, 2026),
//   roms/ep6800/6800_system.bin, runs on the 6800 at F800-F9FF while the firmware is mapped
//   (load_firmware; notes/emulators.md "The 6800 board's PROMs"). Its routines end with a jump
//   to FEF1 (debug SVC 1) or FEF0 (debug SVC 2); the board then waits for the next forced jump.
//   The FETCH PROM (roms/ep6800/6800_fetch.bin), indexed by opcode, gives each instruction's
//   cycle count (low nibble = 17 - cycles: how the board finds the next fetch) and flags.
//   While the firmware is mapped, two flags steer data cycles (inferred, notes/emulators.md):
//   bit 7 clear (PSHA, PULA) sends the stack cycle to 0000, the register block's A; bit 6
//   clear (LDA/STA indexed) sends the data cycle to the prototype in modes 1 and 2 (GET, PUT).
//   Without the PROM (-ep6800 hle), the routines are modelled at high level, from what TEKDOS
//   does around them (DEB INIT 195D-196E, DEB CNTL 15DE-160B and 1733, DSTAT 18E9-1937) and
//   from 070-2711 §1's walkthrough (steps 9-19 restore, 34-37 dump):
//     F9FA  restore: load A, B, X, SP, CC from 0000-0006, then debug SVC 1 (port F1)
//     F9FD  dump: store A, B, X, SP, CC at 0000-0006, then debug SVC 1
//     F9F4/F9F7  PUT/GET a prototype byte (tek8002_emboard.h), then debug SVC 2: TEKDOS's
//           kernel derives them from its table (24CB: F9 FF), as F800/F803 on the Z80 board
//   TEKDOS swaps 0000-0006 with its own buffer around each call, so the user's bytes there
//   are kept.
#ifndef TEK_TEK8002_EP6800_H
#define TEK_TEK8002_EP6800_H
#include "machine/tek8002_emboard.h"
#include "cpu/m6800/m6800.h"
#include <string>
#include <vector>
#include <cstdlib>

class tek8002_ep6800 : public tek8002_emboard, private m6800_cpu_device::bus_interface
{
public:
	static constexpr u16 FW_PUT = 0xf9f4, FW_GET = 0xf9f7, FW_RESTORE = 0xf9fa, FW_DUMP = 0xf9fd, SVC_BASE = 0xfef0;

	explicit tek8002_ep6800(slave_bus &bus) : tek8002_emboard(bus, 0xe2), cpu(*this) {}
	const char *name() const override { return "6800 Emulator Processor"; }
	double board_clock_hz() const override { return 1.0e6; }
	m6800_cpu_device cpu;
	// the SYSTEM and FETCH PROMs (512 bytes each) from DIR/6800_system.bin and 6800_fetch.bin;
	// without them, the high-level routines
	bool load_firmware(const std::string &dir, std::string &err);
	bool prom_firmware() const { return !m_sys.empty(); }

protected:
	// RESET sets the I mask and loads the PC; the other registers and CC bits stay (MC6800 data
	// sheet; the core's reset clears CC). Millennium's 6800 FETCH test (DE221) sets CC with TAP,
	// then steps a BMI with RESET + JUMP and expects it taken
	void cpu_reset() override { const u8 cc = cpu.cc(); m_unknown_op = false; cpu.reset(); cpu.set_cc(u8(cc | 0x10)); }
	void cpu_step() override { cpu.step(); }
	void cpu_jump(u16 target) override { cpu.clear_wait(); m_unknown_op = false; cpu.set_pc(target); }
	// An opcode the 6800 lacks reads 00 in the FETCH PROM, and the board halts the processor on
	// it: Millennium's 6800 DEBUG test (DE211, slave reset) runs CLI; JMP 0064 onto an 04 and
	// expects the halt interrupt with PC last 0064, and its FETCH test (DE221) ends each block with
	// an 04 and expects PC next FFFF after every unknown opcode, the bus idle while halted
	u16 fetch_address() const override { return m_unknown_op ? 0xffff : pc(); }
	u16 cpu_pc() const override { return cpu.pc(); }
	// one clock per bus cycle, for the RTPA's time counts (the 6800 runs at 1 MHz without waits)
	void bus_states(int &f, int &m, int &io) const override { f = m = io = 1; }
	bool cpu_halted() const override { return cpu.waiting() || m_unknown_op; }
	u64 cpu_cycles() const override { return cpu.total_cycles() + m_stretch; }
	// An access to the SVC window FEF0-FEF7 takes one clock more (inferred): Millennium's 6800
	// hardware analyzer test (D3A30) counts one clock more per loop than the instructions take,
	// in two loops that each read FE00 and write FEF7 (TRACE I/O, TRACE I/O READS). A stretch of
	// the whole I/O page would give two; the SVC decode, which must decide whether to divert the
	// cycle, is the one path the write takes and the read does not. A stretch of I/O-page reads
	// only, or writes only, fits the test as well (notes/rtpa.md)
	u64 m_stretch = 0;
	void svc_window_stretch(u16 a) { if ((a & 0xfff8) == 0xfef0) m_stretch++; }
	bool firmware(u16 entry) override;
	// control bit 2 does not gate the debug signals here: the diagnostics' 6800 DEBUG test
	// (DE211) runs the board with E2 <- 90 and expects breakpoints and single cycle
	bool debug_signals() const override { return true; }
	// the SVC page FE00-FEFF ("the 6800 microprocessor uses memory mapped I/O", 070-2349 §10) is the
	// board's I/O: the 8002 diagnostics' 6800 hardware analyzer test traces FE00 as I/O cycles
	int io_page() const override { return 0xfe; }
	// In mode 0 the board turns that page into I/O cycles on the 8002 bus, not program memory
	// (070-2349 §10: "the 6800 microprocessor uses memory mapped I/O, the memory region FE00
	// through FEFF is reserved"): D3A30's trace expects a read of FE00 to return the idle I/O
	// bus's 00 with four program memory boards fitted, where memory at FE00 holds other data
	static bool io_page_address(u16 a) { return (a >> 8) == 0xfe; }
	bool firmware_code(u16 pc) const override { return !m_sys.empty() && pc >= 0xf800 && pc <= 0xf9ff; }
	bool prom_byte(u16 a, u8 &d) const override { if (!firmware_code(a)) return false; d = m_sys[a - 0xf800]; return true; }
	// Bit 7: TEKDOS's debug control resets the board and resumes at PC last when it reads 0
	// (DEB CNTL 1550-155C, 1602-1607); what drives it is not known, so it reads 1. Bit 6: a
	// 6802 fitted: DSTAT names the processor "6802" when it is set, "6800" when not (DSTAT
	// 1951-1957; E2 is the "6800/6802 Emulator Processor", 070-2711 Appendix E). A 6800 here.
	u8 board_status() const override { return 0x80; }
	void target_lines(bool reset_edge, bool nmi_edge, bool irq) override;

private:
	u8 read(u16 a, bool opcode) override;
	void write(u16 a, u8 d) override;
	// SVCs only while the Debug module takes them, the firmware's too: the diagnostics' SVC
	// mask test (DE211 subtest 0E) masks them around a firmware call and expects no SVC
	bool svc_address(u16 a) const { return (a & 0xfff8) == SVC_BASE && m_bus.svcs_enabled(); }
	std::vector<u8> m_sys;                    // the SYSTEM PROM, at F800-F9FF while the firmware is mapped
	std::vector<u8> m_fetch;                  // the FETCH PROM: per-opcode cycle count and flags
	u8 m_op = 0;                              // the opcode of the instruction running
	// a firmware data cycle steered by the FETCH PROM: 0 none, 1 to 0000 (bit 7), 2 to the prototype (bit 6)
	int steer() const;
	// the instruction's own operand bytes are not steered, only its data cycles (FETCH PROM bit 4
	// marks the instructions with operands): Millennium's 6800 FETCH test (DE221) single-steps a
	// JSR extended, flagged to 0000 like PSHA and PULA, and expects it to reach its target
	bool operand(u16 a) const { return !m_fetch.empty() && !(m_fetch[m_op] & 0x10) && u16(a - m_op_pc) >= 1 && u16(a - m_op_pc) <= 2; }
	u16 m_op_pc = 0;
	bool m_unknown_op = false;               // halted on an opcode the FETCH PROM does not know
};
#endif
