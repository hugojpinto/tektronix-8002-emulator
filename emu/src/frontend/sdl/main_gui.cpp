// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// tek8002-gui: the 8002A µProcessor Lab with a graphical CT8100 and front panels (SDL3).
// The same system and options as the tty front end (system/tek8002_system.*); this file only
// draws, takes the keyboard and the mouse, and paces the emulation to real time.
//   tek8002-gui [tek8002 options] [-style crt|sharp] [-scale F] [-panels on|off] [-artwork DIR] [-shot SECONDS FILE.bmp]
// With -cycles the run is flat out and deterministic (batch); -shot saves the windows at a
// machine time, or at the end of the run if it ends sooner (SDL_VIDEODRIVER=dummy runs it
// with no display).
// Hotkeys (Cmd on macOS, Ctrl+Alt elsewhere): V paste, C copy the screen, T turbo, F full
// screen, S sharp/CRT, P show the panels, R SYSTEM RESTART, Q quit.
// Front panels: artwork/8002a.lay and fdu.lay (tools/gen_panels.py). Click the switches;
// drop a disc image on a drive door to insert it, click the door to take it out.
#include "system/tek8002_system.h"
#include "host/ct8100_raster.h"
#include "frontend/sdl/layout.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

namespace {
bool is_toggle(const std::string &n) { return n == "8002:power" || n == "fdu:power"; }
bool is_momentary(const std::string &n) { return n == "8002:restart" || n == "8002:diag"; }

constexpr int RW = ct8100_raster::W, RH = ct8100_raster::H;   // 640 x 240 dots
constexpr int TW = RW, TH = RH * 2;                            // each scan line doubled: 640 x 480
constexpr int OVER_X = 40, OVER_Y = 36;                        // the tube's border
constexpr int LW = TW + 2 * OVER_X, LH = TH + 2 * OVER_Y;

struct rgb { float r, g, b; };
const rgb BG = { 5, 9, 6 }, GREEN = { 70, 255, 120 };          // a green phosphor (the CT8100's is not documented)

struct terminal_view {
	bool crt = true;
	std::vector<uint8_t> dots = std::vector<uint8_t>(size_t(RW) * RH);
	std::vector<float> lum[3];
	std::vector<uint32_t> px = std::vector<uint32_t>(size_t(TW) * TH);

	void compose()
	{
		for (auto &l : lum) l.assign(size_t(TW) * TH, 0.f);
		for (int y = 0; y < RH; y++)
			for (int x = 0; x < RW; x++) {
				if (!dots[size_t(y) * RW + x]) continue;
				for (int k = 0; k < 2; k++) {
					const float f = (crt && k) ? 0.55f : 1.f;     // the second scan line dimmer
					const size_t i = size_t(y * 2 + k) * TW + x;
					lum[0][i] = GREEN.r * f; lum[1][i] = GREEN.g * f; lum[2][i] = GREEN.b * f;
				}
			}
		if (crt) bloom();
		for (size_t i = 0; i < px.size(); i++) {
			auto ch = [&](int k, float bg) { float v = bg + lum[k][i]; return uint32_t(v > 255 ? 255 : v); };
			px[i] = 0xff000000u | ch(0, BG.r) << 16 | ch(1, BG.g) << 8 | ch(2, BG.b);
		}
	}
	// phosphor glow: a blurred copy added back (box passes per axis)
	void bloom()
	{
		std::vector<float> tmp(size_t(TW) * TH), blur(size_t(TW) * TH);
		const int R = 3;
		for (int k = 0; k < 3; k++) {
			const std::vector<float> &src = lum[k];
			for (int y = 0; y < TH; y++) {
				float acc = 0; const float *s = &src[size_t(y) * TW]; float *d = &tmp[size_t(y) * TW];
				for (int x = -R; x < TW + R; x++) {
					if (x + R < TW) acc += s[x + R];
					if (x - R - 1 >= 0) acc -= s[x - R - 1];
					if (x >= 0 && x < TW) d[x] = acc;
				}
			}
			for (int x = 0; x < TW; x++) {
				float acc = 0;
				for (int y = -R; y < TH + R; y++) {
					if (y + R < TH) acc += tmp[size_t(y + R) * TW + x];
					if (y - R - 1 >= 0) acc -= tmp[size_t(y - R - 1) * TW + x];
					if (y >= 0 && y < TH) blur[size_t(y) * TW + x] = acc;
				}
			}
			const float g = 0.55f / float((2 * R + 1) * (2 * R + 1));
			for (size_t i = 0; i < blur.size(); i++) lum[k][i] += blur[i] * g;
		}
	}
};

struct bell_sound {                              // BEL: a short beep
	SDL_AudioStream *stream = nullptr;
	std::vector<int16_t> wave;
	void open()
	{
		SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, 22050 };
		stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
		if (!stream) return;
		for (int i = 0; i < 22050 / 8; i++) wave.push_back(int16_t(((i * 880 * 2 / 22050) & 1) ? 3000 : -3000));
		SDL_ResumeAudioStreamDevice(stream);
	}
	void ring() { if (stream && SDL_GetAudioStreamQueued(stream) < 4000) SDL_PutAudioStreamData(stream, wave.data(), int(wave.size() * 2)); }
};

