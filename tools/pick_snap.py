#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
"""Pick one CT8100 snapshot out of a series (tek8002 -snap taken at short intervals).

Prints the path of the first snapshot, in time order, that holds every NEEDLE and stands at a
bare TEKDOS prompt (its last non-blank line is ">"): the screen as it was when that output was
complete, before the next command was typed. Exits 1 if there is none, so that a script that
makes pictures stops instead of showing the wrong moment.
usage: pick_snap.py DIR NEEDLE [NEEDLE ...]
"""
import glob, os, sys

def main():
    d, needles = sys.argv[1], sys.argv[2:]
    for f in sorted(glob.glob(os.path.join(d, '*.scr'))):
        t = open(f, encoding='latin-1').read()
        lines = [l for l in t.split('\n') if l.strip()]
        if lines and lines[-1].strip() == '>' and all(n in t for n in needles):
            print(f)
            return 0
    print('pick_snap: no snapshot in %s holds %r at a prompt' % (d, needles), file=sys.stderr)
    return 1

if __name__ == '__main__':
    sys.exit(main())
