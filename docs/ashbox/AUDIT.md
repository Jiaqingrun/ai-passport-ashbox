[简体中文](./AUDIT.zh_CN.md) · **English**

# Ashbox gameplay / lifecycle / compliance audit

Checked against popular idle loops (produce → throughput → warehouse; sparse premium drops; return hooks = full storage / ready harvest / upgrades) for this three-key offline game.

## Findings (pre-fix)

| ID | Issue | Fix |
| --- | --- | --- |
| A1 | Farm buried in Manage | Hub first entry → standalone farm page |
| A2 | Stash cap tied only to kitchen | Independent `stash_lv` 1–3 |
| A3 | Seeds only from Iron Shed | Rare expedition seed drops |
| A4 | Ember almost deep-only | Tiny ember chance on non-deep success |
| A5 | 12-item Manage too deep | Hub (9) + Build subpage |
| A6 | Full stash silent | Radio “stash full · expand” |
| A7 | Ready harvest not on Home | Card / hub cue |

## Phase C

| ID | Content | Gate / cap |
| --- | --- | --- |
| C1 | Ash River ferry camp | Quest-2 finish or hunt encounter; separate `trade2_day` |
| C2 | Stash furniture | Shelf +20 cap; drying rack costs token, ~×8/7 grow |
| C3 | Grain / root crops | Root needs ferry; higher yield, longer grow; seeds capped apart |
| C4 | “Done but meaningless” | Hub/farm/trade/build/help copy states tradeoffs: grain fast/low, root slow/full, ferry for root, shelf capacity, drying speed |

## Full-direction re-audit

| ID | Sev | Issue | Fix |
| --- | --- | --- | --- |
| D1 | high | Report ack discarded pocket overflow | Overflow into stash + clamp |
| D2 | high | Journal masked stash/farm/hunger radio hooks | Hooks before journal |
| D3 | high | Empty pocket with stash food blocked depart | Auto pull stash→pocket on settle |
| D4 | med | Jerky not convertible to food | Stash “jerky→food” + `use_jerky` |
| D5 | med | Trade CTA enabled without materials | `trade_can_pay` + UI shortfall label |
| D6 | med | Drying only at plant time | Tick ×8/7 grow |
| D7 | med | v3/v4/v5 migrate skipped CRC | Verify source CRC |
| D8 | low | Hunt ferry open had dead quest_done arm | Encounter-only `seed%4` |

## Playability / linkage / atmosphere re-audit

| ID | Sev | Challenge | Fix |
| --- | --- | --- | --- |
| E1 | high | Journal still buried lore/camp atmosphere | Radio lanes: hard → soft → lore/camp → journal → idle |
| E2 | high | Home subline was only short-probe count | `ash_game_home_nudge`: harvest/stash/trade/plant/gate/quest |
| E3 | high | Daily trade had no revisit hook | Radio + hub “trade today / already traded” |
| E4 | med | Empty pocket + stash still felt like hunger lock | Instant `try_pocket_refill`; radio “refill pocket” |
| E5 | med | Away lines ignored tier tone | Hunt/deep use colder tail lines |
| E6 | med | Untouched quest text stole early home focus | Nudge only when quest `step>0` |
| E7 | low | Hub “Quests” hid active thread name | Show current quest name |

## F-pass: soft-hook noise / feedback

| ID | Sev | Challenge | Fix |
| --- | --- | --- | --- |
| F1 | high | “Trade today” buried radio all day even broke | Nudge only when `trade_hook_worth` |
| F2 | high | Plantable plots buried atmosphere all day | Radio plant only if all plots empty |
| F3 | med | Fatigue lock felt like a bug | Nudge “too tired — rest” |
| F4 | med | Low food + jerky had no path | Nudge/radio jerky→food |
| F5 | med | High threat, no night order while away | Away interrupt + home “no order” |
| F6 | med | Zero-seed plant still looked runnable | Farm CTA “no seed/wait” |
| F7 | low | Crisis “requirements unmet” opaque | Cost/need lines per option |
| F8 | low | Report hid tag theme | Home subline shows tag |

## G-pass: empty wait after depart

| ID | Sev | Challenge | Fix |
| --- | --- | --- | --- |
| G1 | high | Home is only a countdown while away | Timed station chores (fan/stove/box/gate) |
| G2 | high | Chore had no primary action | Home CTA “Station chore” resolves small reward |
| G3 | med | No chore still felt empty | `wait_nudge` points to order/build/farm |
| G4 | low | Help omitted wait loop | Help line mentions station chores |

