[简体中文](./AUDIT_STRICT.zh_CN.md) · **English**

# Ashbox — strict audit round (Flash + whole-code)

| Field | Value |
| --- | --- |
| Date | 2026-11-06 (Asia/Shanghai) |
| Authority | [`PROJECT.md`](../../PROJECT.md) audit rails · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| Method | Verified archive measure + host ashbox tests + source/map/`nm` evidence |
| Not a device acceptance | No on-device concurrency / stress this round |

```text
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS
```

```text
Build: REUSED verified archive (not rebuilt this round)
Host tests: PASS (ashbox game + embackup + font + sprites verify + dead-branch); full ./tools/validate.sh --static FAIL (see C-DOC)
Device tests: NOT RUN
Unverified: NVS wear under long-press sleep path; crowbar haul weight distribution on device; LVGL Montserrat-14 removal smoke
```

---

## 1. Flash / firmware size

### Image identity

| Field | Value |
| --- | --- |
| Why not rebuilt | Working tree dirty with docs/rules; product binary unchanged vs last package; reuse + verify is valid per AUDIT_REQUIREMENTS |
| Archive | `build/firmware/d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76/` |
| `full.bin` sha256 | `d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76` |
| `app.bin` | **2,894,928** B (2.761 MiB); sha256 `6d08942b…` |
| ELF / appdesc | sha256 `b5064708…`; IDF **v5.5.3**; version `1802df5-dirty` |
| Verify | `python3 tools/archive_firmware.py verify …` → **PASS** |

### Budget

| Item | Value |
| --- | --- |
| Factory | `0x7F0000` = 8,323,072 B |
| Used | 34.8% |
| Free | **5,428,144 B (65.2%)** |

### Top consumers (measured)

| Bucket | Bytes | Notes |
| --- | --- | --- |
| Sprites ASR1 (36× `_binary_*_bin`) | **1,316,600** (1.256 MiB) | Matches disk `assets/ashbox/sprites_rle`; pixel `--verify-only` PASS |
| Radio PCM (5 clips) | **247,884** | Unchanged vs FLASH_SIZE_PLAN radio set |
| Hearth PCM | **384,000** | `hearth_fire.pcm` — dominant growth vs P2 |
| Font `glyph_bitmap` (ashbox + Montserrat-14) | **111,321** | Ashbox ≈102,429; Montserrat-14 ≈8,832 still linked |
| `.flash.rodata` | **2,190,328** | `riscv32-esp-elf-size -A` |
| `.flash.text` | **621,592** | |
| `.dram0.bss` | **140,496** | Includes `s_fb` 84,480 (one RGB565 frame) |

Accounted art+audio+font ≈ **2.06 MiB** of app image.

### Plan alignment ([FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md))

| Checkpoint | Plan | This round | Drift |
| --- | --- | --- | --- |
| P2 indexed+RLE app | 2,490,992 (≈70% free) | 2,894,928 (65.2% free) | **+403,936 B (~394 KiB)** |
| Sprite payload | ≈1.26 MiB | 1.256 MiB | None |
| Radio five blobs | ≈242 KiB | 247,884 | None |
| Hearth | not in P2 §4 table | +384,000 | **≈95% of app growth** |

P2 lossless sprite work remains in force. Headroom still **>40% free** → no emergency cut; P3 optional.

### Allowed vs forbidden

**Lossless / allowed next (P3-ish):**

1. Update `tools/ashbox_flash_size_report.py` buckets for ASR1 `_binary_*_bin_*` + hearth (today reports sprites/radio as 0).
2. Confirm whether `lv_font_montserrat_14` (~9 KiB glyphs + dsc) can be disabled after setting LVGL default to `ashbox_font_16`; `CONFIG_LV_FONT_MONTSERRAT_20=y` is **already unreferenced** in the map (config fat only).
3. Optional LVGL widget / printf family trim (`_vfprintf_r` / `_svfprintf_r` still large in `nm`).

**Forbidden unless user overrides:** cut scenes/frames; shrink below 240×176; lossy art; shorten radio PCM content; gameplay tier cuts for size.

---

## 2. Whole-codebase optimization

### Findings

