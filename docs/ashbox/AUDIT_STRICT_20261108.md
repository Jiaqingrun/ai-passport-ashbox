[简体中文](./AUDIT_STRICT_20261108.zh_CN.md) · **English**

# Ashbox — strict audit, round 2 (2026-11-08)

| Field | Value |
| --- | --- |
| Date | 2026-11-08 (Asia/Shanghai) |
| Authority | [`PROJECT.md`](../../PROJECT.md) audit rails · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| Method | Rebuilt with `./tools/validate.sh --firmware`, re-ran `--ashbox`; `ashbox_flash_size_report.py`; evidence from `_Static_assert` / `nm -S` / map; on-device preserve-save flash + 20 s boot log |
| Scope | Flash size · whole-code optimization · player copy layout · save ABI v13 · session documentation and ops fixes |
| Device | Preserve-save flashed `445875b7…`, boot clean; **screen appearance still needs the user's confirmation** |

```text
Audit rail: Flash size = PASS
Audit rail: Code optimization = PASS
Audit rail: Copy layout = PASS
```

```text
Build: PASS (this round's archive e02c57b5…; the device runs 445875b7…, same source and size)
Host tests: PASS (--ashbox: repo + glyphs + dead branches + copy layout + glossary + sprites + game/embackup/save, including the new frozen v12 fixture)
Device tests: PASS (preserve-save flash + 20 s boot log, ELF hash matched, no panic)
Unverified: help-page line wrapping by eye · NVS erase rescue path · high-load USB import
```

---

## 1. Flash / firmware size

### Image identity

| Field | Value |
| --- | --- |
| Rebuilt this round | Yes — `--firmware`, archive `e02c57b5…` |
| Device image | `445875b7…` (same source `feeac35`; boot log `ELF sha256 d76466af…`) |
| `app.bin` | **2,924,960** B (2.789 MiB) |
| ELF / appdesc | IDF **v5.5.3**; version `79a3436-dirty` |
| Verification | `python3 tools/archive_firmware.py verify …` → **PASS** |

Both builds produced an identical `app.bin` size; the `full.bin` hash differs only because the embedded git version string changed (`1` → `79a3436-dirty`). That is not code drift.

### Budget

| Item | Value |
| --- | --- |
| factory | `0x7F0000` = 8,323,072 B |
| Used | **35.1%** |
| Free | **5,398,112 B (64.9%)** |

### Main consumers (measured)

| Bucket | Bytes | Note |
| --- | --- | --- |
| Sprite ASR1 | **1,316,600** | 36 RLE frames, matches the host pixel gate |
| Radio PCM ×5 | **247,884** | |
| Hearth PCM | **384,000** | |
| `glyph_bitmap` total | **106,480** | Montserrat symbols **0** |
| `.flash.text` | **647,912** | |
| `.flash.rodata` | **2,193,984** | |
| `.dram0.bss` | **147,784** | `s_fb` 84,480 + LVGL builtin pool `work_mem_int` 49,152 |

### Drift vs the previous round

Compared with [AUDIT_FULL_20261108](./AUDIT_FULL_20261108.md): `app` **+3,280 B** (the `feeac35` injury survival window and wanderer confirmation), `.flash.text` +2,080, `.flash.rodata` +1,176; **sprites / PCM / glyph buckets show zero drift**. Against the [FLASH_SIZE_PLAN](./FLASH_SIZE_PLAN.md) P2 baseline the image is still about +430.7 KB, all of it from code and copy.

### Allowed / forbidden

**Allowed:** trim the printf family (about **28.3 KiB**: `_vfprintf_r` 9,328 / `_svfprintf_r` 9,216 / `__ssvfiscanf_r` 6,806 / `_dtoa_r` 3,620); split the large UI and rules units; add the hearth PCM to the size baseline table.  
**Forbidden (unless the user overrides on the spot):** fewer scenes or frames, shrinking 240×176, lossy art, shorter radio or hearth PCM, cutting gameplay to save space.

---

## 2. Whole-code optimization

### Healthy items (evidence)

- **Rules/UI boundary**: the `--ashbox` `rg lv_` check passes — `ashbox_game.c`, `ashbox_game_loot.c`, `ashbox_content.c`, `ashbox_save.c`, `ashbox_meta.c` contain no LVGL symbols.
- **Lock order**: the `ashbox_ui.c` module header states `ashbox_runtime_lock → bsp_lvgl_lock`; the cross-task entry `ashbox_ui_on_key` takes runtime first, then LVGL; `ashbox_ui_flush_persistent` (called from the USB task) makes no `lv_` calls, so NVS is never written under LVGL (C-CB1).
- **Save ABI**: `ASHBOX_SAVE_VER=13`, blob **776**; `main/ashbox_save.c` holds **19** `_Static_assert`s (per-generation `crc` offsets and sizes for v4…v12, the 6-byte `pad_v13`, `crc` last, 8-byte alignment).
- **No ABI drift this round**: `feeac35` only added `#define ASHBOX_INJURE_DEAD_S`; the struct is untouched and `sizeof==776` still holds.
- **Dead branches**: 37 branches / 35 reachable, `system_only=[27, 28]`.
- **Host-tested units**: game, game_loot, content, meta, save, embackup_proto + the NVS host stub.

