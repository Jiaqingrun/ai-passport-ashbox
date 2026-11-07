**简体中文** · [English](./AUDIT_UI_VISUAL.md)

# 画面 / UI / 布局 / 按钮 / 重叠 / 字库 — 三轮严格审计

- **日期**：2026-11-06（Asia/Shanghai）
- **方式**：主代理几何/门禁核对 + 三路并行只读子代理（布局、精灵与刷新、字库）
- **范围**：家园重构后现状（240×152 场景 + 此刻带）
- **总评（审计时）**：**FAIL**；**修复进度（2026-11-06）**：P0/P1 与主要 P2 已落地（字库重生、此刻分区、timer 指纹、列表 clip/视窗、顶条/次键/Modal）

---

## 总览

| 轮次 | 主题 | 结论 |
|------|------|------|
| R1 | 布局 / 重叠 / 功能按钮 / 层级 | **FAIL** |
| R2 | 画面尺寸 / 刷新路径 / 功能态 | **FAIL（有条件）** |
| R3 | 字库 / 缺字 / 占位 | **FAIL** |

| 维度 | 结论 |
|------|------|
| 精灵 C / PNG / encode / STAGE_H | **PASS**（均为 240×152） |
| 家园骨架 14 / 194 / 248 / 266 | **PASS** |
| home ↔ list 互斥 | **PASS** |
| 列表页相对 194 内容区 | **FAIL**（多页溢出） |
| 此刻带字叠字 | **FAIL** |
| timer 增量刷新 vs 五态卡片 | **FAIL** |
| 中文字形门禁与位图 | **FAIL**（「此」「眼」） |

```text
Build: NOT RUN（本审计）
Host tests: glyph 脚本 FAIL；其余 static 未因本审计单独重跑全量
Device tests: NOT RUN（需补「此刻」与列表溢出真机点验）
```

---

## 第 1 轮 — 布局 / 重叠 / 按钮

### 骨架（符合约定）

顶条 y=0 h=14；`s_home_layer`/`s_list_layer` y=14 h=194；场景 152 + 缝 2 + 此刻带 40；hint y=248；底键 y=266；`show_home_chrome` 互斥正确。

### 问题

| ID | 严重度 | 问题 | 证据 | 建议 |
|----|--------|------|------|------|
| A1 | High | 设置列表铺满后底边超出 194 | `paint_settings` `20+i*36`，6 项底≈234 | 视窗滚动或压行高；list 层 clip |
| A2 | High | 纪事多条时溢出进 hint | `paint_journal` 至 8 条 | 分页/滚动 |
| A3 | High | 此刻带 title/value 无固定宽，长标题盖住主状态 | title x=42、value x=78；「站内危机」「远征中」 | title CLIP+定宽，或 value 右移 |
| A4–A9 | Med | pick/crisis/quest/order/help/build/farm/trade 行距未按 194 约束 | 各 `paint_*` 行距 48–56 | 统一 viewport；farm/trade 满项必滚 |
| A10 | Med | Modal h=200 居中压到 hint | `s_modal` + hint y=248 | Modal 时藏 hint 或缩小 Modal |
| A11 | Low | 资源三字与右对齐 status 在高位数时可能挤 | 叠层 y=`STAGE_H-34` | status CLIP |
| A13 | Med | 顶条 h=14 < 字号 16，标签 y=-1 易裁切 | `build_screen` hdr | 顶条≥18 或字号/位置下移 |
| A14 | High | `card_value` 宽与 `card_sub` 同排重叠约 x=130–150 | `CARD_VALUE_W=72` 起 78；`CARD_SUB_X=130` | sub 起点≥ value 右缘+间隙 |
| B1 | High | 次键宽 68，「确认覆盖」易裁切 | `paint_settings_confirm` | 加宽次键或缩短文案 + CLIP |
| B2 | Med | 禁用按钮无 LVGL DISABLED，靠逻辑挡 | `set_btn` | 禁焦点或禁 OK |
| C1 | Med | Modal 不由 `show_home_chrome` 统一关闭 | 依赖各 paint | `enter_page` 默认关 Modal |
| D1 | Med | 滚动条按 `HUB_ROW_H=34` 算，建设轨高可 >194 | `paint_hub_scrollbar` | 按真实行高/层高裁切 |

**R1 结论：FAIL**

---

## 第 2 轮 — 画面 / 刷新 / 功能

### 通过项

- PNG / `ashbox_sprites.c` header / encode / `STAGE_H` 均为 **240×152**；12×3 帧齐全；`get_frame` 边界安全。
- 完整 `paint_home()` 五态（危机/战报/远征中/禁止出发/可出发）与按钮、电台、场景 API 对齐。
- 省电停动画安全。

### 问题

| ID | 严重度 | 问题 | 证据 | 建议 |
|----|--------|------|------|------|
| P1 | High | 主页多数 tick 只走 `paint_home_timers_only`，此刻标题/副文/按钮/status 不随 settle 变 | `ashbox_ui_tick` → timers_only | 状态指纹变化时调完整 `paint_home` |
| P2 | Med | 威胁跨 80 时 timer 路径不切换 `s_threat_lbl` 显隐 | timers_only 只改文案 | 同步显隐 |
| P3 | Med | `s_status` 在 timer 路径不刷新 | 断粮等未必 `need_wake` | 同 P1 |
| P4 | Low | radio 缓存 63 字节前缀比较 | `set_radio_text` | 全串比较（现网句长足够） |
| P5 | Low | 空电台仍滚 `"  ·  "` | `set_radio_text("")` | 空则清空/隐藏 |
| P6 | Low | 场景切换未 force 时帧相位可能不归零 | `tick_sprite_anim` | 换景 reset 帧 |

**R2 结论：FAIL（有条件）** — 资源链 PASS；运行时增量刷新 FAIL。

---

## 第 3 轮 — 字库

| ID | 严重度 | 问题 | 证据 | 建议 |
|----|--------|------|------|------|
| F01 | Critical | 门禁缺「此」；主页标签「此刻」 | `check_ashbox_font_glyphs.py`；`ashbox_ui.c` | symbols 加「此」并重生成字体 |
| F02 | High | 位图无「此」 | `ashbox_font_16.c` 无 U+6B64 | 同 F01 |
| F03 | Med | symbols 有「眼」但位图未含 | txt 末字 vs font 807 字 | 重跑 lv_font_conv 对齐 |
| F04 | Med | 守夜危机副文「眼下…」缺字 | `paint_crisis` | 同 F03 |
| F05 | Low | 校验只对 txt，不对 .c | 脚本设计 | 增加 symbols ⊆ bitmap 断言 |

动态拼接为闭集；Ashbox 已统一 `ashbox_font_16`，无 Montserrat 中文回退路径。缺字表现为占位方框。

**R3 结论：FAIL**

---

## 修复落地（2026-11-06）

1. **P0 已做**：symbols 补「此」；重生 `ashbox_font_16.c`（含「眼」）；glyph 脚本兼查位图
2. **P0 已做**：此刻带 title/value/sub 分区定宽 + CLIP（A3/A14）
3. **P1 已做**：`home_ui_fingerprint` 变化时全量 `paint_home`；timer 路径同步威胁显隐（P1–P3）
4. **P1 已做**：list 层裁切；设置/纪事/建设/帮助/菜地/贸易视窗化；多页行距压入 194
5. **P2 已做**：顶条 h=18；次键文案「覆盖」+ CLIP；Modal 缩高并藏 hint；滚动条轨高钳制

---

## 审计方式备注

三路并行只读子代理覆盖布局、精灵刷新、字库；结论已并入上文问题表与修复落地节。
