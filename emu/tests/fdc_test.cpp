// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Stage B of the low-level disc controller: the processor runs the real microprogram
// (roms/fdc) against a drive that only turns (index and sector pulses, track 00, a head
// that steps) and a host that speaks the protocol of 070-2587 Table 1-2. Checked: the
// power-on delay ends, the SIO/OTD handshake works byte by byte, the command and address
// bytes land in their registers (Fig. 3-7b), and the seek steps the head to the target.
#include "machine/tek8002_fdc.h"
#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

// a Qume/CalComp drive with no data: 360 rpm, 32 sector holes and an index hole
struct turning_drive : fdc_drive_bus {
	u64 t = 0; int track = 40, steps = 0, selected = -1; bool loaded = false, wgate = false;
	static constexpr u64 REV = 166'666'667, SECT = REV / 32, PULSE = 200'000;   // ns
	bool track00() override { return track == 0; }
	bool index() override { return (t % REV) < PULSE; }
	bool sector() override { return (t % SECT) < PULSE; }
	bool ready() override { return true; }
	bool write_protect() override { return false; }
	// FM bit cells every 4 us, all data 0 (a blank but readable track): the separated clock
	// sets Data Ready once per cell
	u64 taken = 0;
	bool data_ready() override { return t / 4000 > taken; }
	bool data_bit() override { return false; }
	void data_taken() override { taken = t / 4000; }
	void select(int d) override { selected = d; }
	void step(bool in) override { steps++; track += in ? 1 : -1; if (track < 0) track = 0; if (track > 76) track = 76; }
	void head_load(bool on) override { loaded = on; }
	void write_gate(bool on) override { wgate = on; }
	void write_data(bool) override {}
	void elapse(u64 ns) override { t += ns; }
};

int main(int argc, char **argv)
{
	const std::string dir = argc > 1 ? argv[1] : "../roms/fdc";
	turning_drive drv;
	tek8002_fdc fdc;
	std::string err;
	if (!fdc.load_rom(dir, err)) { printf("%s\n", err.c_str()); return 1; }
	fdc.drive = &drv;
	fdc.reset();
	fdc.host_control(7);

	// power on: a delay (about 1-2.5 s, §3 "Executive Routine"), then the executive scan loop at EX3 (01C-061)
	u64 t = 0;
	while (t < 4'000'000'000ull && !(fdc.pc() >= 0x1c && fdc.pc() <= 0x61)) { fdc.run(1000); t += 1000; }
	printf("executive reached after %.2f s, pc %03X\n", t / 1e9, fdc.pc());
	CHECK(fdc.pc() >= 0x1c && fdc.pc() <= 0x61);
	CHECK(t > 500'000'000ull);

	// the host's handshake (TEKDOS's 26B1/26BE): strobe, wait for FLAG; release, wait for FLAG low
	auto wait_flag = [&](bool want) { for (int i = 0; i < 200000 && fdc.host_flag() != want; i++) fdc.run(200); return fdc.host_flag() == want; };
	auto request = [&](int code, int byte = -1) {
		if (byte >= 0) fdc.host_data_out(u8(byte));
		fdc.host_control(u8(code << 1));        // /S low
		const bool ack = wait_flag(true);
		fdc.host_control(7);                    // RST: /S high
		const bool done = wait_flag(false);
		return ack && done;
	};
	// the power-on autoload (Normal Read of drive 0, track 0, sector 0; EXEC 000-019) finds no
	// sync byte on a blank track and goes through error recovery (wipe: out to 00, in to 76,
	// back). FLAG stays up until it is over; the boot PROM's first RST waits for it to drop.
	u64 waited = 0;
	while (fdc.host_flag() && waited < 20'000'000'000ull) { fdc.run(1'000'000); waited += 1'000'000; }
	printf("autoload over, FLAG low after %.2f s more; status %02X, R3 %02X, R0 sense %02X\n", waited / 1e9, fdc.host_data_in(), fdc.reg(3), fdc.reg(0));
	CHECK(!fdc.host_flag());
	CHECK(fdc.host_data_in() & 0x02);            // UNIT CHECK: the autoload failed
	CHECK(request(3));                           // SIO
	printf("after SIO: status %02X, R13 %02X\n", fdc.host_data_in(), fdc.reg(13));
	CHECK(request(2, 0x88));                     // OTD command: Normal Read
	CHECK(request(2, 0x00));                     // drive 0
	CHECK(request(2, 0x05));                     // sector 5
	// the track byte: acknowledged; TEKDOS then releases the strobe (RST, with the disc
	// interrupt enabled) without waiting, and takes the FLAG transitions that follow
	fdc.host_data_out(0x10); fdc.host_control(4);
	CHECK(wait_flag(true));
	fdc.run(10'000);                             // FLAG rises at 067 (SFG), before the byte is stored (08E LRA R11)
	printf("R6 command %02X, R4 drive %02X, R9 sector %02X, R11 track %02X\n", fdc.reg(6), fdc.reg(4), fdc.reg(9), fdc.reg(11));
	CHECK(fdc.reg(6) == 0x88 && fdc.reg(4) == 0 && fdc.reg(9) == 0x05 && fdc.reg(11) == 0x10);
	CHECK(drv.selected == 0);
	fdc.host_control(7);
	// the seek steps the head to track 16; on this blank disc the read then finds no sync
	// byte, the recovery runs (070-2587 §3 "Error Recovery"), and FLAG drops with UNIT CHECK
	bool reached = false; int flag_drops = 0; bool f = fdc.host_flag();
	for (int i = 0; i < 24000 && !(flag_drops && !fdc.host_flag()); i++) {
		fdc.run(250'000);
		if (drv.track == 16) reached = true;
		if (fdc.host_flag() != f) { f = fdc.host_flag(); if (!f) flag_drops++; }
	}
	printf("head reached track 16: %d; FLAG dropped: %d; status %02X, sense R0 %02X, %.2f s\n", int(reached), flag_drops, fdc.host_data_in(), fdc.reg(0), fdc.time_ns() / 1e9);
	CHECK(reached);
	CHECK(flag_drops == 1);
	CHECK(fdc.host_data_in() & 0x02);            // UNIT CHECK (F.D. FAILURE)
	CHECK(fdc.reg(0) & 0x08);                    // SYNC ERROR
	printf("%s\n", failures ? "fdc_test: FAILED" : "fdc_test: all passed");
	return failures ? 1 : 0;
}