## Lifecycle (post-fix)

New save → short runs → build → gate/deep → shed/farm/hunt/Ash Tide. Mid: full stash → expand / shelf; no seed → expedition or trade; quest-2 → ferry + root crop. Soft-lock guards unchanged; all growth capped.

## Compliance

Play-first copy only; no paywall; no wipe spiral; screenshots not faked as device captures. No IAP, ads, gacha, second wall clock, or wipe prestige. Credentials stay outside the repo (publisher config under `~/.config/folotoy/`).

## Phase D planning anchor

Dual-role host sims and compliance slices live in [EXTENSION.md](./EXTENSION.md) Phase D: D0 ember pity / Tide soft-lock first, then D1 hunt gear and D2 reputation; no wipe prestige or paid skips. **Code status (J-round):** `ember_dry` pity, `gear_lv`, `rep_shed` / `rep_ferry`, and `lore_twist` are in `ash_game_t` (save ver 7) and exercised by host tests.

## Re-audit gate

- [x] Host tests + glyph check (through G1–G4; J-round re-run PASS)
- [x] Static gate `./tools/validate.sh --static` (J-round PASS)
- [x] Firmware gate after J1 PCM alignment fix (`272894e19ae6…`, app ≈1.75 MiB / 78% free)
- [ ] On-device: chore ~25s after depart, resolve CTA, wait_nudge without chore
- [ ] On-device: idle 60 s dim / 180 s blank / expedition 5 min or station 10 min deep sleep; key restores; serial shows `mem[ui_start]` / `power dim`
- [ ] On-device: radio voice PCM events within first ~6 s once; stash upgrade first row + scroll to item list

## Round H: memory and power (ESP32-C3, no PSRAM)

Checked against the linked map for firmware `11e5a631…` and the live idle/sleep paths. **J-round code re-check:** paths still present; flash now also embeds ~242 KiB radio PCM (rodata).

### Link-time memory

| Item | Value | Verdict |
| --- | --- | --- |
| IRAM `.iram0.text` | ~68 KiB | OK |
| Static DRAM (incl. 48 KiB LVGL pool) | ~130 KiB of ~319 KiB segment | ~180 KiB-class heap left |
| LVGL pool | 48 KiB | Needed for denser home; log `lv_mem_monitor` at start |
| DMA draw buffer | 240×40×2 ≈19.2 KiB single | Fits no-PSRAM budget |
| `s_game` BSS | 568 B | Small |
| Scene sprites | Flash rodata; `.bss`/`.data` = 0 | **Pass** |
| Font subset | Flash ~75 KiB-class | Pass |
| Radio PCM (5 clips) | Flash ~242 KiB | Pass (factory 8 MB) |
| Bluetooth | `CONFIG_BT_ENABLED=n` | Pass |
| Wi-Fi stack | Default Kconfig still links `libesp_wifi`; app never inits | **Residual**: RF stays cold; unlinking needs deeper component surgery |

### Leaks / peaks

| ID | Severity | Concern | Fix |
| --- | --- | --- | --- |
| H1 | low | Page create/destroy leak | Home/list layers are resident; hide/show only |
| H2 | med | Rounded ARGB layer blows 48 KiB pool | Keep BSP flush mask; no root `clip_corner` (**residual**: list/radio panels still set `clip_corner`) |
| H3 | med | No LVGL/heap observation | `mem[ui_start]` log at UI start |

### Power / sleep

| ID | Severity | Concern | Fix |
| --- | --- | --- | --- |
| H4 | high | Full backlight until manual sleep | Dim at 60 s; blank at 180 s |
| H5 | high | Default radio noise keeps I2S/codec busy | Mute radio from dim onward |
| H6 | high | Sprite frame swaps keep SPI warm while idle | Stop anim when not `PWR_ACTIVE`; skip redraw when blank |
| H7 | high | Auto-sleep only on expedition | Station deep-sleep fallback at 10 min; never during report/crisis |
| H8 | med | 20 ms LVGL refresh too eager | `CONFIG_LV_DEF_REFR_PERIOD=33` |
| H9 | low | Deep-sleep peripheral teardown | Already matches BSP order; GPIO + timer wake |

Deep-sleep entry itself (`ashbox_enter_deep_sleep`) was already correct; this round does not change that path.

## Round I: three parallel passes × one aspect each (21 directions)

Subagent spawn was blocked by an environment hook; the same 21 single-aspect reviews ran in-session across three waves.

### Wave 1 (core loop)

