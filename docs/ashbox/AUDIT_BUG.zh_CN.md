**简体中文** · [English](./AUDIT_BUG.md)

# 余烬 — 游戏代码严格 Bug 审计

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-07（Asia/Shanghai） |
| 权威 | [`PROJECT.md`](../../PROJECT.md) §审计硬轨 · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| 范围 | `ashbox_game` / `ashbox_content` / `ashbox_save` / `ashbox_embackup*` / `ashbox_ui` 接线 |
| 方法 | 源码取证 + 宿主 ashbox 测对照 + 已校验固件归档；并行只读子轨交叉 |
| 非范围 | 玩法许愿、纯风格重构、真机压力并发 |

```text
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS
总评（bug）: P0+P1 已修；远征禁自动息屏；深睡唤醒夹紧补算
修订: 2026-11-07 循环修 + P1 + 社区息屏反馈对策；ASHBOX_SAVE_VER=11 / blob 648
```

```text
Build: NOT RUN（本轮仅 --ashbox；固件待用户确认后编）
Host tests: PASS（./tools/validate.sh --ashbox）
Device tests: NOT RUN
Unverified: 真机远征息屏/深睡补算；USB 导入失败链；背包双撬棍手感
```

---

## 1. Flash / 固件体积

### 镜像身份

| 项 | 值 |
| --- | --- |
| 归档 | `build/firmware/308c21bd9c0bb74d4b06dc11e020f3765596d10f66911d7d18b2385a1183c86f/` |
| `full.bin` sha256 | `308c21bd9c0bb74d4b06dc11e020f3765596d10f66911d7d18b2385a1183c86f` |
| `app.bin` | **2,906,560** B（≈2.772 MiB） |
| 校验 | `python3 tools/archive_firmware.py verify` → **PASS** |
| 为何未重编本轮 | 与 11-07 保档刷机产物一致；审计只读复用 |

### 预算

factory `0x7F0000`（8,323,072 B）；占用 **34.9%**；**空闲 5,416,512 B（65.1%）**。

### 主要消耗（对照 `FLASH_SIZE_PLAN`）

| 桶 | 字节 | 说明 |
| --- | --- | --- |
| 精灵 ASR1（36 帧 RLE） | ≈1,316,600 | 与 encode `--check` / 既有 PIXEL PASS 一致 |
| 电台 PCM×5 | 247,884 | `assets/ashbox/sfx/radio_*.pcm` 合计 |
| 暖炉 PCM | 384,000 | `hearth_fire.pcm` |
| 字库 C 源 | 760,386（磁盘） | 链入 bitmap 仍远小于源文本 |
| 空闲 | >40% | 非紧急；P3 可选关 Montserrat 等仍成立 |

### 可做 / 禁做

**可做（无损）：** 体积报告桶校准；确认关 Montserrat-14；LVGL/printf 修剪。  
**禁止（除非当场改口）：** 砍景/砍帧、缩小 240×176、有损图、缩电台 PCM、为体积砍玩法档。

---

## 2. Bug 清单（按严重度）

图例：**证实** = 源码路径闭合；**假说** = 缺宿主断言或真机复现。

