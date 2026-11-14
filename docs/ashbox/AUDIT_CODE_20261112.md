[简体中文](./AUDIT_CODE_20261112.zh_CN.md) · **English**

# Code audit — 2026-11-12 (HEAD `0dfba4c`)

Scope: current implementation of the Ember/Ashbox firmware, source only plus the archived
firmware image. Method: repository gates first, then eleven read-only source slices
(`ashbox_game.c` ×3, `ashbox_ui.c` ×3, save/meta/settings, content/loot, runtime/sleep/audio,
backup/demo, UI pages), each finding cited by `file:line`. Prior findings from
[`AUDIT_GAMEPLAY_20261108.md`](./AUDIT_GAMEPLAY_20261108.md) were re-checked at HEAD instead of
being repeated.

Environment: macOS host, no activated ESP-IDF (so no rebuild), firmware image reused (see 1.1).
Marking: **[V]** = evidence re-read and confirmed by the audit lead; **[S]** = slice report,
evidence site cited but not re-read line by line.

## 1. Mandatory: flash / firmware size — PASS

| Check | Result |
| --- | --- |
| Image identity | Reused `build/firmware/8a17a64a…/` (app.bin sha256 `8a17a64a…`, full image `669a48…`/`8a17a64a…`); embedded `version = 0dfba4c` = HEAD, `idf_ver = v5.5.3`, `build_date Nov 9 2026 13:34`. Not rebuilt: no activated ESP-IDF, and no source file is newer than the image (latest `main/*.c` = Nov 8 23:16, HEAD = docs-only). |
| Budget | app.bin 2,920,608 B of 8,323,072 B factory = **35.1 % used, 64.9 % free** |
| Top consumers | sprites ASR1 1,316,600 B (1.256 MiB); PCM 631,884 B (hearth 384,000 + radio ×5 247,884); glyph bitmaps 106,480 B; `.flash.text` 642,352 B; `.flash.rodata` 2,195,316 B; `.dram0.bss` 147,792 B |
| Plan alignment | Consistent with [`FLASH_SIZE_PLAN.md`](./FLASH_SIZE_PLAN.md); no new rodata added since the 2026-11-08 snapshot |
| Bans restated | No cut scenes/frames, no smaller stage, no lossy art, no shortened radio copy |
| Observation | Two archives exist for the same commit (`669a48…`, `8a17a64a…`) with equal size but different sha256 — the build is not bit-reproducible (embedded timestamp). Archive identity should be recorded per build, not inferred from size. |

## 2. Mandatory: whole-codebase optimization — PASS (with findings below)

Host gates (`./tools/validate.sh --static`): **PASS** — repository checks (433 files), font glyphs,
dead branches (61/61 reachable), copy layout, glossary, sprites (36 PNG @ 240×176, RLE save
1,724,680 B), all three Ashbox host-test binaries, key-guard, battery, button, LVGL init, audio,
low-power, BLE/Wi-Fi, layout and Python contract tests.

Coverage gap: `main/demo_*.c` (9 files, ~1,100 lines) is **not compiled** by
`main/CMakeLists.txt` yet is cited as shipped behaviour by `docs/README.md` and
`tests/test_deep_sleep_contract.py:56` — dead code presented as a capability **[S]**.

## 3. Mandatory: player-facing copy layout — PASS with copy-accuracy defect

`check_ashbox_copy_layout.py` PASS (help 94 rows, 3 visible, row 68/70, label width 196),
`check_ashbox_glossary.py` PASS, help uses `LV_LABEL_LONG_MODE_WRAP`. No wrap/clip/truncation
regression found in the audited surfaces. One copy **content** defect (NV10) and one bounded-buffer
overflow (NV19) are listed below; the layout gate cannot catch either.

## 4. New findings

### 4.1 S1 — memory safety, lock discipline, irreversible confirm

