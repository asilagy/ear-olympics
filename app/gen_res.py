#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
"""Pack Ear Olympics' read-only art and sound into one file for the FM-1's resource region.

    python3 gen_res.py GEN_DIR          (the build runs this: fm1.toml gen = ["gen_res.py {gen}"])

It reads the five SOURCE headers in app/res/ (they are no longer compiled into the app) and writes two files into GEN_DIR:

    eo_res.bin      the payload fm1.toml resources points at: every array, little-endian, each starting on a 4-byte
                    boundary, in the order of ITEMS below. (The base tool adds the 32-byte header and the CRC.)
    eo_res_map.h    where each array starts (EO_RES_OFF_*), how long the payload is (EO_RES_LEN), and the sizes
                    (EO_CAST_N ...) that the source headers #define. app/eo_res.h turns these into pointers.

The numbers are checked as they are read: an array that does not have the size its declaration says, or a row table that does
not end where its picture ends, stops the build with a message rather than packing a broken picture. tools/budget.py and
tests/res_test.py import this file, so the size they report is the size that is built."""
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
RES = HERE / "res"
ALIGN = 4                                        # every array starts on a 4-byte boundary (the 16-bit tables are read in place)

# (map name, source file, C symbol, how to store it): "u8", "u16" or "char" (rows of letters, each followed by a 0 byte, as C does)
ITEMS = [
    ("CAST_PAL", "eo_cast.h", "EO_CAST_PAL", "u16"),
    ("CAST", "eo_cast.h", "EO_CAST", "u8"),
    ("INTRO", "eo_intro.h", "EO_INTRO", "u8"),
    ("PRES_PAL", "eo_title.h", "EO_PRES_PAL", "u16"),
    ("PRES_ROW", "eo_title.h", "EO_PRES_ROW", "u16"),
    ("PRES_RLE", "eo_title.h", "EO_PRES_RLE", "u8"),
    ("TITLE_PAL", "eo_title.h", "EO_TITLE_PAL", "u16"),
    ("TITLE_ROW", "eo_title.h", "EO_TITLE_ROW", "u16"),
    ("TITLE_RLE", "eo_title.h", "EO_TITLE_RLE", "u8"),
    ("DOG", "eo_dog.h", "EO_DOG", "char"),
    ("VAN", "eo_van.h", "EO_VAN", "u8"),                     # Van's 7 poses and Meow's 4 extra ones (cast palettes), then his room
    ("MEOWX", "eo_van.h", "EO_MEOWX", "u8"),
    ("ROOM_PAL", "eo_van.h", "EO_ROOM_PAL", "u16"),
    ("ROOM_ROW", "eo_van.h", "EO_ROOM_ROW", "u16"),
    ("ROOM_RLE", "eo_van.h", "EO_ROOM_RLE", "u8"),
]
SIZE = {"u8": 1, "u16": 2, "char": 1}
NUM = re.compile(r"(?<![\w.])(0[xX][0-9a-fA-F]+|\d+)(?![\w.])")


class ResError(Exception):
    pass


def defines(text):
    """the single-line #define EO_x values of a header: name -> (the text, an integer when it is one)"""
    out = {}
    for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(EO_\w+)[ \t]+([^\n]*?)[ \t]*(?:/\*.*)?$", text, re.M):
        v = m.group(2).strip()
        n = re.fullmatch(r"(0[xX][0-9a-fA-F]+|\d+)[uU]?", v)
        out[m.group(1)] = (v, int(n.group(1), 0) if n else None)
    return out


def dim_value(expr, defs, where):
    """a declared size ([EO_CAST_W * EO_CAST_H / 2], [241] ...) as an integer"""
    e = expr
    for name in sorted(defs, key=len, reverse=True):
        if defs[name][1] is not None:
            e = re.sub(rf"\b{name}\b", str(defs[name][1]), e)
    e = re.sub(r"(?<=\d)[uU]\b", "", e)
    if not re.fullmatch(r"[0-9+\-*/() ]+", e):
        raise ResError(f"{where}: cannot work out the size [{expr}]")
    return int(eval(e.replace("/", "//"), {"__builtins__": {}}))        # (digits and + - * / only)


def find_array(text, symbol, defs, where):
    """-> (dims, body) of `static const TYPE SYMBOL[..]... = { body };`"""
    m = re.search(rf"static\s+const\s+(?:unsigned\s+)?\w+\s+{symbol}((?:\[[^\]]*\])+)\s*=\s*\{{", text)
    if not m:
        raise ResError(f"{where}: no array {symbol}")
    dims = [dim_value(d, defs, f"{where} {symbol}") for d in re.findall(r"\[([^\]]*)\]", m.group(1))]
    i, depth = m.end(), 1
    while depth and i < len(text):
        depth += (text[i] == "{") - (text[i] == "}")
        i += 1
    if depth:
        raise ResError(f"{where}: {symbol} is not closed")
    return dims, text[m.end():i - 1]


