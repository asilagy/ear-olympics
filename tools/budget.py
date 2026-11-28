#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
"""Flash budget for Ear Olympics: what the code's data and the resource region hold, what the planned content would add, and what to watch.

    python3 tools/budget.py            # from the repo root

Two places in the FM-1's flash hold our data:
  the app slot (0x8DFBC = 581,564 bytes): the code, the fonts, the cat head and the game tables: what the app needs to start;
  the resource region (app.h APP_RES_BASE, APP_RES_SECTORS; fm1.toml resources; fm1-base device/res.c): 0x97000..0xDBFFF, 282,592
  bytes after a 32-byte header, holding the read-only art and sound. app/gen_res.py packs it from app/res/*.h and this tool asks it
  for the real sizes, so what is printed is what is built. The panel-calibration object sits at 0xDC000, where the region stops.

This tool counts the DATA in the app's headers; it cannot see the compiled code, which only the device build knows. After
./build.sh the image app/build/earolympics.bin and the package are measured too. Exit status is 1 when something is flagged, so a
script may fail on it; tests/run_tests.sh only prints the summary."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "app"))
import gen_res                                   # noqa: E402  (the packer: the sizes below are the ones it builds)

SLOT = 0x8DFBC                                  # the app slot: the code, and what the app needs to start
DATA_LO, DATA_HI = 0x97000, 0xE0000             # fm1-base plat.h PLAT_DATA_LO / PLAT_DATA_HI: where data may live (no app_end here)
SECTOR, HEAD = 0x1000, 32                       # a flash sector; the region's header (res.c RES_HEAD)
SIZES = {"uint8_t": 1, "int8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4}


def app_h_number(name):
    """a number #defined in app/app.h, or None"""
    m = re.search(rf"^\s*#\s*define\s+{name}\s+(0[xX][0-9A-Fa-f]+|\d+)[uU]?\b", (ROOT / "app/app.h").read_text(), re.M)
    return int(m[1], 0) if m else None


def store_map():
    """the store's objects (app.h APP_STORE_MAP): each is an A/B pair of two 4 KiB sectors"""
    m = re.search(r"#\s*define\s+APP_STORE_MAP\s*\{([^}]*)\}", (ROOT / "app/app.h").read_text())
    return [int(v, 0) for v in re.findall(r"0[xX][0-9A-Fa-f]+", m[1])] if m else []


RES_BASE, RES_SECTORS = app_h_number("APP_RES_BASE"), app_h_number("APP_RES_SECTORS")
RES_LO = RES_BASE or 0
RES_HI = RES_LO + (RES_SECTORS or 0) * SECTOR      # the region's end (exclusive): 0xDC000, where the panel's object starts
RES = (RES_SECTORS or 0) * SECTOR - HEAD            # 282,592 bytes (276 KB) after the 32-byte header


def layout_errors():
    """the region's place in the flash, checked as device/res.c and tools/build.py check it (they only run on the device build)"""
    if RES_BASE is None or RES_SECTORS is None:
        return ["app.h has no APP_RES_BASE / APP_RES_SECTORS (the resource region is not set up)"]
    errors = []
    if RES_BASE % SECTOR:
        errors.append(f"APP_RES_BASE {RES_BASE:#x} is not on a 4 KiB sector")
    if RES_BASE < DATA_LO or RES_HI > DATA_HI:
        errors.append(f"the region {RES_BASE:#x}..{RES_HI:#x} is outside the data area {DATA_LO:#x}..{DATA_HI:#x}")
    for at in store_map():                       # (each object keeps a pair of sectors)
        if at < RES_HI and RES_BASE < at + 2 * SECTOR:
            errors.append(f"the region {RES_BASE:#x}..{RES_HI:#x} overlaps the store object at {at:#x}")
    return errors


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


# category -> (files or packed items, a ceiling in bytes: what it may grow to before it is a problem, where)
# where: "slot" (in the app image: counted from the header) or "res" (in the resource region: the size gen_res.py packs, by item name)
CATS = [
    ("Fonts (6 sizes, ASCII)", ["app/build/gen/felucca_font.h"], 80_000, "slot"),
    ("Cast sprites (11 characters x 2 poses, 96x72, 4-bit)", ["CAST_PAL", "CAST"], 160_000, "res"),
    ("Intro music (the movie's sample)", ["INTRO"], 90_000, "res"),
    ("Title and presents art", ["PRES_PAL", "PRES_ROW", "PRES_RLE", "TITLE_PAL", "TITLE_ROW", "TITLE_RLE"], 50_000, "res"),
    ("Dogtor sprites (1 byte per pixel, unpacked, 7 poses)", ["DOG"], 30_000, "res"),
    ("Cat head", ["app/eo_cat.h"], 5_000, "slot"),
    ("Game tables (Copycat, Dogtor, Simon)", ["app/eo_copycat.h", "app/eo_dogtor.h", "app/eo_simon.h"], 10_000, "slot"),
]

