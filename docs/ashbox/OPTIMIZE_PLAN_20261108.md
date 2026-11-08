**English** · [简体中文](./OPTIMIZE_PLAN_20261108.zh_CN.md)

# Ashbox optimization plan (final)

Companion to [AUDIT_GAMEPLAY_20261108.md](./AUDIT_GAMEPLAY_20261108.md) (34 findings) and the
slices in [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md). This file is the implementation spec:
every item is sized, quantified, and testable. Findings are mapped to slices in the appendix.

Baseline: `feature/ashbox-ember` at `16c2fa5`, save ABI v13 (blob 776 B), app 2,924,960 B in an
8,323,072 B factory partition (64.9 % free).

## 1. Target

| Axis | Today | After this plan |
| --- | --- | --- |
| Awake water production | 0 (integer flooring) | 1/h pit, 6/h well, all day |
| Population | hard cap 3, auto-join ignores beds | 2 → 6, gated by kitchen tier, beds and the super core |
| Parts | 39/person-day, builds done by day 2 | 19.5/person-day, build tree spread over ~week 1 |
| Threat | 11 crises/week, identical each time | 4–5/week with three escalation bands and four options |
| Decisions per return | one lookup table, 3 fixed rows | 3 rows selected from 5 candidates by state |
| Quest pacing | one beat per line per UTC day (08:00 local) | one per local day, plus one paid second beat |
| After the clear | nothing | a re-armed second arc with modifiers |
| Copy | four truncation classes | zero; gate extended to the nine page units |
| Home screen | no resources visible | five-cell resource strip |

## 2. Decisions and constraints

1. **No save ABI change in P0–P2.** `ash_report_t.branch_ids[3]` and `journal[8][36]` stay.
   Depth comes from *which* three branches are emitted, not from more rows; journal copy is
   shortened rather than the record widened.
2. **Food production stays at 4/h.** Weak survivors produce 2/h and the worst case consumption is
   1.65/h: that margin is the recovery invariant from zero food. Food abundance is a comfort
   mechanic; the brakes are water, threat, and time.
3. **Parts become the build currency that lasts** (halved income, stretched cost curve).
4. **Every pending event is visible from every page; every irreversible action confirms.**
5. **Copy is measured in pixels**: one list line = 196 px = 12 CJK cells = 36 B.
6. **Font subset unchanged**: no new glyphs are introduced (see §7.3), so no regeneration.

| Constraint | Value | Consequence |
| --- | --- | --- |
| Save ABI | v13, `branch_ids[3]` mid-record | candidate pool 5, emitted 3 |
| Layout lock | 240×176 stage, header 2–20, now band 196–248 | strip lives at y=197 |
| Font | 866 CJK glyphs, 16 px advance, line height 18 | new copy must be glyph-checked |
| Host tests | 533 asserts in `test_ashbox_game.c` | rate changes follow the invariant table |
| Flash | 5.4 MB free | content is free; art and PCM are capped |

## 3. P0 — defects and gate violations (16 slices, no ABI change)

