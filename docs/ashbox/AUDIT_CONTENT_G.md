**English** · [简体中文](./AUDIT_CONTENT_G.zh_CN.md)

# Ember content audit · Phase G (narrative / UX)

Audit date: 2026-11-06 (Asia/Shanghai).  
Scope: player-facing copy and three-button information architecture; **no** new camps, currencies, or second clocks.  
Baseline: F0–F5 landed; see [CONTENT_FILL.md](./CONTENT_FILL.md) and [AUDIT_10R.md](./AUDIT_10R.md).

## Inventory (current code)

| Item | Count |
| --- | ---: |
| Branches | 37 (35 reachable; system-only 27/28) |
| Outcomes / tags / away | 13 / 12 / 40 |
| Radio idle / lore / twist / watch | 20 / 12 / 8 / ≥8 |
| Help lines | 23 |
| Quest lines | 3 × 5 steps |

## Findings N01–N11

| ID | Sev | Issue | Phase G fix |
| --- | --- | --- | --- |
| N01 | High | Hub hid step-0 names for q>0 | **G1** named hub row |
| N02 | High | Deep success double-advanced quest1 0→2 | **G2** split advances |
| N03 | High | Thin post-quest narrative | **G4** watch pool |
| N04 | Medium | Report L1 became ledger after pick | **G5** scene then ledger |
| N05 | Medium | Tide 27/28 only via systems | **G6** explicit teaching |
| N06 | Medium | Help missing rep / shard path | **G7** |
| N07 | Medium | Migration catch-up silent | **G3** journal note |
| N08 | Low | Chore hint/log not tiered | **G8** |
| N09 | Low | Crisis disabled copy drift | **G9** |
| N10 | Info | Full on-device F loop unverified | acceptance |
| N11 | Info | CONTENT_FILL stale snapshot | refreshed |

## Phase G slices

| Slice | Status |
| --- | --- |
| G1 hub clues | landed |
| G2 ember deep step | landed + host test |
| G3 milestone journals | landed |
| G4 late watch pool | landed + host test |
| G5–G9 copy | landed; glyphs PASS |

Gate: `./tools/validate.sh --static` PASS. Device smoke (G1/G2/G4): not run — no USB device.

**Out of scope:** fourth quest line, new economy, rate changes, orphan branches.
