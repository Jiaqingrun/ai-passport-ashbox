<p align="right">
  <a href="CONTENT_FILL.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Ashbox Content Fill — Strict Audit and Plan

Scope: player-facing tables in `main/ashbox_content.c`, copy in `main/ashbox_game.c`, help/report shell in `main/ashbox_ui.c`, with loop constraints from [EXTENSION.md](./EXTENSION.md).  
Audit date: 2026-11-04 (Asia/Shanghai).  
**Implementation status (2026-11-04 Asia/Shanghai): F0–F5 landed** (wiring, tier outcomes/tags, report/crisis/radio/chore/help/character scent); save version unchanged; host tests cover Ember step-0 reachability and hunt/night pools.

---

## 1. Inventory

> **Current inventory (2026-11-06, F0–F5 + Phase G content):** treat code and [AUDIT_CONTENT_G.md](./AUDIT_CONTENT_G.md) as authoritative (37 branches / 13 outcomes / 12 tags / 40 away / idle·lore·twist·watch 20·12·8·≥8; 23 help lines).

| Category | Count | Location | Notes |
| --- | --- | --- | --- |
| Return branches | 30 | `k_branch` | Includes quest finales |
| Outcome pools | 9 | `k_outcome` | Hunt and overnight **reuse** short/deep pools (pre-F) |
| Away lines | 25 | `k_away` | Long tiers bias last ~4 lines |
| Quests | 3 × (5 steps + done) | `k_quest_*` | Tin guest / Ember whisper / Ash tide |
| Encounter tags | 8 | `k_tag` | May replace branch set on success |
| Crisis options | 3 | Labels only | No per-option flavor |
| Loadouts / orders | 3 + 4 | Name + hint | Clear, thin atmosphere |
| People | 3 + traits | A-He / Lao Que / Xiao Mang | No per-person return lines |
| Radio idle / lore / twist | 13 / 8 / 3 | `ashbox_game.c` | Lore indexed by shard tier |
| Duty / chore | 4 + 4 | Label + effect | Same set on 8h overnight |
| Report lines | 3 × 32 chars | Often loot accounting | Weak story peak |
| Help | 8 lines | `paint_help` | Ops tips, little world path |
| Expedition tiers | 5 | Incl. overnight | Numbers exist; **choice copy lacks overnight identity** |

Systems from Phases A–E are in place; gaps are mostly **narrative wiring and tier identity**.

---

## 2. Findings (severity)

| ID | Sev | Issue | Evidence |
| --- | --- | --- | --- |
| C01 | **Critical** | Ember-whisper step 0 soft-lock: branches 4/19 never appear in any outcome or tag pool | Union scan; deep only advances `need_step == 1` |
| C02 | High | Overnight reuses deep outcomes 5/6 | `pick_outcome_id` |
| C03 | High | Hunt reuses short branch pool | `HUNT → 1/2` |
| C04 | High | Dead branches 4/19/20/21/27/28; 20/21 unreachable | Static coverage |
| C05 | Medium | Long-tier away lines rotate ~4 strings | Radio path `base = n-4` |
| C06 | Medium | Report lines are loot ledgers | `resolve_return` |
| C07 | Medium | Crisis has button labels only | `k_crisis_label` |
| C08 | Medium | Only 3 twist radio lines after lore climax | `k_radio_twist` |
| C09 | Medium | Chores ignore tier/person | `tick_chore` |
| C10 | Medium | Help omits quest/gate/gear teaching | `paint_help` |
| C11 | Low | Journal 8×36 truncates | `ASHBOX_JOURNAL_LEN` |
| C12 | Low | Lore has little same-tier variants | Index by shards |
| C13 | Info | Crop naming consistent; no chip/framework leaks in sampled copy | — |
| C14 | Info | Phase D/E fairness exists; copy under-teaches it | EXTENSION D0–E2 |

**Verdict:** Mid/late systems playable, but **quest wiring holes**, **hunt/overnight narrative reuse**, and **weak report/crisis peaks** dominate. Fill content by repairing wiring and tier identity — not by adding resource loops.

---

## 3. Non-negotiables

1. Keep depart → wall/sleep → return choice → crisis/quest.  
2. ~70% idle economy + ~30% awake drama; peaks at return and watch.  
3. Caps everywhere; activity is bonus, not gate.  
4. No chip/framework in public copy; new glyphs → font subset + rebuild.  
5. Prefer wiring dead branches over orphan flavor text.  
6. Three-button UI: ≤3 choices; single-line radio/away readable on 240px.

---

## 4. Phase F plan

**Player line:** Every road home feels different; the box whisper can finally be heard; hunt and overnight smell like themselves; a watch night feels like fear.

### F0 — Wiring first

- Put branch 4 or 19 into tag “ember heat” and/or deep success / post-tutorial short success.  
- Optional catch-up: first deep success at step 0 advances to 1 with a journal line.  
- Resurrect 20/21 via a “station quarrel” tag or fail pool.  
- Keep quest-2 steps 27/28 on trade/harvest; clue UI should say so.  
- Host tests: reachable path for quest1 0→1; dead-branch scanner in CI.

### F1 — Tier identity for outcomes/tags

- Short: keep tutorial/social; add a few stall-scent branches.  
- Block: dedicated success/fail pools (split + new).  
- Deep: keep shard/camp/ember; ensure step 0/1 visible.  
- Hunt: **new** outcomes/tags (jerky / reverse-tide); stop defaulting to short social trio.  
- Night: **new** outcomes (long road, gate wind); fail leans rest/suppress/watch.  
- Extend `k_outcome` (~+4) and `pick_outcome_id`; +2–3 tags. Content-only; save ver optional.

### F2 — Report and crisis peaks

- Per tier×result×person environment line + loot line (still ≤32 bytes).  
- Crisis option subtitles and distinct journal tones.  
- Branch logs stay scenic; effects stay numeric.

### F3 — Away and radio bulk

- Away 25→≥40 segmented by tier; long tiers must not only use the last four.  
- Idle ≥20; lore variants per shard tier; twist 3→8.  
- Keep hard hooks (starve/full/ready) above atmosphere.

### F4 — Chore skins and help

- Same rewards; tier-skinned labels/hints.  
- Help = 4 ops + 4 progress (gate, hunt unlock, ash-tide seed/shard, overnight sleep).

### F5 — Character scent (no new systems)

- Optional second return line by person; one Lao-Que repair tag.  
- No fourth person; no new affection meter.

### Out of scope

New currency, fourth camp, more farm plots, mandatory duties, prestige wipe, paid speedups, auto-skipping Ember whisper without copy, uncontrolled font bloat.

---

## 5. Slice order

| Slice | Player feel | Gate |
| --- | --- | --- |
| **F0** | Ember whisper advances | Host tests + dead-branch scan |
| **F1** | Hunt/night choices change | Pool coverage asserts |
| **F2** | Return/watch feel dramatic | Device: three lines readable |
| **F3** | Long idle less repetitive | Glyph gate; hard hooks first |
| **F4** | Less guessing | Help stays player language |
| **F5** | Three people distinct | No new save fields |

Recommended: F0→F1→F2; F3–F5 can parallelize copy work.

---

## 6. Relation to prior phases

A–E = systems/numbers baseline. **F = content identity and wiring.** **G (content) = narrative/UX continuity** (hub clues, step milestones, watch radio)—not a fourth questline; see [AUDIT_CONTENT_G.md](./AUDIT_CONTENT_G.md).
