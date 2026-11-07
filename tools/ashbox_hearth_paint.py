#!/usr/bin/env python3
"""Paint 《余烬》 hearth-room stages (style 丙) at native 240×176.

Twelve scenes share one room. State is told by fire, people, and the door.
Three frames flicker the fire, eyes, rain, and embers.

    python3 tools/ashbox_hearth_paint.py
"""
from __future__ import annotations

import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 240, 176
ROOT_ASSET = None  # set in main

BAYER = (
    np.array(
        [
            [0, 8, 2, 10],
            [12, 4, 14, 6],
            [3, 11, 1, 9],
            [15, 7, 13, 5],
        ],
        dtype=np.float32,
    )
    / 16.0
)

# Shared stage anchors
STOVE = (168, 86)
WIN = (12, 28, 52, 40)
DOOR = (88, 50, 26, 82)  # x, y, w, h


def hsh(x: int, y: int) -> int:
    return (int(x) * 73856093 ^ int(y) * 19349663) & 0xFFFFFFFF


def fill_rect(dst, x, y, w, h, c):
    Hd, Wd = dst.shape[:2]
    x0, y0 = max(0, int(x)), max(0, int(y))
    x1, y1 = min(Wd, int(x + w)), min(Hd, int(y + h))
    if x1 > x0 and y1 > y0:
        dst[y0:y1, x0:x1] = c


def ellipse(dst, cx, cy, rx, ry, c, mask=None):
    if rx < 1 or ry < 1:
        return
    Hd, Wd = dst.shape[:2]
    y0, y1 = max(0, int(cy - ry - 1)), min(Hd, int(cy + ry + 2))
    x0, x1 = max(0, int(cx - rx - 1)), min(Wd, int(cx + rx + 2))
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
            if 0 <= xx < dst.shape[1] and 0 <= yy < dst.shape[0]:
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
    rim = (~m) & (up | dn | lf | rt)
    rgb[rim] = (8, 6, 8)


def apply_lights(albedo, lights, ambient=0.16, amb_col=(40, 28, 32)):
    Hd, Wd, _ = albedo.shape
    yy, xx = np.indices((Hd, Wd))
    acc = np.zeros((Hd, Wd, 3), np.float32)
    acc += (np.array(amb_col, np.float32) / 255.0) * ambient * 4.0
    alb = albedo.astype(np.float32) / 255.0
    for lx, ly, rad, power, col in lights:
        d2 = (xx - lx) ** 2 + (yy - ly) ** 2
        fall = np.exp(-d2 / (2.0 * rad * rad)).astype(np.float32)
        acc += fall[..., None] * (np.array(col, np.float32) / 255.0) * power
    lit = np.clip(alb * np.clip(acc, 0, 1.65), 0, 1)
    bayer = BAYER[np.arange(Hd) % 4][:, np.arange(Wd) % 4]
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


