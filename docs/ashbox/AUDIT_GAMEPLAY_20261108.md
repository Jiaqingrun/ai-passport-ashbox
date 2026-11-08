[简体中文](./AUDIT_GAMEPLAY_20261108.zh_CN.md) · **English**

# Ashbox gameplay content audit and optimization plan

Audit date: 2026-11-08 (Asia/Shanghai).
Scope: game content, rule/economy balance, progression, and player-facing feedback of the
Ember application.
Baseline: branch `feature/ashbox-ember` at `16c2fa5` plus the uncommitted UI page split;
firmware archive `bec09599…` (app `2,924,960 B`, ELF `f54e73a8…`).

Method: three independent read-only passes (rules and numbers; content and progression; UI and
feedback) over `main/ashbox_game.c` (4,403 L), `main/ashbox_content.c` (437 L),
`main/ashbox_ui.c` (3,379 L), the nine page units, and the help copy. Every defect below was
re-checked against the cited line. Arithmetic claims list their inputs in
[Appendix A](#appendix-a-arithmetic-inputs).

## 1. Mandatory audit sections

### 1.1 Flash / firmware size — PASS

| Check | Result |
| --- | --- |
| Image identity | `build/firmware/bec09599…/`, merged `full.bin` 2,990,496 B, app 2,924,960 B, ELF `f54e73a8…` (reused: no rules or UI change since the verified build) |
| Budget | factory `0x7f0000` = 8,323,072 B; app 2,924,960 B → **35.1 % used, 5,398,112 B (64.9 %) free** |
| Top consumers | `.flash.rodata` 2,193,984 B (scene sprites ≈1.32 MB RLE from `PIXELS 36 png`, font subset ≈0.77 MB, all remaining strings and tables ≈0.11 MB); `.flash.text` 647,914 B; `.iram0.text` 74,956 B; `.dram0.data` 7,644 B; `.dram0.bss` 147,792 B |
| Plan alignment | Consistent with [FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md); no drift |
| Allowed vs forbidden | Allowed: text/data content growth, new branches/tags/radio, one more tier. Forbidden: more sprite frames, larger 240×176 stage, lossy art, shortened radio content, PCM growth |

Content expansion in this plan costs flash in `.flash.rodata` text only: the whole P2 slice
(24 branches, 6 tags, 14 radio lines, ~16 help lines) is estimated ≪ 40 KB, i.e. under 0.5 % of
the remaining budget.

### 1.2 Whole-codebase optimization — PASS with findings

| Item | Evidence | Target |
| --- | --- | --- |
| Rules unit size | `main/ashbox_game.c` 4,403 L is the last large unit | P3-2 split by concern (economy / expedition / quest / crisis) behind `ashbox_game.h`, as the UI split did |
| Dead legacy paths | `exp.loadout` always `ASH_LOAD_NONE` (`ashbox_game.c:2679`), outcomes 7/8 and branches 27/28 unreachable, legacy ember pity (`ember_pity_need`), `crew_exhaust_remain_s`, `ash_game_work_locked` always false | P0-10 remove or mark legacy; content table shrinks |
| Duplicated state | `report.lines` 32 B vs `journal` 36 B vs page buffers 48 B: three truncation domains | P0-2 single UTF-8-safe copy helper |
| Untested host paths | no host test asserts production rates, caps, or crisis arm threshold | P1-8 rate-invariant test table |
| Save ABI | v13 frozen, 19 static asserts; pad bytes are load-bearing for alignment | no ABI change in P0/P1; P3-1 only if P2 saturates |

### 1.3 Player-facing copy layout — FAIL (fix-forward)

`tools/check_ashbox_copy_layout.py` and `tools/check_ashbox_glossary.py` both PASS, but they only
cover the help painter. Four truncation classes are live in the product and outside gate coverage:

| Class | Site | Overflow |
| --- | --- | --- |
| Person picker line | `ashbox_ui_pick.c:12` `char line[48]`, `:73-81` | the first survivor's trait row is 60 B → cut mid-codepoint |
| Crisis hint | `ashbox_ui.c:1575` `char sub[48]`, `:1568-1613` | hide hint 60 B, fortify hint 56 B → the 4-hour production penalty and the part unit are lost |
| Journal | `ashbox_game.h:11` 36 B, `ashbox_game.c:1522-1529` | 14 of 76 log literals exceed 35 B (up to 49 B) |
| Guide page | `ashbox_ui.c:1149-1165`, `:1921` 184×72 label | first-run page needs 5 wrapped lines in a 4-line box |

Verdict: **FAIL** — P0-2 and P0-9 fix it; the gate must be extended in the same slice so the class
cannot return. Fixes land before the next delivery that touches UI copy.

## 2. What the game currently is

An offline survival idle loop with a 240×320 screen: send one survivor on a timed run, come back to
a branch choice, spend food/parts on buildings, farm, trade, and advance three five-beat quest
lines that are capped at one beat per line per day.

### 2.1 Live rates (per station hour, no order, not deep)

| Flow | Value | Source |
| --- | --- | --- |
| Food production | +4 per healthy person (weak 2, asleep 2) | `ashbox_game.c:2337`, `:107-122` |
| Food consumption | −1.5 per present person (kitchen L3 −10 %, stove −1 step, ration ×0.7) | `:2328-2350` |
| Parts production | +2 per healthy person at workshop L1 (3 / 4 / 4.8 / 5.6 at L2–L5); work jobs up to ×2.2 | `:2326-2343`, `:1796-1812` |
| Threat | +0.3 × rise (3.0/h at the start, 2.1/h after quest 0) | `:2400`, `:2274-2286` |
| Water | −0.5 per present person per hour, only 07:00–22:00 local → 7.5/person/day | `:832-868` |
| Water production | pit 12/day, well 36/day (whole-hour flooring, see C04) | `:871-902` |

### 2.2 Expedition return on invest (one person, single run)

| Tier | Duration | Food/water cost | Food loot avg | Parts E | Iron avg | Ember | Death chance at threat 0 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 short | 3 min | 1 / 1 | 3.0 | 0.15 | 1.0 | — | 0.10 % |
| 1 street | 1 h | 2 / 1 | 6.5 | 0.33 | 3.5 | — | 0.96 % |
| 2 deep | 4 h | 8 / 4 | 11.5 | 0.64 | 6.0 | 35 % | 3.24 % |
| 3 hunt | 2 h | 4 / 2 | 5.5 | 0.10 | 2.0 | 20 % | 2.40 % |
| 4 night | 8 h | 16 / 8 | 19.0 | 0.95 | 7.5 | 45 % | 4.84 % |

Staying home for the same 4 hours yields 16 food; a deep run nets 3.5–6 food and 4 water. **Runs
are not a food engine — they buy ember, quest beats, and tags.** That is a defensible design, but
nothing in the copy says so (C21, C31).

### 2.3 Binding constraints

| Constraint | Limit | Consequence |
| --- | --- | --- |
| Population | cap 3 (kitchen L2 → 2, workshop L2 → 3, then min(beds)) | slots 4–6, beds 4–6, most wanderers, and one achievement are unreachable (C01) |
| Water | 7.5/person/day vs well 36/day | water is the only real brake at 3 people; above that it depends on the trade chain |
| Food storage | pocket 50, stash 40 at L1 | overflow, not scarcity, is the food problem |
| Parts | +2/h → 48/day | every upgrade costs 8–100 → parts stop mattering after the first day (C13) |
| Ember | 17 total sink, 999 cap | after gate + gear L1/L2 + box lamp, ember is dead weight (C09) |

## 3. Content inventory

| Item | Count | Notes |
| --- | --- | --- |
| Branch definitions | 37 | effective choice set is 12 tag pools × 3 + 5 failure outcomes; branches 27/28 unreachable, outcomes 7/8 dead (C11) |
| Distinct branch ids reachable | 35 | 27 appear in tag pools; the rest only through outcome lists |
| Tags | 12 | 3 branches each, fixed; no state filtering (C19) |
| Away / radio lines | 40 / 50 | radio: 20 idle, 12 lore, 8 twist, 10 watch |
| Quests | 3 × 5 beats | one beat per quest per day (C17); after completion a line is done for the run |
| Crisis options | 3 | identical every time, no escalation (C15) |
| Tiers / trades / crafts / chores / duties | 5 / 12 / 5 / 4 / 4 | tier 3 (hunt) is the only gated one |
| Person traits / work traits | 6 / 8 | two work traits apply regardless of the assigned job (`:2344-2347`, `:3549-3551`) |
| Meds / items / achievements | 3 / 4 / 5 | meds cost 24–30 parts via trade |
| Help lines | 94 | flat list, 3 visible, no headings, duplicated first/last line |
| Player-facing text | ≈7,600 CJK chars | 3,548 in content tables, 1,741 in help |

## 4. Findings

Severity: **S1** blocks progress or loses information, **S2** weakens a core decision, **S3**
polish. All are code facts unless marked *inference*.

### 4.1 Rules and economy

| ID | Sev | Finding | Evidence |
| --- | --- | --- | --- |
| C01 | S1 | Population can never exceed 3, so 3 of 6 save slots, beds 4–6, most wanderer joins, and the crowd achievement are dead content | `ashbox_game.c:675-685`, `:3996-4003`, `ashbox_meta.c:113` |
| C02 | S1 | A wipeout is undone by any building upgrade: `ash_game_upgrade` has no population guard and repopulates via `try_join_people` | `:2726-2757`, `:2704-2724` |
| C03 | S1 | Buying workshop L2 at population 1 creates a duplicate of person 0: slot 1 is never initialised and reads as healthy | `:2715-2723` vs `:1630-1644` |
| C33 | S1 | Auto-join ignores beds and the cap: kitchen/workshop upgrades push population to 2–3 even with two beds, so the station can hold more people than it can house | `:2704-2723` vs `:3996-4003` |
| C32 | S1 | A crisis can become unresolvable: with population 0 and parts below the fortify cost, talk and hide both require a present person, so no option is legal while the crisis also blocks sleep | `:3052-3070`, `:764-772` |
| C04 | S1 | Pit/well production floors to zero on short settles: 1 s awake ticks produce no water at all, so wells only work across sleep | `:886` |
| C05 | S2 | The gate's ember ×1.2 is a no-op: `(ember*12+5)/10` maps 1 to 1 | `ashbox_game_loot.c:19-21` |
| C06 | S2 | Ammo and pistol are refunded on every non-death return, so one 2-iron round buys −4 pp fail and death insurance forever | `ashbox_game.c:2188-2192`, `:2137-2142` |
| C07 | S2 | Branch costs are not enforced: negative food/parts deltas are merged into the pending report and clamped at zero, so a cost branch is free when the stock is empty | `:2999-3009`, `:2947-2958` |
| C08 | S2 | Success and loot are a pure function of `(day_key, expedition_total)` with no person term: identical results for every survivor, no reroll, memorisable | `:2538`, `ashbox_game_loot.c:212` |
| C09 | S2 | Ember has 17 points of lifetime sink (gate 3, gear 3+5, lamp 6), then accumulates to 999 with no conversion | `:2938`, `:2795-2802`, `:2885-2889` |
| C10 | S3 | Silent iron traps: bucket 21+ spends 18 iron and a slot for nothing, ammo above 250 and crowbars above 20 still charge | `:3917-3935`, `:406-412` |
| C11 | S3 | Dead branches: loadout is always none, so outcomes 7/8 and branches 27/28 are unreachable; legacy pity/exhaust/lock paths never run on new saves | `:2679`, `:2700`, `:2003-2028` |
| C12 | S2 | Dorm L2 raises food use 10 %; only kitchen L3 removes it again, and beds above 3 are unusable | `:2266-2272`, `:2328-2330` |
| C13 | S2 | Late sinks never repay: workshop L3→5 costs 45/70 for +0.8 parts/person-hour, stove lining needs 74 parts to save 0.45 food/h, door bolt needs 24 crises | `:22`, `:2326`, `:2331`, `:3047` |
| C14 | S2 | Two dominated choices: hide is weaker than talk whenever person 0 is home (and adds a 4 h food penalty); the no-order option has no upside | `:3085-3121`, `:2333-2335` |

### 4.2 Progression and content depth

| ID | Sev | Finding | Evidence |
| --- | --- | --- | --- |
| C15 | S2 | Crisis never escalates: same three options, same numbers, same text, any number of times | `:3057-3123` |
| C16 | S3 | Root outclasses grain (240 vs 30 base, same seed price at the two camps), so grain is obsolete once the ferry opens | `ashbox_game.c:3732-3742`, `:3535-3548` |
| C17 | S2 | Quest beats are capped at one per line per day, and the day rolls at UTC midnight = 08:00 local: the "come back tomorrow" promise is off by up to 10 hours and nothing shows the reset time | `:2571`, `:1600-1607` |
| C18 | S1 | After the clear there is no new content: 3 lines done, 10 watch lines unlocked, achievements need ≥4 clears, nothing else changes | `:3220-3230`, `ashbox_meta.c:106-147` |
| C19 | S1 | Choices are a lookup table: 12 fixed pools of 3 fixed branches, no filtering by stock, trust, threat or quest step, so a repeated tag is a repeatable answer | `ashbox_content.c:295-309`, `:2242-2250` |

### 4.3 Interaction and feedback

| ID | Sev | Finding | Evidence |
| --- | --- | --- | --- |
| C20 | S1 | The branch is chosen before the player sees the trip result, injuries or death; row 0 is preselected and the OK guard is 200 ms | `ashbox_ui.c:1623-1652`, `:2138` |
| C21 | S1 | The settlement modal drops report lines whenever ember is non-zero, so the death cause is never shown; the title stays neutral even when someone died | `:1652-1674` |
| C22 | S1 | Copy truncation (see 1.3) cuts UTF-8 mid-codepoint in three surfaces | see 1.3 |
| C23 | S1 | Home hides food, water, parts and iron: the widgets exist, are hidden once and never repainted, so every reward and burn is invisible where decisions happen | `:69-73`, `:971-974` |
| C24 | S1 | Sub-pages never refresh, so a report, crisis, chore or harvest that arms while the player is in the hub is invisible | `:3278-3292` |
| C25 | S2 | The super core is consumed by one OK with no confirmation and burns a career flag | `:2545-2550`, `ashbox_game.c:4333-4345` |
| C26 | S2 | The crisis page has no exit: back, sleep and auto-sleep are all blocked, so the device stays lit until a choice is made | `:3113-3115`, `:764-772` |
| C27 | S2 | The trade page shows neither stock nor result; trading is observable only by sound | `ashbox_ui_trade.c:54-154` |
| C28 | S3 | The equip list is fixed at 12 person rows regardless of population, and craft rows show no state | `ashbox_ui_equip.c:6-10`, `:39-63` |
| C29 | S3 | The modal footer promises any key closes, but up/down do not; the exile confirm is not a real modal | `:511`, `:3078-3089`, `:2691-2712` |
| C30 | S2 | Help is 94 unsectioned lines at 3 per screen, and visiting it leaks its wrap/height settings into later pages | `ashbox_ui_help.c:29-31`, `ashbox_ui_survivors.c:36-42` |
| C31 | S2 | The tier's own food/water cost only appears at the final step, and per-person bonuses are never numerised | `ashbox_ui_pick.c:34-40`, `:101-126` |
| C34 | S1 | The header clock is UTC while every game system uses UTC+8: `localtime_r` runs without any timezone being set, so the displayed time is eight hours behind the clock that drives water, wanderers and the daily reset | `ashbox_ui.c:590`, `ashbox_game.c:776-782`, `:4196` |

## 5. Optimization plan

Sequencing: P0 removes defects and gate violations; P1 makes the existing systems meaningful; P2
adds content with the flash headroom; P3 is only needed if P2 saturates.

### P0 — fix what is broken (no save ABI change)

| Slice | Change | Files | Verify |
| --- | --- | --- | --- |
| P0-1 | Water production by absolute clock: `expected = min(rate × (end_unix % 86400) / 3600, day_cap)`, add the difference against `water_made_today`; on pit/well completion seed the counter from the current clock so no retroactive water | `ashbox_game.c:871-902`, `:905-935` | host test: 3600 s of 1 s ticks with a well yields 6 water; crossing midnight resets |
| P0-2 | One UTF-8-safe copy helper for every fixed field; shorten the 6 literals that cannot fit | `ashbox_game.c`, `ashbox_ui_pick.c`, `ashbox_ui.c`, `ashbox_content.c` | extended `check_ashbox_copy_layout.py`: longest reachable literal ≤ field size |
| P0-3 | Settlement modal shows the whole report: title (return / settle / someone did not come back), resources, water, narrative, ledger | `ashbox_ui.c:1652-1674` | device walk: ember run keeps the death line |
| P0-4 | Home resource strip: food / water / parts / iron, repainted every home refresh | `ashbox_ui.c:69-73`, `:971-974` | screenshot |
| P0-5 | Pending-event visibility off home: bar badge plus auto-return when a report or crisis arms | `ashbox_ui.c:3278-3292` | device walk |
| P0-6 | Enforce branch costs: a row whose negative food/parts delta exceeds stock is disabled with a reason | `ashbox_game.c:2990-3034`, `ashbox_ui.c:1623-1652` | host test: cost branch at zero stock does nothing |
| P0-7 | Result-before-choice: insert a result step, clear the default row, raise the OK guard to 400 ms | `ashbox_ui.c:1623-1652`, `:2138` | device walk |
| P0-8 | Confirm the super core; allow leaving the crisis page to home so the device can sleep, keeping the crisis armed | `ashbox_ui_build.c:193-194`, `ashbox_ui.c:3113-3115` | device walk |
| P0-9 | Journal copy fits: shorten the 14 long log literals to ≤35 B | `ashbox_game.c`, `ashbox_content.c` | extended gate |
| P0-10 | Retire the dead loadout path (outcomes 7/8, branches 27/28) and the always-false lock, keeping save compatibility | `ashbox_content.c`, `ashbox_game.c:699-704` | `--ashbox` |

### P1 — make the systems mean something (no save ABI change)

| Slice | Change | Numbers | Risk |
| --- | --- | --- | --- |
| P1-1 | Population cap by tier: `cap = min(beds, 1 + kitchen≥2 + kitchen≥3 + kitchen≥4 + kitchen≥5 + workshop≥3)` → 2 at kitchen 2, 3 at kitchen 3, 4 at kitchen 4, 5 at kitchen 5, 6 with workshop 3 on top; without the super core the cap stops at 4 | revives `person_ext`, beds 4–6, wanderers, crowd achievement | needs P1-2 water first; add host tests for every cap step |
| P1-2 | Water: well day cap 36 → 54, pit 12 → 18; bucket 18 → 12 iron | 6 people need 45/day | water stops being a soft lock at 4–5 people |
| P1-3 | Crop rebalance: grain base 30 → 45 (24 h), root base 240 → 150 (48 h) | grain 1.9/h, root 3.1/h per plot | root keeps its edge without obsoleting grain |
| P1-4 | Crisis bands: 80–89 current, 90–99 drops −8 and a fourth option (hand over food, threat −45), 100 → forced fortify failure; hide penalty 4 h → 2 h with drop 32; the no-order option adds +10 % threat rise | makes hide and the order real choices | band logic lives in `ash_game_resolve_crisis` |
| P1-5 | Branch depth: filter pools by affordability and trust, plus 2 new branches per tag (24 definitions) with genuine trade-offs | choice count 3 → 4–5 per return | content-only, watch the copy gate |
| P1-6 | Roll variation: `seed = mix(day, total, person, settle second)` in a pure helper | restores per-survivor variance, keeps determinism by history | host test fixes the sequence |
| P1-7 | Ember sinks: trades ember → parts (1:8), ember → water (1:6), ember → stim (1:1); gear L3 at 8 ember | removes the 999 ceiling | trade enum only, no save field |
| P1-8 | Caps refuse instead of charging: bucket 20, ammo 250, crowbar 20 | no silent iron loss | one guard per craft |
| P1-9 | Rate-invariant host test table: food/parts/threat/water per hour, cap edges, crisis threshold | regression net for all of P1 | new `test_ashbox_game` cases |

### P2 — content growth (uses the 64.9 % flash headroom)

| Slice | Change | Estimate |
| --- | --- | --- |
| P2-1 | Post-clear layer: a restart option that keeps gear and re-arms the three lines with a +10 threat floor and −10 % loot; show `clear_count` | reuses existing fields; no ABI change |
| P2-2 | Volume: tags 12 → 18, branches 37 → 61, away lines 40 → 56, watch radio 10 → 24, help 94 → ~110 with six section headers | ≪ 40 KB of rodata |
| P2-3 | New tier (night raid) gated by threat, with its own loot table and failure profile | one enum value, no save change |
| P2-4 | Achievements: three more bits (already in the meta mask) plus a visible clear counter | `ashbox_meta.c` only |

### P3 — save v14 (only if P2 saturates)

Journal 36 → 48 B, report lines 32 → 40, five quest slots, cumulative stats. Cost: a new blob
version, migration, a frozen v13 fixture, and the 19 static asserts re-derived. Do not start
before P0–P2 are delivered and measured.

## 6. Non-goals

No new currencies, no second clock, no networking, no more sprite frames, no larger stage, no
lossy art, no CJK copy growth without a font subset check. No save ABI change in P0/P1.

## 7. Verification and acceptance

| Gate | Content |
| --- | --- |
| `./tools/validate.sh --ashbox` | every slice; P0 adds truncation and rate tests |
| `./tools/validate.sh --firmware` | P1-1/P1-2/P2-3 (rate and enum changes) |
| Device | P0-3/P0-4/P0-5/P0-7/P0-8 need a screen walk; P1-1 needs a four-person station |
| Acceptance | C01–C04, C07, C20–C24 closed; water no longer zero while awake; no reachable literal exceeds its field |

## Appendix A: arithmetic inputs

- Food production `labor_s·4·sn/(3600·sd)` with a healthy person worth `labor_s = dt` (`:2337`, `:2545-2553`).
- Food use `present_s·3·sn·consume/(72000·sd)`, `consume = max(8, (kitchen≥3 ? 9 : 10)·dorm/10 − stove)` (`:2349-2350`, `:2328-2332`).
- Parts `labor_s·2·mult·sn/(36000·sd)`, `mult` = 10/15/20/24/28 by workshop level times the work boost (`:2326-2343`).
- Threat `dt·3·rise·sn/(36000·sd)` (`:2400`); crisis arms at 80 after three expeditions (`:2297-2306`).
- Water use `pop_present·consume_s/7200`, consuming hours 07:00–22:00 local → 15 h × 0.5 = 7.5/person/day (`:832-868`).
- Expedition costs: water = hours, food = hours × 2 (short = 1) (`:423-432`); durations 180/3600/14400/7200/28800 s (`:17`).
- Ember sink: gate 3 (`:2938`), gear 3 + 5 (`:25-26`), box lamp 6 (`:2885-2889`) = 17.
- Upgrade costs 8/20/45/70 parts (`:22`); workshop marginal 0.8 parts/person-hour at L3→5 (`:2326`).
