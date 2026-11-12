// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002: the Tektronix 8002A µProcessor Lab on a text terminal. The 8002's system terminal
// is this terminal: what TEKDOS writes goes to stdout as it is (it writes plain text), and
// keys go to the 8002. The machine is system/tek8002_system; options are system/options.
//   tek8002 [options]   (tek8002_options::usage(); emu/README.md)
// Interactive runs are paced to real time; with stdin not a terminal, or -cycles, the run is
// flat out. Ctrl-] enters the monitor.
#include "system/tek8002_system.h"
#include "host/tty.h"
#include <cstdio>
#include <string>
#include <chrono>
#include <thread>

static bool monitor(tek8002_system &sys)
{
	tek8002 &m = sys.machine();
	host::tty_raw(false);
	printf("\n[monitor] "); sys.show(stdout);
	char line[256];
	for (;;) {
		printf("mon> "); fflush(stdout);
		if (!fgets(line, sizeof line, stdin)) return false;
		unsigned a = 0, n = 0; int k = sscanf(line + 1, "%x %x", &a, &n);
		switch (line[0]) {
		case 'q': return false;
		case 'c': case '\n': host::tty_raw(true); return true;
		case 'r': sys.show(stdout); break;
		case 'd': { if (k < 1) a = m.cpu.pc(); if (k < 2) n = 64;
			for (unsigned i = 0; i < n; i++) { if (i % 16 == 0) printf("%s%04x:", i ? "\n" : "", (a + i) & 0x7fff); printf(" %02x", m.read((a + i) & 0x7fff)); }
			printf("\n"); break; }
		case 'u': { if (k < 1) a = m.cpu.pc(); if (k < 2) n = 16; u16 pc = u16(a);
			for (unsigned i = 0; i < n; i++) { int len; std::string s = sys.dasm(pc, &len); printf("%04x: %s\n", pc, s.c_str()); pc += len ? len : 1; }
			break; }
		case 's': { if (k < 1) a = 1; for (unsigned i = 0; i < a; i++) m.cpu.run(1); sys.show(stdout); break; }
		case 'i': m.trace_io = !m.trace_io; printf("io trace %s\n", m.trace_io ? "on" : "off"); break;
		case 'R': sys.press("8002:restart"); printf("SYSTEM RESTART\n"); break;
		case 'D': sys.press("8002:diag"); printf("DIAG INTRPT\n"); break;
		case 'T': { ct8100 &t = sys.terminal(); t.tty_lock = !t.tty_lock; printf("TTY LOCK %s\n", t.tty_lock ? "on" : "off"); break; }
		case 'p': { char name[64]; if (sscanf(line + 1, "%63s", name) == 1) printf("%s\n", sys.press(name) ? "pressed" : "no such switch"); break; }
		case 'm': { int d; char path[512]; std::string err;
			if (sscanf(line + 1, "%d %511s", &d, path) == 2) printf("%s\n", sys.mount(d, path, err) ? "mounted" : err.c_str());
			break; }
		case 'e': { int d; std::string err; if (sscanf(line + 1, "%d", &d) == 1) printf("%s\n", sys.eject(d, err) ? "ejected" : err.c_str()); break; }
		case 'h': printf("c continue | q quit | r regs | d [addr [n]] | u [addr [n]] | s [n] step | i io-trace\n"
		                 "R SYSTEM RESTART | D DIAG INTRPT | T TTY LOCK | p SWITCH (press: 8002:power, fdu:power ...) | m N FILE mount | e N eject\n"); break;
		default: printf("?\n");
		}
	}
}

int main(int argc, char **argv)
{
	tek8002_options opt;
	std::string bad;
	if (!opt.parse(argc, argv, bad)) { fprintf(stderr, "unknown option %s\nusage: %s %s\n", bad.c_str(), argv[0], tek8002_options::usage()); return 1; }
	tek8002_system sys(opt);
	std::string err;
	if (!sys.start(err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
	sys.on_term_tx = [](uint8_t c) { fputc(c, stdout); fflush(stdout); };

	const bool interactive = host::is_tty(0) && opt.max_cycles < 0;
	sys.batch = !interactive;
	if (interactive) { host::tty_raw(true); fprintf(stderr, "[tek8002: Ctrl-] for monitor]\r\n"); }
	using clk = std::chrono::steady_clock;
	auto t0 = clk::now();
	const double e0 = sys.now();
	while (!sys.finished()) {
		int k;
		while ((k = host::poll_key()) >= 0) {
			if (k == 035) { if (!monitor(sys)) goto done; t0 = clk::now(); continue; }
			sys.terminal().type(u8(k == '\n' ? '\r' : k));   // through the CT8100's keyboard (TTY LOCK)
		}
		// stdin ended and nothing more will be typed: the run is over
		if (k == -2 && !interactive && opt.max_cycles < 0 && !sys.timed_typing_left()) break;
		for (int i = 0; i < 10; i++) sys.tick();      // 10 ms of machine time
		if (interactive) {
			auto want = t0 + std::chrono::microseconds(long((sys.now() - e0) * 1e6));
			if (want > clk::now()) std::this_thread::sleep_until(want);
		}
	}
done:
	host::tty_raw(false);
	sys.shutdown(!interactive);                       // the console text alone on stdout
	return 0;
}