| ID | Finding | Evidence |
| --- | --- | --- |
| NV1 **[V]** | Trade page writes 22 rows into 16-row stack arrays. `k_camp_trades` = 13 + `k_ferry_trades` = 8 + tide 1 = 22, but `titles[ASH_LIST_MAX][40]`, `subs[ASH_LIST_MAX][56]`, `enough_a[ASH_LIST_MAX]` with `ASH_LIST_MAX 16`; the fill loop is `for (i = 0; i < n; i++)`. Late game with both camps open smashes the stack. | `ashbox_ui_trade.c:85-88`, `:29-32`; `ashbox_ui_page.h:19` |
| NV2 **[V]** | LVGL is written from the app task without `bsp_lvgl_lock()`. `ashbox_ui_tick()` is called from `main.c:270`, and inside it `maybe_show_meta_toasts()` runs **before** the lock at `:3452`, calling `lv_label_set_text` on modal lines. The module's own contract (top comment) is `runtime → LVGL`, and `ashbox_ui_on_key` does lock (`:3169`) — the tick path does not. | `ashbox_ui.c:3406`, `:2914`, `:2936`, `:3452`; `main.c:270` |
| NV3 **[V]** | The super-core confirm can never confirm, and its armed flag survives navigation. Arming sets `s_core_confirm = true; s_block_modal = true` and shows "long-press to cancel, press OK again to use", but the next OK hits the block-modal branch, which treats an armed `s_core_confirm` as **cancel**. Long-press OK is handled before that branch, calls `go_back()`, and `enter_page()` only hides the modal — it does not clear `s_core_confirm`. `activate_build` is then reachable with the flag still armed, so one unconfirmed OK consumes the career one-shot. The exile confirm (`:2809-2829`, `:3298-3300`) uses the same inverted protocol. | `ashbox_ui.c:2642-2663`, `:3289-3305`, `:3230-3262`, `:2186-2191` |

### 4.2 S2 — economy, state machines, lifecycle

| ID | Finding | Evidence |
| --- | --- | --- |
| NV4 **[V]** | Top-band breach penalty fires after **every** crisis option, including a paid FORTIFY that already logged "the gate held": at band 3 the code re-clamps threat to 79, burns a part, adds 10 fatigue and logs "the gate was forced open, the bolt is broken". `door_bolt` is never cleared, so the bolt keeps its −1 rise / −1 fortify benefit. | `ashbox_game.c:3353`, `:3380-3385`, `:3416-3425` |
| NV5 **[V]** | Trades charge the input even when the output pool is full: `ash_game_trade_can_pay` tests only the paying side, and `ash_game_give_water/parts/…` results are ignored (`(void)`), so food/ember/iron evaporate at the water/parts/item caps. Craft in the same file refuses at cap instead. | `ashbox_game.c:4034-4099`, `:604-623`, `:4275` |
| NV6 **[V]** | The water day budget rolls **twice** per day: `tick_water_economy` resets at UTC midnight, `ash_game_note_wall_time` resets again at UTC+8 midnight, so a well can pay up to two day caps per local day. Separately, `seed_water_made_today` credits hours elapsed since UTC midnight *before* the source existed, so a pit/well finished after 00:00 UTC yields nothing until the next UTC midnight while logging "seepage started". | `ashbox_game.c:918`, `:2825-2830`, `:830-840`, `:940` |
| NV7 **[V]** | `hide_penalty_h` and `crisis.reserved` decrement by `dt_s / 3600` on a tick that carries ~1 s while awake, so a 4 h hide penalty never expires during a session (only across sleeps) and the home status keeps showing "hiding ~4 h". | `ashbox_game.c:2610-2617`, `:2626-2630`; `ashbox_ui.c:1071` |
| NV8 **[V]** | 1 s awake settles still floor food/parts production and water drain to zero (`* 4 / 3600`, `pop * consume_s / 7200`), so nothing accrues and nobody drinks while the player watches. Threat got a carry accumulator for exactly this reason; these three did not. | `ashbox_game.c:2533-2546`, `:890`, `:2604-2609` |
| NV9 **[V]** | A water-only empty pocket is never refilled. `apply_economy` calls `ash_game_try_pocket_refill` only when `food == 0 && stash_food > 0`, although the helper also refills water; the WEAK flip at `:2578` then hits the whole crew and departure is refused while the stash holds water. | `ashbox_game.c:2562`, `:540-552`, `:1129-1131`, `:2578` |
| NV10 **[V]** | Help states the pre-rebalance numbers: "grain ~1 day, base 30 food; sweet potato ~3 days, base 240", while the constants are grain base 45, root base 150, root growth 172,800 s (2 days). The P1-3 rebalance changed the code, not the copy. | `ashbox_help_copy.c:61`; `ashbox_game.h:34-35`; `ashbox_game.c:48-49` |
| NV11 **[S]** | Departure picker shows the fixed slot personality, not the crew member's real work trait; loot uses the real trait, so the preview misleads. | `ashbox_content.c:187`, `ashbox_game.c:298`, `ashbox_ui_pick.c:76`, `ashbox_game_loot.c:279` |
| NV12 **[S]** | Backup lifecycle holes: no timeout in `ST_EXPORT_SEND` (busy state can wedge until reconnect), host PULL snapshots NVS without flushing the in-RAM save, and import commit deinits/erases NVS while the 15 s autosave keeps writing. | `ashbox_embackup_proto.c:467`, `:301-307`; `ashbox_embackup.c:284`, `:162-164`; `ashbox_ui.c:2181` |
| NV13 **[S]** | Save load never compares slot recency: a decodable but stale primary wins over a newer backup. Legacy fix-ups also re-run on current-version loads, re-granting `water = 40` and resetting a ≤600 s injury back to 24 h on every reboot. | `ashbox_save.c:1390`, `:1192-1200`; `ashbox_game.c:201-203`, `:223` |
| NV14 **[S]** | A pending report sets `s_list_sel = -1` regardless of the current page; on the equip page `s_list_sel / 2` maps −1 to person 0, so a stray OK toggles the wrong survivor's backpack. | `ashbox_ui.c:3420`, `:2391` |
| NV15 **[S]** | Clear/end toasts call `set_modal_visible(true)` without setting `s_block_modal`, so the underlying page keeps receiving input and the toast is repainted away. | `ashbox_ui.c:2913`, `:2935`, `:3289` |
| NV16 **[V]** | If the boot LVGL lock times out, `ashbox_ui_start` returns before `build_screen()` while `s_page` is already `UI_HOME`; the next tick paints into NULL widgets (`lv_obj_clear_flag(s_water, …)`). No retry and no `s_ui_built` guard exist. | `ashbox_ui.c:2048`, `:2079`, `:3452-3458`, `:120` |
| NV17 **[S]** | Layout caps: person-picker rows at `12 + i * 52` put row 5 at y=272 under a 228 px clip layer (no scroll on that step); the farm page loops `plot < ASHBOX_FARM_PLOTS` (2) although the super core unlocks 4. | `ashbox_ui_pick.c:96`; `ashbox_ui_farm.c:32`; `ashbox_game.c:4748` |
| NV18 **[S]** | The threat badge is right-aligned over the iron cell, so the iron cell ("iron %u") is covered exactly at threat ≥ 80. | `ashbox_ui.c:1907`, `:1899-1900`, `:1046` |
| NV19 **[V]** | Four-effect branch text needs > 40 B but is rendered into `char effect[40]`, cutting the threat word mid-word (branch 20). | `ashbox_content.c:238`; `ashbox_ui.c:1697` |
| NV20 **[S]** | Crisis page pre-selects row 0 and a single OK commits it; the report page deliberately starts at −1 because of the same reason. | `ashbox_ui.c:2227`, `:2893` |

