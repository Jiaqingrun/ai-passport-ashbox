#!/usr/bin/env python3
"""Encode sprites PNG → indexed+RLE blobs (+ optional legacy RGB565 .c).

Default (Flash plan P2): assets/ashbox/sprites_rle/<scene>_f{0,1,2}.bin
Format ASR1:
  u32 magic 'ASR1'
  u16 width, height
  u16 palette_n
  u16 palette[palette_n]  (RGB565 LE)
  u8  runs[]  (count:u8, index:u8)* covering width*height pixels

Usage:
  python3 tools/ashbox_sprites_encode.py
  python3 tools/ashbox_sprites_encode.py --check
  python3 tools/ashbox_sprites_encode.py --format rgb565   # legacy giant .c
  python3 tools/ashbox_sprites_encode.py --verify-only
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from PIL import Image

W, H = 240, 176
MAGIC = b"ASR1"
ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / "assets" / "ashbox" / "sprites"
OUT_RLE = ROOT / "assets" / "ashbox" / "sprites_rle"
OUT_C = ROOT / "main" / "ashbox_sprites.c"

SCENES = [
    "idle",
    "away",
    "weak",
    "threat",
    "return",
    "night",
    "gate",
    "duo",
    "farm",
    "stash",
    "trade",
    "watch",
]


def to565(r: int, g: int, b: int) -> int:
    return ((r * 31) // 255) << 11 | ((g * 63) // 255) << 5 | ((b * 31) // 255)


def frame_rgb565(path: Path) -> list[int]:
    im = Image.open(path).convert("RGB")
    if im.size != (W, H):
        raise SystemExit(f"{path} size {im.size}, need {W}x{H}")
    px = im.load()
    return [to565(*px[x, y]) for y in range(H) for x in range(W)]


def pack_indexed_rle(vals: list[int]) -> bytes:
    cmap: dict[int, int] = {}
    palette: list[int] = []
    indices: list[int] = []
    for v in vals:
        i = cmap.get(v)
        if i is None:
            i = len(palette)
            if i > 255:
                raise SystemExit("frame exceeds 256 RGB565 colors")
            cmap[v] = i
            palette.append(v)
        indices.append(i)

    runs = bytearray()
    i = 0
    n = len(indices)
    while i < n:
        j = i + 1
        while j < n and indices[j] == indices[i] and j - i < 255:
            j += 1
        runs.append(j - i)
        runs.append(indices[i] & 0xFF)
        i = j

    hdr = struct.pack("<4sHHH", MAGIC, W, H, len(palette))
    pal = struct.pack("<" + "H" * len(palette), *palette)
    return hdr + pal + bytes(runs)


def decode_indexed_rle(blob: bytes) -> list[int]:
    if len(blob) < 10 or blob[:4] != MAGIC:
        raise SystemExit("bad ASR1 magic")
    _magic, w, h, pn = struct.unpack_from("<4sHHH", blob, 0)
    if w != W or h != H or pn == 0 or pn > 256:
        raise SystemExit(f"bad geometry/palette {w}x{h} n={pn}")
    off = 10
    palette = list(struct.unpack_from("<" + "H" * pn, blob, off))
    off += pn * 2
    expect = w * h
    out: list[int] = []
    while off + 1 < len(blob) and len(out) < expect:
        count = blob[off]
        idx = blob[off + 1]
        off += 2
        if idx >= pn or count == 0:
            raise SystemExit("bad RLE run")
        out.extend([palette[idx]] * count)
    if len(out) != expect or off != len(blob):
        raise SystemExit(f"RLE length/trailing junk out={len(out)} off={off}/{len(blob)}")
    return out


def verify_all() -> tuple[int, int]:
    total = 0
    raw = 0
    for name in SCENES:
        for fi in range(3):
            path = ASSET / f"{name}_f{fi}.png"
            vals = frame_rgb565(path)
            blob = pack_indexed_rle(vals)
            if decode_indexed_rle(blob) != vals:
                raise SystemExit(f"PIXEL FAIL {path.name}")
            total += len(blob)
            raw += len(vals) * 2
    return total, raw


def emit_rgb565_frame(name: str, fi: int) -> str:
    path = ASSET / f"{name}_f{fi}.png"
    vals = frame_rgb565(path)
    arr = f"s_station_{name}_f{fi}_map"
    lines: list[str] = [f"static const uint16_t {arr}[] = {{"]
    for i in range(0, len(vals), 12):
        chunk = ", ".join(f"0x{v:04X}" for v in vals[i : i + 12])
        lines.append(f"    {chunk}, ")
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_image_dsc_t ashbox_sprite_{name}_f{fi} = {{")
    lines.append("    .header = {")
    lines.append("        .magic = LV_IMAGE_HEADER_MAGIC,")
    lines.append("        .cf = LV_COLOR_FORMAT_RGB565,")
    lines.append("        .flags = 0,")
    lines.append(f"        .w = {W},")
    lines.append(f"        .h = {H},")
    lines.append(f"        .stride = {W * 2},")
    lines.append("        .reserved_2 = 0,")
    lines.append("    },")
    lines.append(f"    .data_size = sizeof({arr}),")
    lines.append(f"    .data = (const uint8_t *){arr},")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def build_rgb565_c() -> str:
    parts = ['#include "ashbox_sprites.h"', "#include <stddef.h>", ""]
    for name in SCENES:
        for fi in range(3):
            parts.append(emit_rgb565_frame(name, fi))
    parts.append("const lv_image_dsc_t *ashbox_sprite_get_frame(ashbox_scene_t scene, int frame)")
    parts.append("{")
    parts.append("    if (frame < 0) frame = 0;")
    parts.append("    frame %= 3;")
    parts.append("    static const lv_image_dsc_t *const table[ASHBOX_SCENE_COUNT][ASHBOX_SPRITE_FRAMES] = {")
    for name in SCENES:
        refs = ", ".join(f"&ashbox_sprite_{name}_f{i}" for i in range(3))
        parts.append(f"        {{ {refs} }},")
    parts.append("    };")
    parts.append("    if ((unsigned)scene >= ASHBOX_SCENE_COUNT) scene = ASHBOX_SCENE_IDLE;")
    parts.append("    return table[scene][frame];")
    parts.append("}")
    parts.append("")
    parts.append("const lv_image_dsc_t *ashbox_sprite_get(ashbox_scene_t scene)")
    parts.append("{ return ashbox_sprite_get_frame(scene, 0); }")
    parts.append("")
    return "\n".join(parts)


def write_rle_bins() -> tuple[int, int]:
    OUT_RLE.mkdir(parents=True, exist_ok=True)
    total, raw = 0, 0
    for name in SCENES:
        for fi in range(3):
            path = ASSET / f"{name}_f{fi}.png"
            vals = frame_rgb565(path)
            blob = pack_indexed_rle(vals)
            if decode_indexed_rle(blob) != vals:
                raise SystemExit(f"PIXEL FAIL {path.name}")
            out = OUT_RLE / f"{name}_f{fi}.bin"
            out.write_bytes(blob)
            total += len(blob)
            raw += len(vals) * 2
            print(f"wrote {out.relative_to(ROOT)} ({len(blob)} bytes)")
    return total, raw


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="only verify PNG sizes")
    ap.add_argument("--verify-only", action="store_true", help="encode/decode round-trip, no write")
    ap.add_argument(
        "--format",
        choices=("indexed_rle", "rgb565"),
        default="indexed_rle",
        help="indexed_rle (default) or legacy rgb565 .c",
    )
    ap.add_argument("scenes", nargs="*", help="legacy rgb565: emit selected scenes to stdout")
    args = ap.parse_args()

    if args.check:
        bad = []
        for name in SCENES:
            for fi in range(3):
                p = ASSET / f"{name}_f{fi}.png"
                im = Image.open(p)
                if im.size != (W, H):
                    bad.append(f"{p.name} {im.size}")
        if bad:
            raise SystemExit("size mismatch:\n  " + "\n  ".join(bad))
        print(f"ashbox sprites: PASS ({len(SCENES) * 3} png @ {W}x{H})")
        return

    if args.verify_only:
        total, raw = verify_all()
        print(f"PIXELS PASS  rle={total} raw={raw} save={raw - total}")
        return

    if args.format == "rgb565":
        if args.scenes:
            sys.stdout.write("\n".join(emit_rgb565_frame(s, fi) for s in args.scenes for fi in range(3)))
            return
        text = build_rgb565_c()
        OUT_C.write_text(text, encoding="utf-8")
        print(f"wrote {OUT_C} ({OUT_C.stat().st_size} bytes, {W}x{H} rgb565)")
        return

    total, raw = write_rle_bins()
    print(f"indexed_rle total {total} bytes ({total/1024/1024:.2f} MiB)  "
          f"vs raw {raw}  save {raw - total}")


if __name__ == "__main__":
    main()
