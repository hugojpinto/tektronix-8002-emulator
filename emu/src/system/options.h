// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002 command-line options, shared by every front end (tty, SDL).
#ifndef TEK_SYSTEM_OPTIONS_H
#define TEK_SYSTEM_OPTIONS_H
#include <string>
#include <vector>

struct tek8002_options
{
	struct timed_t { double at; std::string text; };   // at < 0: at TEKDOS's next prompt (-type prompt)

	std::string prom;                  // -prom FILE (default: roms/tekdos_boot.bin, found from the repository root or emu/)
	std::string disks[4];              // -0 .. -3 IMAGE ("new:FILE": a never-formatted disc)
	bool writeback = false;            // -rw
	bool fdc_hle = false;              // -fdc hle|lle
	bool ep6800_hle = false;           // -ep6800 hle|prom: the 6800 board's firmware at high level, or its SYSTEM PROM
	bool epz80_hle = false;            // -epz80 hle|prom: the Z80 board's firmware at high level, or the rebuilt PROM
	bool tty_lock = true;              // -ttylock on|off: the CT8100's TTY LOCK key
	long long max_cycles = -1;         // -cycles (2650 cycles at 1.25 MHz)
	std::vector<timed_t> typed;        // -type SECONDS|prompt TEXT
	std::vector<timed_t> snaps;        // -snap SECONDS FILE: the CT8100 screen as text
	std::vector<timed_t> switches;     // -input SECONDS NAME=VALUE, -press SECONDS NAME
	std::vector<timed_t> discs;        // -mount SECONDS N=FILE, -eject SECONDS N
	std::vector<timed_t> parity;       // -parity SECONDS ADDR | U:ADDR (hex): a memory parity fault
	std::string tracefile, logfile;    // -trace FILE (2650 instructions), -log FILE (console output)
	bool io = false, fdutrace = false; // -io, -fdutrace
	std::string targetspec;            // -target unix:PATH|tcp:HOST:PORT (the prototype)
	std::string remspec, lptspec, pptspec;   // -rem, -lpt, -ppt (System Communications links)

	bool parse(int argc, char **argv, std::string &unknown);   // false on an unknown option
	static const char *usage();
	// "\r" "\n" "\t" "\\" and "\xHH" in -type text
	static std::string unescape(const std::string &t);
};
#endif
