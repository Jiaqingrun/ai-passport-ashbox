[简体中文](./META_PROGRESS.zh_CN.md) · **English**

# Meta progress · clear · restart · super core

Authoritative design for survivors hub, achievements, clear-and-continue, forced wipeout end, duty jobs, and the super core. Conflict order: `AGENTS.md` / `PROJECT.md` → [LAYOUT_LOCK.md](./LAYOUT_LOCK.md) / save ABI → this file → [README.md](./README.md).

## Save layers

| Layer | NVS | Magic | Notes |
| --- | --- | --- | --- |
| Run | `ashbox` / `save` | `ASH2` | `ASHBOX_SAVE_VER=13` (blob 776) |
| Meta | `ashbox` / `meta` | `ASHM` | `ASHBOX_META_VER=1` |

Community full-image install still wipes NVS (run + meta). Store copy must say so; prefer EmBackup before reinstall. EmBackup should include `meta` in the same milestone when possible.

## Clear event (unique)

**Clear** = quests 0/1/2 all `quest_done` and `lore_twist == 1` (same radio "settled" idea).  
Sets `run_cleared` once; play may continue. One settlement toast may unlock several achievements.

## Population and beds

- `ASHBOX_PERSON_MAX = 6`
- Dorm beds: L1=2, L2=3, L3=4, L4=5, L5=6 (L5 needs super-core build cap)
- Pop cap still `min(beds, kitchen/workshop labour tier)`

## Duty (per person, at base)

| Job | Effect |
| --- | --- |
| Rest (default) | Fatigue recovery |
| Farm | Speeds active crop timers |
| Workshop | Raises parts production weight |

Away = locked “out”. Weak may work at half efficiency.

## Fatigue

Rest: base −12/h, +2 per dorm level, hard cap −24/h.  
Farm/workshop: half recovery or none. Do not stack an unconditional −24/h with full work recovery.

## Wipeout / forced end (MVP)

1. `pop==0`: station food/water/threat consume = 0; water well + farms still tick; parts production = 0.
2. Wanderers still schedule; join costs unchanged.
3. Forced fail: `pop==0` and two consecutive cannot-join / dismiss-while-broke events (`wipeout_reject_count >= 2`).

## Achievements (meta bitmask)

| Bit | Id | Unlock | New-run grant |
| --- | --- | --- | --- |
| 0 | Lone | Clear with `pop==1` | Pistol ×1 |
| 1 | Together | Clear with `pop` 2 or 3 | Backpack ×2 |
| 2 | Crowd | Clear with `pop>=5` | Pistol ×2 + ammo ×10 |
| 3 | Pacifist | Never equipped/carried pistol or ammo until clear | Stash food capacity +50 |
| 4 | No one left | Clear with `pop==0`, or clear then forced end still empty and never rejoined | Super core ×1 once |

Unlock mid-run → toast “restart to apply”; **current run does not receive grants**.  
Settings → Restart: wipe `save`, keep `meta`, apply permanent grants; super core inventory only if unlocked and never consumed.

## Super core (run item)

Use once per career (`meta.super_core_consumed`):

- Farm plots 2 → 4
- Build level cap 3 → 5 (`build_cap_bonus=2`)
- Successful expedition food/parts/iron ×1.5 (`loot_bonus_perm_pct=50`); empty-handed rate unchanged

## Hub / settings

- Hub **Survivors**: list name + state + fatigue; detail trust / trait / injury / job / gear
- Hub **Achievements**: locked grey / unlocked gold; condition + grant in plain language
- Settings **Restart** with cancel-default confirm (same risk class as import)

## Verification

Host: `--ashbox`. Device: clear toast, restart grants, pop=0 consume, duty fatigue, super-core use.
