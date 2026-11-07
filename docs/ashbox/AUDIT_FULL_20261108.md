[简体中文](./AUDIT_FULL_20261108.zh_CN.md) · **English**

# Ashbox — full strict audit (2026-11-08)

| Field | Value |
| --- | --- |
| Date | 2026-11-08 (Asia/Shanghai) |
| Authority | [`PROJECT.md`](../../PROJECT.md) audit rails · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| Method | Fresh `./tools/validate.sh --ashbox` + `--firmware`; `ashbox_flash_size_report.py`; source/`nm`/size evidence; fix-forward on FAIL |
| Scope | Flash size · whole-code optimization · player copy layout · save ABI v13 · stack/boot regression follow-up |
| Not device acceptance | No preserve-save reflash in this audit pass |

```text
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS
Audit mandatory: Copy layout = PASS
```

```text
Build: PASS (archive 05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27)
Host tests: PASS (--ashbox: repo + font + dead-branch + copy-layout + sprites + game/embackup/save)
Device tests: NOT RUN
Unverified: on-device help wrap feel; NVS-erase rescue path; long USB import under load
```

---

## 1. Flash / firmware size

### Image identity

| Field | Value |
| --- | --- |
| Rebuilt | Yes — this audit |
| Archive | `build/firmware/05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27/` |
| `full.bin` sha256 | `05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27` |
| `app.bin` | **2,921,680** B (2.786 MiB); sha256 `…` (see archive `manifest.json`) |
| ELF / appdesc | IDF **v5.5.3**; version `1802df5-dirty` |
| Verify | `python3 tools/archive_firmware.py verify …` → **PASS** |

### Budget

| Item | Value |
| --- | --- |
| Factory | `0x7F0000` = 8,323,072 B |
| Used | 35.1% |
| Free | **5,401,392 B (64.9%)** |

### Top consumers (measured)

| Bucket | Bytes | Notes |
| --- | --- | --- |
| Sprites ASR1 | **1,316,600** (1.256 MiB) | Matches host pixel verify |
| Radio PCM ×5 | **247,884** | |
| Hearth PCM | **384,000** | Dominant growth vs early P2 baseline |
| `glyph_bitmap` sum | **106,480** | Montserrat symbols **0** (removed vs older audits) |
| `.flash.rodata` | **2,192,808** | `riscv32-esp-elf-size -A` |
| `.flash.text` | **645,832** | |
| `.dram0.bss` | **147,784** | Includes `s_fb` 84,480 |

Art+audio+font ≈ **2.06 MiB** of app image.

### Plan alignment ([FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md))

| Checkpoint | Plan / prior | This round | Drift |
| --- | --- | --- | --- |
| P2 indexed+RLE app | 2,490,992 (~70% free) | 2,921,680 (64.9% free) | **+430,688 B** |
| Sprite payload | ≈1.26 MiB | 1.256 MiB | None |
| Radio five blobs | ≈242 KiB | 247,884 | None |
| Hearth | not in early P2 §4 | +384,000 | ~89% of growth vs P2 |
| Montserrat | was ~9 KiB linked | **0** symbols | Improved |

Headroom still **>40% free** → no emergency cut. P3 optional only.

### Allowed vs forbidden

**Lossless / allowed next:**

1. Optional printf-family trim (`_vfprintf_r` / `_svfprintf_r` still large in `nm`).
2. Split oversized UI/game TUs (code clarity; tiny Flash side effect).
3. Keep documenting hearth in FLASH_SIZE_PLAN baseline table.

**Forbidden unless user overrides:** cut scenes/frames; shrink below 240×176; lossy art; shorten radio/hearth PCM; gameplay tier cuts for size.

---

## 2. Whole-codebase optimization

### Healthy (evidence)