| ID | Pri | Evidence | Suggestion | Experience / save impact | Next step |
| --- | --- | --- | --- | --- | --- |
| C-DOC | P0 | `./tools/validate.sh --static` aborts in `check_repo.py`: root `PROJECT.md` (zh, no peer), `AUDIT_REQUIREMENTS.md:5` flagged non-English | Unblock the standing gate without diluting project rules | No player impact; **blocks CI-shaped local gate** | Decide: exclude `PROJECT.md` from Passport root-md rules **or** add English stub + `PROJECT.zh_CN.md` + move narrative; fix AUDIT_REQUIREMENTS EN-only cross-ref wording |
| C-CB1 | **P0** | Under runtime+LVGL locks: LONG sleep `flush_save` (`ashbox_ui.c` ~3529); depart success `flush_save` (~2912); settings `persist_cfg`→`ashbox_settings_save` (~3220–3236). Contrasts with tick `persist_after_settle` which flushes **before** taking LVGL (~3669 vs ~3671) | Key path only `mark_save` / flag; NVS under runtime **without** LVGL (tick/worker) | Input jank / LVGL stall; save correct if store OK | Unify all UI NVS to the tick-side pattern; sleep: unlock LVGL → flush → deep-sleep prep |
| C-BAT | **P0** | `paint_home` / `paint_home_timers_only` call `refresh_battery()` (~818, ~3352) → sync CW2017 I2C inside LVGL lock | Cache SOC/mV at ≤1 Hz outside LVGL; paint reads cache only | Home refresh hitch; no save risk | Add `s_bat_cache` + power tick sample |
| C-SAVE | **P0** | `ashbox_save.c` has `migrate_from_v*` (incl. v9→v10 iron/meds/wanderer); `validate.sh` never compiles `ashbox_save.c`; host only sizeof/offsetof + `ash_game_after_migrate` smoke | Pure `ashbox_save_decode(raw,len,out)` + host tests for synthetic v8/v9 blobs | Silent ABI/migrate regress; USB/old-save trust | `tests/test_ashbox_save.c` + one gcc line in validate |
| C-UI1 | P1 | `ashbox_ui.c` ≈3769 LOC; `build_screen` 3388 B text; many `paint_*` | Split page builders; keep LAYOUT_LOCK constants in one header | No save ABI change | Extract `UI_TRADE`/`UI_EQUIP`/`UI_RES` first |
| C-GAME1 | P1 | `ashbox_game.c` ≈3901 LOC; `resolve_return` ≈397 lines / 3612 B text | Split `apply_success_loot` / `apply_crowbar_haul` / `apply_failure_return` (API stable) | ABI unchanged if structs stay in header | Land static splits first, then optional `ashbox_loot.c` |
| C-TEST1 | P1 | Crowbar smoke only `any_gain>=1`; **no** host assert for backpack `food*=2`/`iron*=2` (~1964–1968) or `iron_loot_for_tier` bounds | Fixed-seed bands for haul + backpack doubling + iron-by-tier | Locks v10 economy | Extend `test_ashbox_game.c` |
| C-COPY | P1→done | Radio lanes + med display names moved to `ashbox_content`; UI uses accessors | Dual-source CN reduced for radio/meds | Remaining craft/`return "..."` polish optional |
| C-TOOL1 | P2 | `ashbox_flash_size_report.py` still matches `s_station_*` / radio `_start` size field | Sum ASR1 `_binary_*_bin_end-_start` + hearth | Misleads audits | Fix tool + fixture on archive `d57c2…` |
| C-FAT1 | P2 | Montserrat-14 glyphs still linked (~8.8 KiB); `MONTSERRAT_20=y` but **0** map refs | Disable unused Montserrat after default→`ashbox_font_16` | Tiny Flash win | `MONTSERRAT_20=n`; try `14=n` + glyph smoke |
| C-SIZE1 | P2 | App +394 KiB vs P2 ≈ `hearth_fire.pcm` (384 KiB) | Document hearth in FLASH_SIZE_PLAN §4; do not shorten without design OK | Atmosphere already shipped | Update baseline table |
| C-BOUND1 | P2 | `ashbox_game.*` clean of `lv_`; product CMake has no `demo_*` | CI forbid `lv_` in game/content; optional `validate.sh --ashbox` skipping upstream demo host tests | Host-testability / faster ashbox gate | Grep gate + `--ashbox` mode |

### Healthy

- Save blob: host `sizeof(ash_game_t)=632` == `ASHBOX_SAVE_BLOB_SIZE`; `ASHBOX_SAVE_VER=10`; offsetof canaries present.
- SFX: `ashbox_sfx_play` is queue-to-worker (not blocking I2S in UI).
- Lock order documented: runtime → LVGL; tick-side persist-before-LVGL is the correct pattern to extend.
- Product `main/CMakeLists.txt` links ashbox only (no demo UI shell in app component).
- Host: `test_ashbox_game` / `test_ashbox_embackup` PASS; sprites pixel verify PASS; dead-branch scan PASS (system_only 27/28).
- Embackup confirm is flag-only; USB work stays off the UI callback.

### Sprint priority (implementation)

1. NVS + battery out of LVGL critical sections (C-CB1, C-BAT)  
2. Unblock / narrow validate gate (C-DOC; optional `--ashbox`)  
3. Save migrator host tests (C-SAVE)  
4. Harden v10 economy tests (C-TEST1)  
5. Split `resolve_return` / copy sink / size-tool (C-GAME1, C-COPY, C-TOOL1, C-SIZE1)

---

## Related

- Size plan: [FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md)  
- Prior code audit: [AUDIT_CODE.md](./AUDIT_CODE.md)  
- Ten-round: [AUDIT_10R.md](./AUDIT_10R.md)  
- Standing gate: [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md)  
