#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Package a binary release: the programs with the ROMs, discs and front-panel artwork beside them
# (the programs look for roms/ and artwork/ next to themselves), as a .tar.gz, or a .zip for
# Windows. Used by .github/workflows/release.yml; works locally too.
# usage: tools/package_release.sh PLATFORM BUILD_DIR OUT_DIR    (e.g. macos-universal emu/build dist)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME=tektronix-8002-emulator
BINS=(tek8002 tek8002-gui dasm)
PLATFORM="$1"; BUILD="$2"; OUT="$3"
[ -n "$PLATFORM" ] && [ -d "$BUILD" ] && [ -n "$OUT" ] || { echo "usage: $0 PLATFORM BUILD_DIR OUT_DIR"; exit 1; }
VERSION="$(cat "$ROOT/VERSION")"
EXE=; case "$PLATFORM" in windows*) EXE=.exe;; esac
D="$OUT/$NAME-$VERSION-$PLATFORM"
rm -rf "$D"; mkdir -p "$D"
for b in "${BINS[@]}"; do cp "$BUILD/$b$EXE" "$D/"; done
[ -f "$BUILD/SDL3.dll" ] && cp "$BUILD/SDL3.dll" "$D/"
cp -R "$ROOT/roms" "$ROOT/disks" "$D/"
cp -R "$ROOT/emu/artwork" "$D/artwork"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/NOTICE.md" "$ROOT/CHANGELOG.md" "$ROOT/VERSION" "$D/"
X=./; [ -n "$EXE" ] && X=
cat > "$D/QUICKSTART.txt" <<TXT
Tektronix 8002A µProcessor Lab emulator $VERSION ($PLATFORM)

  cp disks/TekDOS_3.1_Z80.img z80.img
  ${X}tek8002$EXE -rw -0 z80.img        the lab on this terminal, booting TEKDOS Z80 3.1;
                                     Ctrl-] opens the monitor, q quits
  ${X}tek8002-gui$EXE -rw -0 z80.img    the CT8100 window and the 8002A and disc unit front panels

Then, at TEKDOS's prompt, Tektronix's own Z80 demonstration:
  ASM DEMO;O DEMO;A DEMO%S
  LINK DEMO;L DEMO;K DEMO;O
  LOAD DEMO;L
  PATCH 500 0102030405
  DEBUG
  BKPT 0108 R
  GO 100
  DSTAT

Keep roms/ and artwork/ beside the programs. Without -rw a disc is write-protected (ASM then
fails with I/O ERROR 0F); use -rw on a copy. TEKDOS takes upper case only (TTY LOCK is on).
${X}tek8002$EXE -version prints the version. Options, tutorials and the hardware notes:
https://github.com/hugojpinto/tektronix-8002-emulator
TXT
case "$PLATFORM" in
macos*) echo "macOS: the programs are not notarised. Before the first run: xattr -dr com.apple.quarantine ." >> "$D/QUICKSTART.txt";;
esac
cd "$OUT"
if [ -n "$EXE" ]; then rm -f "$NAME-$VERSION-$PLATFORM.zip"; zip -qr "$NAME-$VERSION-$PLATFORM.zip" "$NAME-$VERSION-$PLATFORM"; echo "$OUT/$NAME-$VERSION-$PLATFORM.zip"
else tar czf "$NAME-$VERSION-$PLATFORM.tar.gz" "$NAME-$VERSION-$PLATFORM"; echo "$OUT/$NAME-$VERSION-$PLATFORM.tar.gz"; fi
