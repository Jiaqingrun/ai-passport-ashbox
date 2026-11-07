[简体中文](./LAYOUT_LOCK.zh_CN.md) · **English**

# Ashbox home layout lock

**Status: LOCKED** (2026-11-06, user-accepted)

Do not change these numbers or the home chrome structure without an explicit new design decision. Casual “tighten/loosen” edits are out of scope.

## Screen

- Panel: 240×320, rounded case; keep only thin safe insets.
- Sprites: **240×176**, 12 scenes × 3 frames, RGB565.

## Vertical stack (home)

| Region | Constant | Value | Notes |
|--------|----------|-------|--------|
| Top safe | `SAFE_TOP` | 2 | Thin; do not restore large top pad |
| Header | `HDR_H` | 18 | Day / clock / scavenged cell battery |
| Content origin | `CONTENT_Y` | 20 | `SAFE_TOP + HDR_H` |
| Stage | `STAGE_H` | 176 | Pure art; no resource/status overlays on the sprite |
| Now band | `NOW_BAND_H` | 52 | Resources / radio / now — below the stage |
| Content height | `LIST_LAYER_H` | 228 | `STAGE_H + NOW_BAND_H` |
| Hint | `HINT_Y` | 250 | |
| Button bar | `BAR_Y` / `BAR_H` | 268 / 44 | Dual buttons share `BTN_Y` |

## Home HUD rules (locked)

- Population is **not** shown as a headcount label; presence is via scene art (`IDLE` / `DUO` / `AWAY`, …).
- Expedition / crisis / report states are shown on the **now** row only — do not duplicate on the resource-row status.
- Battery is the segmented scavenged-cell widget (no percent, no ember-resource label on the cells).
- Primary and secondary bar buttons must stay on the same `BTN_Y` with the shared dual geometry.

## Authoritative code

`main/ashbox_ui.c` layout `#define`s next to `STAGE_H` / `SAFE_TOP`. Sprite size: `tools/ashbox_sprites_encode.py` (`W, H = 240, 176`) and `assets/ashbox/sprites/`.