struct panel {
	std::string tag;
	SDL_Window *win = nullptr; SDL_Renderer *ren = nullptr;
	lay::layout l; const lay::view *v = nullptr;
	const lay::item *pressed = nullptr; double pressed_at = 0;
	bool shown = true;
};

std::string base_name(const std::string &p)
{
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}
}

int main(int argc, char **argv)
{
	std::string style = "crt", artwork = "artwork"; double scale = 1.25; bool panels_on = true;
	struct shot_t { double at; std::string file; };
	std::vector<shot_t> shots;
	std::vector<char *> rest = { argv[0] };
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "-style" && i + 1 < argc) style = argv[++i];
		else if (a == "-scale" && i + 1 < argc) scale = atof(argv[++i]);
		else if (a == "-panels" && i + 1 < argc) panels_on = std::string(argv[++i]) != "off";
		else if (a == "-artwork" && i + 1 < argc) artwork = argv[++i];
		else if (a == "-shot" && i + 2 < argc) { double t = atof(argv[++i]); shots.push_back({ t, argv[++i] }); }
		else rest.push_back(argv[i]);
	}
	tek8002_options opt;
	std::string bad;
	if (!opt.parse(int(rest.size()), rest.data(), bad)) {
		fprintf(stderr, "unknown option %s\nusage: %s %s\n        [-style crt|sharp] [-scale F] [-panels on|off] [-artwork DIR] [-shot SECONDS FILE.bmp]\n", bad.c_str(), argv[0], tek8002_options::usage());
		return 1;
	}
	tek8002_system sys(opt);
	std::string err;
	if (!sys.start(err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
	const bool batch = opt.max_cycles >= 0;
	sys.batch = batch;
	ct8100 &ct = sys.terminal();

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	SDL_Window *win = nullptr; SDL_Renderer *ren = nullptr;
	if (!SDL_CreateWindowAndRenderer("Tektronix CT8100 \xe2\x80\x94 8002A", int(LW * scale), int(LH * scale), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &win, &ren)) {
		fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1;
	}
	SDL_SetRenderLogicalPresentation(ren, LW, LH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_SetRenderVSync(ren, batch ? 0 : 1);
	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TW, TH);
	terminal_view view; view.crt = style != "sharp";
	SDL_SetTextureScaleMode(tex, view.crt ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	SDL_StartTextInput(win);
	bell_sound bell; bell.open();

	// the front panels, to the right of the terminal
	std::vector<panel> panels;
	if (panels_on) {
		int tx = 0, ty = 0, tw = 0, th = 0;
		SDL_GetWindowPosition(win, &tx, &ty); SDL_GetWindowSize(win, &tw, &th);
		for (const char *tag : { "8002a", "fdu" }) {
			panel p; p.tag = tag;
			std::string e;
			if (!p.l.load(artwork + "/" + tag + ".lay", e) || p.l.views.empty()) { fprintf(stderr, "[panel %s: %s]\n", tag, e.c_str()); continue; }
			panels.push_back(std::move(p));
		}
		int py = ty;
		for (panel &p : panels) {
			p.v = &p.l.views[0];
			const float ps = float(scale) * 0.6f;
			if (!SDL_CreateWindowAndRenderer(p.v->name.c_str(), int(p.v->b.w * ps), int(p.v->b.h * ps), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &p.win, &p.ren)) continue;
			SDL_SetRenderLogicalPresentation(p.ren, int(p.v->b.w), int(p.v->b.h), SDL_LOGICAL_PRESENTATION_LETTERBOX);
			SDL_SetWindowPosition(p.win, tx + tw + 12, py);
			py += int(p.v->b.h * ps) + 40;
		}
		SDL_RaiseWindow(win);
	}

	// lamps: the fraction of the frame each was lit, with a short afterglow
	std::vector<std::string> lamp_names;
	for (panel &p : panels) for (auto &it : p.v->items) if (!it.output.empty() && !is_toggle(it.output) && !is_momentary(it.output)) lamp_names.push_back(it.output);
	std::vector<double> lamp_acc(lamp_names.size()), lamp_level(lamp_names.size());
	std::map<std::string, size_t> lamp_index;
	for (size_t i = 0; i < lamp_names.size(); i++) lamp_index[lamp_names[i]] = i;
	long frame_ticks = 0;
	auto panel_of = [&](SDL_WindowID id) -> panel * { for (panel &p : panels) if (p.win && SDL_GetWindowID(p.win) == id) return &p; return nullptr; };

	std::deque<uint8_t> paste;                   // pasted text, typed at the terminal port's rate
	double next_paste = 0;
	const double char_time = 10.0 / 2400;        // 2400 baud (the console UART's usual setting)
	bool turbo = batch, fullscreen = false, quit = false, redraw = true, cursor_on = true;
	Uint64 wall0 = SDL_GetTicksNS(); double emu0 = sys.now();
	auto resync = [&]() { wall0 = SDL_GetTicksNS(); emu0 = sys.now(); };
	std::string title_now;
	auto update_title = [&]() {
		std::string t = "Tektronix CT8100 \xe2\x80\x94 8002A";
		if (turbo && !batch) t += "  [turbo]";
		if (t != title_now) { title_now = t; SDL_SetWindowTitle(win, t.c_str()); }
	};
	update_title();

	for (bool over = false; !quit && !over; ) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_EVENT_QUIT: quit = true; break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (panel *p = panel_of(e.window.windowID)) { SDL_HideWindow(p->win); p->shown = false; }
				else quit = true;
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
				panel *p = panel_of(e.button.windowID);
				if (!p) break;
				SDL_ConvertEventToRenderCoordinates(p->ren, &e);
				if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
					// a click holds a momentary switch for at least a human-like 150 ms of machine time
					if (p->pressed && is_momentary(p->pressed->input)) sys.release_at(p->pressed->input, p->pressed_at + 0.15);
					p->pressed = nullptr; break;
				}
				const lay::item *it = lay::hit(*p->v, e.button.x, e.button.y, [](const lay::item &i) { return !i.input.empty(); });
				if (!it) break;
				p->pressed = it; p->pressed_at = sys.now();
				if (it->input.compare(0, 5, "drive") == 0) {
					const int d = it->input[5] - '0'; std::string er;
					if (!sys.disc(d).empty() && !sys.eject(d, er)) fprintf(stderr, "[drive %d: %s]\n", d, er.c_str());
				} else if (is_toggle(it->input)) sys.input(it->input, !sys.out.get(it->input));
				else sys.input(it->input, 1);
				break;
			}
			case SDL_EVENT_DROP_FILE: {
				panel *p = panel_of(e.drop.windowID);
				if (!p || !e.drop.data) break;
				const std::string path = e.drop.data;
				SDL_ConvertEventToRenderCoordinates(p->ren, &e);
				const lay::item *it = lay::hit(*p->v, e.drop.x, e.drop.y, [](const lay::item &i) { return i.drive >= 0; });
				if (!it) break;
				std::string er;
				if (!sys.mount(it->drive, path, er)) fprintf(stderr, "[drive %d: %s]\n", it->drive, er.c_str());
				break;
			}
			case SDL_EVENT_TEXT_INPUT:
				for (const char *c = e.text.text; *c; c++)
					if (uint8_t(*c) < 0x80) ct.type(uint8_t(*c));
				break;
			case SDL_EVENT_KEY_DOWN: {
				const SDL_Keycode k = e.key.key; const SDL_Keymod m = e.key.mod;
				const bool hot = (m & SDL_KMOD_GUI) || ((m & SDL_KMOD_CTRL) && (m & SDL_KMOD_ALT));
				if (hot) {
					if (k == SDLK_Q) quit = true;
					else if (k == SDLK_V) {
						if (char *t = SDL_GetClipboardText()) {
							for (const char *c = t; *c; c++) {
								uint8_t ch = uint8_t(*c);
								if (ch == '\r') continue;
								if (ch == '\n') ch = '\r';
								if (ch < 0x80) paste.push_back(ch);
							}
							SDL_free(t);
						}
					}
					else if (k == SDLK_C) SDL_SetClipboardText(ct.text().c_str());
					else if (k == SDLK_T) { turbo = !turbo; resync(); update_title(); }
					else if (k == SDLK_F) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); }
					else if (k == SDLK_S) { view.crt = !view.crt; SDL_SetTextureScaleMode(tex, view.crt ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST); redraw = true; }
					else if (k == SDLK_R) sys.press("8002:restart");
					else if (k == SDLK_P) for (panel &p : panels) { p.shown = true; SDL_ShowWindow(p.win); }
					break;
				}
				// the keys that send control characters (letters and symbols come as text)
				const bool cn = m & SDL_KMOD_CTRL;
				switch (k) {
				case SDLK_RETURN: case SDLK_KP_ENTER: ct.type((m & SDL_KMOD_SHIFT) ? 0x0a : 0x0d); break;   // Shift: LINE FEED
				case SDLK_BACKSPACE: ct.type(0x08); break;
				case SDLK_DELETE: ct.type(0x7f); break;                                                   // RUB OUT
				case SDLK_TAB: ct.type(0x09); break;
				case SDLK_ESCAPE: ct.type(0x1b); break;
				default:
					if (cn && !(m & SDL_KMOD_ALT)) {                                                     // CTRL+letter, CTRL+[\]
						if (k >= SDLK_A && k <= SDLK_Z) ct.type(uint8_t(k - SDLK_A + 1));
						else if (k == SDLK_LEFTBRACKET) ct.type(0x1b);
						else if (k == SDLK_BACKSLASH) ct.type(0x1c);
						else if (k == SDLK_RIGHTBRACKET) ct.type(0x1d);
					}
					break;
				}
				break;
			}
			case SDL_EVENT_WINDOW_EXPOSED: redraw = true; break;
			default: break;
			}
		}

		// run the machine: to real time, or flat out for ~14 ms per frame in turbo; batch runs
		// are deterministic, 1/60 s of machine time per frame
		const Uint64 frame0 = SDL_GetTicksNS();
		const double target = batch ? sys.now() + 1.0 / 60 : emu0 + double(frame0 - wall0) / 1e9;
		while (!sys.finished()) {
			const Uint64 now = SDL_GetTicksNS();
			if (batch ? sys.now() >= target : turbo ? now - frame0 > 14'000'000 : (sys.now() >= target || now - frame0 > 30'000'000)) break;
			if (!paste.empty() && sys.now() >= next_paste) {
				const uint8_t c = paste.front(); paste.pop_front();
				ct.type(c);
				next_paste = sys.now() + char_time + (c == '\r' ? 0.25 : 0);   // let the line be taken
			}
			sys.tick();
			for (size_t i = 0; i < lamp_names.size(); i++) lamp_acc[i] += sys.out.get(lamp_names[i]);
			frame_ticks++;
		}
		if (frame_ticks) {
			for (size_t i = 0; i < lamp_names.size(); i++) {
				const double duty = lamp_acc[i] / double(frame_ticks);
				lamp_level[i] = std::max(duty, lamp_level[i] * 0.55);
				lamp_acc[i] = 0;
			}
			frame_ticks = 0;
		}
		if (turbo || sys.now() < target - 0.25) resync();      // never try to catch up a backlog
		if (ct.take_bell()) bell.ring();
		over = sys.finished();                                 // a last frame, with the shots still due
		if (batch) {                                           // batch runs draw only for a capture, or ~30 times a second
			static Uint64 last_draw = 0;
			bool shot_due = false;
			for (auto &sh : shots) if (!sh.file.empty() && (over || sys.now() >= sh.at)) shot_due = true;
			if (!shot_due && SDL_GetTicksNS() - last_draw < 33'000'000) continue;
			last_draw = SDL_GetTicksNS();
		}

		const bool b = batch ? int(sys.now() * 2) % 2 == 0 : (SDL_GetTicksNS() / 500'000'000) % 2 == 0;   // the cursor blinks, 1 Hz
		if (b != cursor_on) { cursor_on = b; redraw = true; }
		if (sys.terminal_dirty) { sys.terminal_dirty = false; redraw = true; }
		if (redraw) {
			ct8100_raster::draw(ct, cursor_on, view.dots.data());
			view.compose();
			SDL_UpdateTexture(tex, nullptr, view.px.data(), TW * 4);
			redraw = false;
		}
		SDL_SetRenderDrawColor(ren, Uint8(BG.r), Uint8(BG.g), Uint8(BG.b), 255);
		SDL_RenderClear(ren);
		const SDL_FRect dst = { float(OVER_X), float(OVER_Y), float(TW), float(TH) };
		SDL_RenderTexture(ren, tex, nullptr, &dst);
		auto level = [&](const lay::item &it) -> float {
			if (is_toggle(it.output) || is_momentary(it.output)) return float(sys.out.get(it.output));   // levers: no afterglow
			if (!it.output.empty()) return float(lamp_level[lamp_index[it.output]]);
			for (panel &p : panels) if (p.pressed == &it) return 1.f;
			return 0.f;
		};
		auto caption = [&](const lay::item &it) -> std::string { return it.drive >= 0 ? (sys.disc(it.drive).empty() ? "(empty)" : base_name(sys.disc(it.drive))) : ""; };
		for (panel &p : panels) {
			if (!p.shown || !p.ren) continue;
			SDL_SetRenderDrawColor(p.ren, 30, 30, 32, 255);
			SDL_RenderClear(p.ren);
			lay::draw(p.ren, p.l, *p.v, level, caption);
		}
		for (auto &sh : shots)
			if (!sh.file.empty() && (over || sys.now() >= sh.at)) {
				if (SDL_Surface *sf = SDL_RenderReadPixels(ren, nullptr)) { SDL_SaveBMP(sf, sh.file.c_str()); SDL_DestroySurface(sf); }
				for (panel &p : panels) {                      // FILE.bmp -> FILE_8002a.bmp, FILE_fdu.bmp
					std::string f = sh.file; const size_t dot = f.rfind('.');
					f = (dot == std::string::npos ? f : f.substr(0, dot)) + "_" + p.tag + ".bmp";
					if (SDL_Surface *sf = SDL_RenderReadPixels(p.ren, nullptr)) { SDL_SaveBMP(sf, f.c_str()); SDL_DestroySurface(sf); }
				}
				sh.file.clear();
			}
		for (panel &p : panels) if (p.shown && p.ren) SDL_RenderPresent(p.ren);
		SDL_RenderPresent(ren);
		if (!turbo && SDL_GetTicksNS() - frame0 < 4'000'000) SDL_Delay(4);
	}

	for (panel &p : panels) { if (p.ren) SDL_DestroyRenderer(p.ren); if (p.win) SDL_DestroyWindow(p.win); }
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	sys.shutdown(false);
	return 0;
}
