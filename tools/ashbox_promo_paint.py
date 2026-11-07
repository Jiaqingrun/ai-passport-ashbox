#!/usr/bin/env python3
"""Portrait promo stills in the same pixel language as the hearth sprites.

No type. 240×320, then nearest-neighbor to 1200×1600 (exact 3:4).
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

W, H = 240, 320
BAYER = (
    np.array(
        [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]],
        dtype=np.float32,
    )
    / 16.0
)


def hsh(x: int, y: int) -> int:
    return (int(x) * 73856093 ^ int(y) * 19349663) & 0xFFFFFFFF


def fill_rect(dst, x, y, w, h, c):
    x0, y0 = max(0, int(x)), max(0, int(y))
    x1, y1 = min(W, int(x + w)), min(H, int(y + h))
    if x1 > x0 and y1 > y0:
        dst[y0:y1, x0:x1] = c


def ellipse(dst, cx, cy, rx, ry, c, mask=None):
    if rx < 1 or ry < 1:
        return
    y0, y1 = max(0, int(cy - ry - 1)), min(H, int(cy + ry + 2))
    x0, x1 = max(0, int(cx - rx - 1)), min(W, int(cx + rx + 2))
    if y1 <= y0 or x1 <= x0:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1]
    m = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1.0
    dst[y0:y1, x0:x1][m] = c
    if mask is not None:
        mask[y0:y1, x0:x1][m] = True


def line(dst, x0, y0, x1, y1, c, mask=None, thick=1):
    n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
    for i in range(n):
        t = i / max(n - 1, 1)
        x = int(round(x0 + (x1 - x0) * t))
        y = int(round(y0 + (y1 - y0) * t))
        for k in range(thick):
            yy, xx = y + k // 2, x + (k % 2)
            if 0 <= xx < W and 0 <= yy < H:
                dst[yy, xx] = c
                if mask is not None:
                    mask[yy, xx] = True


def outline(rgb, mask):
    m = mask.astype(bool)
    up = np.zeros_like(m)
    up[1:] = m[:-1]
    dn = np.zeros_like(m)
    dn[:-1] = m[1:]
    lf = np.zeros_like(m)
    lf[:, 1:] = m[:, :-1]
    rt = np.zeros_like(m)
    rt[:, :-1] = m[:, 1:]
    rgb[(~m) & (up | dn | lf | rt)] = (8, 6, 8)


def apply_lights(albedo, lights, ambient=0.28, amb_col=(48, 32, 30)):
    yy, xx = np.indices((H, W))
    acc = np.zeros((H, W, 3), np.float32)
    acc += (np.array(amb_col, np.float32) / 255.0) * ambient * 4.0
    alb = albedo.astype(np.float32) / 255.0
    for lx, ly, rad, power, col in lights:
        d2 = (xx - lx) ** 2 + (yy - ly) ** 2
        fall = np.exp(-d2 / (2.0 * rad * rad)).astype(np.float32)
        acc += fall[..., None] * (np.array(col, np.float32) / 255.0) * power
    lit = np.clip(alb * np.clip(acc, 0, 1.45), 0, 1)
    bayer = BAYER[np.arange(H) % 4][:, np.arange(W) % 4]
    lit = np.clip(lit + (bayer[..., None] - 0.5) * (1.0 / 14.0), 0, 1)
    lit = np.floor(lit * 14.0) / 14.0
    return (lit * 255.0).astype(np.uint8)


def rgb565(img: np.ndarray) -> np.ndarray:
    a = img.astype(np.uint16)
    r = (a[:, :, 0] >> 3) << 3
    g = (a[:, :, 1] >> 2) << 2
    b = (a[:, :, 2] >> 3) << 3
    r = r | (r >> 5)
    g = g | (g >> 6)
    b = b | (b >> 5)
    return np.stack([r, g, b], axis=-1).astype(np.uint8)


def room(alb):
    alb[:, :] = (92, 74, 68)
    yy, xx = np.mgrid[0:H, 0:W]
    jitter = ((xx * 13 + yy * 7) % 7).astype(np.int16) - 3
    wall = alb.astype(np.int16)
    wall[:, :, 0] += jitter
    wall[:, :, 1] += jitter // 2
    alb[:] = np.clip(wall, 0, 255).astype(np.uint8)
    # soot ceiling, sagging beam, water stain, mold
    fill_rect(alb, 0, 0, W, 18, (42, 32, 30))
    fill_rect(alb, 0, 16, W, 3, (46, 36, 32))
    fill_rect(alb, 18, 8, 150, 4, (36, 28, 24))
    fill_rect(alb, 20, 12, 4, 26, (36, 28, 24))
    fill_rect(alb, 160, 12, 4, 18, (36, 28, 24))
    ellipse(alb, 40, 36, 22, 14, (22, 70, 62))
    ellipse(alb, 28, 44, 10, 7, (14, 48, 42))
    ellipse(alb, 200, 58, 18, 12, (26, 78, 68))
    ellipse(alb, 120, 30, 16, 8, (48, 40, 34))
    for by in range(118, 168, 5):
        for bx in range(156, 230, 11):
            if hsh(bx, by) % 3 == 0:
                continue
            shade = 78 + (hsh(bx, by) % 28)
            fill_rect(alb, bx, by, 9, 4, (shade, shade // 3 + 18, shade // 4 + 12))
    # cracked plaster
    line(alb, 54, 20, 48, 96, (32, 26, 24), thick=1)
    line(alb, 150, 24, 158, 110, (36, 28, 26), thick=1)
    fill_rect(alb, 0, 210, W, H - 210, (58, 40, 32))
    for i, y in enumerate(range(212, H, 6)):
        col = (58, 40, 32) if i % 2 == 0 else (32, 22, 18)
        fill_rect(alb, 0, y, W, 1, col)
        if i % 4 == 1:
            fill_rect(alb, 20 + (i * 17) % 160, y, 18, 5, (16, 12, 12))
    fill_rect(alb, 78, 262, 70, 14, (62, 22, 26))
    fill_rect(alb, 82, 265, 60, 3, (96, 32, 30))


def ruin_pane(alb, x, y, w, h, kind):
    # Same window grammar as the hearth sprites: flat sky bands, black ruins.
    if kind == "dusk":
        bands = [(160, 110, 70), (120, 60, 40), (70, 28, 32), (24, 12, 16)]
    elif kind == "threat":
        bands = [(80, 16, 22), (120, 24, 28), (40, 10, 16), (16, 8, 12)]
    elif kind == "ash":
        bands = [(96, 86, 74), (70, 62, 54), (46, 40, 36), (28, 24, 22)]
    else:
        bands = [(18, 28, 52), (28, 40, 72), (16, 22, 40), (10, 12, 22)]
    fill_rect(alb, x, y, w, h, bands[-1])
    bh = max(1, (h - 8) // len(bands))
    for i, c in enumerate(bands):
        fill_rect(alb, x + 3, y + 3 + i * bh, w - 6, bh, c)
    for sx, sh in ((6, 22), (16, 36), (26, 16), (36, 30), (48, 20)):
        if sx + 5 < w:
            fill_rect(alb, x + sx, y + h - 8 - min(sh, h - 12), 5, min(sh, h - 12), (10, 8, 10))
    if kind == "night":
        alb[y + 10, x + 14] = (200, 206, 196)
    fill_rect(alb, x, y, w, 3, (46, 34, 30))
    fill_rect(alb, x, y + h - 3, w, 3, (46, 34, 30))
    fill_rect(alb, x, y, 3, h, (46, 34, 30))
    fill_rect(alb, x + w - 3, y, 3, h, (46, 34, 30))
    fill_rect(alb, x + w // 2, y, 2, h, (36, 26, 24))


def jars(alb, y=52, broken=True):
    fill_rect(alb, 70, y + 16, 52, 3, (48, 34, 28))
    cols = [(90, 100, 96), (28, 70, 62), (90, 28, 28), (110, 96, 64)]
    for i in range(4):
        x = 74 + i * 12
        col = cols[i]
        hh = 8 if broken and i == 2 else 12
        fill_rect(alb, x, y + (4 if hh == 8 else 0), 7, hh, col)
        fill_rect(alb, x + 2, y - 3, 3, 3, (40, 32, 28))
    if broken:
        fill_rect(alb, 96, y + 18, 6, 3, (70, 24, 22))


def stove(alb, frame, power, y=156):
    fill_rect(alb, 164, y, 56, 44, (24, 22, 22))
    fill_rect(alb, 168, y + 6, 48, 30, (12, 12, 12))
    fill_rect(alb, 182, y - 8, 18, 10, (32, 28, 26))
    fill_rect(alb, 188, y - 22, 5, 16, (36, 30, 26))
    fill_rect(alb, 196, 28, 5, y - 28, (40, 34, 30))
    # rust
    fill_rect(alb, 170, y + 34, 10, 4, (90, 42, 24))
    if power < 0.25:
        fill_rect(alb, 184, y + 22, 14, 6, (70, 24, 16))
        fill_rect(alb, 188, y + 24, 5, 3, (140, 48, 20))
        return
    scale = 0.5 + 0.4 * power
    shapes = [
        [(184, 16, 5, 12), (194, 14, 4, 14), (190, 8, 3, 7)],
        [(182, 18, 6, 10), (196, 12, 3, 16), (189, 6, 3, 8)],
        [(186, 16, 4, 13), (198, 15, 4, 11), (191, 4, 2, 9)],
    ]
    hot = (220, 170, 90) if frame != 1 else (230, 190, 120)
    mid = (190, 90, 28)
    low = (140, 42, 18)
    for i, (fx, fy, fw, fh) in enumerate(shapes[frame % 3]):
        col = hot if i == 2 else (mid if i == 0 else low)
        fh = max(2, int(fh * scale))
        fill_rect(alb, fx, y + fy + (12 - fh), fw, fh, col)


def door_slab(alb, x, y, w, h, mode, frame):
    if mode == "shut":
        fill_rect(alb, x, y, w, h, (62, 46, 38))
        fill_rect(alb, x + 3, y + 8, w - 6, 18, (86, 62, 48))
        fill_rect(alb, x + 3, y + 40, w - 6, 18, (74, 52, 40))
        fill_rect(alb, x + 3, y + 70, w - 6, 16, (58, 40, 32))
        alb[y + h // 2, x + w - 5] = (120, 90, 48)
        line(alb, x + 6, y + 4, x + w - 4, y + h - 6, (22, 16, 14))
        return
    if mode == "ajar":
        fill_rect(alb, x, y, w - 8, h, (32, 24, 22))
        gap = 7
        fill_rect(alb, x + w - gap, y + 4, gap, h - 8, (12, 16, 22))
        for yy in range(y + 4, y + h - 8, 3):
            alb[yy, x + w - gap] = (90, 80, 60)
        return
    if mode == "threat":
        fill_rect(alb, x, y, w, h, (26, 18, 18))
        fill_rect(alb, x + w // 2 - 1, y + 6, 3, h - 14, (120, 24, 22))
        return
    # open: swung slab on the right, hole stays for the caller to fill
    fill_rect(alb, x + w - 6, y, 10, h, (40, 30, 24))


def ruins_in(alb, x, y, w, h):
    bands = [(110, 100, 88), (72, 64, 56), (44, 38, 34), (26, 22, 20)]
    bh = max(1, h // len(bands))
    for i, c in enumerate(bands):
        fill_rect(alb, x, y + i * bh, w, bh, c)
    for sx, sh in ((2, 46), (14, 78), (28, 36), (40, 64), (54, 28)):
        if sx + 8 < w:
            top = y + h - sh - 8
            fill_rect(alb, x + sx, top, 8, sh, (12, 10, 10))
            if hsh(sx, sh) % 2 == 0 and top > y:
                alb[top, x + sx + 3] = (170, 64, 24)
    fill_rect(alb, x, y + h - 8, w, 8, (36, 28, 22))
    # rain
    for i in range(0, h, 7):
        alb[y + i, x + (i * 3) % max(w - 1, 1)] = (170, 176, 168)


def head(alb, mask, cx, cy, frame, hair=(28, 20, 18)):
    ellipse(alb, cx, cy, 7, 8, (150, 112, 90), mask)
    ellipse(alb, cx, cy - 6, 8, 5, hair, mask)
    if 0 <= int(cy) < H and 0 <= int(cx) + 4 < W:
        alb[int(cy), int(cx) + 4] = (24, 16, 14) if frame != 1 else (110, 70, 60)


def person_sit(alb, mask, x, foot, frame, coat=(92, 28, 30)):
    fill_rect(alb, x + 2, foot - 10, 18, 6, (28, 24, 32))
    mask[foot - 10 : foot - 4, x + 2 : x + 20] = True
    fill_rect(alb, x + 18, foot - 8, 16, 5, (28, 24, 32))
    mask[foot - 8 : foot - 3, x + 18 : x + 34] = True
    fill_rect(alb, x, foot - 32, 16, 24, coat)
    mask[foot - 32 : foot - 8, x : x + 16] = True
    for i in range(16):
        px = x + 14 + i
        py = foot - 24 + i // 2
        if 0 <= px < W and 0 <= py < H:
            alb[py : py + 3, px] = coat
            mask[py : py + 3, px] = True
    head(alb, mask, x + 10, foot - 42, frame)
    fill_rect(alb, x + 4, foot - 34, 12, 3, (140, 72, 28))
    mask[foot - 34 : foot - 31, x + 4 : x + 16] = True


def person_stand(alb, mask, x, foot, frame, coat=(36, 40, 48), pack=False):
    fill_rect(alb, x + 4, foot - 18, 5, 18, (28, 24, 30))
    fill_rect(alb, x + 11, foot - 18, 5, 18, (28, 24, 30))
    mask[foot - 18 : foot, x + 4 : x + 16] = True
    fill_rect(alb, x + 2, foot - 40, 16, 24, coat)
    mask[foot - 40 : foot - 16, x + 2 : x + 18] = True
    fill_rect(alb, x + 14, foot - 34, 10, 4, coat)
    mask[foot - 34 : foot - 30, x + 14 : x + 24] = True
    head(alb, mask, x + 10, foot - 50, frame, hair=(18, 16, 16))
    if pack:
        fill_rect(alb, x - 4, foot - 38, 7, 12, (40, 30, 24))
        mask[foot - 38 : foot - 26, x - 4 : x + 3] = True


def cat(alb, mask, x, y, frame):
    ellipse(alb, x, y, 8, 4, (20, 16, 16), mask)
    ellipse(alb, x + 8, y - 4, 3, 3, (20, 16, 16), mask)
    eye = (28, 20, 16) if frame == 2 else (180, 110, 28)
    if 0 <= y - 5 < H:
        alb[y - 5, x + 7] = eye
        alb[y - 5, x + 10] = eye


def tin(alb, x, y):
    fill_rect(alb, x, y, 14, 8, (40, 38, 34))
    fill_rect(alb, x + 1, y + 1, 12, 3, (70, 64, 54))
    alb[y + 3, x + 6] = (200, 90, 30)
    alb[y + 4, x + 7] = (140, 50, 20)


def finish(alb, lights, masks, ambient):
    lights = list(lights) + [(120, 250, 110, 0.34, (110, 58, 36))]
    rgb = apply_lights(alb, lights, ambient=ambient)
    for m in masks:
        outline(rgb, m)
    return rgb565(rgb)


def paint_hearth(frame=0):
    alb = np.zeros((H, W, 3), np.uint8)
    room(alb)
    ruin_pane(alb, 10, 36, 62, 64, "dusk")
    jars(alb, 48, True)
    stove(alb, frame, 0.55, 150)
    door_slab(alb, 92, 86, 28, 130, "shut", frame)
    people = np.zeros((H, W), np.bool_)
    person_sit(alb, people, 112, 268, frame)
    cm = np.zeros((H, W), np.bool_)
    cat(alb, cm, 168, 252, frame)
    lights = [
        (192, 168, 70, 0.9, (255, 120, 40)),
        (192, 164, 16, 0.7, (255, 180, 90)),
        (40, 68, 40, 0.85, (140, 90, 60)),
        (130, 230, 40, 0.35, (120, 50, 30)),
    ]
    return finish(alb, lights, [people, cm], 0.28)


def paint_leave(frame=0):
    alb = np.zeros((H, W, 3), np.uint8)
    room(alb)
    ruin_pane(alb, 8, 40, 40, 36, "night")
    # tall open door onto the waste
    ruins_in(alb, 48, 36, 78, 200)
    door_slab(alb, 48, 36, 78, 200, "open", frame)
    fill_rect(alb, 44, 32, 4, 208, (54, 40, 32))
    fill_rect(alb, 122, 32, 4, 208, (54, 40, 32))
    stove(alb, frame, 0.35, 168)
    people = np.zeros((H, W), np.bool_)
    person_stand(alb, people, 78, 250, frame, coat=(48, 40, 32), pack=True)
    cm = np.zeros((H, W), np.bool_)
    cat(alb, cm, 168, 246, frame)
    lights = [
        (190, 186, 48, 0.7, (200, 80, 30)),
        (86, 120, 90, 0.85, (100, 90, 76)),
        (90, 220, 28, 0.3, (80, 40, 28)),
    ]
    return finish(alb, lights, [people, cm], 0.26)


def paint_return(frame=0):
    alb = np.zeros((H, W, 3), np.uint8)
    room(alb)
    ruin_pane(alb, 10, 34, 58, 52, "night")
    jars(alb, 44, True)
    stove(alb, frame, 0.4, 156)
    door_slab(alb, 96, 78, 26, 120, "ajar", frame)
    people = np.zeros((H, W), np.bool_)
    person_sit(alb, people, 118, 270, frame, coat=(80, 26, 28))
    person_stand(alb, people, 78, 262, frame, coat=(32, 30, 34), pack=True)
    tin(alb, 124, 248)
    cm = np.zeros((H, W), np.bool_)
    cat(alb, cm, 176, 256, frame)
    lights = [
        (190, 174, 56, 0.75, (200, 90, 36)),
        (130, 250, 16, 0.85, (200, 80, 24)),
        (36, 58, 36, 0.75, (70, 90, 120)),
        (100, 200, 36, 0.28, (90, 40, 30)),
    ]
    return finish(alb, lights, [people, cm], 0.26)


def paint_door(frame=0):
    alb = np.zeros((H, W, 3), np.uint8)
    room(alb)
    ruin_pane(alb, 8, 28, 52, 70, "threat")
    # rain
    for i in range(18):
        rx = 12 + (i * 7) % 44
        ry = 34 + (i * 11) % 58
        fill_rect(alb, rx, ry, 1, 4, (120, 130, 140))
    stove(alb, frame, 0.3, 162)
    door_x, door_y, door_w, door_h = 86, 48, 40, 176
    ruins_in(alb, door_x, door_y, door_w, door_h)
    # stranger filling the doorway, blocky, empty hands
    stranger = np.zeros((H, W), np.bool_)
    person_stand(alb, stranger, door_x + 10, door_y + door_h - 8, frame, coat=(18, 18, 20))
    door_slab(alb, door_x, door_y, door_w, door_h, "open", frame)
    # leave only a gap of the figure: darken the slab edges already drawn
    people = np.zeros((H, W), np.bool_)
    person_sit(alb, people, 24, 286, frame, coat=(88, 26, 28))
    cm = np.zeros((H, W), np.bool_)
    cat(alb, cm, 168, 268, frame)
    lights = [
        (190, 180, 50, 0.65, (180, 70, 28)),
        (106, 120, 80, 0.95, (130, 120, 108)),
        (32, 60, 36, 0.7, (140, 40, 36)),
        (40, 250, 30, 0.3, (120, 40, 28)),
    ]
    return finish(alb, lights, [stranger, people, cm], 0.24)


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / "assets" / "ashbox" / "promo"
    out.mkdir(parents=True, exist_ok=True)
    frames = {
        "01-cover.png": paint_hearth,
        "02-depart.png": paint_leave,
        "03-return.png": paint_return,
        "04-door.png": paint_door,
    }
    for name, fn in frames.items():
        native = Image.fromarray(fn(0), "RGB")
        big = native.resize((W * 5, H * 5), Image.Resampling.NEAREST)
        big.save(out / name, optimize=True)
        native.save(out / name.replace(".png", "-native.png"), optimize=True)
        print(name, big.size)


if __name__ == "__main__":
    main()
