#!/usr/bin/env python3
"""Host probe: palette / indexed / RLE sizes vs raw RGB565 with pixel-exact round-trip.

Usage:
  python3 tools/ashbox_sprites_compress_probe.py
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

W, H = 240, 176
ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / "assets" / "ashbox" / "sprites"
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


def pack_indexed(vals: list[int]) -> tuple[list[int], list[int], bytes]:
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
    raw = bytes(indices)
    return palette, indices, raw


def rle_indices(indices: list[int]) -> bytes:
    out = bytearray()
    i = 0
    n = len(indices)
    while i < n:
        j = i + 1
        while j < n and indices[j] == indices[i] and j - i < 255:
            j += 1
        out.append(j - i)
        out.append(indices[i] & 0xFF)
        i = j
    return bytes(out)


def decode_indexed(palette: list[int], indices: list[int]) -> list[int]:
    return [palette[i] for i in indices]


def decode_rle(palette: list[int], blob: bytes, expect: int) -> list[int]:
    out: list[int] = []
    i = 0
    while i + 1 < len(blob):
        count = blob[i]
        idx = blob[i + 1]
        i += 2
        out.extend([palette[idx]] * count)
    if len(out) != expect:
        raise SystemExit(f"RLE length {len(out)} != {expect}")
    return out


def main() -> int:
    raw_total = 0
    indexed_total = 0
    rle_total = 0
    max_colors = 0
    frames = 0
    for name in SCENES:
        for fi in range(3):
            path = ASSET / f"{name}_f{fi}.png"
            vals = frame_rgb565(path)
            palette, indices, idx_bytes = pack_indexed(vals)
            rle = rle_indices(indices)
            if decode_indexed(palette, indices) != vals:
                raise SystemExit(f"PIXEL FAIL indexed {path.name}")
            if decode_rle(palette, rle, len(vals)) != vals:
                raise SystemExit(f"PIXEL FAIL rle {path.name}")
            n = len(palette)
            max_colors = max(max_colors, n)
            raw_total += len(vals) * 2
            indexed_total += n * 2 + len(idx_bytes)
            rle_total += n * 2 + len(rle)
            frames += 1
            print(
                f"{path.name:16s} colors={n:3d}  "
                f"raw={len(vals)*2:6d}  idx={n*2+len(idx_bytes):6d}  "
                f"rle={n*2+len(rle):6d}"
            )

    print("---")
    print(f"frames {frames}  max_colors {max_colors}")
    print(f"raw RGB565     {raw_total:8d}  ({raw_total/1024/1024:.2f} MiB)")
    print(f"palette+I8     {indexed_total:8d}  ({indexed_total/1024/1024:.2f} MiB)  "
          f"save {raw_total-indexed_total}")
    print(f"palette+RLE    {rle_total:8d}  ({rle_total/1024/1024:.2f} MiB)  "
          f"save {raw_total-rle_total}")
    print("PIXELS PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