def wall_and_floor(alb):
    alb[:, :] = (92, 78, 74)
    yy, xx = np.mgrid[0:H, 0:W]
    jitter = ((xx * 13 + yy * 7) % 7).astype(np.int16) - 3
    wall = alb.astype(np.int16)
    wall[:, :, 0] += jitter
    wall[:, :, 1] += jitter // 2
    alb[:] = np.clip(wall, 0, 255).astype(np.uint8)
    ellipse(alb, 34, 16, 16, 9, (36, 120, 104))
    ellipse(alb, 24, 20, 8, 5, (20, 80, 70))
    ellipse(alb, 214, 42, 14, 10, (40, 130, 110))
    for by in range(36, 78, 5):
        for bx in range(156, 214, 11):
            if hsh(bx, by) % 4 == 0:
                continue
            shade = 110 + (hsh(bx, by) % 30)
            fill_rect(alb, bx, by, 9, 4, (shade, shade // 2 + 20, shade // 3 + 16))
    fill_rect(alb, 0, 132, W, H - 132, (58, 40, 32))
    for i, y in enumerate(range(134, H, 5)):
        col = (72, 50, 38) if i % 2 == 0 else (48, 32, 26)
        fill_rect(alb, 0, y, W, 1, col)
    fill_rect(alb, 70, 148, 78, 16, (90, 28, 32))
    fill_rect(alb, 74, 151, 70, 3, (140, 48, 40))


def draw_window(alb, kind, frame):
    x, y, w, h = WIN
    fill_rect(alb, x, y, w, h, (18, 16, 28))
    if kind == "dusk":
        bands = [(255, 200, 140), (240, 120, 70), (180, 50, 60), (60, 20, 40)]
    elif kind == "night":
        bands = [(18, 28, 52), (28, 40, 72), (16, 22, 40), (10, 12, 22)]
    elif kind == "threat":
        bands = [(80, 16, 22), (120, 24, 28), (40, 10, 16), (16, 8, 12)]
    else:
        bands = [(200, 160, 90), (120, 80, 40), (40, 30, 24), (20, 16, 16)]
    bh = h // len(bands)
    for i, c in enumerate(bands):
        fill_rect(alb, x + 4, y + 4 + i * bh, w - 8, bh, c)
    if kind == "night":
        ellipse(alb, x + 18, y + 14, 5, 5, (230, 236, 220))
        ellipse(alb, x + 17, y + 13, 4, 4, (180, 190, 200))
    for sx, sh in ((x + 8, 14), (x + 16, 22), (x + 24, 10), (x + 34, 18)):
        fill_rect(alb, sx, y + h - 10 - sh, 5, sh, (16, 8, 16))
    if kind == "dusk":
        fill_rect(alb, x + 18, y + 18, 2, 3, (255, 180, 80))
        fill_rect(alb, x + 36, y + 22, 2, 3, (255, 140, 60))
    fill_rect(alb, x, y, w, 3, (46, 34, 30))
    fill_rect(alb, x, y + h - 3, w, 3, (46, 34, 30))
    fill_rect(alb, x, y, 3, h, (46, 34, 30))
    fill_rect(alb, x + w - 3, y, 3, h, (46, 34, 30))
    fill_rect(alb, x + w // 2, y, 2, h, (36, 26, 24))
    for i, (rx, ry) in enumerate(((4, 8), (12, 16), (22, 10), (30, 20), (8, 26))):
        fill_rect(alb, x + rx, y + 6 + ((ry + frame * 5 + i * 3) % (h - 10)), 1, 4, (180, 200, 210))


def draw_jars(alb, n=4, y=22):
    fill_rect(alb, 78, 36, 60, 3, (70, 48, 36))
    cols = [(180, 200, 190), (40, 140, 120), (160, 40, 40), (200, 180, 120), (90, 90, 100), (220, 200, 160)]
    for i in range(n):
        x = 82 + (i % 4) * 14
        yy = y if i < 4 else y + 18
        col = cols[i % len(cols)]
        fill_rect(alb, x, yy, 8, 14, col)
        fill_rect(alb, x + 1, yy + 1, 2, 6, (230, 230, 220))
        fill_rect(alb, x + 2, yy - 4, 4, 4, (60, 50, 44))


def draw_stove(alb, frame, power):
    fill_rect(alb, 168, 86, 52, 40, (28, 30, 34))
    fill_rect(alb, 172, 90, 44, 28, (16, 16, 18))
    fill_rect(alb, 186, 78, 16, 10, (36, 38, 42))
    ellipse(alb, 194, 78, 8, 4, (50, 52, 56))
    fill_rect(alb, 192, 68, 4, 10, (40, 42, 46))
    fill_rect(alb, 200, 18, 6, 68, (48, 46, 44))
    fill_rect(alb, 200, 18, 28, 5, (48, 46, 44))
    if power < 0.2:
        fill_rect(alb, 184, 108, 16, 6, (90, 30, 20))
        fill_rect(alb, 190, 110, 6, 3, (160, 50, 24))
        return
    scale = 0.55 + 0.45 * power
    shapes = [
        [(188, 100, 5, 10), (196, 98, 4, 12), (192, 92, 3, 6)],
        [(186, 102, 6, 8), (196, 96, 3, 14), (191, 90, 3, 7)],
        [(190, 100, 4, 11), (198, 99, 4, 9), (193, 88, 2, 8)],
    ]
    hot = (255, 230, 160) if frame != 1 else (255, 250, 210)
    mid = (255, 150, 40)
    low = (220, 70, 24)
    for i, (fx, fy, fw, fh) in enumerate(shapes[frame % 3]):
        col = hot if i == 2 else (mid if i == 0 else low)
        fh = max(2, int(fh * scale))
        fill_rect(alb, fx, fy + (10 - fh), fw, fh, col)
    if frame != 2:
        alb[88 - frame, 186 + frame * 4] = (255, 200, 80)


def draw_bunk(alb):
    fill_rect(alb, 70, 78, 46, 4, (62, 44, 32))
    fill_rect(alb, 70, 100, 46, 4, (62, 44, 32))
    fill_rect(alb, 70, 78, 3, 26, (48, 34, 26))
    fill_rect(alb, 113, 78, 3, 26, (48, 34, 26))
    fill_rect(alb, 74, 80, 36, 3, (90, 70, 78))


def head(alb, mask, cx, cy, frame, hair=(40, 26, 22)):
    ellipse(alb, cx, cy, 7, 8, (214, 168, 132), mask)
    ellipse(alb, cx, cy - 6, 8, 5, hair, mask)
    if 0 <= int(cy) < H and 0 <= int(cx) + 4 < W:
        if frame == 1:
            alb[int(cy), int(cx) + 4] = (160, 110, 90)
        else:
            alb[int(cy), int(cx) + 4] = (30, 18, 16)
        alb[int(cy) + 2, int(cx) + 3] = (150, 80, 76)


def person_sit(alb, mask, x, foot, frame, coat=(120, 32, 36)):
    fill_rect(alb, x + 2, foot - 10, 18, 6, (36, 32, 48))
    mask[foot - 10 : foot - 4, x + 2 : x + 20] = True
    fill_rect(alb, x + 18, foot - 8, 16, 5, (36, 32, 48))
    mask[foot - 8 : foot - 3, x + 18 : x + 34] = True
    fill_rect(alb, x + 32, foot - 6, 8, 5, (24, 20, 20))
    fill_rect(alb, x, foot - 32, 16, 24, coat)
    mask[foot - 32 : foot - 8, x : x + 16] = True
    for i in range(16):
        px = x + 14 + i
        py = foot - 24 + i // 2
        if 0 <= px < W and 0 <= py < H:
            alb[py : py + 3, px] = coat
            mask[py : py + 3, px] = True
    head(alb, mask, x + 10, foot - 42, frame)
    fill_rect(alb, x + 4, foot - 34, 12, 3, (224, 120, 40))
    mask[foot - 34 : foot - 31, x + 4 : x + 16] = True


def person_stand(alb, mask, x, foot, frame, coat=(90, 36, 40), pack=False, rifle=False):
    fill_rect(alb, x + 4, foot - 18, 5, 18, (36, 32, 44))
    fill_rect(alb, x + 11, foot - 18, 5, 18, (36, 32, 44))
    mask[foot - 18 : foot, x + 4 : x + 16] = True
    fill_rect(alb, x + 2, foot - 40, 16, 24, coat)
    mask[foot - 40 : foot - 16, x + 2 : x + 18] = True
    fill_rect(alb, x + 14, foot - 34, 12, 4, coat)
    mask[foot - 34 : foot - 30, x + 14 : x + 26] = True
    head(alb, mask, x + 10, foot - 50, frame)
    if pack:
        fill_rect(alb, x - 4, foot - 38, 7, 12, (48, 36, 28))
        mask[foot - 38 : foot - 26, x - 4 : x + 3] = True
    if rifle:
        line(alb, x + 16, foot - 36, x + 8, foot - 62, (24, 22, 20), mask, thick=2)


def person_slump(alb, mask, x, y, frame):
    ellipse(alb, x + 16, y, 18, 6, (70, 32, 36), mask)
    ellipse(alb, x + 34, y - 4, 7, 6, (214, 168, 132), mask)
    ellipse(alb, x + 36, y - 8, 7, 4, (40, 26, 22), mask)
    if frame != 1:
        alb[y - 4, x + 38] = (30, 18, 16)


def person_sleep(alb, mask, frame):
    ellipse(alb, 96, 92, 14, 5, (70, 40, 44), mask)
    ellipse(alb, 110, 88, 5, 4, (214, 168, 132), mask)
    fill_rect(alb, 106, 86, 7, 3, (40, 28, 24))
    mask[86:90, 106:114] = True
    if frame != 1:
        alb[88, 112] = (30, 18, 16)


def cat(alb, mask, x, y, frame):
    ellipse(alb, x, y, 8, 4, (30, 26, 24), mask)
    ellipse(alb, x + 8, y - 4, 3, 3, (30, 26, 24), mask)
    eye = (40, 28, 20) if frame == 2 else (255, 180, 40)
    if 0 <= y - 6 < H:
        alb[y - 6, x + 7] = eye
        alb[y - 6, x + 10] = eye


def seedlings(alb, frame):
    for i in range(4):
        x = 16 + i * 14
        fill_rect(alb, x, 118, 12, 8, (70, 52, 36))
        fill_rect(alb, x + 1, 116, 10, 3, (40, 28, 22))
        h = 8 + (i + frame) % 3
        fill_rect(alb, x + 5, 116 - h, 2, h, (40, 120, 60))
        alb[116 - h, x + 4] = (70, 160, 70)
        alb[116 - h, x + 7] = (50, 140, 60)


def crates(alb, mask):
    for i, (x, y, w, h) in enumerate(((108, 108, 28, 22), (134, 116, 26, 18), (100, 124, 22, 16))):
        fill_rect(alb, x, y, w, h, (96, 64, 36))
        fill_rect(alb, x + 2, y + 2, w - 4, 3, (130, 90, 50))
        line(alb, x + 2, y + 4, x + w - 3, y + h - 3, (60, 40, 24), mask)
        line(alb, x + w - 3, y + 4, x + 2, y + h - 3, (60, 40, 24), mask)


def doorway(alb, mode, frame):
    x, y, w, h = DOOR
    if mode == "shut":
        fill_rect(alb, x, y, w, h, (42, 32, 28))
        fill_rect(alb, x + 4, y + 8, w - 8, 16, (58, 42, 36))
        fill_rect(alb, x + 4, y + 36, w - 8, 16, (58, 42, 36))
        alb[y + 40, x + w - 6] = (180, 140, 60)
        return
    if mode == "ajar":
        fill_rect(alb, x, y, w - 8, h, (42, 32, 28))
        gap = 8 + frame
        fill_rect(alb, x + w - gap, y + 4, gap, h - 8, (16, 22, 36))
        for yy in range(y + 4, y + h - 8):
            alb[yy, x + w - gap] = (210, 170, 110)
        return
    if mode == "threat":
        fill_rect(alb, x, y, w, h, (36, 22, 22))
        crack = 2 + (frame % 2)
        fill_rect(alb, x + w // 2, y + 6, crack, h - 16, (180, 24, 28))
        for yy in range(y + 8, y + h - 12, 6):
            alb[yy, x + w // 2] = (255, 80, 60)
        return
    # open
    bands = [(255, 186, 120), (220, 90, 50), (90, 30, 36), (20, 10, 16)]
    bh = max(1, (h - 8) // len(bands))
    for i, c in enumerate(bands):
        fill_rect(alb, x + 2, y + 4 + i * bh, w - 8, bh, c)
    fill_rect(alb, x + w - 4, y, 8, h, (48, 34, 28))  # swung slab
    for yy in range(y, y + h):
        alb[yy, x + 2] = (255, 210, 150)


def coat_hook(alb):
    fill_rect(alb, 64, 40, 1, 28, (50, 40, 36))
    fill_rect(alb, 56, 66, 16, 22, (90, 32, 36))
    fill_rect(alb, 58, 68, 8, 4, (224, 120, 40))


SPECS = {
    # fire, amb, window, door, who
    "idle": dict(fire=0.85, amb=0.16, window="dusk", door="shut", who="sit", cat=True),
    "away": dict(fire=0.4, amb=0.12, window="night", door="ajar", who="none", cat=True),
    "weak": dict(fire=0.18, amb=0.11, window="night", door="shut", who="slump", cat=False),
    "threat": dict(fire=1.0, amb=0.12, window="threat", door="threat", who="guard", cat=False),
    "return": dict(fire=0.7, amb=0.14, window="dusk", door="open", who="welcome", cat=True),
    "night": dict(fire=0.35, amb=0.11, window="night", door="shut", who="sleep", cat=True),
    "gate": dict(fire=0.6, amb=0.14, window="dusk", door="open", who="leave", cat=False),
    "duo": dict(fire=1.0, amb=0.18, window="dusk", door="shut", who="duo", cat=True),
    "farm": dict(fire=0.55, amb=0.15, window="dusk", door="shut", who="farm", cat=False),
    "stash": dict(fire=0.45, amb=0.14, window="dusk", door="shut", who="count", cat=False),
    "trade": dict(fire=0.5, amb=0.14, window="dusk", door="shut", who="trade", cat=False),
    "watch": dict(fire=0.4, amb=0.12, window="night", door="shut", who="guard", cat=True),
}


def paint(scene: str, frame: int) -> np.ndarray:
    spec = SPECS[scene]
    alb = np.zeros((H, W, 3), np.uint8)
    masks = []
    wall_and_floor(alb)
    draw_window(alb, spec["window"], frame)
    if scene != "farm":
        extra = 6 if scene == "stash" else 4
        draw_jars(alb, extra)
    else:
        draw_jars(alb, 2)
        seedlings(alb, frame)
    draw_bunk(alb)
    draw_stove(alb, frame, spec["fire"])
    doorway(alb, spec["door"], frame)
    if scene == "away":
        coat_hook(alb)
    if scene == "stash":
        cm = np.zeros((H, W), np.bool_)
        crates(alb, cm)
        masks.append(cm)
    if scene == "gate":
        line(alb, 70, 150, 78, 100, (90, 70, 40), thick=2)  # spear
        fill_rect(alb, 60, 140, 14, 12, (120, 90, 50))  # sack
    if scene == "trade":
        fill_rect(alb, 18, 58, 10, 8, (200, 170, 80))
        fill_rect(alb, 30, 60, 8, 8, (40, 140, 110))
        ellipse(alb, 48, 48, 6, 8, (20, 16, 18))  # outsider hood in the glass

    people = np.zeros((H, W), np.bool_)
    who = spec["who"]
    if who == "sit":
        person_sit(alb, people, 118, 150, frame)
    elif who == "duo":
        person_sit(alb, people, 108, 150, frame)
        person_stand(alb, people, 78, 156, frame, coat=(48, 56, 70))
    elif who == "slump":
        person_slump(alb, people, 130, 156, frame)
    elif who == "sleep":
        person_slump(alb, people, 124, 158, 1)
        fill_rect(alb, 132, 152, 28, 6, (70, 48, 64))
        people[152:158, 132:160] = True
    elif who == "guard":
        person_stand(alb, people, 58, 158, frame, coat=(36, 40, 48), rifle=True)
    elif who == "welcome":
        person_sit(alb, people, 140, 150, frame)
        person_stand(alb, people, 92, 156, frame, coat=(28, 24, 28), pack=True)
    elif who == "leave":
        person_stand(alb, people, 78, 158, frame, coat=(70, 40, 32), pack=True)
    elif who == "farm":
        person_sit(alb, people, 36, 156, frame, coat=(48, 70, 44))
    elif who == "count":
        person_stand(alb, people, 150, 156, frame, coat=(80, 48, 36))
    elif who == "trade":
        person_stand(alb, people, 62, 150, frame, coat=(90, 48, 40))
    if who != "none":
        masks.append(people)
    if spec["cat"] and who != "slump":
        cm = np.zeros((H, W), np.bool_)
        cat(alb, cm, 168, 142, frame)
        masks.append(cm)

    lights = []
    if spec["fire"] >= 0.2:
        lights.append((194, 104, 34, 0.7 + spec["fire"], (255, 150, 60)))
        lights.append((194, 100, 12, spec["fire"], (255, 230, 180)))
    else:
        lights.append((190, 112, 16, 0.35, (180, 60, 30)))
    if spec["window"] == "night":
        lights.append((36, 48, 28, 0.45, (70, 100, 160)))
    elif spec["window"] == "threat":
        lights.append((36, 48, 26, 0.35, (140, 40, 36)))
    else:
        lights.append((36, 48, 28, 0.4, (90, 110, 160)))
    if spec["door"] == "open":
        lights.append((100, 90, 30, 0.7, (255, 170, 90)))
    elif spec["door"] == "threat":
        lights.append((104, 90, 22, 0.55 + 0.15 * frame, (200, 30, 30)))
    elif spec["door"] == "ajar":
        lights.append((112, 90, 14, 0.25, (80, 90, 120)))
    if scene == "farm":
        lights.append((40, 120, 24, 0.35, (60, 140, 70)))
    if scene == "weak":
        lights.append((150, 148, 24, 0.45, (170, 70, 36)))
    if scene == "watch":
        lights.append((70, 120, 30, 0.4, (80, 110, 160)))

    rgb = apply_lights(alb, lights, ambient=spec["amb"])
    for m in masks:
        outline(rgb, m)
    # steam when the fire is alive
    if spec["fire"] > 0.4:
        for i, s in enumerate(((192, 66), (196, 58), (190, 52))):
            yy = s[1] - frame * 2 + i
            xx = s[0] + (frame + i) % 2
            if 0 <= yy < H and 0 <= xx < W:
                rgb[yy, xx] = (220, 210, 200)
    return rgb565(rgb)


def main():
    from pathlib import Path

    root = Path(__file__).resolve().parents[1]
    asset = root / "assets" / "ashbox" / "sprites"
    prev = asset / "preview"
    prev.mkdir(parents=True, exist_ok=True)
    scenes = list(SPECS)
    sheet = Image.new("RGB", (W * 4, (H + 14) * 3), (12, 10, 10))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/STHeiti Light.ttc", 12, index=0)
    except OSError:
        font = ImageFont.load_default()
    for i, name in enumerate(scenes):
        for fi in range(3):
            im = Image.fromarray(paint(name, fi), "RGB")
            im.save(asset / f"{name}_f{fi}.png", optimize=True)
        thumb = Image.open(asset / f"{name}_f0.png")
        col, row = i % 4, i // 4
        ox, oy = col * W, row * (H + 14)
        sheet.paste(thumb, (ox, oy + 14))
        draw.text((ox + 4, oy), name, font=font, fill=(232, 180, 90))
    sheet.save(prev / "hearth_all_f0.png", optimize=True)
    print(f"painted {len(scenes) * 3} frames -> {asset}")


if __name__ == "__main__":
    main()
