[简体中文](./AUDIT_10R.zh_CN.md) · **English**

# Ashbox — ten-round strict audit

> **Standing gate (added later):** every future audit must also include **Flash size** and **whole-code optimization** sections — see [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md) and root [`PROJECT.md`](../../PROJECT.md). The body below is a 2026-11-05 historical snapshot that did not yet mandate those two blocks; new audits must not reuse this file alone and skip the gate.

Subject: `feature/ashbox-ember` working tree (Phases D–G0 / F content / player-facing loot planning copy / help selected slow-scroll).  
Audit time: 2026-11-05 09:47 (Asia/Shanghai).  
Method: EXTENSION compliance rails + live static scan + host-gate evidence; **do not mark untested device items PASS**.

```text
Gate snapshot
./tools/validate.sh --static → PASS (glyphs / dead-branch scan / 47 host tests)
Latest preserve-save flash → PASS (boot to UI ready; app ~1.89 MiB)
Device → help selected slow-scroll flashed pending confirmation; Phase F full playthrough not closed
Community → public 0.5; 0.6 was submitted pending (creator-side status)
```

---

## Verdict (read first)

| Dimension | Result |
| --- | --- |
| Compliance | **PASS** (no IAP / wipe-reroll / second clock; no chip leaks in player strings) |
| Content wiring | **PASS (conditional)** (F0–F5 in; ash-tide 27/28 system-only by design) |
| Interaction | **PASS (conditional)** (three-key loop; help selected scroll updated; hub still deep) |
| Extension bounds | **PASS** (no new camp / currency) |
| Gameplay flywheel | **PASS (conditional)** (D0–G0 in; late sinks still thin) |
| Code | **PASS (conditional)** (rules host-testable; `ashbox_ui.c` ~2.6k lines fat) |
| Power | **Conditional PASS** (idle ladder complete; Wi-Fi may still link; no instrumented current) |
| Planning copy | **High inaccuracy fixed this round** (gate +20% excludes hunt); device re-read pending |
| Optimization | See rounds 8 / 10 backlog — **does not block continued testing** |

**Deltas vs prior 10R**

1. Player-facing planning copy on route / person / loadout / build / Help (20 lines).  
2. Help scrolls only the selected row (~18 s cycle).  
3. Audit found and fixed: gate loot bonus copy implied hunt also gains +20%, but hunt skips `apply_loot_bias`.

---

## Round 1 · Compliance

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R1-01 | Critical | No IAP / ads / gacha / speed tickets | **PASS** | No store/ad APIs; EXTENSION rails |
| R1-02 | Critical | No wipe-reroll; no forced DAU penalty | **PASS** | Append-only `ASHBOX_SAVE_VER=8`; offline settle |
| R1-03 | High | Player strings have no chip / framework / bus names | **PASS** | String scan on content/game/ui; `lvgl.h` is include-only |
| R1-04 | High | Art does not fake device screenshots | **PASS** | Community copy declares mood art |
| R1-05 | High | Activity is a bonus, not a gate | **PASS** | Duty ≤3; chores capped; expeditions independent |
| R1-06 | Medium | Credentials stay out of the repo | **PASS** | Publisher under `~/.config/folotoy/`; staging has no token |
| R1-07 | Medium | Growth capped; saturated math | **PASS** | Caps + `sat_add` / clamp |
| R1-08 | Low | Community changelog honesty | **Conditional** | Staging changelog must align before next publish that mentions planning copy |

**Round verdict:** Compliance rails intact. Public community wording must still avoid gacha-pity commercial phrasing (EXTENSION).

---

## Round 2 · Game content

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R2-01 | Critical | Ember Whisper step 0 reachable | **PASS** | Branches 4/19 in pools; deep catch-up; host tests |
| R2-02 | High | Hunt / overnight own outcome pools | **PASS** | Outcomes 9–12; tier-first `pick_outcome_id` |
| R2-03 | High | Dead-branch scan | **Conditional** | CI: `reachable=35 system_only=[27,28]` intentional |
| R2-04 | Medium | Away / radio depth | **PASS** | Away 40; idle/lore/twist present |
| R2-05 | Medium | Report / crisis peaks | **PASS** | Tier return lines; tight width |
| R2-06 | Medium | Help teaches planning | **PASS (strengthened)** | 20 scrollable lines; selected slow-scroll |
| R2-07 | High | Planning copy matches rules | **Fixed this round** | Gate +20% **excludes hunt**; fail-floor wording = non-short |
| R2-08 | Low | Person loot bias on hunt | **Info** | Hunt skips loot bias; fail-rate person modifiers still apply |

