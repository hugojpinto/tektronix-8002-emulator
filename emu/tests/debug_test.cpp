// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Unit test of the Debug module (machine/tek8002_debug): breakpoints, single cycle, the
// interrupt mask, SVCs, forced reset and jump, PC last/next, as tek8002_debug.h states
// them, on a scripted slave. TEKDOS exercises the SVC and halt paths in tests/asm_link.sh;
// breakpoints and single cycle need an emulator board's DEBUG, so they are pinned here.
#include "machine/tek8002_debug.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

// a slave whose instructions are scripted: each is 3 bytes long and may read, write or OUT
struct op { int read = -1, write = -1, out = -1; bool halt = false; };

struct fake_bus : slave_bus {
	tek8002_debug *dbg = nullptr;
	u8 slave_read(u16 a, bool) override { dbg->slave_access(a, false); return 0; }
	void slave_write(u16 a, u8) override { dbg->slave_access(a, true); }
	u8 slave_in(u16) override { return 0xff; }
	void slave_out(u16 port, u8) override { dbg->slave_out(port); }
};

struct fake_slave : tek8002_slave {
	std::vector<op> prog;
	u16 m_pc = 0; bool m_halt = false; u64 m_cyc = 0; int resets = 0;
	explicit fake_slave(slave_bus &b) : tek8002_slave(b) {}
	const char *name() const override { return "test"; }
	double clock_hz() const override { return 1.25e6; }
	bool decodes(u8) const override { return false; }
	void command(u8, u8) override {}
	bool active() const override { return true; }
	void reset() override { m_pc = 0; m_halt = false; resets++; }
	void jump(u16 t) override { m_pc = t; m_halt = false; }
	void step() override {
		m_cyc += 4;
		if (m_halt) return;
		const op o = (m_pc / 3 < prog.size()) ? prog[m_pc / 3] : op();
		m_bus.slave_read(m_pc, true);                   // the fetch
		if (o.read >= 0) m_bus.slave_read(u16(o.read), false);
		if (o.write >= 0) m_bus.slave_write(u16(o.write), 0);
		if (o.out >= 0) m_bus.slave_out(u16(o.out), 0);
		if (o.halt) m_halt = true;
		else m_pc += 3;
	}
	u16 pc() const override { return m_pc; }
	bool halted() const override { return m_halt; }
	u64 cycles() const override { return m_cyc; }
};

struct fake_host : debug_host {
	std::vector<int> ints;
	void debug_interrupt(int n) override { ints.push_back(n); }
	bool master_interrupt_pending() const override { return false; }   // the master takes them at once
};

struct rig {
	fake_host host;
	tek8002_debug dbg{host};
	fake_bus bus;
	fake_slave s{bus};
	rig() { bus.dbg = &dbg; dbg.reset(); s.prog.resize(64); }
	void run(int cycles = 200) { dbg.run_slave(s, cycles, 1.25e6); }
	void bp(int n, u16 a) { dbg.write(u8(0xfc + 2 * n), u8(~a)); dbg.write(u8(0xfd + 2 * n), u8(~a >> 8)); }
	void jump_to(u16 a) { dbg.write(0xfa, u8(~a)); dbg.write(0xfb, u8(~a >> 8)); dbg.write(0xf9, 0xfb); dbg.write(0xf9, 0xff); }
	u16 pc_last() { return u16(~(dbg.read(0xfd) << 8 | dbg.read(0xfc))); }
	u16 pc_next() { return u16(~(dbg.read(0xff) << 8 | dbg.read(0xfe))); }
};

