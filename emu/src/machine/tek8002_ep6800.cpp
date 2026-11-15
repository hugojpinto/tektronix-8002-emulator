// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002 6800 Emulator Processor -- see tek8002_ep6800.h.
#include "machine/tek8002_ep6800.h"

#include <cstdio>

static bool load_82s115(const std::string &path, std::vector<u8> &out, std::string &err)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) { err = "missing 6800 board PROM: " + path; return false; }
	std::vector<u8> p(513);
	p.resize(fread(p.data(), 1, p.size(), f));
	fclose(f);
	if (p.size() != 512) { err = path + ": not a 512-byte 82S115 image"; return false; }
	out = p;
	return true;
}

bool tek8002_ep6800::load_firmware(const std::string &dir, std::string &err)
{
	std::vector<u8> sys, fetch;
	if (!load_82s115(dir + "/6800_system.bin", sys, err) || !load_82s115(dir + "/6800_fetch.bin", fetch, err)) return false;
	m_sys = sys; m_fetch = fetch;
	return true;
}

int tek8002_ep6800::steer() const
{
	if (m_fetch.empty() || !firmware_mapped()) return 0;
	const u8 f = m_fetch[m_op];
	if (!(f & 0x80)) return 1;
	if (!(f & 0x40) && emulation_mode() != 0) return 2;
	return 0;
}

// The SVC window answers while the Debug module takes SVCs (F9 bit 4), for the firmware's
// debug SVCs too: the diagnostics' SVC mask test (DE211 subtest 0E) expects none while they
// are masked. Otherwise FEF0-FEF7 are memory (the prototype's in modes 1 and 2). The firmware's jump to FEF1/FEF0 stops the board at that fetch
// until TEKDOS's next forced jump, so whatever answers there is never executed: modelled
// as a NOP. The 6800 has no instruction-fetch pin, so the prototype sees reads and writes only.
u8 tek8002_ep6800::read(u16 a, bool opcode)
{
	if (svc_address(a)) { svc(a); return opcode && firmware_mapped() ? 0x01 : m_bus.slave_read(a, false); }
	u8 d;
	// the SYSTEM PROM: F800-F9FF while the firmware is mapped (its entries are F9F4-F9FD;
	// whether it is mirrored elsewhere is not known)
	if (!m_sys.empty() && firmware_mapped() && a >= 0xf800 && a <= 0xf9ff) d = m_sys[a & 0x1ff];
	else if (!opcode && steer() == 1) d = m_bus.slave_read(0x0000, false);
	else if (!opcode && steer() == 2) d = target->mem_read(a);
	else if (proto_mem(a)) d = proto_read(a, false);
	else d = m_bus.slave_read(a, opcode);
	if (opcode) m_op = d;
	return d;
}

void tek8002_ep6800::write(u16 a, u8 d)
{
	if (svc_address(a)) { svc(a, a, d); return; }   // a store: the RTPA sees it (the 6800 hardware analyzer test arms on STAA FEF7)
	if (steer() == 1) { m_bus.slave_write(0x0000, d); return; }
	if (steer() == 2) { target->mem_write(a, d); return; }
	if (proto_mem(a)) { proto_write(a, d); return; }
	m_bus.slave_write(a, d);
}

void tek8002_ep6800::target_lines(bool reset_edge, bool nmi_edge, bool irq)
{
	if (reset_edge) cpu.reset();
	if (nmi_edge) { cpu.set_input_line(INPUT_LINE_NMI, ASSERT_LINE); cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
	cpu.set_input_line(M6800_IRQ_LINE, irq ? ASSERT_LINE : CLEAR_LINE);
}

// The register block at 0000-0006: A, B, X (high, low), SP (high, low), CC (DSTAT 18E9-1937)
bool tek8002_ep6800::firmware(u16 entry)
{
	if (!m_sys.empty()) return false;        // the real PROM runs on the 6800
	if (entry == FW_DUMP) {
		const u8 cc = cpu.cc();
		put(0, cpu.a()); put(1, cpu.b());
		put(2, u8(cpu.x() >> 8)); put(3, u8(cpu.x()));
		put(4, u8(cpu.sp() >> 8)); put(5, u8(cpu.sp()));
		put(6, cc);
		dump_block("dump", entry, 7);
	} else if (entry == FW_RESTORE) {
		dump_block("restore", entry, 7);
		cpu.set_a(mem(0)); cpu.set_b(mem(1));
		cpu.set_x(u16(mem(2) << 8 | mem(3)));
		cpu.set_sp(u16(mem(4) << 8 | mem(5)));
		cpu.set_cc(mem(6));
	} else if (entry == FW_PUT || entry == FW_GET) {
		get_put(entry == FW_PUT);
		return true;
	} else return false;
	svc(1);                                  // port F1: the debug SVC 1
	return true;
}
