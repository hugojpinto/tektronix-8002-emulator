// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002 command-line options -- see options.h.
#include "system/options.h"
#include <cstdlib>

std::string tek8002_options::unescape(const std::string &t)
{
	std::string o;
	for (size_t i = 0; i < t.size(); i++) {
		if (t[i] != '\\' || i + 1 == t.size()) { o += t[i]; continue; }
		char c = t[++i];
		if (c == 'r') o += '\r'; else if (c == 'n') o += '\n'; else if (c == 't') o += '\t';
		else if (c == 'x' && i + 2 < t.size() + 1) { o += char(strtol(t.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
		else o += c;
	}
	return o;
}

const char *tek8002_options::usage()
{
	return "[-prom FILE] [-0..-3 IMG|new:FILE] [-rw] [-fdc hle|lle] [-ep6800 hle|prom] [-epz80 hle|prom] [-rtpa [probe]] [-ttylock on|off] [-cycles N] [-type SEC|prompt TEXT]\n"
	       "        [-snap SEC FILE] [-input SEC NAME=VALUE] [-press SEC NAME] [-mount SEC N=FILE] [-eject SEC N] [-parity SEC [U:]ADDR]\n"
	       "        [-target unix:PATH|tcp:HOST:PORT] [-rem SPEC] [-lpt SPEC] [-ppt SPEC] [-trace FILE] [-io] [-fdutrace] [-log FILE] [-version]";
}

bool tek8002_options::parse(int argc, char **argv, std::string &unknown)
{
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
		auto when = [&]() { std::string w = next(); return w == "prompt" ? -1.0 : atof(w.c_str()); };
		if (a == "-prom") prom = next();
		else if (a.size() == 2 && a[0] == '-' && a[1] >= '0' && a[1] <= '3') disks[a[1] - '0'] = next();
		else if (a == "-rw") writeback = true;
		else if (a == "-cycles") max_cycles = atoll(next().c_str());
		else if (a == "-type") { double t = when(); typed.push_back({ t, unescape(next()) }); }
		else if (a == "-snap") { double t = atof(next().c_str()); snaps.push_back({ t, next() }); }
		else if (a == "-input") { double t = atof(next().c_str()); switches.push_back({ t, next() }); }
		else if (a == "-press") { double t = atof(next().c_str()); switches.push_back({ t, next() }); }
		else if (a == "-mount") { double t = atof(next().c_str()); discs.push_back({ t, next() }); }
		else if (a == "-parity") { double t = atof(next().c_str()); parity.push_back({ t, next() }); }
		else if (a == "-eject") { double t = atof(next().c_str()); discs.push_back({ t, next() }); }
		else if (a == "-trace") tracefile = next();
		else if (a == "-log") logfile = next();
		else if (a == "-io") io = true;
		else if (a == "-fdutrace") fdutrace = true;
		else if (a == "-ttylock" && i + 1 < argc && (std::string(argv[i + 1]) == "on" || std::string(argv[i + 1]) == "off")) tty_lock = next() == "on";
		else if (a == "-target") targetspec = next();
		else if (a == "-rem") remspec = next();
		else if (a == "-lpt") lptspec = next();
		else if (a == "-ppt") pptspec = next();
		else if (a == "-fdc" && i + 1 < argc && (std::string(argv[i + 1]) == "hle" || std::string(argv[i + 1]) == "lle")) fdc_hle = next() == "hle";
		else if (a == "-ep6800" && i + 1 < argc && (std::string(argv[i + 1]) == "hle" || std::string(argv[i + 1]) == "prom")) ep6800_hle = next() == "hle";
		else if (a == "-rtpa") { rtpa_on = true; if (i + 1 < argc && std::string(argv[i + 1]) == "probe") { i++; rtpa_probe = true; } }
		else if (a == "-epz80" && i + 1 < argc && (std::string(argv[i + 1]) == "hle" || std::string(argv[i + 1]) == "prom")) epz80_hle = next() == "hle";
		else { unknown = a; return false; }
	}
	return true;
}
