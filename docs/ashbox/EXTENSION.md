[简体中文](./EXTENSION.zh_CN.md) · **English**

# Ashbox formal-content extension

The test build's end is the start of formal content. This page defines **how systems grow** so resources, saves, and the three-key UI stay safe.

## Principles

1. **Keep the existing loop**: depart → wall-clock / deep sleep → return choices → crisis / quests. New systems hang off `ash_game_settle` and Manage.
2. **Play-first copy**: no chip/framework talk in player-facing text.
3. **Hard caps everywhere**: saturating add/sub; production stops at cap; no uint wrap.
4. **Gated unlocks**: gate → hunt; kitchen Lv2 → farm; deep contact → Iron Shed; quest-2 finish / hunt encounter → Ash River ferry.
5. **Append-only saves**: `ASHBOX_SAVE_VER=11`, new fields before `crc`; migrate older versions safely. Gameplay retunes may reuse spare bytes (`pad_v10b` / `pad_v11`) without bumping the version when layout allows.

## World layer

| Beat | Delivery |
| --- | --- |
| Beyond the gate | Radio/journal lines after the gate opens; “ash shards” from deep runs |
| Disaster cause | Revealed across shard count 0→4 (never one dump); quest 2 finishes the reveal |
| Other camps | **Iron Shed**: contact after a deep success; daily trade. **Ash River ferry**: after quest 2 or a hunt encounter; separate daily trade for root seeds |
| Quest 2 | Hear → contact shed → trade grain seed → harvest → assemble shards |

## Systems

- **Stash**: capacity from `stash_lv` (40/100/180); `furn_lv` shelf (+20) and drying rack (~×8/7 grow on tick, mid-upgrade OK); upgrade with parts (+ token for rack). Pocket empty with stash food auto-pulls on settle; report loot overflows to stash.
- **Items**: rice seed / jerky / token / shard (cap 20) plus sweet-potato seed `seed_root` (cap 20). Jerky converts to +4 food from stash UI, or via camp trades.
- **Farm** (2 fields; player copy says "field", not the old "plot/ridge" wording): rice (~4h, +4..6) or sweet potato (~6h, +7..10, needs ferry). Gate unlock = open the station door so hunt is available.
- **Trade**: Iron Shed once per `day_key`; ferry once per `trade2_day`; fixed ratios; all clamped.
- **Expedition tiers**: short / block / deep / hunt / **overnight (~8h for sleep hang)**.
- **Hunt tier**: open gate; food+jerky; may open ferry / drop root seed.

## Caps

Pocket food ≤ kitchen cap (≤150); stash food ≤ stash_lv (+ shelf); parts ≤999; items/root ≤20; threat 0..100; each camp 1 trade/day.

## Roadmap

| Phase | Scope | Status |
| --- | --- | --- |
| A | Stash / items / farm / trade / hunt + save v4 + host tests | done |
| B | Quest line “Ash Tide Beyond the Gate” | done |
| B+ | Hub UI, independent stash tier, rare seed/ember drops, audit | done |
| C | Second camp, stash furniture, dual crops + save v6 | done |
| D | Ember sinks, hunt gear, camp reputation, lore twist (below) | **landed (save v7)** |

**Phase C player-facing purpose (one line):** Iron Shed covers parts/grain seed; Ash River ferry covers slower, fuller root crops; shelf stops full-stash halt, drying rack speeds farm growth — mid-game throughput for a three-person station, not more parallel systems.

## Dual-role discussion and host sims (Phase D basis)

Host-only probe `tools/sim_ashbox_roles.c` (not shipped in firmware) runs ~96 wall-clock hours on live `ash_game_*` with two priority profiles.

### Role A · Box-keeper (investigation first)

- **Plan:** short warmup → deep early → unlock gate → keep metro runs for shards/ember; kitchen first for farm.
- **Sim:** deep contact + grain seed ~4h; gate at ~12h on 3 ember; shards can reach 4; by 96h pocket+stash full, parts 300+, ember idle at 11.
- **Complaint:** ember has no sink after the gate; Ash Tide sticks on “jerky for seed” even though free camp seed can plant — without hunt/jerky the step never moves; radio only shouts stash-full.
- **Wants next:** ember gear slot, Ash Tide alternate path, lore twist after shards fill.

### Role B · Hunt-path (throughput first)

- **Plan:** short → block for food/parts → open gate → hunt for jerky/ferry.
- **Sim:** ~40 block returns in 96h with **ember still 0** (block ember ~2%); gate never opens → hunt/jerky/ferry locked; parts ~500, stash full.
- **Complaint:** skipping deep makes gate an unlock desert; parts inflate with no dignified sink; chores are fun but do not feed mid-game goals.
- **Wants next:** stable non-deep ember (soft pity), hunt gear for growth feel, reputation turning daily trade into a mid loop.

### Shared verdict