**Round verdict:** Main content risks remain closed; transparency mismatch closed in copy, pending device re-read.

---

## Round 3 · Interaction

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R3-01 | Critical | Long-press sleep does not permanently mute | **PASS** | Soft stop; abort restores; prior device confirm |
| R3-02 | High | Report / crisis awake-only | **PASS** | `power_blocks_autosleep` |
| R3-03 | High | Key anti-misfire | **PASS** | `key_guard` / `ok_guard` / wake guard |
| R3-04 | Medium | List vs screen height | **Conditional** | Route scrolls; hub still deep |
| R3-05 | Medium | Help scroll UX | **PASS (conditional)** | Selected-only circular scroll, 18 s; speed awaiting user OK |
| R3-06 | Medium | Dim / blank recoverable | **PASS** | `power_on_input` |
| R3-07 | Low | Input queue depth 8 | **Info** | Fast taps may drop |
| R3-08 | Low | Crisis non-returnable | **PASS (design)** | Forced choice |

---

## Round 4 · Extension bounds

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R4-01 | High | No fourth camp / second currency | **PASS** | Iron Shed + ferry only |
| R4-02 | High | No second clock | **PASS** | Single settle |
| R4-03 | High | Append-only saves | **PASS** | VER 8 (door bolt) |
| R4-04 | Medium | A–G0 remain the core | **PASS** | Stash/farm/trade/gear/rep/duty/bolt |
| R4-05 | Medium | “Do not” list intact | **PASS** | No extra plots; duty not mandatory production |
| R4-06 | Low | Doc layering | **Conditional** | CONTENT_FILL still has pre-F snapshot tone |

---

## Round 5 · Gameplay flywheel

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R5-01 | High | Gate desert relief | **PASS** | Soft ember catch-up; deep/night guarantee |
| R5-02 | High | Tide jerky soft-lock | **PASS** | Shard alternate + harvest catch-up |
| R5-03 | Medium | Short/mid/long loops | **PASS** | 3 min / 1 h / 2–8 h |
| R5-04 | Medium | Late voluntary sinks | **PASS (G0)** | Quiet gear + door bolt |
| R5-05 | Medium | Stash-full hooks | **PASS** | Nudge / stop production |
| R5-06 | Medium | Short daily cap | **PASS** | 3 / day |
| R5-07 | Low | Post-quest narrative | **Conditional** | Twist radio; no new quests by design |
| R5-08 | Low | Failure feel | **PASS** | Injury/threat; non-short fail floor ≥5% |

---

## Round 6 · Code

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R6-01 | High | Rules host-testable | **PASS** | 47 host cases |
| R6-02 | High | LVGL lock discipline | **PASS** | Key path locked; tick timed lock |
| R6-03 | High | Deep-sleep teardown order | **PASS** | `ashbox_sleep.c` |
| R6-04 | Medium | `ashbox_ui.c` size | **Conditional** | ~2645 lines |
| R6-05 | Medium | Sprite / font Flash | **Conditional** | sprites ~3.0 MiB source; font ~0.68 MiB; app ~1.89 MiB |
| R6-06 | Medium | Wi-Fi may still link | **Conditional** | App never `esp_wifi_init`; `demo_wifi.c` remains; no current measurement |
| R6-07 | Low | Dead-branch CI | **PASS** | `check_ashbox_dead_branches.py` |
| R6-08 | Low | Overnight pity constant | **Info** | Overnight success always grants ember and clears dry streak |

---

## Round 7 · Power