### 4.3 S3 — dead state, feedback, test gaps

`report_waste_flags` is write-only (no reader in `main/` or `tests/`); `clear_count` is incremented
only on achievement unlock and never read; meta unlock returns unmasked bits (`ashbox_meta.c:96`), so
out-of-mask bits re-toast forever; `wanderer_next_s` writes are dead under `ASHBOX_PACE_V13` and
`ash_game_wanderer_next_wake_s` has no caller; `note_wipeout_reject` keeps an empty `if` block;
branch trust is re-targeted by person-slot shifts because `exp.person_id` is not remapped on
removal; three medicine log lines exceed `ASHBOX_JOURNAL_LEN` and leave a dangling UTF-8 lead byte;
the second crowbar only re-seeds the haul; the mascot jump drifts upward on re-trigger; 
`ui_pixel_math.c` is test-only dead code; the help long-press section jump is unreachable
(`ashbox_ui.c:3194-3213` consumes the event before `:3217`); `ashbox_save.c:1193` memcpy's into the
caller's struct before the CRC test; the rescue scan whitelists only v10+ although decode migrates
from v3 **[S]**.

## 5. Prior findings at HEAD

| Status | IDs |
| --- | --- |
| Fixed | C01, C02, C03, C04, C06, C12, C19, C20, C22, C23, C25, C28, C29, C31, C32, C33, C34 |
| Partial | C07 (cost charged at ack, not frozen at pick), C08 (death roll still lacks a person term), C09 (ember now converts at the camps; lifetime sink unchanged), C11 (outcomes 7/8 state-routed; loadout still always NONE), C14 (hide buffed, penalty never expires → NV7), C15 (bands escalate numerically; text/options still identical), C17 (day now rolls at local midnight; beat price still hidden), C18 (arcs re-arm; `clear_count` meaningless), C24 (sub-pages still do not refresh while idle), C26 (exit exists; auto-sleep still blocked → see `power_blocks_autosleep`), C27 (trade still shows no stock), C30 (help sections exist but the jump is unreachable) |
| Open | C05 (gate ×1.2 still a no-op at ember 1), C13 (late sinks still do not repay) |
| Disputed | C16 — the 2026-11-08 numbers (30/240) were rebalanced to 45/150, so the cited lines no longer hold; the remaining asymmetry (root ≈75 vs grain 45 food/day at the same seed price) is a balance call, not a defect |

