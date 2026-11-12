// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002A Flexible Disc Unit's drives (118-0195-00), as the controller sees them through
// fdc_drive_bus. Each drive holds hard-sectored, single-density tracks as bit cells, and
// converts them to and from the 315,392-byte images (notes/fdc.md "Stage C").
//
// - 360 rpm and 32 sector holes, with the index hole between sectors 31 and 0 (070-2587 §1).
//   A track is 32 × 1302 cells of 4 µs (Fig. 1-7), so a turn is 166.66 ms.
// - A cell is one FM bit cell. The drive's data separator (SEP CLK, SEP DATA) gives the
//   controller one Data Ready per cell, and the data bit with it. FM clocks are not kept:
//   this format has no missing-clock marks (the sync byte FB is found by its bit pattern,
//   RD1 at 14B).
// - A sector (Fig. 1-5) starts at its sector pulse: 128 cells of 0 (the 8 the write routine
//   spends before WRITE GATE and the preamble of 120), sync FB, track, sector, 128 data
//   bytes, CRC high and low, then 0s to the next pulse. Bits go MSB first (RD1-RD3 shift
//   them in at the LSB). The CRC is CCITT (x^16 + x^12 + x^5 + 1) from 0 over track,
//   sector and data (§3 "CRC Generation", ICRC at 1A3).
// - Physical sector k (counting from the index) holds logical sector ((k + 1) × step) mod
//   32, step = the interleave input: the controller zeroes its sector count R7 at INDEX and
//   adds INL at every SECTOR pulse (02B, 03F-042), and reads or writes when R7 equals the
//   sector asked for (04F).
#ifndef TEK_TEK8002_FDD_H
#define TEK_TEK8002_FDD_H
#include "machine/tek8002_fdc.h"
#include <string>
#include <vector>

class tek8002_fdd : public fdc_drive_bus
{
public:
	static constexpr int DRIVES = 4, TRACKS = 77, SECTORS = 32, SECTOR_BYTES = 128;
	static constexpr int IMAGE_BYTES = TRACKS * SECTORS * SECTOR_BYTES;
	static constexpr int CELL_NS = 4000, SECTOR_CELLS = 1302, TRACK_CELLS = SECTORS * SECTOR_CELLS;
	static constexpr int LEADER_CELLS = 128;
	static constexpr u64 PULSE_NS = 500'000;  // sector and index pulse width (not given in 070-2587; any
	                                          // width under a sector's trailer works the same)

	// images (logical sector order) <-> tracks
	void load(int drive, const std::vector<u8> &image, bool writable, int step);
	bool unload(int drive, std::vector<u8> &image, std::string &problems) const;   // decode every sector
	void remove(int drive);
	bool present(int drive) const { return m_d[drive].present; }
	static u16 crc(const u8 *p, int n, u16 c = 0);

	// fdc_drive_bus
	bool track00() override { return sel() && sel()->track == 0; }
	bool index() override;
	bool sector() override;
	bool ready() override { return sel() != nullptr; }
	bool write_protect() override { return sel() && !sel()->writable; }
	bool data_ready() override;
	bool data_bit() override;
	void data_taken() override { m_taken = m_t / CELL_NS; }
	void select(int d) override { m_sel = d & 3; }
	void step(bool in) override;
	void head_load(bool on) override { m_loaded = on; }
	void write_gate(bool on) override { m_wgate = on; }
	void write_data(bool) override {}
	void elapse(u64 ns) override { m_t += ns; }

	int head_track(int d) const { return m_d[d].track; }
	std::vector<u8> &track_cells(int d, int t) { return m_d[d & 3].cells[t]; }   // tests and debugging
	int selected() const { return m_sel; }
	bool head_loaded() const { return m_loaded; }

private:
	struct drive_t {
		bool present = false, writable = false;
		int track = 0;
		std::vector<std::vector<u8>> cells;   // [track][cell] = data bit
	};
	drive_t m_d[DRIVES];
	int m_sel = 0;
	bool m_loaded = false, m_wgate = false;
	u64 m_t = 0, m_taken = 0;

	drive_t *sel() { return m_d[m_sel].present ? &m_d[m_sel] : nullptr; }
	int cell() const { return int((m_t / CELL_NS) % TRACK_CELLS); }
};
#endif
