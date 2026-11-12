// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Debug and Front Panel I/O module (see tek8002_debug.h).
#include "machine/tek8002_debug.h"
#include <cstdio>

void tek8002_debug::reset()
{
	m_control = 0xff; m_command = 0xff;
	m_jump_lo = m_jump_hi = 0xff;
	for (auto &b : m_bp) b = 0xff;
	m_pc_last = m_pc_next = 0;
	m_halt_cycles = 0; m_halt_reported = false;
	m_reset_pending = m_jump_pending = false;
	m_events = m_held = 0;
	m_frac = 0;
}

void tek8002_debug::write(u8 port, u8 d)
{
	switch (port) {
	case 0xf8: m_control = d; break;
	case 0xf9: {
		// forced reset and forced jump act on a 1 -> 0 transition (ztpe.nl Annex B, F9);
		// they are taken when the slave next runs (forced interrupt: no slave uses it)
		if ((m_command & 0x01) && !(d & 0x01)) m_reset_pending = true;
		if ((m_command & 0x04) && !(d & 0x04)) m_jump_pending = true;
		const bool was_masked = masked();
		m_command = d;
		if (was_masked && !masked() && m_held) { const u32 h = m_held; m_held = 0; raise(h); }
		break;
	}
	case 0xfa: m_jump_lo = d; break;
	case 0xfb: m_jump_hi = d; break;
	default:   m_bp[port - 0xfc] = d; break;
	}
}

u8 tek8002_debug::read(u8 port) const
{
	switch (port) {
	case 0xfc: return u8(~m_pc_last);
	case 0xfd: return u8(~m_pc_last >> 8);
	case 0xfe: return u8(~m_pc_next);
	case 0xff: return u8(~m_pc_next >> 8);
	default:   return 0xff;                // F8-FB are write-only
	}
}

void tek8002_debug::raise(u32 events)
{
	if (masked()) { m_held |= events; return; }
	for (int n = 16; n < 32; n++)
		if (events & (1u << n)) {
			if (trace) fprintf(stderr, "[debug: interrupt %d]\n", n);
			m_host.debug_interrupt(n);
		}
}

void tek8002_debug::master_halted(int cycles)
{
	if (m_halt_reported) return;
	if ((m_halt_cycles += cycles) < io_clock_cycles) return;
	m_halt_reported = true;               // the detector sets once per hand-over
	raise(1u << INT_SLAVE_HALTED);
}

void tek8002_debug::diag_switch() { raise(1u << INT_DIAG); }

void tek8002_debug::slave_access(u16 a, bool write)
{
	if (!debugging()) return;
	for (int n = 0; n < 2; n++) {
		const bool enabled = !(m_control & (1 << (2 * n + (write ? 1 : 0))));   // bits 0/1 BP1 read/write, 2/3 BP2
		if (enabled && a == breakpoint(n)) m_events |= 1u << (INT_BP1 + n);
	}
}

bool tek8002_debug::slave_out(u16 port)
{
	const u8 p = u8(port);
	if (p < 0xf0 || p > 0xf7 || !(m_command & 0x10)) return false;
	m_events |= 1u << (INT_SVC1 + (0xf7 - p));   // F7 = SVC 1 ... F0 = SVC 8
	return true;
}

void tek8002_debug::run_slave(tek8002_slave &s, int cycles2650, double master_hz)
{
	master_running();                      // a slave holds the bus: no "slave halted" from the detector
	if (m_host.master_interrupt_pending()) return;
	if (m_reset_pending) { m_reset_pending = false; s.reset(); }
	if (m_jump_pending) {
		m_jump_pending = false;
		if (trace) fprintf(stderr, "[debug: %s forced jump to %04x]\n", s.name(), jump_address());
		s.jump(jump_address());
	}
	m_frac += cycles2650 * (s.clock_hz() / master_hz);
	while (m_frac > 0) {
		const u64 c0 = s.cycles();
		const u16 pc = s.pc();
		m_events = 0;
		s.step();
		m_frac -= double(s.cycles() - c0);
		if (s.halted()) m_events |= 1u << INT_SLAVE_HALTED;
		if (debugging() && !(m_control & 0x10)) m_events |= 1u << INT_SINGLE_CYCLE;
		if (m_events) {
			m_pc_last = pc; m_pc_next = s.pc();
			// an SVC alone lets the slave continue where it stopped; anything else arms a
			// forced jump to the address the master sets
			const u32 svcs = 0xffu << INT_SVC1;
			if (m_events & ~svcs) m_jump_pending = true;
			if (trace) fprintf(stderr, "[debug: %s stops at %04x, next %04x]\n", s.name(), pc, m_pc_next);
			const u32 e = m_events;
			m_events = 0;
			raise(e);
			if (masked()) { m_jump_pending = false; if (s.halted()) break; continue; }   // held: the slave runs on
			break;
		}
		if (m_host.master_interrupt_pending()) break;
	}
	if (m_frac > 0) m_frac = 0;
}
