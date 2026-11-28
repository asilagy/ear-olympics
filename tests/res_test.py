#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
"""The resource region's set-up, checked without a device (tests/run_tests.sh runs it after the host build):
  - the configuration the base needs is all there (fm1.toml backup, gen, resources; app.h APP_RES_BASE, APP_RES_SECTORS, the backup hooks)
  - the region's place in the flash obeys the base's rules (the same checks device/res.c and tools/build.py make on the device build)
  - the packer is steady (the same file every time), the file fits, and what it wrote to the build folder is what it builds
  - the packer refuses a broken header (a short array, a row table that does not fit) instead of packing it
  - the art and sound headers are not compiled into the app any more (that would put them back in the image)
Only the device can say that the flash really holds it: see docs/RESOURCE-REGION.md."""
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import budget                                    # noqa: E402  (also puts app/ on the path for gen_res)
import gen_res                                   # noqa: E402

fails = 0


def check(name, ok, why=""):
    global fails
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f": {why}"))
    fails += not ok


toml = (ROOT / "app/fm1.toml").read_text()
apph = (ROOT / "app/app.h").read_text()
appc = (ROOT / "app/app.c").read_text()

# 1. the configuration
check("fm1.toml: backup = true", re.search(r"^backup\s*=\s*true", toml, re.M), "the resource write is the restore's transfer")
check("fm1.toml: gen runs the packer", re.search(r'^gen\s*=\s*\[\s*"gen_res\.py \{gen\}"', toml, re.M))
check("fm1.toml: resources = the packed file", re.search(r'^resources\s*=\s*"\{gen\}/eo_res\.bin"', toml, re.M))
check("app.h: APP_RES_BASE and APP_RES_SECTORS", budget.RES_BASE is not None and budget.RES_SECTORS is not None)
for need in ("APP_BACKUP_OBJS", "APP_BACKUP_MAGIC", "APP_BACKUP_VERSION", "app_backup_playing", "app_backup_flush",
             "app_backup_hold", "app_backup_restored"):
    check(f"app.h: {need}", need in apph)
    if need.startswith("app_"):
        check(f"app.c defines {need}", re.search(rf"^\w[\w ]*\b{need}\(", appc, re.M))
check("the backup leaves out the panel's calibration", re.search(r"APP_BACKUP_OBJS\s*\{OBJ_SETTINGS\}", apph))

# 2. the region's place in the flash
check("the region sits in the data area, whole sectors, clear of the store, below the panel object",
      not budget.layout_errors(), "; ".join(budget.layout_errors()))
check("the region is 0x97000..0xDBFFF", (budget.RES_LO, budget.RES_HI) == (0x97000, 0xDC000), f"{budget.RES_LO:#x}..{budget.RES_HI:#x}")
check("the panel object is at 0xDC000 and the region ends there", 0xDC000 in budget.store_map() and budget.RES_HI == 0xDC000)
check("the region holds 282,592 bytes after its header", budget.RES == 282_592, str(budget.RES))

# 3. the packer
a, b = gen_res.build(), gen_res.build()
check("the packer makes the same file every time", a["payload"] == b["payload"])
check("the payload fits the region", len(a["payload"]) <= budget.RES, f"{len(a['payload']):,} B of {budget.RES:,} B")
check("every array starts on a 4-byte boundary", all(o % 4 == 0 for o in a["offsets"].values()))
built = ROOT / "app/build/gen/eo_res.bin"
check("app/build/gen/eo_res.bin is what the packer builds now", built.exists() and built.read_bytes() == a["payload"],
      "run ./build.sh --host")
mapfile = ROOT / "app/build/gen/eo_res_map.h"
check("eo_res_map.h is what the packer writes now", mapfile.exists() and mapfile.read_text() == gen_res.map_header(a),
      "run ./build.sh --host")
check("the sizes are what the cast, music, title and dog should be",
      (a["sizes"]["CAST"], a["sizes"]["INTRO"], a["sizes"]["PRES_RLE"], a["sizes"]["TITLE_RLE"], a["sizes"]["DOG"]) ==
      (76032, 71279, 9196, 25042, 21952), str(a["sizes"]))

# 4. the packer refuses broken headers
real = gen_res.RES
with tempfile.TemporaryDirectory() as d:
    tmp = Path(d)
    for f in real.glob("eo_*.h"):
        shutil.copy(f, tmp / f.name)
    gen_res.RES = tmp
    try:
        def broken(fname, old, new):
            p = tmp / fname
            text = p.read_text()
            p.write_text(text.replace(old, new, 1))
        # a palette with a colour missing
        broken("eo_title.h", "0x0000,0xEE10,0xFC63,", "0x0000,0xFC63,")
        try:
            gen_res.build()
            check("a palette with a colour missing is refused", False, "it was packed")
        except gen_res.ResError as e:
            check("a palette with a colour missing is refused", "EO_PRES_PAL" in str(e), str(e))
        shutil.copy(real / "eo_title.h", tmp / "eo_title.h")
        # a Dogtor row one pixel short
        broken("eo_dog.h", '"................................................"', '"..............................................."')
        try:
            gen_res.build()
            check("a Dogtor row one pixel short is refused", False, "it was packed")
        except gen_res.ResError as e:
            check("a Dogtor row one pixel short is refused", "wide" in str(e), str(e))
        shutil.copy(real / "eo_dog.h", tmp / "eo_dog.h")
        # a row table that no longer ends where its picture ends
        broken("eo_title.h", "9196};", "9194};")
        try:
            gen_res.build()
            check("a row table that does not fit its picture is refused", False, "it was packed")
        except gen_res.ResError as e:
            check("a row table that does not fit its picture is refused", "row table" in str(e), str(e))
        shutil.copy(real / "eo_title.h", tmp / "eo_title.h")
        check("the untouched copy still packs", gen_res.build()["payload"] == a["payload"])
    finally:
        gen_res.RES = real

# 5. the source headers stay out of the image
for h in ("eo_cast.h", "eo_intro.h", "eo_title.h", "eo_dog.h", "eo_van.h"):
    check(f"app.c does not include {h} (it lives in app/res/ now)", f'"{h}"' not in appc and not (ROOT / "app" / h).exists())

print("RES SET-UP FAILED" if fails else "the resource region's set-up checks out (the flash itself: only on the device)")
sys.exit(1 if fails else 0)