- Rules/UI boundary: `validate.sh --ashbox` forbids `lv_` in `ashbox_game*.c` / content / save / meta — PASS.
- Save ABI: `ASHBOX_SAVE_VER=13`, blob **776**; host `test_ashbox_save` covers pad seals + v11 decode; migrators present through v12→v13.
- Battery I2C: `sample_battery()` outside LVGL; `refresh_battery()` paint-only cache (C-BAT closed).
- NVS flush: `flush_save` contract requires runtime lock **without** LVGL (C-CB1 pattern landed).
- Boot stack overflow (prior): CRC / load raw / store / v12 migrate moved to static scratch; this audit also static’d rescue scan + embbackup verify.
- Host: game / embackup / save / font / sprites / dead-branch / **copy-layout** PASS.
- Dead branches: 37 parsed, 35 reachable, system_only `[27, 28]`.

### Findings

| ID | Pri | Evidence | Suggestion | Experience / save | Next step |
| --- | --- | --- | --- | --- | --- |
| F-STACK1 | P0→**fixed** | `rescue_scan_save` had stack `best`+`cand` (2×776); `embackup` verify on stack | Static scratch | Boot/NVS-rescue / import panic risk | Landed this audit |
| F-DOC1 | P0→**fixed** | `PROJECT*` / README / META / `10-project.mdc` still said SAVE_VER **12** or **11** while code is **13**/776 | Sync authoritative docs | Wrong operator expectations | Landed this audit |
| F-UI1 | P1 | `ashbox_ui.c` **4431** LOC; `build_screen` / many `paint_*` still monolith | Split trade/equip/stash/help pages; keep LAYOUT_LOCK constants centralized | No ABI | Extract pages behind `ashbox_ui_*.c` |
| F-GAME1 | P1 | `ashbox_game.c` **4333** LOC; `resolve_return` still large text | Keep loot already in `ashbox_game_loot.c`; further split return/crisis | ABI stable if headers stay | Next structural sprint |
| F-CRC-STATIC | P2 | `ash_game_crc` / load / store / rescue use `static` temps (not re-entrant) | Document single-threaded save path; or mutex if ever multi-caller | OK under current runtime lock | Note in AUDIT_CODE; no change unless second caller |
| F-TEST-V12 | P2 | Host save tests cover v11 + v13 seals; **no** synthetic v12 blob round-trip | Add frozen v12 fixture decode→v13 | Migrator trust | Extend `test_ashbox_save.c` |
| F-PRINTF | P2 | `_vfprintf_r` / `_svfprintf_r` ~9 KiB each | LVGL/printf trim if P3 | Flash only | Optional |
| F-LIST-CLIP | P2 | Dense list titles still CLIP; selected subs often SCROLL_CIRCULAR | Keep for hub density; instruction surfaces already WRAP | UX OK if titles short | Spot-check long craft/route titles on device |

### Sprint priority

1. Device confirm help WRAP + boot after this archive (preserve-save).  
2. Host fixture for v12→v13 migrate (F-TEST-V12).  
3. UI page split (F-UI1) when touching hub again.  
4. Optional Flash P3 printf trim — only if free % threatens 40%.

---

## 3. Player-facing copy layout

| Check | Result |
| --- | --- |
| Help WRAP | **PASS** — `paint_help` uses `LV_LABEL_LONG_MODE_WRAP`, row 68 / step 70 / view 3 |
| No tiny help pad | **PASS** — direct `ashbox_help_line()`; no `padded[96]` |
| Modal WRAP | **PASS** — `label_set_wrap` on modal lines; guide body newline-merged |
| Hint band | **PASS** — width 216 + SCROLL_CIRCULAR |
| Host gate | **PASS** — `tools/check_ashbox_copy_layout.py` |
| Fix-forward | Help/modal/hint already fixed earlier this session; no open clip FAIL on instruction surfaces |

---

## Fix-forward this round

1. Static scratch for NVS rescue scan + rescued blob + embbackup verify (`ashbox_save.c`, `ashbox_embackup.c`).  
2. Synchronized SAVE_VER **13** / blob **776** in `PROJECT.md` / `.zh_CN`, `docs/ashbox/README*`, `META_PROGRESS*`, `.cursor/rules/10-project.mdc`.  
3. Rebuilt + verified firmware archive after fixes.

---

## Related

- Standing gate: [AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md)  
- Size plan: [FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md)  
- Prior strict: [AUDIT_STRICT.md](./AUDIT_STRICT.md) · [AUDIT_20261107.md](./AUDIT_20261107.md)  
- Product index: [README.md](./README.md)
