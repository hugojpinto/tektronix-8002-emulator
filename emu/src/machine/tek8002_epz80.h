// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 Z80 Emulator Processor: a Z80 at 4 MHz (1978 catalogue) that runs the user's
// program out of program memory as a slave of the System Processor (notes/emulators.md).
// Control port E3 (TEKDOS's emulator table, 2032/2033, on its Z80 discs); what all boards
// share is in tek8002_emboard.
//
// - SVCs: OUT to F7 ... F0 (TEKDOS's Z80 DEMO%S: "OUT (0F7H),A ... EXIT FROM USER
//   PROGRAM"), as on the Assembler Processor.
// - HALT is the halt the Debug module detects.
// - Wait states (070-3665 §2, "8001/8002A Systems Jumper Configurations"): "Two wait states
//   are inserted each time the program memory is accessed, or when running in emulation
//   Mode 1. No wait states are inserted in Mode 2" (except in the debug and memswitch jump
//   sequences, which run as firmware here).
// - Modes 1 and 2: memory (with M1 fetches), I/O, INT (with its acknowledge cycle), NMI,
//   RESET and WAIT through the probe, over the same link the 8550 emulator's Z80 board uses
//   (notes/prototype-link.md).
// - Firmware (not preserved), at high level, entries from TEKDOS's Z80 DEB INIT (1958-1969):
//     F806  restore: load the registers from the block at 0000-001B, then debug SVC 1
//     F809  dump: store them there, then debug SVC 1
//     F800/F803  PUT/GET a prototype byte (tek8002_emboard.h), then debug SVC 2
//   The block, from TEKDOS 3.1's Z80 DSTAT (and the swap of 1C bytes in its DEB CONTROL,
//   1702): 1 IM (bits 1-0), IFF1 (bit 6), IFF2 (bit 7); 2 R; 3 I; 4-B L' H' E' D' C' B' A'
//   F'; C-D IY; E-F IX; 10-17 L H E D C B A F; 18-19 SP (low byte first). TEKDOS fills byte
//   1 itself from the board's status after a break, so the dump leaves it; the restore
//   takes IM and the IFFs from it. Bytes 0, 1A and 1B are not shown by DSTAT; the model
//   leaves them alone. The same entries serve the 8550's Z80 board, whose DOS/50 keeps IM
//   in byte 0 and IFF1/IFF2 in bits 7/6 of byte 1: a different firmware version.
// - Forced interrupt (F9 bit 1): TEKDOS adds it to the forced jump to reach a halted Z80
//   (DEB INIT 1AC5-1AD4), as DOS/50 does with the NMI; here the forced jump itself ends the
//   halt.
#ifndef TEK_TEK8002_EPZ80_H
#define TEK_TEK8002_EPZ80_H
#include "machine/tek8002_emboard.h"
extern "C" {
#include "cpu/z80/superzazu/z80.h"
}

class tek8002_epz80 : public tek8002_emboard
{
public:
	static constexpr u16 FW_PUT = 0xf800, FW_GET = 0xf803, FW_RESTORE = 0xf806, FW_DUMP = 0xf809;

	explicit tek8002_epz80(slave_bus &bus);
	const char *name() const override { return "Z80 Emulator Processor"; }
	double board_clock_hz() const override { return 4.0e6; }   // J1 at 4 MHz (070-3665 §2: 2 or 4 MHz; 4 MHz at most in an 8002A)
	z80 cpu{};

protected:
	void cpu_reset() override;
	void cpu_step() override;
	void cpu_jump(u16 target) override { cpu.halted = false; cpu.pc = target; }
	u16 cpu_pc() const override { return cpu.pc; }
	bool cpu_halted() const override { return cpu.halted; }
	u64 cpu_cycles() const override { return cpu.cyc; }
	bool firmware(u16 entry) override;
	// IFF2 (bit 7), IFF1 (bit 6) and the interrupt mode (bits 1-0), which software cannot
	// read on a Z80: TEKDOS stores this byte into the register block's byte 1 after each break,
	// and DSTAT prints IM, IFF1 and IFF2 from it (DEB INIT 1ADA-1AE0, DSTAT 1983-19BC). The
	// board must follow EI, DI, IM and interrupts to know them (inferred: how is not known).
	void target_lines(bool reset_edge, bool nmi_edge, bool irq) override;
	void forced_interrupt() override { z80_gen_nmi(&cpu); }   // the Debug module's: NMI (DE310 subtest 3)
	u8 board_status() const override { return u8((cpu.iff2 ? 0x80 : 0) | (cpu.iff1 ? 0x40 : 0) | (cpu.interrupt_mode & 3)); }

private:
	void wait_states(u16 a) { if (!proto_mem(a) || emulation_mode() == 1) cpu.cyc += 2; }
	static u8 mem_rd(void *u, u16 a);
	static void mem_wr(void *u, u16 a, u8 d);
	static u8 port_in(z80 *z, u16 port);
	static void port_out(z80 *z, u16 port, u8 d);
};
#endif
