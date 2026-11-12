// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Flexible Disc Unit, high level (see tek8002_fdu.h).
#include "machine/tek8002_fdu.h"
#include <cstdio>
#include <cstring>

enum { SR_IND = 0, SR_OTD = 2, SR_SIO = 3 };     // C1 C0 (070-2587 Fig. 1-8)
enum { CMD_DIAG = 0x00, CMD_WRITE_UNF = 0x0d, CMD_WRITE_FMT = 0x0e, CMD_READ = 0x88, CMD_COPY = 0xbe };

bool tek8002_fdu::mount(int d, const std::string &path, bool writable, std::string &err)
{
	if (d < 0 || d >= DRIVES) { err = "drive must be 0-3"; return false; }
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) { err = path + ": cannot open"; return false; }
	std::vector<u8> img(IMAGE_BYTES, 0xe5);
	size_t n = fread(img.data(), 1, img.size(), f);
	int extra = fgetc(f);
	fclose(f);
	if (n != size_t(IMAGE_BYTES) || extra != EOF) {
		err = path + ": not a raw 8002 disc image (" + std::to_string(IMAGE_BYTES) + " bytes: 77 tracks x 32 sectors x 128)";
		return false;
	}
	drive_t &dr = m_drive[d];
	dr.image = std::move(img); dr.path = path; dr.writable = writable; dr.dirty = false;
	return true;
}

void tek8002_fdu::eject(int d) { if (d >= 0 && d < DRIVES) m_drive[d] = drive_t(); }

bool tek8002_fdu::save(int d, std::string &err)
{
	drive_t &dr = m_drive[d];
	if (!dr.dirty || !dr.writable) return true;
	FILE *f = fopen(dr.path.c_str(), "wb");
	if (!f || fwrite(dr.image.data(), 1, dr.image.size(), f) != dr.image.size()) { if (f) fclose(f); err = dr.path + ": write failed"; return false; }
	fclose(f); dr.dirty = false;
	return true;
}

u8 tek8002_fdu::drive_status(int d) const
{
	// DRIVE FAULT reports all drives (their READY lines are wire-ORed, 070-2587 §1):
	// a unit with an empty drive reports it whichever drive is addressed. Only drives
	// with a disc are modelled as present, so an empty drive is one that is absent.
	u8 s = 0;
	if (!mounted(d)) s |= ST_DRIVE_FAULT;
	else if (!m_drive[d].writable) s |= ST_WRITE_PROTECT;
	return s;
}

bool tek8002_fdu::locate(long &off) const
{
	if (m_trk >= TRACKS || m_sec >= SECTORS) return false;
	off = (long(m_trk) * SECTORS + m_sec) * SECTOR_BYTES;   // images are in logical sector order
	return true;
}

void tek8002_fdu::read_sector()
{
	long off;
	if (!locate(off)) { m_status |= ST_ILLEGAL; memset(m_buf, 0, sizeof m_buf); return; }
	if (!mounted(m_dev)) { m_status |= ST_DRIVE_FAULT; memset(m_buf, 0, sizeof m_buf); return; }
	memcpy(m_buf, &m_drive[m_dev].image[off], SECTOR_BYTES);
	if (trace) fprintf(stderr, "[FDU read  drive %d track %2d sector %2d]\n", m_dev, m_trk, m_sec);
}

void tek8002_fdu::write_sector()
{
	long off;
	if (!locate(off)) { m_status |= ST_ILLEGAL; return; }
	if (!mounted(m_dev)) { m_status |= ST_DRIVE_FAULT; return; }
	drive_t &dr = m_drive[m_dev];
	if (!dr.writable) { m_status |= ST_WRITE_PROTECT; return; }
	memcpy(&dr.image[off], m_buf, SECTOR_BYTES); dr.dirty = true;
	if (trace) fprintf(stderr, "[FDU write drive %d track %2d sector %2d]\n", m_dev, m_trk, m_sec);
}

// After a sector, a read carries on with the next one: the boot PROM reads 128
// sectors (tracks 1-4) after a single command, checking the status byte after
// each sector. Inferred from the boot PROM (070-2393 Fig. 6-2); the manual
// describes one-sector transfers only.
void tek8002_fdu::next_sector()
{
	if (++m_sec == SECTORS) { m_sec = 0; m_trk++; }
}

