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
SLOT = 0x8DFBC                                  # the app slot: the code, and what the app needs to start
# The resource region (fm1.toml resources, device/res.c, plat_res): read-only data kept OUT of the image in the flash's data
# area (0x97000..0xDFFFF), written from the package by the update loader and read in place. Our panel-calibration object sits
# at 0xDC000, so the region is 0x97000..0xDBFFF. Needs backup = true and APP_RES_BASE / APP_RES_SECTORS in app.h (not set up yet).
RES_LO, RES_HI = 0x97000, 0xDC000
RES = RES_HI - RES_LO - 32                        # 282,592 bytes (276 KB) after the 32-byte header
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
# where: "slot" (in the app image: needed to start) or "res" (could move to the resource region: read-only art and sound)
CATS = [
    ("Fonts (6 sizes, ASCII)", ["app/build/gen/felucca_font.h"], 80_000, "slot"),
    ("Cast sprites (11 characters x 2 poses, 96x72, 4-bit)", ["app/eo_cast.h"], 160_000, "res"),
    ("Intro music (the movie's sample)", ["app/eo_intro.h"], 90_000, "res"),
    ("Title and presents art", ["app/eo_title.h"], 50_000, "res"),
    ("Dogtor sprites (1 byte per pixel, unpacked)", ["app/eo_dog.h"], 30_000, "res"),
    ("Cat head", ["app/eo_cat.h"], 5_000, "slot"),
    ("Game tables (Copycat, Dogtor, Simon)", ["app/eo_copycat.h", "app/eo_dogtor.h", "app/eo_simon.h"], 10_000, "slot"),
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


# RESERVES: what we have said we will build, as proposed allowances (change them here as the plans firm up). The tool adds
# them to what is in the code and tells you if the whole plan would not fit.
RESERVES = [
    ("Cast poses: game hosts to 4, everyone else to 3", 17 * POSE),
    ("Cutscene stills: ~5 full-screen pictures at ~12 KB (live-drawn scenes, like the prologue, cost almost nothing)", 5 * 12_000),
    ("Three more games' sprites and audio (Name That Interval, Beep Sweep, Tweet Sheet), ~20 KB each", 3 * 20_000),
    ("The shuffled end-game and finale: extra sprites and audio", 20_000),
]


def main():
    flagged = 0
    rows = []
    for name, files, cap, where in CATS:
        b = sum(total(ROOT / f) for f in files)
        rows.append((name, b, cap, where))
    data = sum(b for _, b, _, _ in rows)
    in_res = sum(b for _, b, _, w in rows if w == "res")
    print(f"App slot: {SLOT:,} bytes ({SLOT // 1024} KB)\n")
    print(f"{'what':56} {'bytes':>9} {'% slot':>7} {'ceiling':>9}")
    for name, b, cap, where in rows:
        flag = "  <-- OVER ITS CEILING" if b > cap else ("  <-- near its ceiling" if b > cap * 0.85 else "")
        flagged += b > cap
        print(f"{name:56} {b:9,} {b * 100 / SLOT:6.1f}% {cap:9,}{flag}   [{where}]")
    print(f"{'DATA TOTAL':56} {data:9,} {data * 100 / SLOT:6.1f}%")
    print(f"\nTwo places to keep it: the app slot ({SLOT:,} B: code and what is needed to start) and, once set up, the resource region\n"
          f"({RES:,} B: read-only art and sound). Today everything is in the slot. Moving the [res] items there frees {in_res:,} B of the slot.")
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
    reserved = sum(b for _, b in RESERVES)
    print("\nReserved for what is planned (proposed allowances):")
    for what, b in RESERVES:
        print(f"  {what:100} {b:8,}")
    full = data + reserved
    res_total = in_res + reserved                       # the reserves are all art and sound: they could live in the region
    print(f"  {'IF EVERYTHING PLANNED SHIPS (data now + reserves)':100} {full:8,}")
    print(f"  of which art and sound for the resource region: {res_total:,} B against {RES:,} B of room"
          f" ({res_total * 100 / RES:.0f}%)")
    if res_total > RES:
        print(f"  <-- THE PLAN DOES NOT FIT even with a resource region, by {res_total - RES:,} bytes. Compress, shorten the music, "
              "use letterbox or 2-bit stills, draw scenes live, share art between games.")
        flagged += 1
    else:
        print("  the plan fits in the resource region.")
    print("  (Without a resource region it would be " f"{full * 100 / SLOT:.0f}% of the app slot before any code.)")
    print(f"\nWorking rule until the resource region exists: keep the DATA in the slot under 50% of it ({SLOT // 2:,} bytes);"
          f" {room:,.0f} bytes of room now.")
    print("Anything that every game would use (fonts, shared sprites, music) is the first to flag.")
    return 1 if flagged else 0


if __name__ == "__main__":
    sys.exit(main())
