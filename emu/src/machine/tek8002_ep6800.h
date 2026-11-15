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
	void cpu_reset() override { cpu.reset(); }
	void cpu_step() override { cpu.step(); }
	void cpu_jump(u16 target) override { cpu.clear_wait(); cpu.set_pc(target); }
	u16 cpu_pc() const override { return cpu.pc(); }
	bool cpu_halted() const override { return cpu.waiting(); }
	u64 cpu_cycles() const override { return cpu.total_cycles(); }
	bool firmware(u16 entry) override;
	// control bit 2 does not gate the debug signals here: the diagnostics' 6800 DEBUG test
	// (DE211) runs the board with E2 <- 90 and expects breakpoints and single cycle
	bool debug_signals() const override { return true; }
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
};
#endif
