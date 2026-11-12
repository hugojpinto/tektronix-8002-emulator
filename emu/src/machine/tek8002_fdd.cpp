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
	dr.present = true; dr.writable = writable;
	dr.cells.assign(TRACKS, std::vector<u8>(TRACK_CELLS, 0));
	for (int t = 0; t < TRACKS; t++)
		for (int k = 0; k < SECTORS; k++) {
			const int s = ((k + 1) * step) & 31;
			u8 rec[1 + 2 + SECTOR_BYTES + 2];
			rec[0] = 0xfb; rec[1] = u8(t); rec[2] = u8(s);
			for (int i = 0; i < SECTOR_BYTES; i++) rec[3 + i] = image[(size_t(t) * SECTORS + s) * SECTOR_BYTES + i];
			const u16 c = crc(rec + 1, 2 + SECTOR_BYTES);
			rec[3 + SECTOR_BYTES] = u8(c >> 8); rec[4 + SECTOR_BYTES] = u8(c);
			u8 *cell = &dr.cells[t][size_t(k) * SECTOR_CELLS + LEADER_CELLS];
			for (size_t i = 0; i < sizeof rec; i++)
				for (int b = 7; b >= 0; b--) *cell++ = (rec[i] >> b) & 1;
		}
}

void tek8002_fdd::remove(int d) { m_d[d & 3] = drive_t(); }

// Every sector of every track, by its own header: what the controller wrote is what counts.
bool tek8002_fdd::unload(int d, std::vector<u8> &image, std::string &problems) const
{
	const drive_t &dr = m_d[d & 3];
	if (!dr.present) return false;
	image.assign(IMAGE_BYTES, 0);
	std::vector<bool> seen(size_t(TRACKS) * SECTORS, false);
	char msg[96];
	for (int t = 0; t < TRACKS; t++)
		for (int k = 0; k < SECTORS; k++) {
			const u8 *c = &dr.cells[t][size_t(k) * SECTOR_CELLS];
			int i = 0; u8 sr = 0;
			for (; i < SECTOR_CELLS - 8 * 133; i++) { sr = u8(sr << 1 | c[i]); if (sr == 0xfb) break; }
			if (sr != 0xfb) { snprintf(msg, sizeof msg, "track %d hole %d: no sync\n", t, k); problems += msg; continue; }
			u8 rec[2 + SECTOR_BYTES + 2];
			for (size_t n = 0; n < sizeof rec; n++) { u8 v = 0; for (int b = 0; b < 8; b++) v = u8(v << 1 | c[++i]); rec[n] = v; }
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

bool tek8002_fdd::data_ready()
{
	return sel() && m_loaded && !m_wgate && m_t / CELL_NS > m_taken;
}

bool tek8002_fdd::data_bit()
{
	drive_t *dr = sel();
	return dr && m_loaded && dr->cells[dr->track][cell()];
}

void tek8002_fdd::step(bool in)
{
	if (drive_t *dr = sel()) dr->track = in ? (dr->track < TRACKS - 1 ? dr->track + 1 : dr->track) : (dr->track > 0 ? dr->track - 1 : 0);
}
