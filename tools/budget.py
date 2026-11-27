#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
"""Flash budget for Ear Olympics: what the art, fonts and music cost, what the planned content would add, and what to watch.

    python3 tools/budget.py            # from the repo root

The app slot on the FM-1 is 0x8DFBC bytes (581,564). This tool counts the DATA in the app's headers (sprites, fonts, music,
tables); it cannot see the compiled code, which only the device build knows. If app/build/earolympics.fwsc exists (after
./build.sh) its size is shown beside the slot as a rough check on the whole image. Exit status is 1 when something is flagged,
so a script may fail on it; tests/run_tests.sh only prints the summary."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLOT = 0x8DFBC
SIZES = {"uint8_t": 1, "int8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4}


def arrays(path):
    """(name, bytes) for each static const array in a header: numbers by their type, char arrays by their characters."""
    if not path.exists():
        return []
    t = path.read_text()
    out = []
    for m in re.finditer(r"static\s+const\s+(?:unsigned\s+)?(\w+)\s+(\w+)((?:\[[^\]]*\])+)\s*=\s*\{", t):
        i, depth = m.end(), 1
        while depth and i < len(t):
            depth += (t[i] == "{") - (t[i] == "}")
            i += 1
        body = t[m.end():i - 1]
        if m.group(1) == "char":                               # one char per pixel: count the characters in the strings
            n = sum(len(s) for s in re.findall(r'"([^"]*)"', body))
        else:
            n = len(re.findall(r"(?<![\w.])(?:0x[0-9a-fA-F]+|-?\d+)(?![\w.])", body)) * SIZES.get(m.group(1), 1)
        out.append((m.group(2), n))
    return out


def total(path):
    return sum(b for _, b in arrays(path))


# category -> (files, a ceiling in bytes: what it may grow to before it is a problem)
CATS = [
    ("Fonts (6 sizes, ASCII)", ["app/build/gen/felucca_font.h"], 80_000),
    ("Cast sprites (11 characters x 2 poses, 96x72, 4-bit)", ["app/eo_cast.h"], 160_000),
    ("Intro music (the movie's sample)", ["app/eo_intro.h"], 90_000),
    ("Title and presents art", ["app/eo_title.h"], 50_000),
    ("Dogtor sprites (1 byte per pixel, unpacked)", ["app/eo_dog.h"], 30_000),
    ("Cat head", ["app/eo_cat.h"], 5_000),
    ("Game tables (Copycat, Dogtor, Simon)", ["app/eo_copycat.h", "app/eo_dogtor.h", "app/eo_simon.h"], 10_000),
]

# what the plans would add: (what, bytes). A pose is 96x72 at 4 bits = 3,456 bytes.
POSE = 96 * 72 // 2
PLANNED = [
    ("Simone: 6 more poses (worried, blank, smug, sunk, thinking, forgetful)", 6 * POSE),
    ("Every character: 4 more poses each (11 x 4)", 11 * 4 * POSE),
    ("One custom font per character at body size, trimmed (6 x ~6 KB)", 6 * 6_000),
    ("One custom font at body size, full ASCII (~9 KB)", 9_000),
    ("A second music sample as long as the intro's", 71_279),
    ("Cutscene stills, 240x240 at 4-bit RLE (each, ~9 KB like the presents card)", 9_196),
]


def main():
    flagged = 0
    rows = []
    for name, files, cap in CATS:
        b = sum(total(ROOT / f) for f in files)
        rows.append((name, b, cap))
    data = sum(b for _, b, _ in rows)
    print(f"App slot: {SLOT:,} bytes ({SLOT // 1024} KB)\n")
    print(f"{'what':56} {'bytes':>9} {'% slot':>7} {'ceiling':>9}")
    for name, b, cap in rows:
        flag = "  <-- OVER ITS CEILING" if b > cap else ("  <-- near its ceiling" if b > cap * 0.85 else "")
        flagged += b > cap
        print(f"{name:56} {b:9,} {b * 100 / SLOT:6.1f}% {cap:9,}{flag}")
    print(f"{'DATA TOTAL':56} {data:9,} {data * 100 / SLOT:6.1f}%")
    fw = ROOT / "app/build/earolympics.fwsc"
    if fw.exists():
        s = fw.stat().st_size
        print(f"\nFirmware package app/build/earolympics.fwsc: {s:,} bytes = {s * 100 / SLOT:.1f}% of the slot (rough: the whole image)")
        if s > SLOT * 0.85:
            print("  <-- the image is over 85% of the slot: stop adding data until something is packed or cut")
            flagged += 1
    else:
        print("\nCode size: unknown here (only the device build has it). After ./build.sh, run this again, or check\n"
              "  ls -l app/build/earolympics.fwsc")
    print("\nWhat the plans would add:")
    room = SLOT * 0.5 - data     # a working rule: keep the DATA under half the slot until the code size is known
    for what, b in PLANNED:
        over = b > room
        print(f"  {what:76} +{b:8,}  {'(would cross 50% of the slot with the data now)' if over else ''}")
    print(f"\nWorking rule: keep the DATA under 50% of the slot ({SLOT // 2:,} bytes) until the code size is known; "
          f"{room:,.0f} bytes of room now.")
    print("Anything that every game would use (fonts, shared sprites, music) is the first to flag.")
    return 1 if flagged else 0


if __name__ == "__main__":
    sys.exit(main())
