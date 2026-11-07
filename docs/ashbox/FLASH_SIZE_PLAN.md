<p align="right">
  <a href="FLASH_SIZE_PLAN.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Ashbox — Flash / Size Lossless Reduction Plan

**Hard constraint:** no change to gameplay, on-screen pixels, animation frame set, SFX content, or rules.  
**Out of scope:** fewer scenes/frames, smaller than 240×176, lossy images, shorter radio PCM.  
**Layout lock:** [LAYOUT_LOCK.md](./LAYOUT_LOCK.md) remains authoritative.

| Item | Value |
| --- | --- |
| Date | 2026-11-06 |
| Status | Phase 2 **indexed+RLE** firmware remeasure PASS; P3/P4 optional |
| Partition | factory `0x7F0000` ≈ 7.94 MiB |
| Sprite contract | 12×3×240×176 RGB565 semantics unchanged |

---

## 0. Goals and acceptance

| Goal | Gate |
| --- | --- |
| Real baseline | After clean `--firmware`, record `app.bin` / `.flash.rodata` / sprite symbol bytes |
| Lossless sprites | Per-frame decoded RGB565 **byte-identical** to current PNG→RGB565 |
| Animation unchanged | Still 12 scenes × 3 frames; `ashbox_sprite_get_frame` semantics unchanged |
| SFX unchanged | Five radio PCM blobs and playback path unchanged |
| Logic unchanged | Host tests green; no loot/tier edits |

Host probe estimates (not firmware measurements):

| Encoding | Sprite payload | vs raw RGB565 |
| --- | --- | --- |
| Raw RGB565 (current) | ≈2.90 MiB | — |
| Exact palette + I8 indices | ≈1.45 MiB | ≈−1.5 MiB |
| Palette + index RLE | ≈1.26 MiB | ≈−1.7 MiB |

---

## 1. Phases

| Phase | Work | Output | Stop if |
| --- | --- | --- | --- |
| **P0 baseline** | Clean firmware gate; measure | Fill §4 | Build fails |
| **P1 probe** | Host indexed/RLE + pixel assert | `tools/ashbox_sprites_compress_probe.py` PASS | Any frame >256 colors |
| **P2 encode** | Extend encoder; runtime decode or LVGL I8 | New sprite blob + small decoder | Round-trip fail → revert |
| **P3 fat** | Unused LVGL widgets; Montserrat off if safe; optional `-Os` | Second size delta | Smoke regress → undo item |
| **P4 device** | Preserve-save flash; 12 scenes + radio listen | Device PASS | No “done” without user OK |

Do not expand sprite frames or radio PCM while this plan runs ([OPTIMIZE_LOOP.md](./OPTIMIZE_LOOP.md) P2).

---

## 2. P2 encoding choice

**Primary:** per-frame exact RGB565 palette + 8-bit indices (all 36 frames ≤98 colors).

- **Path A:** LVGL draws I8 from flash (project palette layout) — no full-frame RAM.
- **Path B:** custom decoder / row expand into a **single RGB565 line** (240×2), per [display-refresh-and-deep-sleep](../reference/shinku-chen/display-refresh-and-deep-sleep.md). Never allocate 240×176×2 on this no-PSRAM part.

**Secondary:** RLE on indices only if more headroom is required after Path A/B.

**Forbidden:** JPEG, scale-down, frame-count cuts, near-duplicate frame aliasing, PCM shrinkage, tier cuts.

---

## 3. Commands

### P0

```bash
source <ESP-IDF-5.5.3>/export.sh
./tools/validate.sh --static
./tools/validate.sh --firmware
python3 tools/ashbox_flash_size_report.py
```

### P1

```bash
python3 tools/ashbox_sprites_compress_probe.py
```

### P2–P4

See the Chinese plan for the detailed checklist (encoder flags, decoder module, widget trim, preserve-save flash). Delivery report must separate Build / Host tests / Device tests / Flash before→after / Unverified.

---

## 4. Baseline log (fill in P0)

| Field | Value |
| --- | --- |
| Archive / app SHA | `build/firmware/16a76ad5f3995d787d5eb792c7d8e6879090a5e781301301cd42eedf73bd42ef`; app.bin sha256 `74069a94…` |
| `app.bin` bytes | **4,215,744** (4.020 MiB) |
| Factory occupancy | **50.7%** used / **49.3%** free |
| Sprite symbols total | **3,041,280** (12×3×84480 raw RGB565) |
| Radio PCM | **247,884** (≈242 KiB; five `_binary_radio_*` blobs linked) |
| Font bitmap total | **≈108 KiB** (`glyph_bitmap`) |
| `.flash.rodata` | **3,525,560** |
| Notes | Gate uses isolated build dir; workspace root `build/*.bin` may be stale — trust the archive |

### P2 remeasure (indexed+RLE, 2026-11-06)

| Field | Value |
| --- | --- |
| Archive | `build/firmware/cbf27c81b2f732e9274c55adbbc368911ad5d3ed507738f86265f732b9af0e83` |
| `app.bin` | **2,490,992** (2.375 MiB) |
| Factory free | **≈70%** (about **+1.65 MiB** vs P0 baseline) |
| Sprite Flash payload | ASR1 blobs **≈1.26 MiB** (was 2.90 MiB RGB565) |
| RAM | One-frame decode buffer `s_fb[240*176]` ≈82.5 KiB `.bss` |
| Host | `--static` PASS (includes pixel `--verify-only`) |

### Post-hearth / v1.3 package (2026-11-06 evening)

| Field | Value |
| --- | --- |
| Archive | `build/firmware/d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76` |
| `app.bin` | **2,894,928** (2.761 MiB) |
| Factory free | **65.2%** |
| Sprite ASR1 | **1,316,600** (unchanged contract) |
| Radio PCM ×5 | **247,884** |
| Hearth PCM | **384,000** (`hearth_fire.pcm` — first-class SFX budget; do not shorten without design OK) |
| Drift vs P2 | **+403,936** ≈ hearth |

---

## 5. Decision gates

1. After P0: if free space still >40%, P2 is scheduleable, not emergency.  
2. After P1: choose **indexed only** vs **indexed+RLE**.  
3. After P2 size delta: whether to run P3.  
4. P4: whether to preserve-save flash for acceptance.
