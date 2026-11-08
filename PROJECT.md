[简体中文](./PROJECT.zh_CN.md) · **English**

# Project conventions · ai-passport-ashbox (Ashbox / Ember)

> **Layering:** Global personal standards live in `00-personal-standards.mdc` (do not repeat conda/Git/`~/QR`/`~/.qr` here).  
> Edit: `qr project-standards --edit ai-passport-ashbox` · Revise from chat: `qr project-standards-revise ai-passport-ashbox`  
> After `qr rules --target .` you **must** `git checkout -- AGENTS.md AGENTS.zh_CN.md` (upstream Passport entry).

**Conflict order (this repo):**  
`AGENTS.md` flash/safety/UI-redesign rails > this file > [`docs/ashbox/LAYOUT_LOCK.md`](./docs/ashbox/LAYOUT_LOCK.md) / save ABI > [`docs/ashbox/README.md`](./docs/ashbox/README.md) index > ops/audit snapshots > `docs/archive/` (if any).

The authoritative Chinese product wording for day-to-day design is in [`PROJECT.zh_CN.md`](./PROJECT.zh_CN.md). This English file is the Passport/default peer.

---

## Purpose

Offline post-apocalyptic base-builder on FoloToy AI Passport: hearth home, expeditions, base building, trade, injury/meds, wanderers, USB save manager.  
**Not** a hardware test demo; **not** an online server game.

Display names: Ember (Chinese title in the `.zh_CN` peer) · tree: `ai-passport-ashbox` · habitual branch: `feature/ashbox-ember`

---

## Stack and layout

| Layer | Location |
| --- | --- |
| Target | ESP32-C3, **8 MB Flash, no PSRAM**, ESP-IDF **5.5.3** |
| Rules (host-testable) | `main/ashbox_game.{c,h}`, `main/ashbox_content.{c,h}` |
| Save / settings / sleep / SFX | `main/ashbox_save.*`, `ashbox_settings.*`, `ashbox_sleep.*`, `ashbox_sfx.*` |
| USB save protocol | `main/ashbox_embackup*.` (`!EMBACKUP` ↔ AI-Passport_BS manager) |
| UI (LVGL) | `main/ashbox_ui.{c,h}`; wiring `main/main.c` |
| Sprites | `main/ashbox_sprites.*` ← `assets/ashbox/sprites/` (**12 scenes ×3 frames ×240×176**) |
| Font | `assets/fonts/ashbox_font_16.c` ← `main/ashbox_font_symbols.txt` |
| Product docs index | [`docs/ashbox/README.md`](./docs/ashbox/README.md) |

QR project id: `ai-passport-ashbox`. Active workspace: the DeepSeek copy under `~/DeepSeek/AI Passport/` (full Git history, branch `feature/ashbox-ember`); the former Cursor workspace `~/QR/dev/ai-passport-ashbox` is read-only reference. The local knowledge-base MCP (`qr-knowledge`) is retired — do not rely on `qr ask` or `qr log -p`.

---

## Build · test · flash

| Goal | Command |
| --- | --- |
| Static + host | `./tools/validate.sh --static` (or `--ashbox` for product-only) |
| Firmware | Activate IDF 5.5.3 then `./tools/validate.sh --firmware` |
| Daily flash | `./tools/flash_ashbox_preserve_save.sh` @ `0x10000` (keeps NVS) |
| Full merge @ `0x0` | Wipes save — blank device or explicit user request only |

Delivery must list: `Build` / `Host tests` / `Device tests` / `Unverified`. Detecting USB ≠ flash consent.

---

## Save ABI

- NVS NS `ashbox`, key `save`, magic `ASH2`, **`ASHBOX_SAVE_VER=13`**, blob `ASHBOX_SAVE_BLOB_SIZE` (776); meta key `meta` / `ASHM` — [docs/ashbox/META_PROGRESS.md](docs/ashbox/META_PROGRESS.md)
- Append-only fields before `crc`; migrators required; never silently break old saves
- Community install/update **wipes** progress — never promise otherwise

---

## Layout lock (summary)

Do not change without an explicit redesign decision: 240×320 screen; sprites 240×176 / 12×3; home vertical geometry in `main/ashbox_ui.c` / LAYOUT_LOCK.  
**Forbidden size cuts:** fewer scenes/frames, smaller resolution, lossy art, shorter radio PCM (unless user overrides on the spot).

---

## Player-facing copy & game content · plain language (hard rule)

Authoritative Chinese wording lives in [`PROJECT.zh_CN.md`](./PROJECT.zh_CN.md) under the matching plain-language section. English store/UI copy must match that meaning. When brevity fights clarity, **clarity wins**. This section covers **UI labels** and **game content** (narrative + mechanics explanation).