def read_item(name, fname, symbol, kind):
    """one array -> (its bytes, its element count, its dims)"""
    where = f"app/res/{fname}"
    text = (RES / fname).read_text()
    defs = defines(text)
    dims, body = find_array(text, symbol, defs, where)
    count = 1
    for d in dims:
        count *= d
    if kind == "char":                                   # rows of letters: each string is one row, then C's 0 byte
        rows = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
        if any("\\" in r for r in rows):
            raise ResError(f"{where}: {symbol} has an escape in a row (not supported)")
        width = dims[-1] - 1
        bad = [i for i, r in enumerate(rows) if len(r) != width]
        if bad:
            raise ResError(f"{where}: {symbol} row {bad[0]} is {len(rows[bad[0]])} wide, not {width}")
        if len(rows) * dims[-1] != count:
            raise ResError(f"{where}: {symbol} has {len(rows)} rows, its size says {count // dims[-1]}")
        return b"".join(r.encode("ascii") + b"\0" for r in rows), count, dims
    vals = [int(v, 0) for v in NUM.findall(re.sub(r"/\*.*?\*/", "", body, flags=re.S))]
    if len(vals) != count:
        raise ResError(f"{where}: {symbol} holds {len(vals)} numbers, its size says {count}")
    top = 0xFF if kind == "u8" else 0xFFFF
    if max(vals) > top:
        raise ResError(f"{where}: {symbol} has a number above {top:#x}")
    return struct.pack("<%d%s" % (len(vals), "B" if kind == "u8" else "H"), *vals), count, dims


def check_rows(items):
    """a picture's row table: starts at 0, rises, ends where its colour runs end (a broken one would crash the drawing)"""
    for pre, nrows in (("PRES", 240), ("TITLE", 240), ("ROOM", 144)):
        rows = struct.unpack("<%dH" % (len(items[pre + "_ROW"][0]) // 2), items[pre + "_ROW"][0])
        rle = items[pre + "_RLE"][0]
        if rows[0] != 0 or rows[-1] != len(rle) or any(b < a or (b - a) % 2 for a, b in zip(rows, rows[1:])):
            raise ResError(f"{pre}: the row table does not fit its picture ({len(rows)} rows, {len(rle)} bytes)")
        if len(rows) != nrows + 1:
            raise ResError(f"{pre}: this picture has {nrows} rows (the row table has {len(rows) - 1})")


def build():
    """-> dict with 'payload' (bytes), 'offsets' ({name: offset}), 'sizes' ({name: bytes}), 'defines' (the #defines copied)"""
    items, defs = {}, {}
    for name, fname, symbol, kind in ITEMS:
        data, count, dims = read_item(name, fname, symbol, kind)
        items[name] = (data, count, dims)
    check_rows(items)
    for fname in sorted({i[1] for i in ITEMS}):
        defs.update({k: v[0] for k, v in defines((RES / fname).read_text()).items()})
    payload, offsets = bytearray(), {}
    for name, _, _, _ in ITEMS:
        payload += b"\0" * (-len(payload) % ALIGN)
        offsets[name] = len(payload)
        payload += items[name][0]
    payload += b"\0" * (-len(payload) % ALIGN)
    return {"payload": bytes(payload), "offsets": offsets, "sizes": {n: len(items[n][0]) for n in items},
            "defines": defs, "dims": {n: items[n][2] for n in items}}


def map_header(b):
    lines = ["/* generated by app/gen_res.py from app/res/eo_*.h: where each array is in the resource region's payload. */",
             "#pragma once", "/* sizes copied from the source headers */"]
    lines += [f"#define {k} {v}" for k, v in sorted(b["defines"].items())]
    lines += [f"#define EO_DOG_POSES {b['dims']['DOG'][0]}", "/* where each array starts, in bytes from the payload's start */"]
    lines += [f"#define EO_RES_OFF_{n} {o}u" for n, o in b["offsets"].items()]
    lines += [f"#define EO_RES_LEN {len(b['payload'])}u      /* the payload: the build checks it against the base tool's */", ""]
    return "\n".join(lines)


def write_if_changed(path, data):
    old = path.read_bytes() if path.exists() else None
    if old != data:
        path.write_bytes(data)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    out = Path(argv[1])
    out.mkdir(parents=True, exist_ok=True)
    try:
        b = build()
    except ResError as e:
        print(f"gen_res: {e}", file=sys.stderr)
        return 1
    write_if_changed(out / "eo_res.bin", b["payload"])
    write_if_changed(out / "eo_res_map.h", map_header(b).encode())
    print(f"gen_res  {len(b['payload']):,} B of art and sound for the resource region")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
