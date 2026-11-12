// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// The 8002A Flexible Disc Unit's drives -- see tek8002_fdd.h.
#include "machine/tek8002_fdd.h"
#include <cstdio>

u16 tek8002_fdd::crc(const u8 *p, int n, u16 c)
{
	for (int i = 0; i < n; i++) {
		c ^= u16(p[i] << 8);
		for (int b = 0; b < 8; b++) c = (c & 0x8000) ? u16((c << 1) ^ 0x1021) : u16(c << 1);
	}
	return c;
}

void tek8002_fdd::load(int d, const std::vector<u8> &image, bool writable, int step)
{
	drive_t &dr = m_d[d & 3];
	dr.present = true; dr.writable = writable; dr.written = false;
	dr.slots.assign(TRACKS, std::vector<u8>(TRACK_SLOTS, 0));
	for (int t = 0; t < TRACKS; t++) {
		u8 *slot = dr.slots[t].data();
		for (int i = 0; i < TRACK_CELLS; i++) slot[2 * i] = 1;          // a clock in every cell
		for (int k = 0; k < SECTORS; k++) {
			const int s = ((k + 1) * step) & 31;
			u8 rec[1 + 2 + SECTOR_BYTES + 2];
			rec[0] = 0xfb; rec[1] = u8(t); rec[2] = u8(s);
			for (int i = 0; i < SECTOR_BYTES; i++) rec[3 + i] = image[(size_t(t) * SECTORS + s) * SECTOR_BYTES + i];
			const u16 c = crc(rec + 1, 2 + SECTOR_BYTES);
			rec[3 + SECTOR_BYTES] = u8(c >> 8); rec[4 + SECTOR_BYTES] = u8(c);
			u8 *data = slot + 2 * (size_t(k) * SECTOR_CELLS + LEADER_CELLS) + 1;
			for (size_t i = 0; i < sizeof rec; i++)
				for (int b = 7; b >= 0; b--, data += 2) *data = (rec[i] >> b) & 1;
		}
	}
}

void tek8002_fdd::load_blank(int d, bool writable)
{
	drive_t &dr = m_d[d & 3];
	dr.present = true; dr.writable = writable; dr.written = false;
	dr.slots.assign(TRACKS, std::vector<u8>(TRACK_SLOTS, 0));
}

void tek8002_fdd::remove(int d) { m_d[d & 3] = drive_t(); }

// One hole's record (track, sector, data, CRC) through the separator rule of reading: an empty
// slot is a data slot. Returns false when there is no sync byte.
bool tek8002_fdd::read_hole(int d, int t, int k, u8 *rec) const
{
	const u8 *s = m_d[d & 3].slots[t].data();
	const int p0 = 2 * k * SECTOR_CELLS, p1 = p0 + 2 * SECTOR_CELLS;
	std::vector<u8> bits;
	int parity = 1; u8 prev = 0;
	for (int p = p0; p < p1; p++) {
		const u8 v = s[p];
		if (!v) parity = p & 1;
		else if ((p & 1) != parity) bits.push_back(prev);
		prev = v;
	}
	size_t i = 0; u8 sr = 0;
	for (; i < bits.size(); i++) { sr = u8(sr << 1 | bits[i]); if (sr == 0xfb) break; }
	if (sr != 0xfb || i + 1 + 8 * HOLE_RECORD > bits.size()) return false;
	for (int n = 0; n < HOLE_RECORD; n++) { u8 v = 0; for (int b = 0; b < 8; b++) v = u8(v << 1 | bits[++i]); rec[n] = v; }
	return true;
}

// Every sector of every track, by its own header: what the controller wrote is what counts.
bool tek8002_fdd::unload(int d, std::vector<u8> &image, std::string &problems) const
{
	if (!m_d[d & 3].present) return false;
	image.assign(IMAGE_BYTES, 0);
	std::vector<bool> seen(size_t(TRACKS) * SECTORS, false);
	char msg[96];
	for (int t = 0; t < TRACKS; t++)
		for (int k = 0; k < SECTORS; k++) {
			u8 rec[HOLE_RECORD];
			if (!read_hole(d, t, k, rec)) { snprintf(msg, sizeof msg, "track %d hole %d: no sync\n", t, k); problems += msg; continue; }
			if (crc(rec, sizeof rec) != 0) { snprintf(msg, sizeof msg, "track %d hole %d: CRC error\n", t, k); problems += msg; }
			if (rec[0] != t || rec[1] >= SECTORS) { snprintf(msg, sizeof msg, "track %d hole %d: header says track %d sector %d\n", t, k, rec[0], rec[1]); problems += msg; continue; }
			const size_t at = size_t(t) * SECTORS + rec[1];
			if (seen[at]) { snprintf(msg, sizeof msg, "track %d: sector %d twice\n", t, rec[1]); problems += msg; }
			seen[at] = true;
			for (int n = 0; n < SECTOR_BYTES; n++) image[at * SECTOR_BYTES + n] = rec[2 + n];
		}
	return problems.empty();
}

bool tek8002_fdd::index()
{
	if (!sel()) return false;
	const u64 at = u64(31 * SECTOR_CELLS + SECTOR_CELLS / 2) * CELL_NS, rev = u64(TRACK_CELLS) * CELL_NS;
	const u64 p = m_t % rev;
	return p >= at && p < at + PULSE_NS;
}

bool tek8002_fdd::sector()
{
	return sel() && (m_t % (u64(SECTOR_CELLS) * CELL_NS)) < PULSE_NS;
}

void tek8002_fdd::elapse(u64 ns)
{
	m_t += ns;
	const u64 now = m_t / SLOT_NS;
	drive_t *dr = sel();
	const u8 *track = dr && m_loaded && !m_wgate ? dr->slots[dr->track].data() : nullptr;
	for (; m_slot < now; m_slot++) {
		const u8 v = track ? track[m_slot % TRACK_SLOTS] : 0;
		if (!v) m_data_parity = int(m_slot & 1);
		else if (int(m_slot & 1) != m_data_parity) { m_cells++; m_bit = m_prev; }   // SEP CLK: the cell it ends
		m_prev = v;
	}
}

u64 tek8002_fdd::write_slot() const
{
	return m_wstart_slot + (m_t - m_wstart_t + SLOT_NS / 2) / SLOT_NS;
}

void tek8002_fdd::erase_to(u64 abs_slot)
{
	drive_t *dr = sel();
	if (!dr || !dr->writable || !m_loaded) { m_wnext = abs_slot; return; }
	if (abs_slot > m_wnext + TRACK_SLOTS) m_wnext = abs_slot - TRACK_SLOTS;
	u8 *track = dr->slots[dr->track].data();
	for (; m_wnext < abs_slot; m_wnext++) track[m_wnext % TRACK_SLOTS] = 0;
	dr->written = true;
}

void tek8002_fdd::write_gate(bool on)
{
	if (on && !m_wgate) { m_wstart_t = m_t; m_wstart_slot = m_wnext = m_t / SLOT_NS; }
	else if (!on && m_wgate) erase_to(write_slot());
	m_wgate = on;
}

void tek8002_fdd::write_data(bool v)
{
	drive_t *dr = sel();
	if (!m_wgate || !dr || !dr->writable || !m_loaded) return;
	const u64 s = write_slot();
	erase_to(s);
	dr->slots[dr->track][s % TRACK_SLOTS] = v;
	m_wnext = s + 1;
}

void tek8002_fdd::step(bool in)
{
	if (drive_t *dr = sel()) dr->track = in ? (dr->track < TRACKS - 1 ? dr->track + 1 : dr->track) : (dr->track > 0 ? dr->track - 1 : 0);
}
