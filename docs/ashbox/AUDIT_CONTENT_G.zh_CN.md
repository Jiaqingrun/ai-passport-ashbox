**简体中文** · [English](./AUDIT_CONTENT_G.md)

# 《余烬》内容审计 · Phase G（叙事 / UX）

审计日期：2026-11-06（Asia/Shanghai）。  
范围：玩法文案与三键信息架构；**不扩**营地/货币/第二时钟。  
基线：F0–F5 已合入；对照 [CONTENT_FILL.zh_CN.md](./CONTENT_FILL.zh_CN.md)、[AUDIT_10R.zh_CN.md](./AUDIT_10R.zh_CN.md)。

## 库存（现码）

| 项 | 量 |
| --- | ---: |
| 分支 | 37（可达 35；系统枝 27/28） |
| Outcome / 标签 / away | 13 / 12 / 40 |
| 电台 idle / lore / twist / 守望 | 20 / 12 / 8 / ≥8 |
| 帮助 | 23 |
| 调查线 | 3×5 步 |

## 本轮问题 N01–N11

| ID | 级 | 问题 | 处置（Phase G） |
| --- | --- | --- | --- |
| N01 | High | 枢纽隐藏 step0 的 q>0 线名 | **G1** 具名显示 |
| N02 | High | 深处成功 quest1 同趟 0→2 | **G2** 拆双 `try_advance` |
| N03 | High | 三线了结后叙事薄 | **G4** 守望句池 |
| N04 | Medium | 抉择后战报 L1 纯会计 | **G5** L1 场面 / L2 账 |
| N05 | Medium | 灰潮 27/28 隐性教程 | **G6** 帮助+线索+交易文案 |
| N06 | Medium | 帮助缺望值/灰片路径 | **G7** 补教 |
| N07 | Medium | 迁移 catch-up 无说明 | **G3** 补记纪事 |
| N08 | Low | 站事 hint/log 不随档 | **G8** |
| N09 | Low | 危机 disabled 文案不准 | **G9** |
| N10 | Info | 真机全流程未闭环 | 验收清单 |
| N11 | Info | CONTENT_FILL 旧快照 | 已刷新 §1 / Phase G |

## Phase G 切片

| 切片 | 状态 |
| --- | --- |
| G1 枢纽线索 | ✅ 已合入 |
| G2 余烬步进 | ✅ 已合入 + 宿主测 |
| G3 里程碑纪事 | ✅ 已合入 |
| G4 守望层 | ✅ 已合入 + 宿主测 |
| G5–G9 文案 | ✅ 已合入；字形门禁 PASS |

**门禁：** static PASS；Device tests：未连线（待抽验 G1/G2/G4）。

**不做：** 第四调查线、新货币/营地、改掉率、无接线幽灵枝。
