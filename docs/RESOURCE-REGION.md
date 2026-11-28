# The resource region (Ear Olympics 0.23.0)

The FM-1's flash has a parking area for data a game does not need in order to start. Ear Olympics keeps its read-only art
and sound there, so the app image holds only code, fonts and tables.

| Where | What | Size |
| --- | --- | --- |
| App slot (0x8DFBC bytes) | code, fonts, cat head, game tables | data 63,061 B (10.8% of the slot); the device build prints the real image size |
| Resource region, 0x97000 to 0xDBFFF | cast sprites, title and presents art, Dogtor sprites, intro music | 204,904 B of 282,592 B (72.5%), after a 32-byte header |
| Panel calibration, 0xDC000 | the platform's; never moved | where the region stops |

`python3 tools/budget.py` prints these numbers from the code (the packer's own sizes).

## How it works

1. `app/res/eo_*.h` are the **source** of the art and sound. They are no longer compiled into the app.
2. At build time `app/gen_res.py` (fm1.toml `gen`) packs them into `app/build/gen/eo_res.bin` and writes `eo_res_map.h` (where each array starts).
3. fm1-base (fm1.toml `resources`) stamps the file's length and CRC-32 into the firmware and puts the file in the update package; the update loader writes it into the region after the app. The file `app/build/earolympics-res.bin` beside the package is for writing it by hand (`base/tools/fm1_resources.py`).
4. At boot the base checks that the region holds exactly this build's file. `plat_res()` then returns a pointer into flash (or nothing).
5. `app/eo_res.h` turns that pointer into the old array names (`EO_CAST`, `EO_INTRO`, `EO_DOG`...), so the drawing and sound code reads them as before.

`backup = true` is required by the base (the resource write reuses the restore's transfer). That also gives the FM-1 a backup and restore of the saved settings over USB (`base/tools/fm1_backup.py`); the panel calibration is never part of it.

## When the region is missing

An install that did not write it, a torn write, or another build's file: the app skips the power-on movie, draws no cast or Dogtor art, plays no intro music, and the game select says ART NOT INSTALLED / Run the install tool again. The games still play. Writing the region later (`fm1_resources.py`, or `fm1_install.py` again) brings the art back without a restart.

## Adding art or sound

Put it in a header in `app/res/` in the same shape, add it to `ITEMS` in `app/gen_res.py` and a pointer in `app/eo_res.h`, then run `tests/run_tests.sh`. It checks every array byte for byte against what the C compiler reads, and `tools/budget.py` shows the room left. Anything the game needs before the region can be read (fonts, tables) stays in the app.

## What is verified where

| Checked | Where |
| --- | --- |
| The packed file matches the source arrays byte for byte | tests/res/res_layout.c (native) |
| Region address, sector count, no overlap with the store or panel, payload fits | tests/res_test.py, tools/budget.py |
| Every screen is pixel-identical to before the move (117 screenshots) | simulator |
| Movie, music, select, games, backup and restore, and the missing-region fallback | app/tests/res_region.txt and the older scripts |
| **The flash layout, the real image size, RAM left for the backup buffer, boot time, the install writing the region, the intro music reading flash at 8 kHz without glitches, a backup and restore on hardware** | **only on the FM-1** |

## Install and what to check on the device

```
cd ~/ear-olympics && git fetch origin
git checkout claude/intro-retime
git pull origin claude/intro-retime
git submodule update --init --recursive
./build.sh
base/.venv/bin/python base/tools/fm1_install.py app/build/earolympics.fwsc
```

1. `./build.sh` ends with a list of checks. Look for `gen_res  204,904 B`, a line `res  ... at 0x97000`, and no `FAIL`. A `SRAM headroom` failure means the backup buffer did not fit.
2. After the install (the installer may say it is writing resources), power-cycle. The movie should play exactly as before: presents card, music, cast, title.
3. If the screen says ART NOT INSTALLED, run `base/.venv/bin/python base/tools/fm1_resources.py app/build/earolympics-res.bin` (stop any sound first: skip the movie with any key).
4. Play Copycat and Dogtor for a minute: the cat, Dogtor and Simone's art should draw with no tearing.
5. Run `python3 tools/budget.py`; it now also reads `app/build/earolympics.bin` for the true image size.
6. Settings still save and load (change the root, press SAVE, power-cycle).
