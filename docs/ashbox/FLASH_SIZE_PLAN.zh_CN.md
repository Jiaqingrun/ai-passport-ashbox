**简体中文** · [English](./FLASH_SIZE_PLAN.md)

# 《余烬》Flash / 体积无损压缩实施方案

**约束（写死）：** 整体游戏功能、画面像素、动效帧集合、声效内容、规则逻辑不变。  
**不做：** 砍景 / 砍帧 / 缩小 `240×176` / 有损图 / 缩电台 PCM。  
**布局锁：** [LAYOUT_LOCK.zh_CN.md](./LAYOUT_LOCK.zh_CN.md) 仍为权威。

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-06 |
| 状态 | Phase 2 **indexed+RLE** 固件复测 PASS；P3/P4 可选 |
| 分区 | factory `0x7F0000` ≈ 7.94 MiB |
| 精灵规格 | 12×3×240×176 RGB565 语义不变 |

---

## 0. 目标与成功标准

| 目标 | 验收 |
| --- | --- |
| 真实基线 | 干净 `--firmware` 后记录 `app.bin` / `.flash.rodata` / 精灵符号总字节 |
| 无损压精灵 | 每帧解码后 RGB565 **逐像素 ==** 现行 PNG→RGB565 结果 |
| 动效不变 | 仍 12 景 × 3 帧；`ashbox_sprite_get_frame` API 语义不变 |
| 声效不变 | 五段电台 PCM 字节与播放路径不变 |
| 逻辑不变 | 宿主单测全绿；不改掉率/档位 |

预估（宿主探针，非固件实测）：

| 编码 | 精灵载荷 | 相对裸 RGB565 |
| --- | --- | --- |
| 裸 RGB565（现行） | ≈2.90 MiB | — |
| 精确调色板 + I8 索引 | ≈1.45 MiB | ≈−1.5 MiB |
| 调色板 + 索引 RLE | ≈1.26 MiB | ≈−1.7 MiB |

---

## 1. 阶段总览

| 阶段 | 内容 | 产出 | 停手条件 |
| --- | --- | --- | --- |
| **P0 基线** | `fullclean` 级干净固件门禁；量体积 | 基线表写入本文 §4 | 编不过则先修编译/链接 |
| **P1 探针** | 宿主脚本：索引 / RLE 体积 + 同像素断言 | `tools/ashbox_sprites_compress_probe.py` PASS | 任一帧色数 >256 → 改方案 |
| **P2 编码落地** | 改 `ashbox_sprites_encode.py`；运行时解码或 LVGL I8 | 新 `ashbox_sprites.c`（或 `.S`）+ 小解码器 | 同像素门禁失败则回滚 |
| **P3 脂肪** | 关未用 LVGL 控件；确认后关 Montserrat；可选 `-Os` | 二次体积对比 | 冒烟回归失败则单项回退 |
| **P4 真机** | 保档刷机；家园 12 景轮询 + 电台耳听 | Device PASS | 用户确认前不合并「已完成」 |

并行禁令：P2 完成前不改玩法/文案扩字库；不扩精灵帧（见 [OPTIMIZE_LOOP.zh_CN.md](./OPTIMIZE_LOOP.zh_CN.md) P2）。

---

## 2. 技术选型（P2）

### 首选：每帧独立精确 RGB565 调色板 + 8-bit 索引

- 实测全 36 帧色数 ≤98 → I8 足够。
- **A 路径（优先试）：** 若本树 LVGL 能直接画 Flash 内 I8（含工程约定的调色板布局），则 `lv_image_dsc_t` 指压缩数据，**零全帧 RAM**。
- **B 路径（保底）：** 自定义 decoder / `set_src` 时按行解到 **单行 RGB565 缓冲**（240×2 字节），对齐参考文档 [display-refresh-and-deep-sleep](../reference/shinku-chen/display-refresh-and-deep-sleep.md) 的「I8 + 行缓冲」做法。禁止申请 240×176×2 全帧缓冲（无 PSRAM）。

### 次选：索引后再做字节 RLE

- 仅当 P2-A/B 体积仍不够预留后续内容时启用。
- 格式建议：`[palette_n:u8][palette:u16×n][runs: (count:u8, index:u8)*]`，行或全帧皆可；解码仍走单行缓冲。

### 明确不做

| 做法 | 原因 |
| --- | --- |
| JPEG / 缩放 / 减帧 | 改画面或动效 |
| 帧指针别名（近似合并） | 帧间无像素级全同；近似合并改动效 |
| 压缩电台 PCM | 改声效 |
| 减远征档 | 改逻辑且几乎不省 Flash |

---

## 3. 工程步骤（可执行清单）

### P0

```bash
source <ESP-IDF-5.5.3>/export.sh
./tools/validate.sh --static          # 宿主 + 仓库检查
./tools/validate.sh --firmware        # 干净配置构建 + 合并镜像
python3 tools/ashbox_flash_size_report.py
```

记录：`app.bin`、factory 占用率、`s_station_*` / `_binary_radio_*` / `glyph_bitmap` 符号合计。