# what the plans would add: (what, bytes, where). A pose is 96x72 at 4 bits = 3,456 bytes.
POSE = 96 * 72 // 2
PLANNED = [
    ("Simone: 6 more poses (worried, blank, smug, sunk, thinking, forgetful)", 6 * POSE, "res"),
    ("Every character: 4 more poses each (11 x 4)", 11 * 4 * POSE, "res"),
    ("One custom font per character at body size, trimmed (6 x ~6 KB)", 6 * 6_000, "slot"),
    ("One custom font at body size, full ASCII (~9 KB)", 9_000, "slot"),
    ("A second music sample as long as the intro's", 71_279, "res"),
    ("Cutscene stills, 240x240 at 4-bit RLE (each, ~9 KB like the presents card)", 9_196, "res"),
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
    errors = layout_errors()
    try:
        packed = gen_res.build()
    except gen_res.ResError as e:
        errors.append(f"gen_res: {e}")
        packed = None
    rows = []
    for name, files, cap, where in CATS:
        if where == "res":
            b = sum(packed["sizes"][f] for f in files) if packed else 0
        else:
            b = sum(total(ROOT / f) for f in files)
        rows.append((name, b, cap, where))
    in_slot = sum(b for _, b, _, w in rows if w == "slot")
    in_res = packed["payload"].__len__() if packed else 0          # (the arrays and the padding between them)
    print(f"App slot: {SLOT:,} bytes ({SLOT // 1024} KB).  Resource region: {RES:,} bytes ({RES / 1024:.0f} KB) after its {HEAD}-byte header"
          f" at {RES_LO:#x}..{RES_HI - 1:#x}\n")
    print(f"{'what':56} {'bytes':>9} {'% place':>8} {'ceiling':>9}")
    for name, b, cap, where in rows:
        room = SLOT if where == "slot" else RES
        flag = "  <-- OVER ITS CEILING" if b > cap else ("  <-- near its ceiling" if b > cap * 0.85 else "")
        flagged += b > cap
        print(f"{name:56} {b:9,} {b * 100 / room:7.1f}% {cap:9,}{flag}   [{where}]")
    print(f"{'DATA TOTAL IN THE APP SLOT':56} {in_slot:9,} {in_slot * 100 / SLOT:7.1f}%")
    print(f"{'RESOURCE REGION: payload (with 4-byte alignment)':56} {in_res:9,} {in_res * 100 / RES:7.1f}%   room left {RES - in_res:,} B")
    for e in errors:
        print(f"  <-- LAYOUT: {e}")
        flagged += 1
    if not errors:
        print(f"  layout ok: whole sectors of the data area, below the panel object at {RES_HI:#x}, clear of the store; "
              f"the payload and its header fit in {RES_SECTORS} sectors")
    if in_res > RES:
        print(f"  <-- THE ART AND SOUND DO NOT FIT THE REGION by {in_res - RES:,} bytes")
        flagged += 1
    elif in_res > RES * 0.85:
        print("  <-- the region is over 85% full")
    img = ROOT / "app/build/earolympics.bin"
    fw = ROOT / "app/build/earolympics.fwsc"
    if img.exists():
        s = img.stat().st_size
        print(f"\nApp image app/build/earolympics.bin: {s:,} bytes = {s * 100 / SLOT:.1f}% of the slot (the whole image: code and data)")
        if s > SLOT * 0.85:
            print("  <-- the image is over 85% of the slot: stop adding data until something is packed or cut")
            flagged += 1
    else:
        print("\nCode size: unknown here (only the device build has it). After ./build.sh, run this again, or check\n"
              "  ls -l app/build/earolympics.bin")
    if fw.exists():
        print(f"Package app/build/earolympics.fwsc: {fw.stat().st_size:,} bytes (it carries the region's file too, so it is bigger than the image)")
    room_slot, room_res = SLOT * 0.85 - in_slot, RES - in_res
    print("\nWhat the plans would add:")
    for what, b, where in PLANNED:
        over = b > (room_slot if where == "slot" else room_res)
        print(f"  {what:76} +{b:8,}  {'(would not fit its place now)' if over else ''}   [{where}]")
    reserved = sum(b for _, b in RESERVES)
    print("\nReserved for what is planned (proposed allowances; all art and sound, so all for the region):")
    for what, b in RESERVES:
        print(f"  {what:100} {b:8,}")
    res_total = in_res + reserved
    print(f"  {'IF EVERYTHING PLANNED SHIPS (region now + reserves)':100} {res_total:8,}")
    print(f"  against {RES:,} B of region ({res_total * 100 / RES:.0f}%)")
    if res_total > RES:
        print(f"  <-- THE PLAN DOES NOT FIT even in the resource region, by {res_total - RES:,} bytes. Compress, shorten the music, "
              "use letterbox or 2-bit stills, draw scenes live, share art between games.")
        flagged += 1
    else:
        print("  the plan fits in the resource region.")
    print(f"\nWorking rules: keep the image (code and data) under 85% of the slot ({int(SLOT * 0.85):,} bytes; the device build measures it);"
          f" keep the region under 85% ({int(RES * 0.85):,} bytes) until the plan is settled; {room_res:,} bytes of region room now.")
    print("Art and sound that only gets read goes in app/res/ (gen_res.py packs it); what the game needs to start stays in the slot.")
    return 1 if flagged else 0


if __name__ == "__main__":
    sys.exit(main())
