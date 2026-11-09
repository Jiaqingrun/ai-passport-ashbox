[简体中文](./PROJECT.zh_CN.md) · **English**

# Project conventions · ai-passport-ashbox (Ashbox / Ember)

> **Layering:** global personal standards live in `00-personal-standards.mdc` (do not repeat conda /
> Git / `~/QR` / `~/.qr` here). Edit: `qr project-standards --edit ai-passport-ashbox`. After
> `qr rules --target .` you **must** `git checkout -- AGENTS.md AGENTS.zh_CN.md`.

**Conflict order:** `AGENTS.md` flash / safety / UI-redesign rails > this file >
[`LAYOUT_LOCK.md`](./docs/ashbox/LAYOUT_LOCK.md) and the save ABI > [`docs/ashbox/README.md`](./docs/ashbox/README.md)
index > audit snapshots > archive. Authoritative Chinese product wording: [`PROJECT.zh_CN.md`](./PROJECT.zh_CN.md).

## Purpose

Offline post-apocalyptic base-builder on FoloToy AI Passport: hearth home, expeditions, base
building, trade, injury and meds, wanderers, USB save manager. **Not** a hardware-test demo, **not**
an online game. Display name Ember · tree `ai-passport-ashbox` · habitual branch `feature/ashbox-ember`.

## Stack and layout

| Layer | Location |
| --- | --- |
| Target | ESP32-C3, 8 MB flash, no PSRAM, ESP-IDF 5.5.3 |
| Rules (host-testable) | `main/ashbox_game.{c,h}`, `main/ashbox_content.{c,h}`, `main/ashbox_game_loot.{c,h}` |
| Save / settings / sleep / SFX / backup | `main/ashbox_save.*`, `ashbox_settings.*`, `ashbox_sleep.*`, `ashbox_sfx.*`, `ashbox_embackup*` |
| UI (LVGL) | `main/ashbox_ui.{c,h}` plus the `main/ashbox_ui_<page>.c` units; wiring `main/main.c` |
| Sprites / font | `assets/ashbox/sprites/` (12 scenes × 3 frames, 240×176) → `main/ashbox_sprites.*`; `main/ashbox_font_symbols.txt` → `assets/fonts/ashbox_font_16.c` |
| Docs index | [`docs/ashbox/README.md`](./docs/ashbox/README.md) |

QR project id `ai-passport-ashbox`. Active workspace: the DeepSeek copy under `~/DeepSeek/AI Passport/`
(full history, branch `feature/ashbox-ember`); `~/QR/dev/ai-passport-ashbox` is read-only reference.
The local knowledge-base MCP (`qr-knowledge`) is retired — no `qr ask` / `qr log -p`.

## Build · test · flash

| Goal | Command |
| --- | --- |
| Static + host | `./tools/validate.sh --static` (or `--ashbox` for product only) |
| Firmware | activate IDF 5.5.3, then `./tools/validate.sh --firmware` |
| Daily flash | `./tools/flash_ashbox_preserve_save.sh` → writes `0x10000`, keeps NVS |
| Merged `0x0` | wipes the save; blank device or explicit request only |

Delivery lists `Build` / `Host tests` / `Device tests` / `Unverified`. Detecting USB is not flash
consent. Details: [BUILD.md](./docs/ashbox/BUILD.md), [firmware-layout](./docs/development/engineering/firmware-layout.md).

## Save ABI

`ashbox` / `save`, magic `ASH2`, **`ASHBOX_SAVE_VER=13`**, blob 776; meta `meta` / `ASHM`. Append
fields before `crc`, migrate old saves, never break them silently. Community install/update **wipes**
progress — never promise otherwise. Details: [EXTENSION.md](./docs/ashbox/EXTENSION.md),
[META_PROGRESS.md](./docs/ashbox/META_PROGRESS.md).

## Layout lock

240×320 panel; sprite stage 240×176 × 36 frames; home geometry in `main/ashbox_ui.c` and
[LAYOUT_LOCK.md](./docs/ashbox/LAYOUT_LOCK.md). Forbidden without an explicit redesign decision:
fewer scenes or frames, smaller stage, lossy art, shorter radio PCM.

## Player-facing copy and content (hard rule)

- **Plain spoken Chinese for players**; store and Help copy may be detailed, in-game text should
  sound like people talking. Never describe an unimplemented feature.
- **One name per thing.** The canonical glossary lives only in `PROJECT.zh_CN.md`; no aliases
  (pistol not "gun", crowbar not "iron bar", workbench not "work station", water can not "bucket").
- **Plain arithmetic for rules** — "300 food = 1 slot · 50 iron = 1 slot"; no compressed jargon.
- **Every rate shown to players names its probability** (or the plain-words equivalent).
- **Scope:** Help, route / loot / person / loadout lines, build / trade / gear / farm / order /
  duty / crisis copy, branch labels, away lines, quest steps, journal, radio, `ash_game_log`.
  New systems update Help and the route sublines in the same change.
- **Fonts:** new CJK goes into `ashbox_font_symbols.txt` and the subset is regenerated; shorten
  copy before cutting it against a buffer.
- **Gates:** `check_ashbox_copy_layout.py`, `check_ashbox_glossary.py`, `check_ashbox_font_glyphs.py`.
- Store copy (`description_*` / `instructions_*` / `changelog_*`) is play-first, incremental against
  the approved baseline. Store artwork is the fixed four illustrations in `assets/ashbox/promo/`
  (`01-cover` as cover, `02-depart` / `03-return` / `04-door` as extras) — re-upload all four every
  time, never regenerate or swap them. Details: [COMMUNITY_PUBLISH.md](./docs/ashbox/COMMUNITY_PUBLISH.md).

## Audits

Every audit covers three sections: **flash size**, **whole-code optimization**, **player copy
layout + glossary**. After any product change, run `./tools/validate.sh --ashbox`, walk the
applicable rows and fix FAIL findings in the same change. Checklist and delivery header:
[AUDIT_REQUIREMENTS.md](./docs/ashbox/AUDIT_REQUIREMENTS.md) · retired snapshots:
[AUDIT_HISTORY.md](./docs/ashbox/AUDIT_HISTORY.md).

## AI collaboration

Required skills: `passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`,
`passport-debug`. After a firmware change, ask before flashing. Default: no `git commit` / `push` /
community publish / full erase / merged `0x0` flash. Record milestone decisions in the current
`docs/ashbox/` document or the commit message.

## Current stage

| Item | Status |
| --- | --- |
| Playable loop | Live in the community store; iterated locally since |
| Save | v13 (blob 776; older saves migrate) |
| Layout | Locked by LAYOUT_LOCK |
| Flash | Tracked in FLASH_SIZE_PLAN; re-measured by every audit |
| Community listing | Baseline is the store's latest **approved** revision; later updates edit incrementally |
| Current image | Newest archive under `build/firmware/`; its hash goes into the latest audit |
