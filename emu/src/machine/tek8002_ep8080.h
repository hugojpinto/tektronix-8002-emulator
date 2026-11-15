// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 8080A/8085A Emulator Processor, as an 8080A (070-2702-01, the 8080A/8085A
// Assembler and Emulator Users Manual), port E1 (070-2711 Appendix E). It runs the user's
// program out of program memory as a slave of the System Processor (notes/emulators.md,
// "The 8080 board"); what all boards share is in tek8002_emboard.
//
// - Its two PROMs (read by Eelco Vriezekolk, ztpe.nl, 2026; roms/ep8080/):
//     P1, 160-0028-00, F800-F9FF: an Intel MDS-800 monitor interface (CI F803, RI F806,
//       CO F809, PO F80C, LO F80F, CSTS F812, IOCHK/IOSET F815/F818, MEMCK F81B, ...),
//       each call made through a TEKDOS SVC;
//     P2, 160-0027-00, FA00-FBFF: P1's SVC templates, routines for a RAM-resident program,
//       and the debug firmware TEKDOS calls through its table at FBEF: FBE4 PUT, FBE7 GET,
//       FBEA restore, FBED dump. The chip's data bit 0 is dead; the image is rebuilt by
//       tools/rebuild_8080_p2.py.
//   They answer at F800-FBFF during firmware calls and during mode-0 user runs, so that
//   programs can call the monitor (inferred: SVCs, which the monitor needs, are off in
//   modes 1 and 2, where F800-FBFF are the prototype's).
// - SVCs: OUT to F7 ... F0 followed by a NOP (070-2702 §11), while the Debug module takes
//   them (F9 bit 4), the firmware's debug SVCs 1 and 2 (OUT F1/F0) too: the diagnostics' SVC
//   mask test (DE211) expects none while they are masked.
// - HLT is the halt the Debug module detects (§12: GO "continues at the beginning of the HLT
//   instruction").
// - Firmware calls see program memory, where TEKDOS lends the register block (0000 L, 01 H,
//   02 E, 03 D, 04 C, 05 B, 06 A, 07 F, 08 scratch, 09/0A SP) and the GET/PUT block. In
//   modes 1 and 2 the firmware's LDAX D and STAX D (GET's and PUT's own accesses) reach the
//   prototype (inferred: GET and PUT must, and those are the only instructions they use for
//   it, as the 6800 board's FETCH PROM singles out its own).
// - Modes 1 and 2: memory, I/O, INTR (with an RST from the data bus) and RESET through the
//   probe (notes/prototype-link.md); the 8080 has no fetch pin of its own on the link (its
//   SYNC status marks M1, sent as fetches).
// - Clock: 3 MHz in mode 0 (070-2785, the RTPA users guide, Appendix A: "Mode 0 System Clock
//   ... 3 MHz"; its worked example measures 2728 us for 6404 clocks plus 1793 wait states).
// - Wait states: one per machine cycle in modes 0 and 1, none from prototype memory in mode 2
//   (070-2785 Tables A-1, A-4), as the Z80 board's two. Memory cycles only: the 8002
//   diagnostics' 8080 hardware analyzer test (D3A21, subtest 0D) counts 694 clocks for 550
//   states, 136 memory cycles and 16 I/O cycles, to the end of the fetch that breaks.
#ifndef TEK_TEK8002_EP8080_H
#define TEK_TEK8002_EP8080_H
#include "machine/tek8002_emboard.h"
#include "cpu/i8085/i8085.h"
#include <cstdlib>
#include <string>
#include <vector>

class tek8002_ep8080 : public tek8002_emboard, private i8085a_cpu_device::bus_interface
{
public:
	static constexpr u16 PROM_BASE = 0xf800, PROM_END = 0xfbff;

	explicit tek8002_ep8080(slave_bus &bus) : tek8002_emboard(bus, 0xe1), cpu(*this, true) {}
	const char *name() const override { return "8080A Emulator Processor"; }
	double board_clock_hz() const override { return 3.0e6; }
	i8085a_cpu_device cpu;
	// P1 and P2 (512 bytes each) from DIR/8080_p1_160-0028-00.bin and 8080_p2_160-0027-00.bin
	bool load_firmware(const std::string &dir, std::string &err);

protected:
	void cpu_reset() override { cpu.reset(); }
	void cpu_step() override { m_waits = 0; m_cyc += u64(cpu.run(1)) + u64(m_waits); }
	void cpu_jump(u16 target) override { cpu.set_pc(target); }
	u16 cpu_pc() const override { return cpu.pc(); }
	bool cpu_halted() const override { return cpu.halted(); }
	u64 cpu_cycles() const override { return m_cyc; }
	bool firmware(u16) override { return false; }   // the PROMs run on the 8080
	// control bit 2 does not gate the debug signals here: the diagnostics' 8080 DEBUG test
	// (DE111) runs the board with E1 <- 90 and expects breakpoints and single cycle
	bool debug_signals() const override { return true; }
	void bus_states(int &f, int &m, int &io) const override { f = 4 + 1; m = 3 + 1; io = 3; }
	bool firmware_code(u16 pc) const override { return !m_prom.empty() && pc >= PROM_BASE && pc <= PROM_END; }
	bool prom_byte(u16 a, u8 &d) const override { if (!firmware_code(a)) return false; d = m_prom[a - PROM_BASE]; return true; }
	u8 board_status() const override { return 0x00; }
	void target_lines(bool reset_edge, bool nmi_edge, bool irq) override;

private:
	u8 read(u16 a) override;
	u8 fetch(u16 a) override;
	void write(u16 a, u8 d) override;
	u8 io_read(u8 port) override;
	void io_write(u8 port, u8 d) override;
	u8 inta() override { return target->irq_vector(); }

	// an I/O cycle on the 8002 bus: the 8080 puts the port on A15-A8 as well as A7-A0 (8080A
	// data sheet), and the board drives A13 high, the line the 2650's E/NE (extended I/O) takes
	// on that bus, which its peripherals decode with the port (070-2711 §3, §5: "A13 is another
	// address range indicator"). Inferred from Millennium's 8080 hardware analyzer test (D3A21),
	// which traces IN 00 as address 2000 and OUT F7 as F7F7. The prototype sees the 8080's pins.
	static u16 bus_port(u8 port) { return u16(port * 0x101 | 0x2000); }
	bool prom_visible() const { return firmware_mapped() || (user_run() && emulation_mode() == 0); }
	// GET/PUT's own cycles: LDAX D or STAX D during a firmware call in modes 1/2
	bool to_prototype() const { return firmware_mapped() && emulation_mode() != 0 && (m_op == 0x1a || m_op == 0x12); }
	u8 mem_read(u16 a, bool fetch);
	std::vector<u8> m_prom;                  // P1 then P2: F800-FBFF
	int m_waits = 0;                         // this instruction's wait states: one per memory cycle
	void wait_state(u16 a) { if (!proto_mem(a) || emulation_mode() == 1) m_waits++; }
	u8 m_op = 0;                             // the opcode of the instruction running
	u64 m_cyc = 0;
};
#endif
