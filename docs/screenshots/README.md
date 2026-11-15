# Screenshots

Every picture here is made by a script from a batch run of the emulator, and comes out byte-identical on every run. The command sequences are those of the session tests in `tests/`.

## The CT8100 screens (`tools/make_screenshots.sh`)

The CT8100's screen as the emulator holds it, drawn on the raster the GUI uses (`emu/src/host/ct8100_raster.*`): 80 × 24 characters, each a 5 × 8 dot glyph in an 8 × 10 dot field, so 640 dots × 240 scan lines. Every scan line is doubled, the second dimmer, for 640 × 480, and the picture sits on a tube face with a phosphor glow (`tools/render_screens.py --crt`). The glyphs are read from `emu/src/host/term_font.inc`, the font both front ends use.

The CT8100's character generator and its phosphor colour are not documented (`notes/ct8100.md`). The font is the one the sibling 8550 emulator draws its CT8500 with: the Tektronix 308's character ROM for upper case, and a lower case drawn to match. The green is the GUI's.

Each session runs twice on fresh copies of its discs. The first run finds where it ends; the second saves the screen (`-snap`) every 0.05 s, and `tools/pick_snap.py` picks the first frame that holds the wanted output at a bare `>` prompt. A `-snap` is text, so the pictures show no cursor, and display fields' looks are not kept (TEKDOS uses none).

| Picture | Session |
|---|---|
| `01_ldir.png` | boot TEKDOS 3.3 (6800) and `LDIR` |
| `02_lower_case.png` | `ldir` in lower case: ERROR 03 |
| `03_backspace_rubout.png` | BACK SPACE overwrites; RUB OUT echoes the deleted character |
| `04_write_protected.png` | ASM without `-rw`: I/O ERROR=0F |
| `10_z80_build.png` | Tektronix's Z80 DEMO: ASM, LINK, LOAD, PATCH, GO, DUMP |
| `11_z80_trace.png` | `DEBUG`, `TRACE ALL`, `GO 100` |
| `12_z80_debug_ended.png` | BKPT after `*DOS* EOJ`: DEBUG has ended |
| `13_z80_debug.png` | BKPT, DSTAT, SET, TRACE ALL STEP |
| `14_z80_halt.png` | HALT: ERROR 46, then a break under DEBUG |
| `20_m68_run.png` | a 6800 program prints by SVC |
| `21_m68_trace.png` | the 6800 traced |
| `22_m68_debug.png` | 6800 BKPT, DSTAT, SET, STEP |
| `30_asm_link.png`, `31_asm_listing.png` | ASM and LINK on the Assembler Processor; the listing |
| `40_syscomm.png` | LDIR to LPT1, COPY to REMO and PPTP |
| `50_format_dup.png` | FORMAT, DUP and LDIR of a never-formatted disc |
| `60_mode1.png`, `61_mode2_trace.png` | modes 1 and 2 against `tools/proto_target.py` |

## The RTPA tutorial (`tools/make_rtpa_shots.sh`)

`rtpa/*.png`, the pictures in `docs/RTPA.md`: one TEKDOS 8080 session with `-rtpa` that types the tutorial's commands in order, snapped and picked by content as above.

| Picture | What |
|---|---|
| `rtpa/r1_break.png` | `EV`, then the first break on event 1 |
| `rtpa/r2_drt.png` | break on event 2 at 0300; `DR 5` |
| `rtpa/r3_count.png` | `CN C` and `CN U`: the count from `G` to the break |
| `rtpa/r4_delay.png` | pass count 20, then a delay of 500 cycles: COUNT 739 |
| `rtpa/r5_arm.png` | `BIF ARM`: 461 cycles between two events |
| `rtpa/r6_ind_c.png` | `BIF IND C`: break lines printed, the program runs on |
| `rtpa/r7_frz.png` | `BIF FRZ` and `DR 11`: the trace frozen at event 1 |
| `rtpa/r8_lim.png` | `BIF LIM`: event 1 within event 2's address window |

## The graphical front end (`tools/make_tutorial_shots.sh`, `tools/make_hero.sh`)

`tek8002-gui -shot` captures of the CT8100 window and both front panels, run with `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software` and composed on a backdrop by `tools/compose_gui.py`. Batch runs of the GUI advance 1/60 s of machine time per frame, so the captures are deterministic.

| Picture | What |
|---|---|
| `tutorials/t1_boot.png` | four seconds after power-up: the bootstrap PROM reading TEKDOS from drive 0 |
| `tutorials/t1_ldir.png` | TEKDOS up, after `LDIR` |
| `hero.png` | the README picture: Tektronix's Z80 DEMO traced under DEBUG |

The front panels are drawn after photos of a surviving pair of units and 070-2701 Fig. 2-6 (`tools/gen_panels.py`). The drive-busy dots on the doors are an emulator aid.

## Regenerating

```
make -C emu && make -C emu gui
tools/make_screenshots.sh           # needs ../tekmato and python3 with Pillow
tools/make_tutorial_shots.sh
tools/make_hero.sh
tools/make_rtpa_shots.sh
```
Each takes an optional work directory. `git status docs/` stays clean unless something visible changed.
