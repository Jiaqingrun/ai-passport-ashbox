[简体中文](./AUDIT_UI_VISUAL.zh_CN.md) · **English**

# UI / Visual / Layout / Buttons / Overlap / Fonts — Triple Strict Audit

- **Date**: 2026-11-06 (Asia/Shanghai)
- **Method**: Lead agent geometry + glyph gate checks; three parallel read-only subagents (layout, sprites/refresh, fonts)
- **Scope**: Post home-shell redesign (240×152 stage + now band)
- **Audit verdict**: **FAIL** (all three rounds FAIL)
- **Fix status (2026-11-06)**: P0/P1 and main P2 items landed (font regen, now-band columns, timer fingerprint, list clip/viewports, header/secondary CTA/modal)

Full tables, IDs, and evidence: see [AUDIT_UI_VISUAL.zh_CN.md](./AUDIT_UI_VISUAL.zh_CN.md).

### Round summary

| Round | Focus | Result |
|-------|--------|--------|
| R1 | Layout / overlap / buttons / z-order | **FAIL** (audit) |
| R2 | Sprite size / refresh paths / home states | **FAIL (conditional)** (audit) |
| R3 | Font glyphs / placeholders | **FAIL** (audit) |

### Pass islands (audit)

- Sprite C / PNG / encode / `STAGE_H` all **240×152**
- Home chrome skeleton and home↔list mutex

### Top blockers (audit) → fix landing

1. Missing glyphs for the now-band label and crisis “at present…” copy → regenerated `ashbox_font_16.c`; glyph checker also asserts symbols ⊆ bitmap
2. Now-band title/value/sub horizontal overlap → fixed widths + CLIP
3. List pages overflow the 194px content layer → clip + viewports / tighter rows
4. `paint_home_timers_only` lagged card/status/threat-label → home UI fingerprint + threat visibility sync

```text
Build: see latest validate run after fix
Host tests: glyph script targeted PASS after regen
Device tests: NOT RUN (offer flash after firmware build)
```