int main()
{
	{	// SVC: OUT F7 = SVC 1 (interrupt 16); the slave continues after it (no forced jump)
		rig r; r.s.prog[2].out = 0xf7;
		r.run();
		CHECK(r.host.ints.size() == 1 && r.host.ints[0] == 16);
		CHECK(r.pc_last() == 6 && r.pc_next() == 9);
		r.host.ints.clear(); r.s.prog[2].out = -1;
		r.run(12);
		CHECK(r.s.pc() > 9);                             // ran on from 9, not from the jump register
	}
	{	// SVC 8 = OUT F0 (interrupt 23); SVCs off with F9 bit 4 low
		rig r; r.s.prog[1].out = 0xf0;
		r.run();
		CHECK(r.host.ints.size() == 1 && r.host.ints[0] == 23);
		rig q; q.dbg.write(0xf9, 0xef); q.s.prog[1].out = 0xf0; q.s.prog[3].halt = true;
		q.run();
		CHECK(q.host.ints.size() == 1 && q.host.ints[0] == 27);   // only the halt
	}
	{	// breakpoint 1 on a data read, enabled by F8 bit 0 and debug enable (bit 5), both low
		rig r; r.bp(0, 0x1234); r.s.prog[4].read = 0x1234;
		r.run();
		CHECK(r.host.ints.empty() || r.host.ints[0] != 24);       // debugging off: no breakpoint
		rig q; q.bp(0, 0x1234); q.s.prog[4].read = 0x1234; q.dbg.write(0xf8, 0xde);
		q.run();
		CHECK(q.host.ints.size() == 1 && q.host.ints[0] == 24);
		CHECK(q.pc_last() == 12 && q.pc_next() == 15);
		// a breakpoint arms the forced jump: the slave continues where the master points it
		q.host.ints.clear(); q.s.prog[4].read = -1; q.dbg.write(0xf8, 0xff);
		q.dbg.write(0xfa, u8(~0x30)); q.dbg.write(0xfb, 0xff);
		q.run(4);
		CHECK(q.s.pc() == 0x33);
	}
	{	// breakpoint 2 on a write only: a read of the address does not trigger it; fetches count as reads
		rig r; r.bp(1, 0x2000); r.dbg.write(0xf8, 0xd7);         // bit 3 (BP2 write) and bit 5 low
		r.s.prog[1].read = 0x2000; r.s.prog[3].write = 0x2000;
		r.run();
		CHECK(r.host.ints.size() == 1 && r.host.ints[0] == 25 && r.pc_last() == 9);
		rig q; q.bp(0, 0x0006); q.dbg.write(0xf8, 0xde);         // BP1 read on the fetch of the third instruction
		q.run();
		CHECK(q.host.ints.size() == 1 && q.host.ints[0] == 24 && q.pc_last() == 6);
	}
	{	// single cycle: interrupt 26 after every instruction (F8 bits 4 and 5 low)
		rig r; r.dbg.write(0xf8, 0xcf);
		r.run();
		CHECK(r.host.ints.size() == 1 && r.host.ints[0] == 26 && r.pc_last() == 0 && r.pc_next() == 3);
		r.host.ints.clear(); r.dbg.write(0xfa, u8(~3)); r.dbg.write(0xfb, 0xff);
		r.run();
		CHECK(r.host.ints.size() == 1 && r.pc_last() == 3 && r.pc_next() == 6);
	}
	{	// the interrupt mask (F9 bit 3 low) holds debug interrupts; releasing it raises them
		rig r; r.dbg.write(0xf9, 0xf7); r.s.prog[1].out = 0xf7; r.s.prog[2].out = 0xf6;
		r.run(12);
		CHECK(r.host.ints.empty());
		r.dbg.write(0xf9, 0xff);
		CHECK(r.host.ints.size() == 2 && r.host.ints[0] == 16 && r.host.ints[1] == 17);
	}
	{	// forced reset and forced jump act on F9's 1 -> 0 edges, when the slave next runs
		rig r; r.s.m_pc = 30;
		r.dbg.write(0xf9, 0xfe);                           // forced reset
		CHECK(r.s.resets == 0);
		r.run(4);
		CHECK(r.s.resets == 1 && r.s.pc() == 3);
		r.dbg.write(0xf9, 0xfe);                           // no edge: nothing new
		r.run(4);
		CHECK(r.s.resets == 1);
		r.jump_to(0x40);
		r.run(4);
		CHECK(r.s.pc() == 0x43);
	}
	{	// no slave holding the bus: SLAVE HALTED after one I/O clock; the DIAG INTRPT switch
		rig r;
		r.dbg.master_halted(20); CHECK(r.host.ints.empty());
		r.dbg.master_halted(20); CHECK(r.host.ints.size() == 1 && r.host.ints[0] == 27);
		r.dbg.master_halted(100); CHECK(r.host.ints.size() == 1);   // once per hand-over
		r.dbg.diag_switch(); CHECK(r.host.ints.size() == 2 && r.host.ints[1] == 28);
	}
	printf("%s\n", failures ? "debug_test: FAILED" : "debug_test: all passed");
	return failures ? 1 : 0;
}
