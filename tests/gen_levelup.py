#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
"""Records the graduation tests: ten rounds played right in a row, then the GOOD JOB card.

The simulator's random state is fixed, so every round's melody is the same on every run, but it depends on how the
games draw their random notes. Playing ten rounds needs each melody, so this drives the simulator: it plays what it
has recorded so far, reads the next round's notes (the host-only `expect ansN`), and adds the key presses for them.
It writes app/tests/levelup_copycat.txt and app/tests/levelup_simon.txt. Run it again whenever the way a game draws
its notes (or its timing) changes, and the level-up tests fail:

    ./build.sh --host && python3 tests/gen_levelup.py
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.path.join(ROOT, "app", "build", "host", "earolympics_host")
OUT = os.path.join(ROOT, "app", "tests")
ROUNDS = 8           # level 1 of Copycat graduates after 8 melodies right; Simon after a sequence of 10
SIMON_ROUNDS = 10    # EO_ROUNDS_UP in app.c
HEAR_EXTRA = 0       # no double hearing any more: PLAY replays the melody


def run(lines):
    """Runs a script in the simulator; returns the lines of FAIL output."""
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "s.txt")
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")
        r = subprocess.run([SIM, path, d], capture_output=True, text=True)
        return [l for l in (r.stdout + r.stderr).splitlines() if l.startswith("FAIL")]


def read_values(lines, names):
    """Plays `lines`, then reads the named host values (each asked for as an impossible number: the FAIL says it)."""
    probe = lines + ["expect %s 99999" % n for n in names]
    fails = run(probe)
    vals = {}
    for l in fails:
        m = re.match(r"FAIL expect (\w+) == 99999, got (-?\d+)", l)
        if m:
            vals[m.group(1)] = int(m.group(2))
    missing = [n for n in names if n not in vals]
    if missing or len(fails) != len(vals):
        sys.exit("the recorded script itself failed before the probe:\n  " + "\n  ".join(fails))
    return [vals[n] for n in names]


def simon_step(n):
    cut = 30 * (n - 4) if n > 4 else 0
    return 330 if cut > 320 else 650 - cut


def simon_listen_ms(n):             # how long a sequence of n notes takes to play: when your turn begins
    step = simon_step(n)
    return 400 + step * (n - 1) + max(160, step * 60 // 100)


def gen_copycat():
    L = ["wait 600", "tap HOME", "wait 50", "turn PRESET 1", "wait 30", "expect game 1", "expect level 1", "expect prog 0"]
    for r in range(ROUNDS):
        if r == 0:
            L += ["tap PLAY", "wait 100", "expect state 1", "wait %d" % (2650 + HEAR_EXTRA)]
        else:
            L += ["wait 1950", "expect state 1", "expect text 1", "wait %d" % (2700 + HEAR_EXTRA)]
        L += ["expect state 2", "expect pos 0"]
        keys = read_values(L, ["ans0", "ans1", "ans2"])
        for i, k in enumerate(keys):
            L += ["key %d down" % k, "wait 30"]
            if r == 0 and i == 1:
                L += ["wait 150", "shot cat_sing.png"]
            if i == 2:
                L += ["expect state 3", "expect prog %d" % (r + 1)]
            L += ["key %d up" % k]
    # the last: the ding, then the card (no pause between rounds), the level kept at once
    L += ["wait 1300", "expect state 6", "expect text 8", "expect level 2", "expect prog 0", "expect top 1", "expect open 4",
          "expect store_writes 1", "expect dirty 0", "expect len 0", "shot cat_celebrate.png", "expect note >0", "wait 2000", "expect note 53", "wait 1300",
          "expect state 6", "shot cat_levelup.png"]
    # PLAY goes on at the new level (lights off: level 2), HOME stops, and the level is still there after a power cycle
    L += ["tap PLAY", "wait 100", "expect state 1", "expect level 2", "expect len 3", "expect text 1",
          "expect drone 60", "tap HOME", "wait 50", "expect state 0", "reboot", "wait 600", "tap HOME", "wait 50", "expect game 1",
          "expect level 2", "expect prog 0"]
    return L


def gen_misscost():
    """Three melodies right (progress 3), then a wrong note: the progress falls by two, to 1 (and kept)."""
    L = ["wait 600", "tap HOME", "wait 50", "turn PRESET 1", "wait 30", "expect game 1", "expect prog 0"]
    for r in range(3):
        if r == 0:
            L += ["tap PLAY", "wait 100", "expect state 1", "wait %d" % (2650 + HEAR_EXTRA)]
        else:
            L += ["wait 1950", "expect state 1", "expect text 1", "wait %d" % (2700 + HEAR_EXTRA)]
        L += ["expect state 2", "expect pos 0"]
        keys = read_values(L, ["ans0", "ans1", "ans2"])
        for i, k in enumerate(keys):
            L += ["key %d down" % k, "wait 30"]
            if i == 2:
                L += ["expect state 3", "expect prog %d" % (r + 1)]
            L += ["key %d up" % k]
    L += ["wait 1950", "expect state 1", "wait %d" % (2700 + HEAR_EXTRA), "expect state 2", "expect prog 3",
          "key 26 down", "wait 50", "expect state 4", "expect prog 1", "expect streak 0", "key 26 up",
          "wait 3000", "expect state 5", "expect store_writes 1", "expect dirty 0",
          "tap HOME", "wait 50", "reboot", "wait 600", "tap HOME", "wait 50", "expect game 1", "expect prog 1"]
    return L


def gen_simon():
    L = ["wait 600", "tap HOME", "wait 50", "turn PRESET -1", "wait 30", "expect game 0", "expect level 1"]
    for n in range(1, SIMON_ROUNDS + 1):
        if n == 1:
            L += ["tap PLAY", "wait %d" % (simon_listen_ms(1) + 100)]
        else:
            L += ["wait 1100", "expect state 1", "expect len %d" % n,
                  "wait %d" % (simon_listen_ms(n) + 30)]
        L += ["expect state 2"]
        keys = read_values(L, ["ans%d" % i for i in range(n)])
        for i, k in enumerate(keys):
            L += ["key %d down" % k, "wait 30"]
            if i == n - 1:
                L += ["expect state 3"]
            L += ["key %d up" % k]
    # the tenth note's sequence played right: the ding, then the card, the level kept, the score is 10
    L += ["wait 700", "expect state 6", "expect text 8", "expect level 2", "expect top 1", "expect open 4", "expect score %d" % SIMON_ROUNDS,
          "expect store_writes 1", "expect dirty 0", "expect len 0", "shot levelup.png"]
    L += ["tap PLAY", "wait 100", "expect state 1", "expect level 2", "expect len 1", "expect dim 84608",
          "tap HOME", "wait 50", "expect state 0", "reboot", "wait 600", "tap HOME", "wait 50", "expect level 2", "expect best 10"]
    return L


def write(name, header, lines):
    with open(os.path.join(OUT, name), "w") as f:
        f.write("\n".join(header) + "\n" + "\n".join(lines) + "\n")
    print("  wrote app/tests/%s (%d lines)" % (name, len(lines)))


def main():
    if not os.path.exists(SIM):
        sys.exit("build the simulator first: ./build.sh --host")
    hdr = ["# RECORDED by tests/gen_levelup.py: do not edit; run `python3 tests/gen_levelup.py` after changing how a game",
           "# draws its notes or its timing. States: 0 ready, 1 listen/watch, 2 your turn, 3 nice, 6 the GOOD JOB card."]
    write("levelup_copycat.txt",
          hdr + ["# Copycat: eight melodies played right at level 1 graduate you to level 2, with the GOOD JOB card."],
          gen_copycat())
    write("levelup_simon.txt",
          hdr + ["# Simon: a sequence of ten played right at level 1 graduates you to level 2, with the GOOD JOB card."],
          gen_simon())
    write("misscost.txt",
          hdr + ["# Copycat: a miss takes two points off the progress toward the next level (3 right, a miss: 1)."],
          gen_misscost())
    for n in ("levelup_copycat.txt", "levelup_simon.txt", "misscost.txt"):
        fails = run(open(os.path.join(OUT, n)).read().splitlines())
        if fails:
            sys.exit("%s fails as recorded:\n  %s" % (n, "\n  ".join(fails)))
        print("  ok   %s plays through" % n)


if __name__ == "__main__":
    main()