| ID | 严重度 | 状态 | 标题 | 证据 | 触发 | 影响 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| B11 | **Critical** | 证实 | USB 导入校验失败仍 `commit=true` | `ashbox_embackup.c` 182–196：`nvs_flash_init` / `ashbox_save_nvs_readable` 失败只改 `result=3`，仍 `return true` → 协议 `STAGED`+重启 | 导入写盘后 NVS 不可读或 init 失败 | 管家侧假成功；机内可能无有效档或下次新建 | `result!=1` 时 `return false`；勿自动 STAGED/重启 |
| B12 | **High** | 证实 | `day_key&0xFF` 日勤闩 256 天不刷新 | `sync_duty_day`：`today=(uint8_t)(day_key&0xFF)`（≈1253） | `day_key=257` 且第 1 天已做满日勤 | `duty_count` 不归零，日勤长期不可用 | 日闩改存完整 `day_key`（或扩字段+迁移） |
| B13 | **High** | 证实 | 贸易日闩低字节碰撞 | `trade_available` / `trade_day` 同用低 8 位（≈3442+） | 第 1 天换过 → `day_key=257` | 误判「今日已换」，铁棚/渡口日限异常约 255 天 | 同上，完整日序 |
| B14 | **High** | 证实 | `ember_trade_day==0` 被 clamp 成 `0xFF`，日限失效 | `clamp_economy`/`after_migrate`：`ember_trade_day==0 → 0xFF`（≈135、150）；成交写 `today&0xFF` | `day_key&0xFF==0` 日零件换余烬 | **同日可重复**换余烬；读档后日闩可丢 | 哨兵与合法日 0 分离；禁止把 0 日写成哨兵 |
| B15 | **High** | 证实 | 强制 flush 失败仍清 `s_save_dirty` | `ashbox_ui_flush_persistent` 2936–2939：第二次 `ashbox_save_store` 返回值忽略后无条件清 dirty | 导入前快照 / NVS 写失败 | 内存以为已落盘，USB 导出可能是旧档 | 仅 `ESP_OK` 清 dirty；失败向上失败 |
| B16 | **High** | 证实 | 过路客未进指纹 / `paint_home_focus_only` | `home_ui_fingerprint` 无 `wanderer_pending`（≈506+）；`paint_home_focus_only` 无接收/打发（≈3482+）；`on_nav_key` 的 `two` 亦缺 | 过路客上门后上下切焦点或静止 tick | 底键显示「出发」等与卡片脱节；易误导 | 指纹+focus_only+`two` 与 `paint_home` 同源 |
| B1 | **High** | 证实 | 背包携带上限未接到出发 UI | `activate_pick`：`crow=(crowbar>0)?1:0`；规则 cap 可达 2 | 装备背包后选「带铁条」 | 永远只带 1 根；副行「上限 2」误导 | `crow=min(cap,库存)`；补测 |
| B2 | **High** | 证实 | 战报/电台仍用「粮/件」玩家黑话 | game ≈2069–2080、3083；UI 过路客「耗20粮」；content `branch_effect` | 战报/电台/过路客 | 违反大白话硬轨 | 统一「口粮/零件」 |
| B17 | Med | 证实 | 块模态不拦上下键 | `s_block_modal` 仅 OK 关闭；UP/DOWN 仍 `on_nav_key` | 「无法进行」浮层时按上下 | 背后列表选中漂移，关层后误确认 | modal 开时跳过导航 |
| B18 | Med | 证实 | v2 CRC 失败静默头-only 迁移 | `ashbox_save.c` ver==2 全量布局 CRC 不等仍 `migrate_from_v1_head` | 损坏/位翻转的 v2 档 | 人口/远征/日记等静默丢失且无报错 | CRC 失败禁止头-only fallback |
| B3 | Med | 证实 | 选路「空手而归概率约 X%」≠ 实算 | `k_tier_loot_hint` vs `fail_chance` 修饰 | 阿禾/信任/猎装/带枪 | 玩家误判概率 | 标明「基础」或显示实算 |
| B4 | Med | 证实 | Modal 主按钮默认可滚动 + `get_child(0)` | `s_modal_btn` 未清 `SCROLLABLE` | 引导/阻挡 Modal | 字可能被滚成横条 | 同底键 layout |
| B5 | Med | 证实 | 携带步导航仍绑 `ASH_LOAD_COUNT` | `on_nav_key` step2；画面是轻装/铁条/手枪 | 碰巧都是 3 | 枚举再增则越界 | `GEAR_PICK_COUNT` |
| B19 | Med | 证实 | 空手+热伤战报文案自相矛盾 | 先写空壳行，热伤只盖一行；日志仍空手句（≈2187–2216） | 失败未死且热伤骰中 | 战报与纪事语义打架 | 统一空手+热伤话术 |
| B6 | Med | 证实 | 伤员不计劳动仍计耗水 | `labor_weight*` vs `count_present` | 负伤后 settle | 或为设计；否则偏弱产 | 产品确认 |
| B7 | Low | 证实 | 归来装备裸 `uint8` 加 | resolve 成功/失败归还路径 | 弹药近顶 | 理论回绕 | `sat_add_u8` |
| B8 | Low | 证实 | 停用携带档文案仍完整 | `k_load_hint` | 展示 loadout hint | 误导 | 改为已停用 |
| B20 | Low | 证实 | 导入后校验过浅 | `ashbox_save_nvs_readable` 只查键可读，不 `decode` | 垃圾 blob 仍「可读」 | 与 B11 叠加 → 重启后新建档 | commit 后跑 decode/CRC |
| B9 | Low | 假说 | 死亡后 `exp.person_id` 旧下标 | `remove_person_at` | 战报未入账 | UI 当前少读；`pick_branch` 有钳 | 置 `0xFF` |
| B10 | Low | 假说 | CRC 含结构尾填充 | `ash_game_crc` 全 `sizeof` | 填充不一致构造 | 理论不稳 | 哈希至 `offsetof(crc)` |