### P1

```bash
python3 tools/ashbox_sprites_compress_probe.py
# 期望：PIXELS PASS；打印 indexed / rle 字节
```

### P2（批准后改代码）

1. 扩展 `tools/ashbox_sprites_encode.py`：`--format rgb565|indexed|indexed_rle`。
2. 新增 `main/ashbox_sprite_decode.c`（若需 B/RLE）：只服务 UI 精灵，不进游戏规则。
3. `ashbox_ui` 仍只调 `ashbox_sprite_get_frame`；解码细节封在 sprites 模块。
4. 门禁：探针脚本进 `./tools/validate.sh --static`（或独立 `check_ashbox_sprites_roundtrip.py`）。
5. 再跑 `--firmware` + `ashbox_flash_size_report.py`，与 P0 基线对比。

### P3

1. `sdkconfig.defaults`：关闭 Ashbox 未用的 `CONFIG_LV_USE_*`（chart/calendar/keyboard/menu/…）；**逐项确认无引用**。
2. 确认无 Montserrat 回退后关闭 `CONFIG_LV_FONT_MONTSERRAT_14/20`（demo 源若仍在树但不链入可忽略）。
3. 可选：`CONFIG_COMPILER_OPTIMIZATION_SIZE`；需完整宿主测 + 真机冒烟。
4. Wi-Fi：应用不 `esp_wifi_init`；关 `CONFIG_ESP_WIFI_ENABLED` 须单独测链接与电流，不与 P2 绑死。

### P4

```bash
./tools/flash_ashbox_preserve_save.sh <port>   # 保档
# 目视：12 景切换 + 3 帧动画；耳听电台事件
```

交付字段：

```text
Build: …
Host tests: …
Device tests: …
Flash baseline → after: app.bin / sprites payload / free%
Unverified: …
```

---

## 4. 基线记录（P0 填写）

| 字段 | 值 |
| --- | --- |
| 归档 / app SHA | `build/firmware/16a76ad5f3995d787d5eb792c7d8e6879090a5e781301301cd42eedf73bd42ef`；app.bin sha256 `74069a94…` |
| `app.bin` 字节 | **4 215 744**（4.020 MiB） |
| factory 占用率 | **50.7%** 已用 / **49.3%** 空闲 |
| 精灵符号合计 | **3 041 280**（12×3×84480，裸 RGB565） |
| 电台 PCM | **247 884**（≈242 KiB；五段 `_binary_radio_*` 已入镜像） |
| 字库 bitmap 合计 | **≈108 KiB**（`glyph_bitmap`） |
| `.flash.rodata` | **3 525 560** |
| 备注 | 门禁隔离构建；工作区根 `build/*.bin` 可能仍是旧镜像，以归档为准 |

### 暖炉 / v1.3 打包后（2026-11-06 晚）

| 项 | 值 |
| --- | --- |
| 归档 | `build/firmware/d57c2b2e54c188f503c6df8f2bb7a543d7c1ea7b8015f283bc12a380cb6d6e76` |
| `app.bin` | **2,894,928**（2.761 MiB） |
| Factory 空闲 | **65.2%** |
| 精灵 ASR1 | **1,316,600** |
| 电台 PCM×5 | **247,884** |
| 暖炉 PCM | **384,000**（`hearth_fire.pcm`；未经理设计勿缩短） |
| 相对 P2 | **+403,936** ≈ 暖炉 |

### P2 复测（indexed+RLE，2026-11-06）

| 字段 | 值 |
| --- | --- |
| 归档 | `build/firmware/cbf27c81b2f732e9274c55adbbc368911ad5d3ed507738f86265f732b9af0e83` |
| `app.bin` | **2 490 992**（2.375 MiB） |
| factory 空闲 | **≈70%**（相对 P0 基线约 **+1.65 MiB**） |
| 精灵 Flash 载荷 | ASR1 blobs **≈1.26 MiB**（原 2.90 MiB RGB565） |
| RAM | 单帧解码缓冲 `s_fb[240*176]` ≈82.5 KiB `.bss` |
| 宿主 | `--static` PASS（含 `--verify-only` 同像素） |

---

## 5. 风险

| 风险 | 缓解 |
| --- | --- |
| 12 MiB 级 `ashbox_sprites.c` 编译过慢 / 吃内存 | P2 改为 `.S` / 二进制 blob + `target_add_binary_data` |
| LVGL I8 布局与预期不符 | 先宿主往返；设备用 B 路径行解码 |
| `-Os` 改变时序 | 单独提交；可回退 |
| 关 LV 控件误伤 | 链接报错或 UI 空白 → 立刻复原单项 |

---

## 6. 决策点（每阶段结束问一次）

1. P0 数字出来后：余量是否仍 >40%？若是，P2 可排期而非紧急。  
2. P1 PASS 后：选 **仅 indexed** 还是 **indexed+RLE**？  
3. P2 固件体积对比后：是否继续 P3 脂肪？  
4. P4 真机：是否保档刷机验收？