| ID | Sev | Check | Result | Evidence |
| --- | --- | --- | --- | --- |
| R7-01 | High | 60 s dim + radio stop → 180 s blank | **PASS** | `PWR_DIM/BLANK` |
| R7-02 | High | Auto sleep 5 min expedition / 10 min station | **PASS** | Report/crisis block |
| R7-03 | High | Peripheral shutdown in deep sleep | **PASS** | Audio/I2C/display prepare; RTC ≤8 h |
| R7-04 | High | BT off | **PASS** | `CONFIG_BT_ENABLED=n` |
| R7-05 | Medium | Wi-Fi cold-RF assumption | **Conditional** | No app init; **no ammeter** |
| R7-06 | Medium | Blank skips redraw | **PASS** | Present |
| R7-07 | Medium | Abort sleep restores audio | **PASS** | Soft-mute path |
| R7-08 | Low | Long radio listen feel | **Not instrumented** | Unverified |

---

## Round 8 · Optimization backlog (no new systems)

| Pri | Item | Player feel | Notes | Risk |
| --- | --- | --- | --- | --- |
| P0 | Keep planning copy rule-aligned | Trust | Gate +20% fixed; optional hunt-bias note/test | Low |
| P0 | Device confirm help + loot copy | Readable planning | T-11 | — |
| P1 | Hub information architecture | Fewer presses | Pin commons / fold help | UI debt |
| P1 | Person bonus wording on hunt | Avoid false +food on hunt | Tier-aware subcopy | Copy |
| P1 | Wi-Fi component baseline | Cleaner power/size | defaults + measure | Needs lab |
| P2 | Refresh CONTENT_FILL inventory | Docs | Mark post-F | Docs |
| P3 | Split UI compile units | Maintainability | Large diff | Process |

**Still forbidden:** new camp, currency, more plots, paid skips, wipe multipliers, drop-rate changes (this phase: visibility only).

---

## Round 9 · Cross-check

| ID | Sev | Item | Result |
| --- | --- | --- | --- |
| R9-01 | High | Transparency vs rates unchanged | **Aligned** |
| R9-02 | High | Gate copy vs `apply_loot_bias` | **Aligned this round** (hunt excluded) |
| R9-03 | High | Sleep fix vs power ladder | **No conflict** |
| R9-04 | Medium | Dead-branch CI vs tide system branches | **Consistent** |
| R9-05 | Medium | 47 host tests vs device | Logic PASS; help feel PARTIAL |
| R9-06 | Medium | Glyph gate | PASS; no pixel self-check this round |
| R9-07 | Low | README points at this audit | Kept |

---

## Round 10 · Decision and actions

### Decision

1. **Continue test-build play acceptance**; no open Criticals.  
2. **One High copy defect closed this round**; remaining High items are mostly unmeasured / unplayed.  
3. Next: **device-confirm help + route copy** → optional hub slim / hunt person wording → do not touch rates yet.

### Actions

| Action | Owner | Status |
| --- | --- | --- |
| Device: help slow-scroll + loot copy readability | Creator | In progress (T-11) |
| Flash corrected “gate +20% excludes hunt” copy | Eng | Code done; flash pending |
| Align community package when publishing planning notes | Creator | Check at publish |
| Optional ammeter on dim/deep sleep | Lab | Unscheduled |
| Optional hub IA slim | Eng | Backlog |

### Gate fields

```text
Build: PASS (last firmware gate; re-build/flash needed after gate-copy fix)
Host tests: PASS
Device tests: PASS (prior flash+boot); PARTIAL (help slow-scroll flashed; gate wording fix not yet)
Unverified: Phase F full playthrough, long radio, deep-sleep µA, font pixel self-check, final help feel
Compliance: PASS
```

---

## Appendix · static inventory (2026-11-05 09:47 CST)

| Item | Value |
| --- | --- |
| Branches | 37 (35 reachable; system 27/28) |
| Host tests | 47 |
| Save | ASH2 / VER 8 |
| Help | 20 lines; selected scroll 18 s |
| Ember catch-up | short 12 / block 10 / hunt 8 (night constant idle) |
| Idle | 60 s dim / 180 s blank / 5–10 min deep sleep |
| BT | off in defaults |
| App Wi-Fi init | none |
| UI / game LOC | ~2645 / ~2396 |
