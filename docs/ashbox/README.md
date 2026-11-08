[简体中文](./README.zh_CN.md) · **English**

# Ashbox (Ember)

Offline post-apocalypse idle game for FoloToy AI Passport.

- Branch: `feature/ashbox-ember`
- UI is a new application shell (not the hardware-test demo menu)
- Rules: `main/ashbox_game.c` + content tables in `main/ashbox_content.c` (host-tested)
- Save: NVS namespace `ashbox` key `save`, magic `ASH2` / `ASHBOX_SAVE_VER=13` blob 776 (older versions migrate; see [EXTENSION.md](./EXTENSION.md)); meta progress key `meta` / `ASHM` — [META_PROGRESS.md](./META_PROGRESS.md); project rules in root [`PROJECT.md`](../../PROJECT.md)
- USB save: Settings items Backup / Import implement `!EMBACKUP` for the [AI-Passport_BS save manager](https://github.com/Jiaqingrun/AI-Passport_BS) (current ZIP: `save-manager-v1.0.1`)
- Next community-update copy checklist (Save Manager URL, `RESTORED 3`, NVS namespace): [COMMUNITY_PUBLISH.md](./COMMUNITY_PUBLISH.md)
- Flash: default `./tools/flash_ashbox_preserve_save.sh <port>` (app at `0x10000` only); merged `0x0` wipes NVS — blank device or explicit wipe only (see [BUILD.md](./BUILD.md)). **Community install/update must warn that progress is wiped; it is not preserve-save.**
- Content-fill audit and Phase F / G plan: [CONTENT_FILL.md](./CONTENT_FILL.md)
- Home layout lock: [LAYOUT_LOCK.md](./LAYOUT_LOCK.md) · Flash / size plan: [FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md)
- **Standing audit gate** (every audit covers Flash size + whole-code optimization + player copy
  layout; post-change strict audit with fix-forward): [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md)
  · project rule in [`PROJECT.md`](../../PROJECT.md) · host gate `tools/check_ashbox_copy_layout.py`
- Retired audit snapshots, one row each: [AUDIT_HISTORY.md](./AUDIT_HISTORY.md)
- **Content findings and implementation spec (current)**: [AUDIT_GAMEPLAY_20261108.md](./AUDIT_GAMEPLAY_20261108.md)
  · [OPTIMIZE_PLAN_20261108.md](./OPTIMIZE_PLAN_20261108.md)

- Hub: station duty (awake play), farm (two fields · grain/root), stash + furniture, Iron Shed / Ash River trade; hunt needs gate open (unlocks the station door to the hunt path)
- Player-facing planning copy: route sublines show loot ranges / empty-handed probability / rares / ember catch-up; person rows show biases; build footer + scrollable Help list the rates and sources
- Scene sprites: 12×3 frames, 240×176 apocalyptic pixel stage; HUD (resources / radio / now) sits in a band below the art; thin top/bottom safe insets
- Font subset: regenerate `assets/fonts/ashbox_font_16.c` from `main/ashbox_font_symbols.txt` after new Chinese UI/copy

## Depth loop (awake-only decisions)

1. **Depart** — pick tier (short / block / deep / hunt·needs gate / overnight), person (trust / fatigue / traits), optional crowbar or pistol+ammo. Food/parts “loadout” presets are retired.
2. **Deep sleep or wait** — expedition, farm, and well production use full wall-clock; food consume, parts produce, threat rise, and water **consume** use `sleep_factor=0.5` while deep-sleeping (water production does not). Idle power: ~60 s dims and pauses radio/anim; ~180 s blanks at the station only (**no auto-blank during an expedition**, so the countdown stays visible); ~5 min on expedition or ~10 min at the station enters deep sleep (never during an open report/crisis; wake catch-up clamps wall-clock gaps). Home OK long-press still sleeps immediately.
3. **Return branch** — settle writes loot plus `outcome_id` / branch choices; unresolved branches persist across sleep; pick only while awake.
4. **Station crisis** — threat ≥ 80 with no expedition and no open report forces talk / fortify / hide. Anyone present can hide or talk; weak people still produce food at half rate so a zero-food crisis cannot soft-lock.
5. **Quests** — Tin Visitor, Ember Whisper, and Ash Tide Beyond the Gate (five steps each); finish branches are in outcome/tag pools; Ash Tide advances via gate / Iron Shed / seed trade / harvest.
6. **Radio journal** — ring buffer of short logs; radio prefers away lines while on expedition, otherwise the latest journal line.
7. **Night standing order** — set before deep sleep (ration / watch / rush); only affects the deep-sleep economy segment.
8. **Return encounter tags** — successful returns pick a tag that biases the awake branch pool.

Building upgrades keep intentional trade-offs: dorm L3 rests faster without extra food use, workshop raises threat pace, kitchen lowers consume, gate boosts loot and cheapens fortify. Well upgrade pauses well water only (station stays usable).

Design notes for the product may live outside this firmware checkout; this folder only tracks firmware-facing facts.
