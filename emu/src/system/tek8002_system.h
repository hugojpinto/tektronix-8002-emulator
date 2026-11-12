// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002_system: the 8002A µProcessor Lab as one system -- the mainframe with its disc unit,
// the CT8100 on the terminal port, the System Communications and prototype links, and the
// scripted events (-type, -snap, -input/-press, -mount/-eject).
//
// The system has no notion of wall-clock time: a front end calls tick() (1 ms of machine
// time) as fast or as slowly as it likes. The tty front end (main8002.cpp) and the SDL
// front end (frontend/sdl/main_gui.cpp) both drive this class.
#ifndef TEK_SYSTEM_TEK8002_H
#define TEK_SYSTEM_TEK8002_H
#include "machine/tek8002.h"
#include "host/ct8100.h"
#include "host/ipc_target.h"
#include "host/serial_link.h"
#include "system/options.h"
#include "system/outputs.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdio>

class tek8002_system
{
public:
	static constexpr int SLICE = 1250;            // 1 ms of the 2650's 1.25 MHz clock

	explicit tek8002_system(const tek8002_options &o) : opt(o) {}
	~tek8002_system();

	bool start(std::string &err);                 // load ROMs, mount discs, open links, power on
	void tick();                                  // 1 ms of machine time
	// the run is over: -cycles reached, or, when commands were typed at prompts (-type
	// prompt), the prompt after the last one (batch runs only: see batch)
	bool finished() const;
	bool batch = false;                           // a run with no one at the keyboard
	bool timed_typing_left() const;               // -type SECONDS text not typed yet
	double now() const { return double(clock()) / tek8002::CPU_HZ; }
	// machine time in 2650 cycles, including time switched off or held in RESTART
	u64 clock() const { return m->cycles() + m_held_cycles; }
	void shutdown(bool show_state);               // discs written back, logs closed

	// the system terminal port
	ct8100 &terminal() { return ct; }
	void key_in(uint8_t c) { m->term.host_rx(c); }   // a key to the 8002 (the CT8100 calls it)
	std::function<void(uint8_t)> on_term_tx;      // every character the 8002 sends (7 bits)
	bool terminal_dirty = false;

	// named outputs and inputs (outputs.h). Lamps, 1 = lit (each the state over the last
	// tick): "8002:pwr", "8002:sys" (the System Processor holds the bus), "8002:run" (a
	// processor is running), "fdu:pwr", "fdu:drive0".."fdu:drive3" (busy). Switches, which
	// are also outputs of the same name: "8002:power", "fdu:power" (rockers: 1 on);
	// "8002:restart" (SYSTEM RESTART: held = 1; the 2650 restarts when it is let go),
	// "8002:diag" (DIAG INTRPT: interrupt 28 when pressed).
	outputs out;
	bool input(const std::string &name, int value = 1);   // false: no such switch
	bool press(const std::string &name, double seconds = 0.2);
	void release_at(const std::string &name, double at);

	// discs, at any time (the drive is empty in between); written back on eject with -rw
	bool mount(int drive, const std::string &path, std::string &err);
	bool eject(int drive, std::string &err);
	const std::string &disc(int drive) const { return m_disc[drive & 3]; }

	tek8002 &machine() { return *m; }
	std::string dasm(u16 pc, int *len = nullptr);
	void show(FILE *f);                           // the 2650's registers

private:
	tek8002_options opt;
	std::unique_ptr<tek8002> m = std::make_unique<tek8002>();
	ct8100 ct;
	ipc_target target;
	serial_link link[tek8002_syscomm::PORTS];
	FILE *logf = nullptr, *tracef = nullptr;
	std::string m_disc[4];
	bool m_power = true, m_held = false;
	u64 m_held_cycles = 0;
	u64 m_last_master = 0, m_last_bus = 0, m_last_slave = 0;
	struct release_t { double at; std::string name; };
	std::vector<release_t> m_releases;
	std::vector<tek8002_options::timed_t> m_prompt_typed;   // -type prompt, in order
	bool m_prompts_used = false, m_prompt_seen = false, m_script_over = false;
	char m_last2[2] = {};
	double m_prompt_t = 0;

	void scripted_events();
	void update_outputs(int slice);
};
#endif
