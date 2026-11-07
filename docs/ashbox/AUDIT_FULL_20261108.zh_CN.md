**简体中文** · [English](./AUDIT_FULL_20261108.md)

# 《余烬》— 全量严格审计（2026-11-08）

| 字段 | 值 |
| --- | --- |
| 日期 | 2026-11-08（Asia/Shanghai） |
| 权威 | [`PROJECT.md`](../../PROJECT.md) 审计硬轨 · [`AUDIT_REQUIREMENTS.zh_CN.md`](./AUDIT_REQUIREMENTS.zh_CN.md) |
| 方法 | 新跑 `./tools/validate.sh --ashbox` + `--firmware`；`ashbox_flash_size_report.py`；源码/`nm`/size 取证；FAIL 当场修 |
| 范围 | Flash 体积 · 整体代码优化 · 玩家文案版式 · 存档 ABI v13 · 开机栈溢出跟进 |
| 非真机验收 | 本轮未保档重刷 |

```text
审计硬轨：Flash 体积 = PASS
审计硬轨：代码优化 = PASS
审计硬轨：文案版式 = PASS
```

```text
Build: PASS（归档 05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27）
Host tests: PASS（--ashbox：仓库 + 字形 + 死枝 + 文案版式 + 精灵 + game/embackup/save）
Device tests: NOT RUN
Unverified: 真机帮助换行手感；NVS 擦除救援路径；高压 USB 导入
```

---

## 1. Flash / 固件体积

### 镜像身份

| 字段 | 值 |
| --- | --- |
| 是否重编 | 是 — 本轮审计 |
| 归档 | `build/firmware/05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27/` |
| `full.bin` sha256 | `05d6b5569fa1edbe4cfdef8179fe5ddb534ce07b4e07131617640700258d2e27` |
| `app.bin` | **2,921,680** B（2.786 MiB） |
| ELF / appdesc | IDF **v5.5.3**；version `1802df5-dirty` |
| 校验 | `python3 tools/archive_firmware.py verify …` → **PASS** |

### 预算

| 项 | 值 |
| --- | --- |
| factory | `0x7F0000` = 8,323,072 B |
| 已用 | 35.1% |
| 空闲 | **5,401,392 B（64.9%）** |

### 主要占用（实测）

| 桶 | 字节 | 备注 |
| --- | --- | --- |
| 精灵 ASR1 | **1,316,600**（1.256 MiB） | 与宿主像素校验一致 |
| 电台 PCM ×5 | **247,884** | |
| 暖炉 PCM | **384,000** | 相对早期 P2 基线的主要增长 |
| `glyph_bitmap` 合计 | **106,480** | Montserrat 符号 **0**（相对旧审计已清掉） |
| `.flash.rodata` | **2,192,808** | |
| `.flash.text` | **645,832** | |
| `.dram0.bss` | **147,784** | 含 `s_fb` 84,480 |

### 方案对照（[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)）

相对 P2 约 **+430,688 B**；精灵/电台无漂移；暖炉约占增长主体。空闲仍 **>40%**，无紧急砍内容必要。

### 可做 / 禁做

**可做：** printf 族裁剪；UI/规则大文件拆分；把暖炉写入体积方案基线表。  
**禁做（除非用户当场改口）：** 砍景/砍帧、缩小 240×176、有损图、缩短电台/暖炉 PCM、为体积砍玩法档。

---

## 2. 整体代码优化

### 健康项（证据）

- 规则/UI 边界：`--ashbox` 禁止规则单元出现 `lv_` — PASS  
- 存档 ABI：`ASHBOX_SAVE_VER=13`，blob **776**；宿主测含 pad 封印与 v11 解码  
- 电量 I2C：采样在 LVGL 外（C-BAT 已关）  
- NVS：`flush_save` 约定不持 LVGL（C-CB1 模式已落地）  
- 开机栈：CRC/加载/存储/v12 迁移已静态化；本轮救援扫描与导入校验也改静态  
- 宿主全绿；死枝 37/35 可达，system_only `[27, 28]`

### 发现

| ID | 优先级 | 证据 | 建议 | 体验/存档 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| F-STACK1 | P0→**已修** | 救援扫描栈上双份 `ash_game_t`；导入校验栈上 776B | 静态缓冲 | 开机/救援/导入恐慌风险 | 本轮已落地 |
| F-DOC1 | P0→**已修** | 权威文档仍写 SAVE_VER 12/11 | 同步为 13/776 | 运维误判 | 本轮已落地 |
| F-UI1 | P1 | `ashbox_ui.c` **4431** 行 | 拆交易/装备/仓库/帮助页 | 无 ABI | 再动枢纽时拆 |
| F-GAME1 | P1 | `ashbox_game.c` **4333** 行 | 继续拆归来/危机 | ABI 可稳 | 结构冲刺 |
| F-CRC-STATIC | P2 | CRC/存档路径用 static，非重入 | 维持单线程+runtime 锁约定 | 现状可接受 | 记入 AUDIT_CODE |
| F-TEST-V12 | P2 | 缺冻结 v12 blob 宿主往返 | 补 fixture | 迁移信任 | 扩 `test_ashbox_save.c` |
| F-PRINTF | P2 | `_vfprintf_r` 等仍大 | P3 可选裁 | 仅 Flash | 空闲逼近 40% 再做 |
| F-LIST-CLIP | P2 | 密列表标题仍 CLIP | 说明面已 WRAP；标题保持短 | UX 可接受 | 真机抽查长标题 |

### 冲刺顺序

1. 真机确认帮助换行 + 本归档开机（保档刷）  
2. v12→v13 宿主 fixture（F-TEST-V12）  
3. UI 拆页（F-UI1）  
4. 可选 Flash P3

---

## 3. 玩家文案版式

| 检查 | 结果 |
| --- | --- |
| 帮助 WRAP | **PASS** |
| 禁小缓冲 | **PASS** |
| 弹窗 WRAP | **PASS** |
| 底提示条 | **PASS**（定宽跑马灯） |
| 宿主门禁 | **PASS** |
| 当场修 | 说明面无未关 FAIL |

---

## 本轮当场修复

1. NVS 救援扫描 / 救援缓冲 / USB 导入校验改为静态 scratch  
2. 权威文档与规则同步 SAVE_VER **13** / blob **776**  
3. 修复后重编并校验固件归档

---

## 相关

- 常设硬轨：[AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md)  
- 体积方案：[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)  
- 先前严格审计：[AUDIT_STRICT.zh_CN.md](./AUDIT_STRICT.zh_CN.md) · [AUDIT_20261107.zh_CN.md](./AUDIT_20261107.zh_CN.md)  
- 产品索引：[README.zh_CN.md](./README.zh_CN.md)
