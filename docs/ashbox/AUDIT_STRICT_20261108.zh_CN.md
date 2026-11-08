**简体中文** · [English](./AUDIT_STRICT_20261108.md)

# 《余烬》严格审计（2026-11-08 · 第二轮）

| 字段 | 值 |
| --- | --- |
| 日期 | 2026-11-08（Asia/Shanghai） |
| 权威 | [`PROJECT.md`](../../PROJECT.md) 审计硬轨 · [`AUDIT_REQUIREMENTS.zh_CN.md`](./AUDIT_REQUIREMENTS.zh_CN.md) |
| 方法 | 本轮重编 `./tools/validate.sh --firmware`、新跑 `--ashbox`；`ashbox_flash_size_report.py`；`_Static_assert` / `nm -S` / `map` 取证；真机保档刷 + 20 s 启动日志 |
| 范围 | Flash 体积 · 整体代码优化 · 玩家文案版式 · 存档 ABI v13 · 本会话文档与运维修复 |
| 真机 | 已保档刷 `445875b7…`，启动无 panic；**屏幕观感待用户确认** |

```text
审计硬轨：Flash 体积 = PASS
审计硬轨：代码优化 = PASS
审计硬轨：文案版式 = PASS
```

```text
Build: PASS（本轮归档 e02c57b5…；设备在跑 445875b7…，同源码、同尺寸）
Host tests: PASS（--ashbox：仓库 + 字形 + 死枝 + 文案版式 + 用语表 + 精灵 + game/embackup/save，含本轮新增 v12 冻结 fixture）
Device tests: PASS（保档刷 + 20 s 启动日志，ELF 哈希匹配，无 panic）
Unverified: 帮助页换行手感 · NVS 擦除救援路径 · 高压 USB 导入
```

---

## 1. Flash / 固件体积

### 镜像身份

| 字段 | 值 |
| --- | --- |
| 是否重编 | 是 — 本轮 `--firmware`，归档 `e02c57b5…` |
| 设备在跑 | `445875b7…`（源码同为 `feeac35`；启动日志 `ELF sha256 d76466af…`） |
| `app.bin` | **2,924,960** B（2.789 MiB） |
| ELF / appdesc | IDF **v5.5.3**；version `79a3436-dirty` |
| 校验 | `python3 tools/archive_firmware.py verify …` → **PASS** |

两次构建 `app.bin` 尺寸完全一致，`full.bin` 哈希不同只因内嵌 git 版本串（`1` → `79a3436-dirty`），不是代码漂移。

### 预算

| 项 | 值 |
| --- | --- |
| factory | `0x7F0000` = 8,323,072 B |
| 已用 | **35.1%** |
| 空闲 | **5,398,112 B（64.9%）** |

### 主要占用（实测）

| 桶 | 字节 | 备注 |
| --- | --- | --- |
| 精灵 ASR1 | **1,316,600** | 36 帧 RLE，与宿主像素门禁一致 |
| 电台 PCM ×5 | **247,884** | |
| 暖炉 PCM | **384,000** | |
| `glyph_bitmap` 合计 | **106,480** | Montserrat 符号 **0** |
| `.flash.text` | **647,912** | |
| `.flash.rodata` | **2,193,984** | |
| `.dram0.bss` | **147,784** | `s_fb` 84,480 + LVGL 内建池 `work_mem_int` 49,152 |

### 相对上一轮的漂移

相对 [AUDIT_FULL_20261108](./AUDIT_FULL_20261108.zh_CN.md)：`app` **+3,280 B**（`feeac35` 的重伤存活窗口与过路客确认），`.flash.text` +2,080、`.flash.rodata` +1,176；**精灵 / PCM / 字库三桶 0 漂移**。相对 [FLASH_SIZE_PLAN](./FLASH_SIZE_PLAN.zh_CN.md) 的 P2 基线仍约 +430.7 KB，增量全部来自代码与文案。

### 可做 / 禁做

**可做：** printf 族裁剪（约 **28.3 KiB**：`_vfprintf_r` 9,328 / `_svfprintf_r` 9,216 / `__ssvfiscanf_r` 6,806 / `_dtoa_r` 3,620）；UI / 规则大文件拆分；把暖炉 PCM 写入体积基线表。  
**禁做（除非用户当场改口）：** 砍景 / 砍帧、缩小 240×176、有损图、缩短电台或暖炉 PCM、为体积砍玩法档。

---

## 2. 整体代码优化

### 健康项（证据）

- **规则/UI 边界**：`--ashbox` 的 `rg lv_` 检查 PASS —— `ashbox_game.c`、`ashbox_game_loot.c`、`ashbox_content.c`、`ashbox_save.c`、`ashbox_meta.c` 五个单元无 LVGL 符号。
- **锁顺序**：`ashbox_ui.c` 模块头写明 `ashbox_runtime_lock → bsp_lvgl_lock`；跨任务入口 `ashbox_ui_on_key` 先 runtime 再 lvgl；USB 任务调的 `ashbox_ui_flush_persistent` 全程无 `lv_` 调用，NVS 不在 LVGL 下（C-CB1）。
- **存档 ABI**：`ASHBOX_SAVE_VER=13`、blob **776**；`main/ashbox_save.c` 共 **19** 处 `_Static_assert`（v4…v12 各代 `crc` 偏移与尺寸封印、`pad_v13` 6 字节、`crc` 封尾、8 字节对齐）。
- **本轮 ABI 无漂移**：`feeac35` 仅新增 `#define ASHBOX_INJURE_DEAD_S`，未动结构体，`sizeof==776` 断言仍过。
- **死枝**：37 分支 / 35 可达，`system_only=[27, 28]`。
- **宿主测覆盖单元**：game、game_loot、content、meta、save、embackup_proto + NVS 宿主桩。