| # | Fix | Change | Files | Test |
| --- | --- | --- | --- | --- |
| P0-1 | Water works while awake | `expected = min(rate × (end_unix % 86400) / 3600, day_cap)`; add `expected − water_made_today`; seed the counter when a pit/well completes | `ashbox_game.c:871-902`, `:905-935` | 3,600 s of 1 s ticks = 6 water; midnight reset |
| P0-2 | UTF-8-safe copy | `copy_utf8()` on codepoint boundaries; replace every `snprintf("%s")` into a fixed field | `ashbox_game.c:1522-1529`, `ashbox_ui.c:1568-1613`, `:1652-1674`, `ashbox_ui_pick.c:12` | gate checks 1–2 |
| P0-3 | Result → choice → settle | `s_report_step {RESULT, CHOICE, SETTLE}`; RESULT shows all three report lines; CHOICE has no preselection; 400 ms guard per step | `ashbox_ui.c:1623-1674`, `:2095`, `:2138` | device: success / injury / death |
| P0-4 | Death is visible | Modal title `return` / `did not return` / `choice` / `settle`; body keeps `lines[0..2]`, no ember special case | `ashbox_ui.c:1652-1674` | device |
| P0-5 | Home resource strip | Five 44 px cells at y=197 (water / food / parts / iron / ember), reusing hidden widgets and the unused people slot; `s_status` retired; fingerprint covers water and iron | `ashbox_ui.c:69-73`, `:971-974` | screenshot |
| P0-6 | Events visible anywhere | Header badge 34×16 at (156,1); priority report > crisis > chore > visitor > harvest / stash full / trade; one-shot hint; long-press OK returns home when an event is pending | `ashbox_ui.c:2436-2520`, `:3278-3292` | device from the trade page |
| P0-7 | Branch costs enforced | Negative food/parts deltas require stock; otherwise the row is disabled with a reason | `ashbox_game.c:2990-3034`, `ashbox_ui.c:1623-1652` | host |
| P0-8 | Irreversible actions confirmed | `s_modal_kind {INFO, CHOICE}`; super core and exile become CHOICE with cancel default; INFO closes on OK only | `ashbox_ui.c:511`, `:2545-2550`, `:2691-2712` | device |
| P0-9 | Crisis always resolvable, and exitable | HIDE allowed with nobody present; long-press leaves to home; auto-sleep unblocked for report/crisis | `ashbox_game.c:3052-3070`, `ashbox_ui.c:3113-3115`, `:764-772` | host: pop 0 + 0 parts resolves |
| P0-10 | Journal copy fits | 14 literals to ≤34 B; `ash_game_log` uses `copy_utf8` | `ashbox_game.c`, `ashbox_content.c` | gate check 3 |
| P0-11 | Guide pages fit | Six pages to ≤4 lines of ≤176 px | `ashbox_ui.c:1149-1165` | gate check 4 |
| P0-12 | Auto-join respects beds and cap | `try_join_people` joins up to `min(beds, tier cap)` and initialises every slot it opens | `ashbox_game.c:2704-2723` | host: kitchen L2 at dorm L1 joins 1, not 3 |
| P0-13 | Wipeout cannot be undone by an upgrade | `ash_game_upgrade` refuses the auto-join when `pop == 0`; repopulation stays on the wanderer path | `ashbox_game.c:2726-2757` | host |
| P0-14 | Retire the dead loadout path | Repurpose outcomes 7/8 and branches 27/28 (see §7.3); drop the always-false work lock | `ashbox_content.c`, `ashbox_game.c:699-704` | `--ashbox` |
| P0-15 | Gate extended | Ten new checks (§10) covering buffers, journal, guide, help wrap, row reset, titles, modal kinds, strip width, and the nine page units | `tools/check_ashbox_copy_layout.py`, `check_ashbox_glossary.py`, `check_ashbox_font_glyphs.py` | gate |
| P0-16 | Clock shows local time | Render with `gmtime_r(sec + 8*3600)` (or `setenv("TZ","CST-8")` + `tzset` at boot) so the header matches the clock the rules already use; today the panel is eight hours behind | `ashbox_ui.c:590` | device: header equals the local wall clock |

## 4. P1 — balance (12 slices, no ABI change)