**Must:** ordinary spoken wording; Help and store copy may be detailed and inviting; in-game story/tips should sound like people talking; never invent unimplemented features.

**Glossary (player-visible Chinese):** keep the forbidden-slang → required-name table only in `PROJECT.zh_CN.md`. One item / one event / one Chinese name (no pistol/gun/short-fire style aliases; crowbar not “iron bar”; craft bench not “work station”; wall-clock not in player copy — say “clock”; water can not “water bucket”). English docs must not paste CJK slang tables; change Chinese player strings against the zh peer.

**Mechanics in plain arithmetic:** stash slots and similar rules must read like “300 food = 1 slot · 50 iron = 1 slot · water can / backpack = 1 slot each”, never compressed jargon.

**Probability:** any success / empty-handed / drop rate shown to players must name the probability in plain words (Chinese copy: follow the zh peer — require the probability word, not a bare percentage).

**Game content (hard):** Help/guide, route/loot/person/loadout lines, build/trade/gear/farm/order/duty/crisis UI copy, branch labels/options, away lines, quest steps, journal, radio pools, and player-visible `ash_game_log` lines. Write cause→effect in plain talk; mechanics sentences must state what the numbers mean; narrative may have mood but must be speakable aloud—no author-only slang. New systems update Help and route sublines in the same change.

**Scope:** `ashbox_ui` / player strings in `ashbox_game` / `ashbox_content` / Help; community `description_*` / `instructions_*` / `changelog_*` (play-first, no chip/framework names; **from 1.7 onward store description/instructions are incremental-only vs the live baseline—no unsolicited full rewrites**, see [`docs/ashbox/COMMUNITY_PUBLISH.md`](./docs/ashbox/COMMUNITY_PUBLISH.md)). Identifiers may keep English (`pistol`, `ammo`); player strings must not leak old slang.

**Font:** new CJK → `ashbox_font_symbols.txt` + regenerate `ashbox_font_16.c`; run `check_ashbox_font_glyphs.py`. Shorten before truncating critical names against `ASHBOX_JOURNAL_LEN` / `snprintf` buffers.

**Check:** Help how-to-play, route/craft labels, one branch or radio line, and store copy — should read the required full names and clear mechanics, not slang; host gates `check_ashbox_copy_layout.py` + `check_ashbox_glossary.py`.
---

## Audit rails (mandatory every audit)

1. **Flash size** — image identity, factory free %, sprites/font/PCM/code, vs `FLASH_SIZE_PLAN`, allowed vs forbidden  
2. **Whole-code optimization** — rules/UI boundary, hot units, duplication, host-test gaps, locks/tasks, save ABI, dead code, build fat; each with evidence + next step  
3. **Player-facing copy layout + glossary** — Help/guide/coach/modals must WRAP; no CLIP-only help; no silent UTF-8 truncate; one canonical Chinese name per item/event; plain arithmetic for slot rules; host gates `tools/check_ashbox_copy_layout.py` + `tools/check_ashbox_glossary.py`

Checklist: [`docs/ashbox/AUDIT_REQUIREMENTS.md`](./docs/ashbox/AUDIT_REQUIREMENTS.md).

### Post-change strict audit (mandatory)

After any Ashbox product change (rules / UI / player copy / help / settings) in a session, before delivery the agent **must**:

1. Run `./tools/validate.sh --ashbox` (includes font, dead-branch, **copy-layout**, sprites, host tests)  
2. Walk the applicable rows in `AUDIT_REQUIREMENTS` (copy layout always when UI/copy touched; Flash size when firmware rebuilt; code optimization when structure/ABI/hot paths change)  
3. **Fix FAIL findings in the same change** — do not defer known clip/wrap/truncation or gate failures to a later audit-only pass  

---

## AI collaboration (this project)

Required skills: `passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`, `passport-debug`.  
After firmware changes, ask whether to flash; do not flash without approval.  
Default: no `git commit` / `push` / community publish / full erase / merge `@0x0` flash.  
Milestone decisions: record them in the current `docs/ashbox/` document or the commit message; the local knowledge-base MCP is retired, so do not use `qr log -p`.  
Post-change: follow **Post-change strict audit** above before claiming the task done.

---

## Current stage (summary)

| Item | Status |
| --- | --- |
| Playable loop | Live in the community store; iterated locally since |
| Save | v13 (`ASHBOX_SAVE_VER=13`, blob 776; older saves migrate) |
| Layout | Locked by LAYOUT_LOCK |
| Flash | Tracked in FLASH_SIZE_PLAN; re-checked by every audit |
| Community listing | The current **1.7** submission is the baseline; only incremental edits from then on (see `COMMUNITY_PUBLISH.md`) |
| Current image | The newest archive under `build/firmware/`; the exact hash goes into the latest audit snapshot |