## 6. Unverified / needs device

- All S1/S2 items are source-verified; none is device-reproduced. NV3, NV6, NV9, NV15, NV18 and the
  power rule below need a screen walk.
- `power_blocks_autosleep()` returns true for `UI_REPORT`/`UI_CRISIS` while its own comment says the
  opposite, so leaving the crisis page open blocks the 300/600 s deep-sleep request (backlight only).
  One of the two is wrong; confirm on device before fixing **[V]** (`ashbox_ui.c:851-859`, `:886-889`).
- Suggested host tests that do not exist yet: water day-cap across a local midnight; 1 s-tick
  production/consumption table; trade refused at output cap; band-3 FORTIFY journal; confirm-modal
  round trip for super core and exile.

## 7. Fix round (2026-11-12, same day)

Applied against this audit in the source only; no save ABI change (`ASHBOX_SAVE_VER` stays 13,
`sizeof(ash_game_t) == 776`, pads untouched).

| ID | Change |
| --- | --- |
| NV1 | Trade page materialises only the visible window (`TRADE_VIEW` rows), so 22 rows no longer index the 16-slot arrays. |
| NV2 | `maybe_show_meta_toasts()` now runs inside the tick's `bsp_lvgl_lock()` block, after the page paint, so no LVGL call happens from the app task unlocked. |
| NV3 | The second OK now executes the armed confirm and spends the core; long-press escape and `enter_page()` clear `s_core_confirm`/`s_exile_confirm`, so nothing stays armed across navigation. |
| NV4 | Corrected finding: band 3 is a *forced fortify failure* by design (P1-4), so the second charge is intended and the existing test stands. The real defect was the contradiction: band 3 now logs "the bolt is dead, the gate is forced open" instead of "the gate held", and `door_bolt` is actually cleared, so the line is true. |
| NV5 | `ash_game_trade_can_pay` also requires destination room (water, parts, food, iron, ember, seed), so a full pool no longer silently destroys the input. |
| NV6 | One water day boundary: production and seeding use local (UTC+8) midnight, and `ash_game_note_wall_time` no longer clears the budget a second time. |
| NV7 | Hour-based timers (`crisis.reserved`, `hide_penalty_h`) burn down on real boundary crossings, so they expire during awake play. |
| NV9 | Pocket auto-refill is gated on either pocket being empty, not food alone. |
| NV10 | Help crop line corrected to 45 / 150 and two days. |
| NV15 | Clear/end toasts set `s_block_modal`, own the screen until dismissed, and are no longer wiped by the same tick's page repaint. |
| NV16 | `s_ui_built` gate: input is dropped and the tick retries `build_screen()` when the boot LVGL lock timed out. |
| NV19 | Report effect buffer 40 -> 56 B. |

Deferred, with the reason rather than a silent drop: **NV8** (a per-settle remainder is needed for
food/parts/water use; the only free space is `pad_v13`, so it belongs to a v13.1 change that updates
the frozen fixture and the pad asserts), **NV11-NV14, NV17, NV18, NV20** (behaviour and layout work
that needs a device pass), **C05, C13**.

Tests (`tests/test_ashbox_game.c`): new `test_audit_20261112_regressions` covers trade room, the
band-3 fortify journal/bolt, hour timers under 1 s settles, water-only refill and the single water
reset; the water fixtures moved to the local-day boundary and the breach-window expectation now
counts real hours.

Gates after the fix: `./tools/validate.sh --static` PASS (every host suite, copy layout, glossary,
dead branches, sprites) and `./tools/validate.sh --firmware` PASS — app.bin 2,921,488 B (+880 B),
merged image 2,987,024 B, archive `build/firmware/b447b700…/`, version `0dfba4c-dirty`.

## 8. Delivery header (post-fix)

```text
Build: PASS (./tools/validate.sh --firmware; app 2,921,488 / 8,323,072 B = 35.1 %, 65 % free)
Host tests: PASS
Device tests: NOT RUN
Unverified: device walk for NV3/NV6/NV9/NV15/NV18, auto-sleep policy, toast dismissal, trade page with both camps open
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS (12 fixed, 7 deferred with reasons)
Audit mandatory: Copy layout = PASS (host gate), copy accuracy = PASS after NV10
```