| ID | Aspect | Verdict | Notes |
| --- | --- | --- | --- |
| I01 | Save / migrate / CRC | conditional PASS | CRC before v3–v6 migrate; **v1 has no CRC**; v2 rejected; store failure logged only |
| I02 | Economy settle / wall clock | **FAIL→fixed** | UI mono dt did `last_settle_unix += dt` vs deep-sleep `gettimeofday`; added `ash_game_note_wall_time` |
| I03 | Expedition / report branches | PASS | Prepay OK; loot deferred; unresolved branches survive sleep; overflow to stash |
| I04 | Crisis / threat soft-lock | PASS | Weak can hide/talk; fortify optional; threat floors avoid chatter |
| I05 | Farm / stash / trade caps | PASS | Full stash clamps; daily trade; dryer ×8/7 on tick; unpaid trades blocked |
| I06 | Quest lines | PASS | Ash Tide catchup; crisis can finish Tin at step 4; exact step gates |
| I07 | Radio / nudge lanes | PASS | Hard > soft > atmosphere > journal; chores while away; soft hooks gated |

### Wave 2 (interaction / runtime)

| ID | Aspect | Verdict | Notes |
| --- | --- | --- | --- |
| I08 | Three-key / OK guards | PASS | Nav on PRESS; DOUBLE→CLICK; 200 ms enter guard; no sleep on report/crisis |
| I09 | CJK glyph coverage | PASS | `check_ashbox_font_glyphs.py` passes |
| I10 | SFX / radio lifecycle | conditional PASS | `stop_all` before sleep; radio muted when dimmed |
| I11 | Deep-sleep wake / key-guard | PASS | GPIO+timer; refuse sleep while held; actions after release |
| I12 | LVGL locking | PASS | Input holds lock; short tick timeouts; blank skips redraw |
| I13 | Settings NVS | PASS | ver=2 blob; step-10; apply syncs BL/vol/radio |
| I14 | Sprite scenes | PASS | Flash-resident; priority crisis>report>weak; anim stops when idle |

### Wave 3 (content / engineering / edges)

| ID | Aspect | Verdict | Notes |
| --- | --- | --- | --- |
| I15 | Content-table consistency | PASS | Outcomes/branches/quests via content API; hunt needs gate |
| I16 | Host-test coverage | conditional PASS | Broad loop coverage; gaps: v1 CRC, power stages; wall resync test added |
| I17 | Community copy compliance | PASS | Play-first; no chip/framework talk |
| I18 | Build / partitions / sdkconfig | PASS | 8 MB factory; BT off; LV pool 48K; refr 33 ms |
| I19 | Station-chore edges | PASS | Max 2 per run; cleared on return; stash overflow OK |
| I20 | Battery / backlight UX | PASS | SOC top-right; idle dim/blank; settings brightness |
| I21 | Input semantics / report persist | PASS | Unresolved branch blocks loot; pending survives sleep |

### Round I actions

| ID | Sev | Issue | Fix |
| --- | --- | --- | --- |
| I02 | high | Mono clock polluted `last_settle_unix` | `ash_game_note_wall_time` + UI/boot/sleep |
| I01a | med | v1 saves lack CRC | Noted; keep current migrate/reject |
| I05a | info | Farm tick ignores sleep 0.5 | Intentional wall-clock growth |
| I16a | low | No host test for power stages | On-device checklist |

Host test `test_note_wall_time_resyncs_day` added.

---

## Round J: full re-audit (A–I + recent deltas)

**Date:** 2026-11-04 (Asia/Shanghai). Method: re-verify every prior ID against current `main/` + host tests; add deltas for SFX v4/v5, stash UX, save ver 7 / Phase D, publish staging, compliance.

### Gate evidence (this pass)

| Check | Result |
| --- | --- |
| `./tools/validate.sh --static` | **PASS** (glyphs + `test_ashbox_game` + BSP/host suites) |
| `./tools/validate.sh --firmware` | **PASS** (merged image verified; archive `272894e19ae6…`) |
| Device tests | **NOT RUN** this pass |

### Prior IDs — still hold?