### Findings

| ID | Priority | Evidence | Suggestion | UX/save impact | Next step |
| --- | --- | --- | --- | --- | --- |
| F-UI1 | P1 | `ashbox_ui.c` **4,628** lines; `style_init` 8,864 B, `build_screen` 3,620 B, `resolve_return` 3,522 B, `paint_build` 3,220 B, `paint_home` 2,816 B | Split trade / gear / stash / help pages | No ABI | Split when the hub is touched next |
| F-GAME1 | P1 | `ashbox_game.c` **4,403** lines | Keep splitting return / crisis | ABI can stay stable | Structural sprint |
| F-UI-HEAP | P2 | LVGL builtin pool is only **48 KiB** (`work_mem_int` 0xC000); after boot `lv_free=16,760`, `lv_used=63%` | Measure widget budget before splitting pages; no extra card layers without a measurement | None | Widget baseline before the UI split |
| F-COVER-UI | P2 | Not in host tests: `ashbox_ui.c` 4,628, `ashbox_sfx.c` 503, `ashbox_embackup.c` 434, `ashbox_sprites.c` 205, `ashbox_settings.c` 162 | Move extractable pure logic (selection, copy assembly, volume mapping) into testable units | None | Do it together with the page split |
| F-TEST-V12 | **Closed** | Added a 744-byte frozen v12 fixture (CRC `0xE28BACD0`) plus migration assertions; three failure injections (bad CRC / dropped water field / wrong `pace_era`) were all caught | — | Migration trust | None |
| F-MIG-PAD | P3 | If a v12 save's CRC was sealed over a dirty pad, the second decode path accepts it but `migrate_from_v12` does not clear `pad_v12`, leaving dirty bytes in RAM | Add one `memset(pad_v12)` line with the next firmware change | No real harm: `ash_game_crc` ignores the pad and `seal_for_store` clears it before writing | Ride along with the next firmware change |
| F-PRINTF | P3 | printf/scanf family about **28.3 KiB** | Optional trim | Flash only | When free space approaches 45% |
| F-LIST-CLIP | P2 | Dense list titles still CLIP (explanation surfaces already WRAP) | Keep titles short | Acceptable UX | Spot-check long titles on device |

### Sprint order

1. Confirm help wrapping and boot on the device (flashed this round, awaiting eyes)
2. Split the UI pages (measure the LVGL heap and widget budget first)
3. Optional Flash P3
4. Carry the one-line F-MIG-PAD fix along with the next firmware change

---

## 3. Player copy layout

| Check | Result |
| --- | --- |
| Help wrapping | **PASS** (`help=94` lines checked) |
| No short buffers | **PASS** |
| Dialog wrapping | **PASS** (`view=3` surfaces) |
| Bottom hint strip | **PASS** (fixed width, `row=68/70`) |
| Label width | **PASS** (`label_w=196`) |
| Glossary / one item one name | **PASS** (10 banned patterns) |
| Host gates | **PASS** (`check_ashbox_copy_layout.py`, `check_ashbox_glossary.py`) |
| On-device appearance | **Pending** (the two new injury/wanderer help lines) |

---

## 4. Non-code fixes this round (docs / ops)

1. `PROJECT.md` / `PROJECT.zh_CN.md`: active workspace and branch, retired `qr-knowledge` MCP, save v13, community 1.7 incremental baseline (commit `79a3436`).
2. `docs/ashbox/BUILD.md` / `.zh_CN.md`: refreshed archive identity, `ver 11` → `ver 13` / blob 776, archive retention rule.
3. Firmware archives **118 → 21** (2.09 GB freed); the kept and pruned sets are recorded in `build/firmware/ARCHIVE_INDEX.md` (local, gitignored).
4. New host test in `tests/test_ashbox_save.c`: a 744-byte frozen v12 fixture plus migration assertions, with three failure injections proving the assertions bite. **Test only; no product code changed.**

Items 1–3 are documentation and ops, item 4 is test-only; none changes firmware behaviour, and no product code has changed since `feeac35`.

---

## Related

- Standing rails: [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md)
- Previous full round: [AUDIT_FULL_20261108.md](./AUDIT_FULL_20261108.md)
- Size plan: [FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md)
- Product index: [README.md](./README.md)
