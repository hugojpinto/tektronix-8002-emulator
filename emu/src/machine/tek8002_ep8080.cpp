// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 8080A Emulator Processor -- see tek8002_ep8080.h.
#include "machine/tek8002_ep8080.h"
#include <cstdio>

bool tek8002_ep8080::load_firmware(const std::string &dir, std::string &err)
{
	std::vector<u8> all;
	for (const char *name : { "8080_p1_160-0028-00.bin", "8080_p2_160-0027-00.bin" }) {
		const std::string path = dir + "/" + name;
		FILE *f = fopen(path.c_str(), "rb");
		if (!f) { err = "missing 8080 board PROM: " + path; return false; }
		std::vector<u8> p(513);
		p.resize(fread(p.data(), 1, p.size(), f));
		fclose(f);
		if (p.size() != 512) { err = path + ": not a 512-byte PROM image"; return false; }
		all.insert(all.end(), p.begin(), p.end());
	}
	m_prom = all;
	return true;
}

u8 tek8002_ep8080::mem_read(u16 a, bool fetch)
{
	if (!m_prom.empty() && a >= PROM_BASE && a <= PROM_END && prom_visible()) return m_prom[a - PROM_BASE];
	if (!fetch && to_prototype()) { m_bus.slave_watch(a, false); return target->mem_read(a); }
	if (proto_mem(a)) return proto_read(a, fetch);
	return m_bus.slave_read(a, fetch);
}

u8 tek8002_ep8080::read(u16 a) { return mem_read(a, false); }

u8 tek8002_ep8080::fetch(u16 a)
{
	// the core reads only opcodes through fetch() (operands through read()): the steering
	// needs the opcode of the instruction running
	m_op = mem_read(a, true);
	return m_op;
}

void tek8002_ep8080::write(u16 a, u8 d)
{
	if (!m_prom.empty() && a >= PROM_BASE && a <= PROM_END && prom_visible()) return;   // a PROM
	if (to_prototype()) { m_bus.slave_watch(a, true); target->mem_write(a, d); return; }
	if (proto_mem(a)) { proto_write(a, d); return; }
	m_bus.slave_write(a, d);
}

// I/O: the SVC ports while the Debug module takes them, and always for the firmware's debug
// SVCs; the rest is the prototype's in modes 1/2 and nobody's in mode 0
u8 tek8002_ep8080::io_read(u8 port)
{
	if (proto_io()) return proto_in(port);
	return m_bus.slave_in(port);
}

void tek8002_ep8080::io_write(u8 port, u8 d)
{
	// SVCs only while the Debug module takes them, the firmware's too (the diagnostics' SVC
	// mask test, DE211 subtest 0E, masks them around a firmware call; GET and PUT in modes 1
	// and 2 still work: TEKDOS enables SVCs for the firmware's runs)
	if ((port & 0xf8) == 0xf0 && m_bus.svcs_enabled()) svc(port);
	else if (proto_io()) proto_out(port, d);
	else m_bus.slave_out(port, d);
}

void tek8002_ep8080::target_lines(bool reset_edge, bool, bool irq)
{
	if (reset_edge) cpu.reset();
	cpu.set_input_line(I8085_INTR_LINE, irq ? ASSERT_LINE : CLEAR_LINE);   // the 8080 has no NMI
}