1. **Gate is the first desert:** deep guarantees +1 ember; short/block are 1–2%. Throughput-only play can miss the gate for days — idle games must progress while idle.
2. **Ash Tide step 2 soft-locks:** catch-up after camp contact demands jerky trade; free seed does not advance; harvest only counts at step 3.
3. **Mid inflation:** after stash cap, parts and ember lack voluntary rewarded sinks.
4. **No second clock, no wipe prestige, no paid skips** — offline three-key toy; compliance and feel both require progress on return without punishing absence.

### Idle references (compliance filter)

From incremental design codex / “Design of Waiting” / indie idle economy practice, **keep only liver-friendly rules**:

| Take | Reject |
| --- | --- |
| Short/mid/long loops on stage; no unlock deserts | Wipe prestige / seasonal power resets |
| Active play is a bonus, never a gate; offline capped (deep-sleep fold already exists) | Mandatory daily login, streak punishment |
| Income ≈ sinks + voluntary sinks; soft caps redirect overflow | Hard power walls, gacha, paid odds |
| New layers change the optimisation problem | Parallel islands, social pressure |

## Phase D · feasible upgrade plan (compliance first)

**Player one-liner:** ember finally arms the hunt path; camps remember your face; Ash Tide’s cause flips once — no IAP, no wipe, no second clock.

### Hard rails (every slice)

1. No IAP, ads, gacha, or “speed tickets”; community copy stays play-first (no chip/framework talk).
2. Append-only save (current `ASHBOX_SAVE_VER=11`); older saves remain playable.
3. Hard caps everywhere; saturating math; production stops at cap.
4. Offline/deep-sleep keep current folding; no “online-only” resource.
5. Active rewards (chores, loadout, rep daily) are **optional multipliers**, never sole paths.
6. Lore twist appends radio/journal layers; does not erase finished quests.

### Slice order (each mergeable + host-tested)

| # | Slice | Player feel | Tech | Caps / gates |
| --- | --- | --- | --- | --- |
| **D0** ✓ | Desert + soft-lock first aid | Block slowly shows ember; Tide not stuck on jerky | Soft pity short/block/hunt (12/10/8); Tide step 2: jerky **or** 2 shards; harvest can catch up step 2 | Ember capped; deep curve unchanged |
| **D1** ✓ | Ember sink · hunt gear | Ember spends after gate; hunt/block steadier | `gear_lv` 0..2: ember 3/5 + parts 4/8; lower fail, +jerky, less threat on hurt | Fail floor ≥5% |
| **D2** ✓ | Camp reputation | Daily trade becomes mid loop | `rep_shed` / `rep_ferry` 0..3; +1 per trade; rep≥2 unlocks same-day second trade | Rep capped; still clamped |
| **D3** ✓ | Voluntary parts sink | Full stash has a parts outlet | Build “quiet gear” 18 parts; threat-rise −2 (floor 5) | Night orders stay |
| **G0** ✓ | Late parts sink II | Parts still useful after quiet gear | Build “door bolt” 24 parts (needs quiet gear); threat −1 more (floor 4); fortify −1 (floor 1); save ver 8 | One-shot capped |
| **D4** ✓ | Lore twist | After shards/Tide, radio changes register | `lore_twist` + three twist lines | Narrative only |
| **D5** ✓ | Host sim gate | No regress into deserts | Host asserts: pity, shard path, gear/rep/quiet, block→gate | CI host |

### Explicit non-goals

- Prestige wipe multipliers, fourth camp, real-time combat, Wi-Fi boards, child-directed spend.
- Re-locking deep behind the gate (logic already allows deep; UI only locks hunt — keep it).
- More farm plots or removing daily trade caps (anti-inflation beats bigger numbers).

### Acceptance before merging D

- `./tools/validate.sh --static` green with new asserts.
- Font coverage for new Chinese strings.
- Community notes: player-visible only; art not fake device screenshots.
- **Required on every community update:** official install overwrites all device content; not local preserve-save; reinstall clears progress — never promise save retention (see [BUILD.md](./BUILD.md)).
- Device: pre-gate block ember or pity; post-gate ember buys gear; Tide shard alternate works without jerky.

## Phase E · thicken existing loops

Designer debate charter: **~70% idle ops + ~30% awake wasteland drama**; deepen the core, no new geography. Peaks at return branches / crisis; activity is bonus, never a gate.

| # | Slice | Player feel | Tech | Caps / gates |
| --- | --- | --- | --- | --- |
| **E0** ✓ | Station duty | Awake play without departing: radio / gate / tally / stove | Reuse former `reserved_v5` as `duty_day` / `duty_count`; ≤3/day; hub first item | Small capped rewards; optional |
| **E1** ✓ | Chore / person depth | Richer chore journals; Xiao Mang biases clue tags | Kind-specific logs; pid2 clue-tag bias | Still ≤2 chores/run |
| **E2** ✓ | Copy / help | Away/radio/help mention duty | Stay within existing font glyphs | — |

**Non-goals (E):** fourth camp, second currency, duty-gated production, more farm plots.
