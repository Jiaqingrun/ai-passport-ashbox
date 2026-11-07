**简体中文** · [English](./AUDIT_REQUIREMENTS.md)

# 《余烬》审计必查要求（常设）

**权威：** [`PROJECT.md`](../../PROJECT.md)「审计硬轨」。本文是操作清单。冲突时：`PROJECT.md` > 本清单 > `docs/archive/` 历史审计文（本规则尚无归档例外）。

本仓库凡称**审计**的任务（十轮 / 代码 / 内容 / UI 视觉 / 发布前 / 对话中明确叫审计的复查），交付物**必须**包含下面 **1–3** 节；缺任一节视为未完成。

凡会话中改了《余烬》产品代码／UI／玩家文案／帮助／设置，交付前 Agent **必须**做一次**改后严格审计**：跑下方宿主门禁、按适用行逐项过本清单，并把 FAIL **在同一次改动里修掉**——禁止把已知换行遮挡、截断或门禁失败留到「下次专门审计」。

## 1. Flash / 固件体积

| 检查 | 通过标准 |
| --- | --- |
| 镜像身份 | 记录近期 `./tools/validate.sh --firmware` 已验证镜像的路径/SHA（或写明沿用旧镜像及未重测原因） |
| 预算 | factory 分区 vs `app.bin`（或等价）及余量百分比 |
| 主要占用 | 至少拆开：精灵、字库、PCM/音效、代码/rodata |
| 方案对照 | 对照 [`FLASH_SIZE_PLAN.zh_CN.md`](./FLASH_SIZE_PLAN.zh_CN.md)，注明漂移 |
| 可做 / 禁做 | 列出无损优化机会；重申禁令（砍景/砍帧、缩小 240×176、有损图、缩电台内容），除非用户当场改口 |

## 2. 整体代码优化

| 检查 | 通过标准 |
| --- | --- |
| 范围 | 规则/UI 边界、过大或热点单元、重复逻辑、可宿主测未测路径、锁与任务、存档 ABI、死代码/死枝、构建脂肪 |
| 证据 | 每条有文件/符号/指标或测试缺口 |
| 可执行 | 每条含建议、是否改体验/存档、优先级；至少一条可立刻跟进的下一步 |
| 禁空话 | 禁止无目标的「建议重构」 |

## 3. 玩家文案版式（禁止遮挡 / 禁止静默截断）

操作说明类界面——帮助「怎么玩」、开局引导／首次提示／拦截弹窗，以及其他多句说明——在 240×320 上必须读得完。

| 检查 | 通过标准 |
| --- | --- |
| 帮助换行 | `paint_help` 使用 `LV_LABEL_LONG_MODE_WRAP`，行高够用；**禁止**只靠 CLIP 或跑马灯才能看完一句帮助 |
| 禁小缓冲 | 帮助（及其他说明绘制）禁止把源文 `snprintf` 进过短缓冲（截断即 FAIL） |
| 弹窗换行 | 弹窗标题/正文定宽 + WRAP，长引导／提示／原因不被卡片边缘裁掉 |
| 底提示条 | `s_hint` 必须定宽；布局锁下提示带约一行高，用 SCROLL_CIRCULAR（或改短文案），禁止无宽 CLIP |
| 宿主门禁 | `python3 tools/check_ashbox_copy_layout.py` PASS（已挂进 `./tools/validate.sh --ashbox`） |
| 用语表 / 一名一物 | `python3 tools/check_ashbox_glossary.py` PASS；玩家串遵守 `PROJECT.zh_CN.md` 正式名（禁短火/铁条/工作站/墙钟/水桶等别名；仓位说明须白话算账） |
| 当场修 | 本节 FAIL 必须在交付前修完；不允许「留给下次审计」处理换行遮挡／截断／用语 |

## 4. 文首声明（推荐）

```text
审计硬轨：Flash 体积 = PASS|FAIL|DEFERRED(原因)
审计硬轨：代码优化 = PASS|FAIL|DEFERRED(原因)
审计硬轨：文案版式 = PASS|FAIL|DEFERRED(原因)
```

仅在测量受阻（如无 IDF 环境量 Flash）时用 `DEFERRED`；只要改了 UI／文案，文案版式宿主门禁**不得**延期。延期时仍须写清定性风险与解锁步骤。

## 相关

- 体积方案：[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)
- 十轮审计：[AUDIT_10R.zh_CN.md](./AUDIT_10R.zh_CN.md)
- 底层代码：[AUDIT_CODE.zh_CN.md](./AUDIT_CODE.zh_CN.md)
- 产品索引：[README.zh_CN.md](./README.zh_CN.md)