| Band | Verdict | Evidence |
| --- | --- | --- |
| A1–A7 | **PASS** | Hub first item is farm; independent `stash_lv`; rare seed/ember drops; hub 9; radio stash-full; farm ready nudge |
| C1–C4 | **PASS** | `ASH_WORLD_CAMP2` / `trade2_day`; `furn_lv` shelf+dryer; grain/root; tradeoff copy in hub/farm/help |
| D1–D8 | **PASS** | `apply_report_loot` overflow; radio hard hooks before journal; `try_pocket_refill`; `use_jerky`; `trade_can_pay`; dryer on tick ×8/7; CRC on v3–v6; hunt ferry `seed%4` only |
| E1–E7 | **PASS** | Lane order in `ash_game_radio` / `home_nudge`; tier away tails; quest nudge gated |
| F1–F8 | **PASS** | `trade_hook_worth`; empty-plot plant radio; jerky/threat/fatigue paths |
| G1–G4 | **PASS** | Chore pending + resolve CTA; `wait_nudge`; help mentions chores |
| H1–H9 | **PASS** (H2 residual) | Dim/blank/sleep timers; mute radio on dim; `mem[ui_start]`; BT off; Wi-Fi residual unchanged |
| I01–I21 | **PASS** (same conditionals) | `note_wall_time` after UI settle; save ver 7; 43 host tests in `test_ashbox_game.c` |

### Recent deltas audited

| Area | Verdict | Notes |
| --- | --- | --- |
| UI SFX (`soft_osc`) | **PASS** (fixed earlier) | Phase uses `phase & 0xFFFF` (was `>>16` DC bug) |
| Radio human PCM | **FAIL→fixed (J1)** | See below |
| Stash UX | **PASS** | Upgrade + furniture first; `STASH_VIEW=5` scroll; scrollbar gets `view`; OK map matches paint |
| Save / Phase D | **PASS** | `ASHBOX_SAVE_VER=7`; migrate v1/v3–v6 with CRC (v1 head exception); gear/rep/pity present |
| Community 0.4 copy | **PASS** | Play-first; discloses illustration ≠ device capture; no chip/IAP |
| Credentials | **PASS** | Token path is user config file, not repo; `.publish-staging/publisher-config` empty |
| Offline / three-key | **PASS** | No Wi-Fi init; BT off |

### Round J actions / findings

| ID | Sev | Issue | Disposition |
| --- | --- | --- | --- |
| J1 | **high** | Embedded `_binary_radio_*_pcm_start` symbols can sit on **odd** Flash addresses; casting to `int16_t *` is undefined / Fault-risk on Xtensa/RISC-V | **Fixed:** keep `const uint8_t *`, assemble LE samples in `samp_at()` |
| J2 | med | `.publish-staging/` held merged bin + copy drafts and was **not** gitignored → accidental credential/bin commit risk | **Fixed:** add `.publish-staging/` to `.gitignore` |
| J3 | low | Five radio clips ≈242 KiB Flash | Accept; within 8 MB factory; cited in H table |
| J4 | low | List/radio `clip_corner` still used (H2 residual) | Keep; do not add root/screen clip; watch `mem[ui_start]` on device |
| J5 | info | Host tests still omit power-stage FSM and PCM playback | On-device checklist; optional future host mock |
| J6 | info | Community project **1943** rev pending as **0.4**; this audit does not re-publish | Update only when user requests |

### J-round overall

| Axis | Result |
| --- | --- |
| Prior A–G playability fixes | Still in code; host-covered where applicable |
| Phase C / D systems | Present; save ver 7 |
| H power / memory | Logic intact; radio mute on dim intact |
| I 21-aspect set | No regression found in source re-read |
| New SFX / stash / publish hygiene | J1+J2 fixed this pass |
| Compliance | Hold |

```text
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: on-device radio PCM, stash scroll/upgrade, power dim/blank/sleep serial markers
```

---

## Round K: HUD / overnight tier / cost colors / copy (2026-11-04 Asia/Shanghai)

Strict pass against the six user requests plus save/expedition/font gates.

### Requirement checklist

| ID | Request | Result | Evidence |
| --- | --- | --- | --- |
| K1 | Day label as "Day N" top-left, synced to wall `day_key` | **PASS** | `refresh_home_day_title`; day still advanced by settle/`note_wall_time` |
| K2 | Remove header brand "Ember"; center wall clock `HH:MM:SS` | **PASS** | Left day / center clock; resource-card ember kept |
| K3 | ~8h expedition option | **PASS** | `ASH_TIER_NIGHT` 28800s; pick list scrolls; sleep alarm cap 28800 |
| K4 | Material text: light blue if affordable, orange-red if not | **PASS** | Build/stash/farm/trade/crisis fortify cost colors |
| K5 | Grain→rice, tuber→sweet potato | **PASS** | Crop/item/trade/farm/help/journal strings |
| K6 | Richer warmer copy | **PASS** (conditional) | Radio/nudge/help/loadout/trade polished; report lines still compact |