| # | Constant | Now | New | Why |
| --- | --- | --- | --- | --- |
| B-1 | Parts base | 2/h per healthy | **1/h** | 39 → 19.5 parts per person-day; the build tree stops finishing on day 2 |
| B-2 | Work boost cap | +120 | **+60** | stops a 2.2× stack on top of the 2.8× workshop |
| B-3 | Upgrade costs | 8/20/45/70 | **8/24/60/100** | first upgrade stays reachable in session 1; the full tree ≈ 670 parts |
| B-4 | Threat rate | 0.3 × rise/h | **0.15 × rise/h** | 11 → 4–5 crises per week, so a crisis is an event, not a chore |
| B-5 | Watch order | ×0.7 | **×0.4** | the order becomes worth 15 % of daily threat instead of 7 % |
| B-6 | No-order penalty | none | **+15 % threat rise** | removes the free "no decision" option |
| B-7 | Water | pit 12, well 36 | **pit 15, well 48**; bucket stays 18 iron | 45/day is the exact 6-person need; 48 leaves 3/day, 54 would remove the trade loop |
| B-8 | Ammo | refunded on every non-death return | **one round consumed per armed trip** at departure, refunded only if the trip never faced an assault | 2 iron stops buying permanent −4 pp fail and death insurance |
| B-9 | Craft caps | silent charge at bucket 20, ammo 250, crowbar 20 | **refuse with a reason** | no more invisible iron loss |
| B-12 | Roll seed | `day × 17 + total × 13` (no person term) | **add person id and the settle hour** through a pure `ashbox_roll_seed(g, salt)` helper | two survivors sent on the same day no longer get identical loot; the helper keeps host tests deterministic |
| B-10 | Dorm food penalty | dorm L2+ sets consume to 11 (+10 %) | **dorm levels no longer change consume** (kitchen L3 still gives −10 %) | removes the trap where the bed upgrade taxes the pantry |
| B-11 | Crops | grain 30/24 h, root 240/72 h | **grain 45/24 h, root 150/48 h** | grain 1.9/h versus root 3.1/h per plot: root keeps its edge without making grain pointless |

Verified outputs with the new set (30-day model, one run per day, 15 awake + 9 sleep hours):
parts spread the six-person build tree to ≈day 5 instead of day 1.6; water at six survivors
settles at 30/68 with the well plus one bucket; crises drop from 11 to ≈4.5 per week; food stays
capped (by design) and the report's waste figure (≈70 %) is the cost of comfort, not a bug.

Already correct, do **not** touch: food 4/h, consumption 10/9 with dorm 11 and floor 8, expedition
costs/durations/loot, stash caps and slot sizes, farm grow times and labour caps, water
consumption 7.5/person/day, crisis threshold and drop values, bucket cap and slot cost, all caps.

## 5. P1 — information design (5 slices)

| # | Fix | Design | Test |
| --- | --- | --- | --- |
| U-1 | Shared row reset | `ui_row_reset(slot)` before every painter; today help sets WRAP + 60 px height and survivors/hub/trade never reset it | gate check 6 |
| U-2 | Help sections | Eight sections (intro / going out / station / build / farm / trade / crisis / story); long-press jumps a section; hint `section 2/8 · line 12/94` | device |
| U-3 | Trade shows stock and result | `iron 12 · 7 after` / `not enough` / `tank full`; footer `today: shed 1, ferry 0`; `just traded: iron -5, parts +1` for 6 s | host + device |
| U-4 | Equip list follows population | `equip_count() = pop*2 + 4`, craft rows start at `pop*2` | host |
| U-5 | Picker previews | Tier sub `food 1, water 1 · empty 5%`; person sub `empty 5% · food +20%`; running cost kept at all three steps | host: preview equals the roll |

## 6. P1 — progression (7 slices)

