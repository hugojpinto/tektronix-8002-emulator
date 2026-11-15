// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 Z80 Emulator Processor -- see tek8002_epz80.h.
#include "machine/tek8002_epz80.h"

#include <cstdio>

tek8002_epz80::tek8002_epz80(slave_bus &bus) : tek8002_emboard(bus, 0xe3) { cpu_reset(); }

bool tek8002_epz80::load_firmware(const std::string &dir, std::string &err)
{
	const std::string path = dir + "/z80_firmware.bin";
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) { err = "missing Z80 board firmware: " + path + " (tools/rebuild_z80_firmware.py makes it; -epz80 hle runs without)"; return false; }
	std::vector<u8> p(1025);
	p.resize(fread(p.data(), 1, p.size(), f));
	fclose(f);
	if (p.size() != 1024) { err = path + ": not a 1K PROM image"; return false; }
	m_prom = p;
	return true;
}

void tek8002_epz80::cpu_reset()
{
	const unsigned long cyc = cpu.cyc;
	z80_init(&cpu);
	cpu.cyc = cyc;                           // its clock keeps counting
	cpu.read_byte = &mem_rd;
	cpu.write_byte = &mem_wr;
	cpu.port_in = &port_in;
	cpu.port_out = &port_out;
	cpu.userdata = this;
}

u8 tek8002_epz80::mem_rd(void *u, u16 a)
{
	tek8002_epz80 &s = *static_cast<tek8002_epz80 *>(u);
	s.wait_states(a);
	if (s.prom_at(a)) return s.m_prom[a - 0xf800];
	if (s.to_prototype(a)) { s.m_bus.slave_watch(a, false); return s.target->mem_read(a); }
	if (s.proto_mem(a)) return s.proto_read(a, s.cpu.m1);
	return s.m_bus.slave_read(a, s.cpu.m1);
}

void tek8002_epz80::mem_wr(void *u, u16 a, u8 d)
{
	tek8002_epz80 &s = *static_cast<tek8002_epz80 *>(u);
	s.wait_states(a);
	if (s.prom_at(a)) return;
	if (s.to_prototype(a)) { s.m_bus.slave_watch(a, true); s.target->mem_write(a, d); return; }
	if (s.proto_mem(a)) s.proto_write(a, d);
	else s.m_bus.slave_write(a, d);
}

// I/O: the SVC ports while the Debug module takes them (TEKDOS disables SVCs in modes 1 and
// 2); the rest is the prototype's in modes 1/2 and nobody's in mode 0
u8 tek8002_epz80::port_in(z80 *z, u16 port)
{
	tek8002_epz80 &s = *static_cast<tek8002_epz80 *>(z->userdata);
	if (s.proto_io()) return s.target->io_read(port);
	return s.m_bus.slave_in(port);
}

void tek8002_epz80::port_out(z80 *z, u16 port, u8 d)
{
	tek8002_epz80 &s = *static_cast<tek8002_epz80 *>(z->userdata);
	if ((port & 0xf8) == 0xf0 && s.m_bus.svcs_enabled()) s.svc(port);
	else if (s.proto_io()) s.target->io_write(port, d);
	else s.m_bus.slave_out(port, d);
}

void tek8002_epz80::cpu_step()
{
	z80_step(&cpu);
	if (proto_io()) cpu.cyc += target->take_wait();   // the prototype's WAIT states (link v3)
}

void tek8002_epz80::forced_interrupt()
{
	cpu.nmi_pending = 0; cpu.halted = false; cpu.iff1 = 0;
	cpu.r = u8((cpu.r & 0x80) | ((cpu.r + 1) & 0x7f));
	cpu.cyc += 11;
	cpu.sp = u16(cpu.sp - 2);                // as the core's NMI: push PC, go to 0066
	mem_wr(this, cpu.sp, u8(cpu.pc));
	mem_wr(this, u16(cpu.sp + 1), u8(cpu.pc >> 8));
	cpu.pc = 0x66;
}

void tek8002_epz80::target_lines(bool reset_edge, bool nmi_edge, bool irq)
{
	if (reset_edge) cpu_reset();
	if (nmi_edge) z80_gen_nmi(&cpu);
	if (irq && cpu.iff1) z80_gen_int(&cpu, target->irq_vector());   // the INTA cycle reads the data bus
}

bool tek8002_epz80::firmware(u16 entry)
{
	if (!m_prom.empty()) return false;   // the PROM runs on the Z80
	z80 &z = cpu;
	// as the PROM does (notes/emulators.md): R as the user left it (the jammed JP that brought
	// the board here counted one M1), SP less what block byte 1A says (2 after TEKDOS's NMI),
	// and bytes 0 and 1A cleared
	auto r_less = [](u8 r, int n) { return u8((r & 0x80) | ((r - n) & 0x7f)); };
	if (entry == FW_DUMP) {
		z.sp = u16(z.sp + mem(0x1a)); put(0x1a, 0); put(0, 0);
		put(2, r_less(z.r, 1)); put(3, z.i);
		put(4, z.l_); put(5, z.h_); put(6, z.e_); put(7, z.d_); put(8, z.c_); put(9, z.b_); put(10, z.a_); put(11, z.f_);
		put(12, u8(z.iy)); put(13, u8(z.iy >> 8)); put(14, u8(z.ix)); put(15, u8(z.ix >> 8));
		put(16, z.l); put(17, z.h); put(18, z.e); put(19, z.d); put(20, z.c); put(21, z.b); put(22, z.a);
		put(23, u8((z.sf ? 0x80 : 0) | (z.zf ? 0x40 : 0) | (z.yf ? 0x20 : 0) | (z.hf ? 0x10 : 0) |
		           (z.xf ? 0x08 : 0) | (z.pf ? 0x04 : 0) | (z.nf ? 0x02 : 0) | (z.cf ? 0x01 : 0)));
		put(24, u8(z.sp)); put(25, u8(z.sp >> 8));
		dump_block("dump", entry, 0x1c);
	} else if (entry == FW_RESTORE) {
		dump_block("restore", entry, 0x1c);
		const u8 b1 = mem(1);
		z.interrupt_mode = b1 & 3;
		if (mem(0)) z.iff1 = z.iff2 = b1 & 0x40;   // EI or DI, only when byte 0 asks (the PROM's)
		z.r = r_less(mem(2), 2); z.i = mem(3);   // the PROM's: the jammed JP to the user adds one
		z.l_ = mem(4); z.h_ = mem(5); z.e_ = mem(6); z.d_ = mem(7); z.c_ = mem(8); z.b_ = mem(9); z.a_ = mem(10); z.f_ = mem(11);
		z.iy = u16(mem(12) | mem(13) << 8); z.ix = u16(mem(14) | mem(15) << 8);
		z.l = mem(16); z.h = mem(17); z.e = mem(18); z.d = mem(19); z.c = mem(20); z.b = mem(21); z.a = mem(22);
		const u8 f = mem(23);
		z.sf = f & 0x80; z.zf = f & 0x40; z.yf = f & 0x20; z.hf = f & 0x10; z.xf = f & 0x08; z.pf = f & 0x04; z.nf = f & 0x02; z.cf = f & 0x01;
		z.sp = u16(mem(24) | mem(25) << 8);
		z.halted = false;
	} else if (entry == FW_PUT || entry == FW_GET) {
		get_put(entry == FW_PUT);
		return true;
	} else return false;
	svc(1);                                  // port F1: the debug SVC 1
	return true;
}
