// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A System Communications module (670-5219; 070-2711 section 5): three
// RS-232 ports on MC6850 ACIAs, and the memory map.
//
//   port   ACIA        device (Table 5-1)              interrupt (Table 3-1)
//   CA/CB  modem       J101  REMI / REMO                6  System Communications 1
//   CC/CD  printer     J102  LPT1 (output only)         7  System Communications 2
//   CE/CF  paper tape  J103  PPTR / PPTP                8  System Communications 3
// Even ports are an ACIA's control/status register, odd ports its data (Table 5-4).
// The manual numbers the module's ports F9-FF (Tables 5-2, 5-3); TEKDOS 3.3 reaches them
// at C9-CF (it master-resets the ACIAs with 03 at CA, CC and CE, 1D18-1D1E, and loads the
// map through C9): the low nibble is the same. Baud rates are jumpers (110-2400 baud,
// separate for transmit and receive on the modem port); modelled at 2400.
//
// Memory map (§5 "Memory Mapping Functions"): 512 x 1 bit, one bit per 128-byte block of
// the emulator processor's 64K (A15-A7), telling whether an access goes to program memory
// or the prototype (UMAP). The System Processor reads and writes it at C9 one bit at a
// time (D0) through an address counter that advances after every access. TEKDOS writes
// 02 and then 512 data bytes; the model takes a write with D1 set as the counter reset
// (*inferred* from that sequence; the manual's RESET condition is unclear in the OCR).
// Nothing uses the map yet: there is no prototype.
#ifndef TEK_TEK8002_SYSCOMM_H
#define TEK_TEK8002_SYSCOMM_H
#include "emu.h"
#include "machine/mc6850.h"

class tek8002_syscomm
{
public:
	enum { MODEM, PRINTER, TAPE, PORTS };
	static constexpr int FIRST_INT = 6;      // modem 6, printer 7, paper tape 8

	tek8002_syscomm();
	bool decodes(u8 port) const { return port >= 0xc9 && port <= 0xcf; }
	u8 read(u8 port);
	void write(u8 port, u8 d);
	void tick(int cycles) { for (auto &a : acia) a.tick(cycles); }
	bool irq(int n) const { return acia[n].irq(); }

	mc6850 acia[PORTS];
	bool map_bit(u16 a) const { return m_map[a >> 7]; }   // the bit for an emulator address

private:
	bool m_map[512] = {};
	int m_counter = 0;
};
#endif