### 发现

| ID | 优先级 | 证据 | 建议 | 体验/存档 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| F-UI1 | P1 | `ashbox_ui.c` **4,628** 行；`style_init` 8,864 B、`build_screen` 3,620 B、`resolve_return` 3,522 B、`paint_build` 3,220 B、`paint_home` 2,816 B | 拆交易 / 装备 / 仓库 / 帮助页 | 无 ABI | 再动枢纽时拆 |
| F-GAME1 | P1 | `ashbox_game.c` **4,403** 行 | 继续拆归来 / 危机 | ABI 可稳 | 结构冲刺 |
| F-UI-HEAP | P2 | LVGL 内建池仅 **48 KiB**（`work_mem_int` 0xC000）；开机后 `lv_free=16,760`、`lv_used=63%` | 拆页前先量控件预算；禁止无测算新增多层卡片 | 无 | 拆 UI 前先做控件基线 |
| F-COVER-UI | P2 | 未进宿主测：`ashbox_ui.c` 4,628、`ashbox_sfx.c` 503、`ashbox_embackup.c` 434、`ashbox_sprites.c` 205、`ashbox_settings.c` 162 | 把可外提的纯逻辑（选择/文案拼装/音量映射）移入可测单元 | 无 | 与拆页同做 |
| F-TEST-V12 | **已收口** | 新增 744 B 冻结 v12 fixture（CRC `0xE28BACD0`）+ 迁移断言；三次失败注入（坏 CRC / 漏拷水位 / 错设 `pace_era`）全部被捕获 | — | 迁移信任 | 无 |
| F-MIG-PAD | P3 | 若 v12 存档的 CRC 是对「脏 pad」封的，decode 第二路径会接受，但 `migrate_from_v12` 不清 `pad_v12`，RAM 里留下脏字节 | 下次改固件时补一行 `memset(pad_v12)` | 无实害：`ash_game_crc` 忽略 pad，落盘前 `seal_for_store` 会清零 | 随下次固件改动 |
| F-PRINTF | P3 | printf/scanf 族约 **28.3 KiB** | 可选裁 | 仅 Flash | 空闲逼近 45% 再做 |
| F-LIST-CLIP | P2 | 密列表标题仍 CLIP（说明面已 WRAP） | 标题保持短 | UX 可接受 | 真机抽查长标题 |

### 冲刺顺序

1. 真机确认帮助换行 + 本镜像开机（本轮已刷，待观感）
2. UI 拆页（先做 LVGL 堆与控件基线）
3. 可选 Flash P3
4. 顺手把 F-MIG-PAD 的一行清零随下次固件带进去

---

## 3. 玩家文案版式

| 检查 | 结果 |
| --- | --- |
| 帮助换行 | **PASS**（`help=94` 行参与版式检查） |
| 禁小缓冲 | **PASS** |
| 弹窗换行 | **PASS**（`view=3` 处） |
| 底提示条 | **PASS**（定宽，`row=68/70`） |
| 标签定宽 | **PASS**（`label_w=196`） |
| 用语表 / 一名一物 | **PASS**（10 条禁词规则） |
| 宿主门禁 | **PASS**（`check_ashbox_copy_layout.py`、`check_ashbox_glossary.py`） |
| 真机观感 | **待确认**（帮助页新增的重伤/过路客两行） |

---

## 4. 本轮非代码修复（文档 / 运维）

1. `PROJECT.md` / `PROJECT.zh_CN.md`：现行工作区与分支、`qr-knowledge` MCP 退役、存档 v13、社区 1.7 增量基线（提交 `79a3436`）。
2. `docs/ashbox/BUILD.md` / `.zh_CN.md`：归档身份更新、`ver 11` → `ver 13` / blob 776、归档保留规则。
3. 固件归档 **118 → 21**（释放 2.09 GB），保留集与删除集登记在 `build/firmware/ARCHIVE_INDEX.md`（本机、已 gitignore）。
4. 新增宿主测试 `tests/test_ashbox_save.c`：744 B 冻结 v12 fixture + 迁移路径断言，另用三次失败注入证明断言有效。**仅测试，不改产品代码。**

以上前 3 项为文档与运维，第 4 项为测试；均不改变固件行为，产品代码自 `feeac35` 起未变。

---

## 相关

- 常设硬轨：[AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md)
- 上一轮全量：[AUDIT_FULL_20261108.zh_CN.md](./AUDIT_FULL_20261108.zh_CN.md)
- 体积方案：[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)
- 产品索引：[README.zh_CN.md](./README.zh_CN.md)
