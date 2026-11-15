# Is the Z80 firmware's F900 page an opcode-indexed table?

`props.c` measures what each Z80 opcode does on the project's Z80 core; `search.c` tries every
wiring of the opcode's data lines to the PROM's A7–A0 (8! orders × 256 inversion masks) and
scores each by mutual information between the PROM value and each property. Result and reading:
`notes/emulators.md`, "The Z80 board's firmware" (no wiring beats chance).

    cc -O2 -Iemu/src/cpu/z80/superzazu -o props tools/z80_decode/props.c emu/src/cpu/z80/superzazu/z80.c
    cc -O3 -pthread -o search tools/z80_decode/search.c -lm
    ./props > props.txt
    ./search TABLE props.txt F900 [THREADS]    # TABLE: "ADDR VALUE" lines, VALUE -1 unknown

A control: shuffle the page's values and run again; the real table should beat every shuffle.
