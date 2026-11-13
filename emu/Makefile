# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Tektronix 8002A µProcessor Lab emulator.
#   make                       native build in build/ (macOS, Linux, or Windows under MSYS2/MinGW)
#   make gui                   + build/tek8002-gui (SDL3, found with pkg-config)
#   make CROSS=x86_64-w64-mingw32-            Windows .exe files from macOS/Linux, in build-win/
#   make gui CROSS=x86_64-w64-mingw32- SDL3_DIR=/path/to/SDL3-x.y.z/x86_64-w64-mingw32
#                              (the official SDL3-devel-*-mingw package; SDL3.dll is copied next to the .exe)
OPT ?= -O2
CXXFLAGS ?= -std=c++17 $(OPT) -g -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
CPPFLAGS += -Isrc -MMD -MP

ifdef CROSS
CXX := $(CROSS)g++
CC  := $(CROSS)gcc
TARGET_OS := windows
BUILD ?= build-win
endif
ifeq ($(OS),Windows_NT)
TARGET_OS := windows
endif
ifeq ($(TARGET_OS),windows)
EXE := .exe
LDLIBS += -lws2_32
# self-contained executables: no MinGW runtime DLLs needed next to them
LDFLAGS += -static -static-libgcc -static-libstdc++
endif
BUILD ?= build

# the version (../VERSION), compiled into every program (-version)
VERSION := $(shell cat ../VERSION 2>/dev/null || echo unknown)
PATHS_OBJ := $(BUILD)/host/paths.o

DASM_OBJ := $(BUILD)/cpu/s2650/2650dasm.o $(BUILD)/cpu/z80/z80dasm.o $(BUILD)/cpu/m6800/6800dasm.o $(BUILD)/cpu/i8085/8085dasm.o
HOST     := $(PATHS_OBJ) $(BUILD)/host/serial_link.o $(BUILD)/host/net.o $(BUILD)/host/ipc_target.o $(BUILD)/host/ct8100.o
SYSTEM   := $(BUILD)/system/tek8002_system.o $(BUILD)/system/options.o
M8002    := $(BUILD)/machine/tek8002.o $(BUILD)/machine/tek8002_fdu.o $(BUILD)/machine/tek8002_fdc.o $(BUILD)/machine/tek8002_fdd.o $(BUILD)/machine/tek8002_debug.o $(BUILD)/machine/tek8002_asmproc.o $(BUILD)/machine/tek8002_emboard.o $(BUILD)/machine/tek8002_ep6800.o $(BUILD)/machine/tek8002_ep8080.o $(BUILD)/machine/tek8002_epz80.o $(BUILD)/cpu/m6800/m6800.o $(BUILD)/cpu/i8085/i8085.o $(BUILD)/machine/tek8002_syscomm.o $(BUILD)/cpu/z80/superzazu/z80.o $(BUILD)/cpu/s2650/s2650.o

all: $(BUILD)/dasm$(EXE) $(BUILD)/tek8002$(EXE)

$(BUILD)/dasm$(EXE): $(BUILD)/tools/dasm.o $(DASM_OBJ) $(PATHS_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(BUILD)/tek8002$(EXE): $(BUILD)/main8002.o $(SYSTEM) $(M8002) $(HOST) $(DASM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(PATHS_OBJ): ../VERSION
$(PATHS_OBJ): CPPFLAGS += -DTEK_VERSION='"$(VERSION)"'

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -O2 -g -Isrc -c $< -o $@
$(BUILD)/tools/%.o: tools/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# optional graphical front end (SDL3); never part of 'all'.
# SDL3 from pkg-config, or from SDL3_DIR (include/ lib/ bin/ of an SDL3 development package)
ifdef SDL3_DIR
SDL_CFLAGS = -I$(SDL3_DIR)/include
SDL_LIBS   = -L$(SDL3_DIR)/lib -lSDL3
SDL_CHECK  = test -f $(SDL3_DIR)/include/SDL3/SDL.h || { echo "SDL3_DIR=$(SDL3_DIR) has no include/SDL3/SDL.h"; exit 1; }
else
SDL_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL_LIBS   = $(shell pkg-config --libs sdl3 2>/dev/null)
SDL_CHECK  = pkg-config --exists sdl3 || { echo "make gui needs SDL3: brew install sdl3 / apt install libsdl3-dev / pacman -S mingw-w64-x86_64-SDL3, or SDL3_DIR=..."; exit 1; }
endif
# SDL itself is a DLL: link it dynamically, everything else statically (Windows: SDL3.dll is the
# only file needed next to the .exe)
# (SDL3_DIR on Windows: -static stays, and SDL3's import library is named by path, which -static allows)
ifeq ($(TARGET_OS),windows)
ifdef SDL3_DIR
SDL_LIBS    = $(SDL3_DIR)/lib/libSDL3.dll.a
GUI_LDFLAGS = $(LDFLAGS)
else
GUI_LDFLAGS = $(filter-out -static,$(LDFLAGS))
endif
else
GUI_LDFLAGS = $(LDFLAGS)
endif
GUI_OBJ := $(BUILD)/frontend/sdl/main_gui.o $(BUILD)/frontend/sdl/layout.o $(BUILD)/host/ct8100_raster.o
gui: $(BUILD)/tek8002-gui$(EXE)
$(BUILD)/tek8002-gui$(EXE): $(GUI_OBJ) $(SYSTEM) $(M8002) $(HOST) $(DASM_OBJ)
	@$(SDL_CHECK)
	$(CXX) $(GUI_LDFLAGS) -o $@ $^ $(SDL_LIBS) $(LDLIBS)
ifeq ($(TARGET_OS),windows)
ifdef SDL3_DIR
	cp $(SDL3_DIR)/bin/SDL3.dll $(BUILD)/
endif
endif
$(GUI_OBJ): CPPFLAGS += $(SDL_CFLAGS)

$(BUILD)/debug_test$(EXE): $(BUILD)/tests/debug_test.o $(BUILD)/machine/tek8002_debug.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(BUILD)/ct8100_test$(EXE): $(BUILD)/tests/ct8100_test.o $(BUILD)/host/ct8100.o $(BUILD)/host/ct8100_raster.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(BUILD)/fdc_test$(EXE): $(BUILD)/tests/fdc_test.o $(BUILD)/machine/tek8002_fdc.o $(BUILD)/machine/tek8002_fdd.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# unit tests, then the TEKDOS sessions (tests/*.sh need ../tekmato for step 2)
test: all $(BUILD)/debug_test$(EXE) $(BUILD)/fdc_test$(EXE) $(BUILD)/ct8100_test$(EXE)
	$(BUILD)/debug_test$(EXE)
	$(BUILD)/ct8100_test$(EXE)
	$(BUILD)/fdc_test$(EXE) ../roms/fdc
	$(BUILD)/fdc_test$(EXE) ../roms/fdc ../disks/TekDOS_3.3_6800_cpy1.img
	cd .. && tests/boot_ldir.sh && tests/asm_link.sh && FDC=hle tests/asm_link.sh && tests/run6800.sh && tests/run8080.sh && tests/runz80.sh && tests/prototype.sh && tests/parity.sh

clean:
	rm -rf $(BUILD)

.PHONY: all clean test gui
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
