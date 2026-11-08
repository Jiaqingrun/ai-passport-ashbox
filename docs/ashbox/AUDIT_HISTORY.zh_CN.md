**简体中文** · [English](./AUDIT_HISTORY.md)

# 审计历史（已归档快照）

每次已归档审计一行。这些审计沉淀下来的规则在
[AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md)（检查清单）、[`PROJECT.zh_CN.md`](../../PROJECT.zh_CN.md)
（审计硬轨）、[LAYOUT_LOCK.zh_CN.md](./LAYOUT_LOCK.zh_CN.md)、[EXTENSION.zh_CN.md](./EXTENSION.zh_CN.md)（存档 ABI）
与 [FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)；当前待办发现见
[AUDIT_GAMEPLAY_20261108.zh_CN.md](./AUDIT_GAMEPLAY_20261108.zh_CN.md)，方案见
[OPTIMIZE_PLAN_20261108.zh_CN.md](./OPTIMIZE_PLAN_20261108.zh_CN.md)。每份快照原文留在 Git 历史里。

| 快照 | 日期 | 范围 | 当时结论 | 留下的东西 |
| --- | --- | --- | --- | --- |
| 玩法／生命周期／合规 | 2026-11-05 | 放置循环、仓储、回访钩子 | 发现已修 | 菜地独立成页、仓库档位与食堂解耦、成本曲线 |
| 十轮严格审计 | 2026-11-05 | 合规·内容·互动·延伸·玩法·代码·功耗·优化 | 有 FAIL，当场修 | 「常设审计门禁」的概念 |
| 底层代码审计 | 2026-11-05 | 存档 ABI·任务与锁·深睡·音频 | 发现已修 | 锁序 `runtime → LVGL`；规则层不得出现 LVGL |
| 严格审计（Flash + 整体代码） | 2026-11-06 | 体积预算与热点单元 | PASS | 与体积方案对齐、每次审计都量 Flash |
| 画面／UI／布局／字库 | 2026-11-06 | 面板几何、精灵、字形 | FAIL，随后修复 | 字库重生成、此刻带分栏、列表视口裁剪、弹窗边界 |
| 内容 Phase G（叙事／UX） | 2026-11-06 | 文案与信息架构 | 11 条发现，全部修复 | 线索行具名、任务推进拆分、守夜电台池 |
| 持久化 | 2026-11-07 | 双槽存档、CRC 封印、墙钟回退 | 发现已修 | 墙钟不回拨；站事后立即落盘 |
| 游戏代码 Bug | 2026-11-07 | 规则·内容·存档·备份·UI 接线 | FAIL，当场修 | 规则核心的一批修复；不新增模块 |
| 全量严格审计 | 2026-11-08 | Flash·代码·文案版式·存档 v13·栈 | 必做项 PASS | `ASHBOX_SAVE_VER=13`、冻结 v12 夹具、栈溢出当场修 |
| 严格审计第二轮 | 2026-11-08 | 同硬轨、ABI 未变、归档保留 | PASS | `build/firmware/<sha256>/` 归档保留规则 |

这些轮次得出的通用做法：先跑宿主门禁；每次审计都保留必做三节；FAIL 在同一次改动内修完，不留给下一次审计。