void tek8002_fdu::start_transfer()
{
	m_status = drive_status(m_dev);
	if (trace) fprintf(stderr, "[FDU command %02x drive %d sector %2d track %2d]\n", m_cmd, m_dev, m_sec, m_trk);
	m_index = 0;
	switch (m_cmd) {
	case CMD_READ:
		read_sector();
		m_phase = phase::data_in;
		break;
	case CMD_WRITE_UNF: case CMD_WRITE_FMT:
		m_phase = phase::data_out;
		break;
	case CMD_COPY:
		write_sector();
		m_phase = phase::idle;
		break;
	default:
		m_status |= ST_ILLEGAL;
		m_phase = phase::idle;
		break;
	}
	m_in = m_status;
	m_op_done = true;                      // FLAG rises when the seek and transfer are done
}

void tek8002_fdu::request(int code)
{
	switch (code) {
	case SR_SIO:
		// SIO presents the disc status; the next OTD starts a new command. A sector
		// already read stays available: TekDOS takes the completion interrupt, issues
		// SIO for the status, then reads the sector with IND (TekDOS 3.3, 25AF-26C0).
		m_expect_cmd = true;
		if (m_phase != phase::data_in) m_status = drive_status(m_dev);
		m_in = m_status;
		break;
	case SR_OTD:
		if (m_expect_cmd) { m_expect_cmd = false; m_phase = phase::cmd; }
		switch (m_phase) {
		case phase::cmd:
			m_cmd = m_out;
			if (m_cmd == CMD_DIAG) { m_phase = phase::diag; m_index = 0; }
			else m_phase = phase::drive;
			break;
		case phase::drive:  m_dev = m_out & 7; m_phase = phase::sector; break;
		case phase::sector: m_sec = m_out; m_phase = phase::track; break;
		case phase::track:  m_trk = m_out; start_transfer(); break;
		case phase::data_out:
			m_buf[m_index++] = m_out;
			if (m_index == SECTOR_BYTES) { write_sector(); m_in = m_status; m_phase = phase::idle; m_op_done = true; }
			break;
		default:
			if (trace) fprintf(stderr, "[FDU OTD %02x out of sequence]\n", m_out);
			break;
		}
		break;
	case SR_IND:
		switch (m_phase) {
		case phase::data_in:
			if (m_index == SECTOR_BYTES) { next_sector(); m_status = drive_status(m_dev); read_sector(); m_index = 0; }
			m_in = m_buf[m_index++];
			break;
		case phase::diag: {
			// R1..R15, then R0 (sense); the controller loops until RST
			static const int order[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0 };
			const int r = order[m_index++ & 15];
			m_in = r == 4 ? m_dev : r == 6 ? m_cmd : r == 7 || r == 9 ? m_sec : r == 10 || r == 11 ? m_trk : 0;
			break;
		}
		default:
			m_in = m_status;
			break;
		}
		break;
	default:
		if (trace) fprintf(stderr, "[FDU unknown service request %d]\n", code);
		break;
	}
}

void tek8002_fdu::control(u8 c)
{
	c &= 7;
	const bool strobe_was = !(m_code & 1), strobe = !(c & 1);
	m_code = c;
	if (strobe && !strobe_was) {
		request(c >> 1);
		flag_after(flag_delay);
	} else if (!strobe && strobe_was) {
		// end of the strobe: FLAG drops; at the end of a sector the status byte is presented
		m_flag = false; m_flag_timer = 0;
		if (m_phase == phase::data_in && m_index == SECTOR_BYTES) m_in = m_status;
		if (m_op_done) { m_op_done = false; flag_after(op_delay); }
	} else if (c == 7 && m_flag && !m_flag_timer) {
		// RST while a completion FLAG is up (the host was not strobing): the controller takes
		// it as the reset request and drops FLAG. TEKDOS's disc handler writes RST on the
		// completion interrupt; its next operation starts with RST and waits for FLAG low
		// (TEKDOS 3.3, 25B4 and 26BE-26C6). Without this a write is never followed by another
		// operation: DELETE, FORMAT and the directory write-back of LDIR stopped there.
		m_flag = false;
		m_in = m_status;
	}
}

void tek8002_fdu::tick(int cycles)
{
	if (m_flag_timer > 0 && (m_flag_timer -= cycles) <= 0) { m_flag_timer = 0; m_flag = true; }
}