### Findings

| ID | Severity | Issue | Disposition |
| --- | --- | --- | --- |
| K7 | High | Expedition remaining used to live only in RAM | Fixed earlier: ≤15s NVS flush; depart flush immediate |
| K8 | Medium | Without NTP, “real time” is device wall clock | Documented; do not fake calendar |
| K9 | Medium | Overnight loot is deep-leaning | Intentional for 8h + fatigue 40; fail ~22% |
| K10 | Low | Five tiers overflow one screen | `PICK_TIER_VIEW=4` + scrollbar |
| K11 | Low | Font subset grew with copy | Regenerated; glyphs gate PASS |
| K12 | Info | Merged flash still wipes NVS | Use preserve-save flash for daily work |

### Gate evidence

| Check | Result |
| --- | --- |
| `./tools/validate.sh --static` | **PASS** |
| `./tools/validate.sh --firmware` | **PASS** (archive `06ebab619ca3…`) |
| Device | **Pending user flash approval** |

```text
Build: PASS
Host tests: PASS
Device tests: NOT RUN
Unverified: on-device day/clock, overnight tier, cost colors, rice/sweet-potato copy
```

---

## Ten-round strict audit (2026-11-05)

Full ten rounds across compliance / content / interaction / extension / gameplay / code / power / existing-gameplay optimization: **[AUDIT_10R.md](./AUDIT_10R.md)** (verdict: compliance PASS; content/interaction/gameplay/code conditional PASS; power conditional PASS; no open Critical; does not block 0.6 testing).

## Round G: P0 dead-branch CI + late parts sink (2026-11-05 Asia/Shanghai)

| ID | Item | Result |
| --- | --- | --- |
| G0 | Build “door bolt” 24 parts (needs quiet gear); threat −1 more (floor 4); fortify −1 (floor 1); `ASHBOX_SAVE_VER=8` | **PASS** |
| G1 | `tools/check_ashbox_dead_branches.py` + host `test_dead_branch_scan`; ash-tide 27/28 system allowlist | **PASS** |

## Round L: deep-sleep mute fix + Phase F content fill (2026-11-05 Asia/Shanghai)

Strict pass against the mute bug, CONTENT_FILL F0–F5, glyph/gates, and community 0.6 update.

### Requirement checklist

| ID | Request | Result | Evidence |
| --- | --- | --- | --- |
| L1 | Deep sleep must not mute forever; any input ends quiet | **PASS** | Soft `stop_all`; abort restore; sleep after key release; wake/pending sleep cancel on press |
| L2 | Ember-whisper step 0 reachable | **PASS** | Deep outcome includes branch 19; tag pool 4/19; deep success catch-up; host assert |
| L3 | Hunt/overnight choice identity | **PASS** | Outcomes 9–12; tags jerky/long-night; `pick_outcome_id` no longer reuses short/deep |
| L4 | Report/crisis/radio/away depth | **PASS** | Tier return lines; crisis hints; away ≥40 segmented; idle/lore/twist expanded |
| L5 | Chore skins + teaching help | **PASS** | Tier-skinned chores; help = ops + progress |
| L6 | Character scent without new systems | **PASS** | Per-person/tier L0; Lao Que bearing tag bias |

### Findings

| ID | Severity | Issue | Disposition |
| --- | --- | --- | --- |
| L7 | Critical→fixed | Long-press sleep called irreversible I2S teardown while key held | **Fixed** (L1) |
| L8 | High→fixed | Ember-whisper step 0 soft-lock | **Fixed** (L2) |
| L9 | Medium | Ash-tide 27/28 still via trade/harvest | **Accepted**; clue hint documents it |
| L10 | Low | Merged flash still wipes NVS | Preserve-save for daily; community copy kept |
| L11 | Info | Community **1943** public **0.5**; this round submits **0.6** pending | Update existing play only |

### Gate evidence

| Check | Result |
| --- | --- |
| `./tools/validate.sh --static` | **PASS** |
| `./tools/validate.sh --firmware` | **PASS** (archive `d39b824a8782…` / content twin `b6ca605f…`) |
| Glyph inventory | **PASS** |
| Device | Preserve-save flashed; deep-sleep/HUD user-confirmed; Phase F feel still optional |

```text
Build: PASS
Host tests: PASS
Device tests: PASS (deep-sleep wake + header, user-confirmed); Phase F content feel partial
Unverified: full playthrough of hunt/overnight choices and long-idle radio
```
