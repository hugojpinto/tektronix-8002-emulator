# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Tektronix 8002A µProcessor Lab emulator.
#   make                       native build in build/ (macOS, Linux, or Windows under MSYS2/MinGW)
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

DASM_OBJ := $(BUILD)/cpu/s2650/2650dasm.o $(BUILD)/cpu/z80/z80dasm.o
M8002    := $(BUILD)/machine/tek8002.o $(BUILD)/machine/tek8002_fdu.o $(BUILD)/machine/tek8002_debug.o $(BUILD)/cpu/s2650/s2650.o

all: $(BUILD)/dasm$(EXE) $(BUILD)/tek8002$(EXE)

$(BUILD)/dasm$(EXE): $(BUILD)/tools/dasm.o $(DASM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(BUILD)/tek8002$(EXE): $(BUILD)/main8002.o $(M8002) $(DASM_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -O2 -g -Isrc -c $< -o $@
$(BUILD)/tools/%.o: tools/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