| # | Change | Detail |
| --- | --- | --- |
| G-1 | Day boundary to local midnight | One helper `ashbox_day_key(unix) = (unix + 8*3600)/86400 + 1`, used at `:2570-2571`, `:2588`, `:2597` — the same offset the wanderer scheduler (`:4196`) and the water window (`:776-782`) already use. No field change, no version bump. A save loaded between 00:00 and 08:00 local gets one extra use of each daily gate, which is player-favourable and cannot rewind a latch |
| G-2 | One paid second beat per line per day | `quest_beat_day` bit 15 means "second beat used today"; the second beat costs 8 parts. Day values stay below 32,768 until 2065 and old saves read as "unused". Keeps the five-day arc at three days for a player who pays |
| G-3 | Crisis bands and a fourth option | `ash_game_crisis_band()`: 0 below 80, 1 = 80–89, 2 = 90–99, 3 = 100. Band 1 is today's table plus a +8 fatigue cost on talk and −8 on hide. Band 2 adds `SALLY` (2 parts + 1 ammo, requires a healthy present fighter, threat −55 floor 8, +25 fatigue, +1 trust, sets `peace_broken`). Band 3 adds no fifth row but scales: talk 26/16/12 floor 25, fortify cost 5 drop 48, hide 38/24 floor 20 with an 8 h food penalty, sally −62 floor 6 |
| G-4 | Breach instead of a forced loss | Finishing a band-3 crisis sets `threat = min(result, 79)` and starts a 6 h grace (`crisis.reserved = 6`): threat rise suspended, no re-arming, +10 fatigue to everyone present, −1 pocket part. The audit's earlier "forced fortify failure at 100" is rejected — it soft-locks a station with no parts |
| G-5 | Every crisis option is legal | Arm only when someone is present; keep HIDE legal with nobody present; add a defensive fallback in `resolve` when no option is affordable. With a two-person station that makes HIDE the guaranteed escape hatch |
| G-6 | Post-clear second arc | Stage in `world_flags` bits 7:6 (none / A / B / done). First clear arms A: lines re-armed to 3/3/2, threat floor 20, rise +1, loot ×0.75. A complete arms B: floor 30, rise +2, loot ×0.60. Rewards are non-ember: A grants +10 permanent loot percent (cap 60) and a token; B promotes a present survivor to the double trait, falling back to stash food +50. Two new achievement bits (watcher, veteran), and the super core becomes grantable twice |
| G-7 | Night orders become four choices | NONE: +8/h fatigue recovery and +1 trust to the lowest-trust present survivor once per deep segment. RATION: consume ×0.7 (×0.6 when threat ≥80). WATCH: rise ×0.85 / ×0.75 / ×0.60 at threat <50 / 50–79 / ≥80. RUSH: parts ×1.3, consume ×1.1, rise ×1.1 / ×1.2 / ×1.5 by band |

An eight-hour two-person night at threat 85, rise 7, shows why the orders now matter: none
12 food / 16 parts / 16.8 threat plus rest and trust; ration 7.2 / 16 / 16.8; watch 12 / 16 /
10.1; rush 13.2 / 20.8 / 25.2.

## 7. P2 — content (5 slices)

| # | Change | Detail |
| --- | --- | --- |
| C-1 | Candidate pools 3 → 5, emit 3 | `k_tag` holds five ids per tag (old three first, two new appended); `ash_game_branch_pool()` filters by affordability, hard gates (camp/farm), soft gates (food/water/parts/threat/trust/fatigue/ember/job), forces the live quest beat, and emits three. Fallback to the tag anchor keeps the pool non-empty |
| C-2 | 24 new branch definitions | ids 37–60, two per tag; 16 spend food/parts/trust, 8 spend threat; 15 are conditional. Two quest-2 alternatives make its dead steps 2 and 3 reachable without the trade/harvest paths |
| C-3 | Repurpose, never delete | branches 27/28 become ordinary trade-off branches (ids are referenced by saved reports); outcomes 7/8 become short-fail / block-fail variants routed by state. Deep-fail 6 and night-fail 12 stay untouched because they carry quest beats |
| C-4 | Legacy branch rebalance | branches 20/21/33 lose their unconditional dominance (20 pays parts, 21 pays parts, 33 pays food) |
| C-5 | Ember becomes convertible | three trades (ember → 8 parts, ember → 6 water, ember → 1 stim) and a third gear level at 8 ember; the 999 ceiling gets an outlet, and the trade enum is not saved, so no ABI change |

The exact Chinese labels and log lines for ids 37–60 are listed in the paired Chinese file.

Copy budget: 24 labels + 24 log lines ≈ 316 CJK characters net, **0 new glyphs** (all characters
already in the subset), longest log 31 B, so both the 32 B report line and the 36 B journal fit.