### 健康面（本轮未挖穿）

- 空手而归判定 `roll >= fail_p` 与「roll < fail_p 失败」语义一致；近路基础 5% 对齐文案基数。
- `ASHBOX_SAVE_VER=11` / `BLOB_SIZE=648` / migrator `offsetof` 静态断言齐全；`test_ashbox_save` 含 v10→11。
- 底键 `layout_btn_label` + 关 `SCROLLABLE` + `paint_pick` 先改宽后 `set_btn` 已落地（横杠类主路径）。
- 电池 I2C 在 LVGL 外采样；存档 `flush` 在解锁后 `flush_nvs_runtime_only`（对照旧 C-BAT/C-CB1）。
- 死枝脚本：branches=37 reachable=35，`system_only=[27,28]`（灰潮线系统枝）。

---

## 3. 整体代码优化（硬轨）

| ID | 优先级 | 证据 | 建议 | 体验/存档 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| O-EM | P0 | B11/B20 导入假成功 | **已修** commit 失败不 STAGED；decode/CRC | 可毁档体验 | `ashbox_embackup.c` |
| O-DAY | P0 | B12–B14 日闩低字节 | **已修** `day_latch_*` uint16 + v10→11 迁移 | 长跑经济/日勤 | `ashbox_game` + save |
| O-NVS | P0 | B15 flush dirty | **已修** 仅成功清 dirty | USB 备份可信 | `ashbox_ui_flush_persistent` |
| O-WAN | P0 | B16/B17 过路客/模态 | **已修** 指纹/focus/`two`；模态拦上下 | 主页误导 | `ashbox_ui.c` |
| O-B1 | P0 | B1 背包撬棍 | **已修** UI `min(cap,库存)` + 宿主测 | 玩法缺口 | `activate_pick` |
| O-B2 | P0 | B2 粮/件 | **已修** 口粮/零件 | 规范 | 文案 |
| O-MIG | P1 | B18 v2 CRC | **已修** 坏 CRC 禁止头迁移 | 旧档安全 | `ashbox_save.c` |
| O-B3 | P1 | B3/B19 概率与战报 | **已修**「基础」概率 + 空手热伤统一 | 无存档影响 | content/game |
| O-B4 | P1 | B4/B17 Modal | **已修** layout + 拦导航；B5 携带计数 | 显示/误触 | UI |
| O-TEST | P1 | day_key≥256、余烬日闩、背包 crow、导入失败 | **已修** 宿主断言 + v2 CRC | 锁回归 | `test_ashbox_*` |
| O-UI1 | P2 | UI 单体过大 | 按页拆 | 无存档 | 交易/装备先拆 |
| O-GAME1 | P2 | settle/resolve 巨函数 | static 拆 | ABI 稳则无档影响 | 失败归来先拆 |
| O-FAT | P2 | 空闲 65% | 可选关 Montserrat | 小体积 | defaults |

### 建议落地顺序

1. **O-EM**（导入假成功）→ 2. **O-DAY**（日闩）→ 3. **O-NVS** → 4. **O-WAN** → 5. **O-B1/O-B2** → 6. **O-MIG/O-TEST** → 其余 P1/P2

---

## 4. 子轨（已并入上表）

| 轨 | 说明 | 主代理复验 |
| --- | --- | --- |
| 规则层 | 并行只读审计（对话轨） | B12–B14、B19 源码闭合；日闩已修 |
| 存档/USB | 并行只读审计（对话轨） | B11、B15、B20 已修；B18 仍 P1 |
| UI 接线 | 并行只读审计（对话轨） | B16、B17 已修；底键横杠主路径维持 |

---

## 相关

[AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md) · [AUDIT_STRICT.zh_CN.md](./AUDIT_STRICT.zh_CN.md) · [AUDIT_CODE.zh_CN.md](./AUDIT_CODE.zh_CN.md) · [FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md) · [PROJECT.zh_CN.md](../../PROJECT.zh_CN.md) §玩家文案
