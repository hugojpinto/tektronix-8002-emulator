// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Tektronix 8002A Flexible Disc Unit, high level: the controller's host protocol
// (service requests RST/SIO/OTD/IND, the FLAG handshake, command and address bytes,
// disc status) over sector images. The real controller is a microprogrammed
// machine (070-2587 section 3, microcode in Appendix C); this model stands in for
// it until that is emulated.
//
// Protocol (070-2587 Fig. 1-8, 1-9a, Table 1-2; 070-2711 Table 3-6):
//   the host writes C1 C0 /S to its control port EA. /S low with
//     C1C0 = 11 SIO  start an I/O sequence: the disc status byte is presented
//            10 OTD  the controller takes the byte on the host's output port EB
//            00 IND  the controller presents the next byte on the host's input port EB
//   The controller answers each strobe by raising FLAG; the host then raises /S
//   (code 111, RST) and the controller drops FLAG, presenting the disc status byte
//   on EB when the sequence is at an end.
//   The strobe that starts an operation (the track byte of a read, the 128th data byte of a
//   write) keeps FLAG up after the host's RST until the operation is done: the bootstrap PROM
//   waits for FLAG low, TekDOS for the disc interrupt, which FLAG going low clocks.
// Command bytes: 00 read diagnostic registers, 0D write unformatted, 0E write
// formatted, 88 normal read, BE copy write; then drive, sector and track bytes.
//
// Low level (use_controller): the host port goes to the real controller instead, running
// its microprogram (tek8002_fdc) against bit-cell tracks (tek8002_fdd) built from the
// images. Reads only so far: the discs are write protected in this mode until the write
// path is modelled (notes/fdc.md, stage D).
#ifndef TEK_TEK8002_FDU_H
#define TEK_TEK8002_FDU_H
#include "emu.h"
#include "machine/tek8002_fdc.h"
#include "machine/tek8002_fdd.h"
#include <string>
#include <vector>

class tek8002_fdu
{
public:
	static constexpr int TRACKS = 77, SECTORS = 32, SECTOR_BYTES = 128;
	static constexpr int IMAGE_BYTES = TRACKS * SECTORS * SECTOR_BYTES;   // 315,392
	static constexpr int DRIVES = 4;        // two disc units of two drives (070-2587 §1)

	// disc status byte (070-2587 Table 1-3)
	enum { ST_FD_FAILURE = 0x02, ST_DRIVE_FAULT = 0x04, ST_WRITE_PROTECT = 0x08, ST_ERROR_RECOVERY = 0x10, ST_ILLEGAL = 0x20 };

	bool mount(int drive, const std::string &path, bool writable, std::string &err);
	void eject(int drive);
	bool save(int drive, std::string &err);           // write a modified image back
	bool mounted(int drive) const { return drive >= 0 && drive < DRIVES && !m_drive[drive].image.empty(); }

	// the real controller and drives instead of the high-level model (before mounting)
	bool use_controller(const std::string &rom_dir, std::string &err);
	bool low_level() const { return m_low_level; }
	tek8002_fdc &controller() { return m_fdc; }
	tek8002_fdd &drives() { return m_fdd; }
	void reset();                          // power on

	// host side, as the System Processor's parallel interface sees it
	void control(u8 c1c0s);                // EA write, bits 2-0
	void data_out(u8 d) { if (m_low_level) m_fdc.host_data_out(d); m_out = d; }   // EB write (output latch)
	u8   data_in() const { return m_low_level ? m_fdc.host_data_in() : m_in; }   // EB read
	bool flag() const { return m_low_level ? m_fdc.host_flag() : m_flag; }        // EA read bit 7
	void tick(int cycles);                 // FLAG answers after a short controller delay
	int  cycle_ns = 800;                   // the host's clock (2650 at 1.25 MHz), for the controller

	int  flag_delay = 20;                  // 2650 clocks from strobe to FLAG (controller polling loop)
	int  op_delay = 1250;                  // 2650 clocks for a seek and sector (1 ms: not modelled in detail)
	bool trace = false;

private:
	struct drive_t {
		std::vector<u8> image;
		std::string path;
		bool writable = false, dirty = false;
	};
	drive_t m_drive[DRIVES];
	bool m_low_level = false;
	tek8002_fdc m_fdc;
	tek8002_fdd m_fdd;

	enum class phase { idle, cmd, drive, sector, track, data_in, data_out, diag };
	phase m_phase = phase::idle;
	u8 m_cmd = 0, m_dev = 0, m_sec = 0, m_trk = 0, m_status = 0;
	int m_index = 0;                       // byte within the current sector (or diagnostic register)
	u8 m_buf[SECTOR_BYTES] = {};           // sector buffer
	u8 m_out = 0, m_in = 0;
	bool m_flag = false;
	u8 m_code = 7;                         // last C1 C0 /S written
	int m_flag_timer = 0;                  // >0: FLAG rises when it reaches 0
	int m_drop_timer = 0;                  // >0: the operation ends (FLAG drops) when it reaches 0
	bool m_expect_cmd = false;              // after SIO: the next OTD is a command byte
	bool m_op_done = false;                // this strobe starts the operation: FLAG is held after it
	void flag_after(int c) { if (c <= 0) m_flag = true; else if (m_flag_timer < c) m_flag_timer = c; }

	void request(int code);                // a service request (C1C0) on /S going low
	void start_transfer();                 // command and address bytes complete
	u8   drive_status(int d) const;
	bool locate(long &off) const;          // byte offset of (m_trk, m_sec) on the selected drive
	void read_sector();
	void write_sector();
	void next_sector();
};
#endif
