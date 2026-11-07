[English](./AUDIT_STRICT.md) · **简体中文**

# 余烬 — 严格审计轮（Flash + 整体代码）

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-06（Asia/Shanghai） |
| 权威 | [`PROJECT.md`](../../PROJECT.md) §审计硬轨 · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| 方法 | 已校验归档实测 + 宿主 ashbox 测 + 源码/map/`nm` 取证 |
| 非真机验收 | 本轮无设备并发/压力 |

```text
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS
```

```text
Build: 复用已校验归档（本轮未重编）
Host tests: PASS（ashbox game + embackup + 字形 + 精灵校验 + 死枝）；完整 ./tools/validate.sh --static FAIL（见 C-DOC）
Device tests: NOT RUN
Unverified: 长按入睡路径 NVS 磨损；撬棍收割权重真机观感；去掉 Montserrat-14 后的字形冒烟
```

---

## 1. Flash / 固件体积

### 镜像身份

| 项 | 值 |
| --- | --- |
| 为何未重编 | 工作树含文档/规则脏文件；产品二进制与最近打包一致；按 AUDIT_REQUIREMENTS 允许复用+校验 |
| 归档 | `build/firmware/d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76/` |
| `full.bin` sha256 | `d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76` |
| `app.bin` | **2,894,928** B（2.761 MiB） |
| 校验 | `archive_firmware.py verify` → **PASS** |

### 预算

factory `0x7F0000`；占用 34.8%；**空闲 5,428,144 B（65.2%）**。

### 主要消耗

| 桶 | 字节 | 说明 |
| --- | --- | --- |
| 精灵 ASR1（36 帧） | **1,316,600**（1.256 MiB） | 与磁盘 RLE 一致；像素校验 PASS |
| 电台 PCM×5 | **247,884** | 与体积方案电台集合一致 |
| 暖炉 PCM | **384,000** | `hearth_fire.pcm`，相对 P2 增长主因 |
| 字库 glyph_bitmap | **111,321** | 自研约 102 KiB；Montserrat-14 仍链入约 8.8 KiB |
| `.flash.rodata` / `.flash.text` | 2,190,328 / 621,592 | |
| `s_fb` BSS | 84,480 | 单帧 RGB565，符合无 PSRAM 约束 |

### 与 FLASH_SIZE_PLAN 对照

相对 P2（app 2,490,992 / 约 70% 空闲）：本轮 **+403,936 B**，其中暖炉 PCM 约占增长的 **95%**。精灵载荷无漂移。空闲仍 **>40%**，非紧急；P3 可选。

### 可做 / 禁做

**可做（无损）：** 修好体积报告工具桶；确认能否关 Montserrat；可选 LVGL/printf 修剪。  
**禁止（除非当场改口）：** 砍景/砍帧、缩小 240×176、有损图、缩电台内容、为体积砍玩法档位。

---

## 2. 整体代码优化

| ID | 优先级 | 证据 | 建议 | 体验/存档 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| C-DOC | P0 | `validate.sh --static` 因根目录中文 `PROJECT.md` 等被 `check_repo` 挡死 | 打通门禁且不稀释项目约定 | 挡本地全静态门 | 排除/英文化 stub + 配对中文；修正 AUDIT_REQUIREMENTS 英文交叉引用 |
| C-CB1 | **P0** | 持 runtime+LVGL 时：长按入睡/`出发成功` `flush_save`；设置 `persist_cfg`；对照 tick 侧 `persist_after_settle` 在取 LVGL **之前** flush | 按键只脏标记；NVS 仅在无 LVGL 临界区落盘 | 卡顿/拖死 LVGL；成功则档仍对 | 统一到 tick 侧模式 |
| C-BAT | **P0** | `paint_home*` → `refresh_battery` → CW2017 I2C 在 LVGL 锁内 | ≤1 Hz 缓存，绘制只读缓存 | 家园刷新顿一下 | `s_bat_cache` + 电源 tick |
| C-SAVE | **P0** | `migrate_from_v*` 无宿主测；`validate.sh` 不编 `ashbox_save.c` | 纯函数 decode + v8/v9→v10 合成档断言 | 旧档/USB 迁档回归靠真机 | `tests/test_ashbox_save.c` |
| C-UI1 | P1 | UI ≈3769 行 | 按页拆，LAYOUT_LOCK 集中 | 不改存档 | 先拆交易/装备/物资 |
| C-GAME1 | P1 | `resolve_return` ≈397 行 / game ≈3901 行 | 先 static 拆 loot/撬棍/失败归来 | ABI 稳定则无档影响 | 再视需要 `ashbox_loot.c` |
| C-TEST1 | P1 | 撬棍仅烟测；背包翻倍/铁掉落无硬断言 | 固定种子 + 区间 | 锁 v10 经济 | 扩 `test_ashbox_game.c` |
| C-COPY | P1 | 电台/制造/药名仍散落 game + UI 硬编码 | 文案收口 `ashbox_content` | 双源中文+字库 | 先迁 `k_radio_*` 与药名 |
| C-TOOL1 | P2 | 体积报告桶过时 | ASR1 start/end + 暖炉 | 误导审计 | 改工具 |
| C-FAT1 | P2 | Montserrat-14 仍链；20 无引用 | 可关则关 | 小体积 | defaults + 字形冒烟 |
| C-SIZE1 | P2 | 相对 P2 ≈暖炉 384 KiB | 写入体积方案 §4 | 已上线氛围成本 | 更新基线 |
| C-BOUND1 | P2 | 固件无 demo；静态门仍跑上游 demo 测 | `--ashbox` 窄门 + 禁 `lv_` 进规则 | 加速本仓门禁 | 改 `validate.sh` |

### 健康面

存档 632/`VER=10`；SFX 入队；tick 侧「先 persist 再 LVGL」正确；产品未链 demo；宿主/精灵/死枝 PASS。

### 建议落地顺序

1. C-CB1 + C-BAT → 2. C-DOC（可选 `--ashbox`）→ 3. C-SAVE → 4. C-TEST1 → 5. C-GAME1 / C-COPY / C-TOOL1

---

## 相关

[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md) · [AUDIT_CODE.zh_CN.md](./AUDIT_CODE.zh_CN.md) · [AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md)