## 8. P3 — save v14 (deferred)

Only if P2 saturates: journal 36 → 48 B, report lines 32 → 40 B, five quest slots, lifetime stats.
Cost: new blob version, migrator, frozen v13 fixture, 19 static asserts re-derived. Do not start
before P0–P2 ship and are measured.

## 9. Execution order

1. P0-1, P0-10, P0-2 (data and copy safety).
2. P0-12, P0-13, P0-9, P0-7, P0-8, P0-16 (rules, guards, deadlock, clock).
3. P0-3, P0-4, P0-11, P0-5, P0-6 (return flow and visibility; one device walk).
4. U-1…U-5, then P0-15 (gates last, so they lock in the fixed text).
5. B-1…B-12 with the invariant table first.
6. G-1…G-7.
7. C-1…C-5 and the content pass.

## 10. Test matrix

| Layer | Content |
| --- | --- |
| Host | water clock, branch cost, branch pool state matrix, quest reachability, crisis with pop 0, band transitions, day boundary, second beat latch, report lines, trade stock, fail preview, rate invariants, outcome routing |
| Gate | ten checks: buffer minimums, worst-case `snprintf`, journal ≤35 B, guide ≤4 lines, help wrap = 3 lines, `ui_row_reset` usage, title ≤80 px, modal kinds, strip ≤44 px, glossary/glyph coverage over `main/ashbox_ui*.c` |
| Device | return flow (success / injury / death), crisis at 0 parts, super core, trade, help sections, badge from a sub-page, four-person station |

## 11. Risks

| Risk | Mitigation |
| --- | --- |
| Branch filter hides a required quest beat | state-matrix test asserts every beat is reachable and every pool non-empty; forced beats bypass soft gates only |
| Outcome repurposing steals a quest fail pool | routing test pins deep-fail 6 and night-fail 12 |
| Rate changes break 533 asserts | invariant table lands first, then constants |
| Population cap raises food/water demand beyond supply | B-7 water first, then P0-12; six-person testing only after both |
| Post-clear flag in a pad byte | documented in [EXTENSION.md](./EXTENSION.md); size unchanged, static asserts prove the ABI |
| Gate too strict blocks legitimate copy | gate checks report the offending literal and the measured width |

## 12. Non-goals

No new currencies, no second clock, no networking, no sprite frames, no larger stage, no lossy art,
no font regeneration in P0–P2, no save ABI change before P3.

## Appendix — finding to slice

| Finding | Slice | Finding | Slice |
| --- | --- | --- | --- |
| C01 population cap | P0-12, C-1 | C18 post-clear | G-6 |
| C02 wipeout undone | P0-13 | C19 static choices | C-1, C-2 |
| C03 duplicate survivor | P0-12 | C20 blind choice | P0-3 |
| C04 water flooring | P0-1, B-7 | C21 hidden death | P0-4 |
| C05 ember ×1.2 no-op | P0-14 | C22 truncation | P0-2, P0-10, P0-11 |
| C06 refunded ammo | B-8 | C23 hidden resources | P0-5 |
| C07 free cost branches | P0-7 | C24 frozen sub-pages | P0-6 |
| C08 deterministic rolls | B-12 | C25 super core | P0-8 |
| C09 ember dead sink | C-5 | C26 crisis no exit | P0-9 |
| C10 iron traps | B-9 | C27 trade blind | U-3 |
| C11 dead paths | P0-14 | C28 equip rows | U-4 |
| C12 dorm penalty | B-10 | C29 modal contract | P0-8 |
| C13 late sinks | B-1…B-3 | C30 help | U-1, U-2 |
| C14 dominated choices | G-3, G-7 | C31 picker blind | U-5 |
| C15 no escalation | G-3, G-4 | C32 crisis deadlock | P0-9, G-5 |
| C16 root dominance | B-11 | C33 join ignores beds | P0-12 |
| C17 day boundary | G-1, G-2 | C34 clock timezone | P0-16 |
