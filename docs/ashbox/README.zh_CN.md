**简体中文** · [English](./README.md)

# 余烬（Ashbox）

面向 FoloToy AI Passport 的离线末世放置游戏。

- 分支：`feature/ashbox-ember`
- 独立应用 UI（禁止沿用硬件测试 demo 菜单壳）
- 规则：`main/ashbox_game.c` + 内容表 `main/ashbox_content.c`（宿主单测）
- 存档：NVS 命名空间 `ashbox`、键 `save`，magic `ASH2` / `ASHBOX_SAVE_VER=13` blob 776（旧版可迁移；见 [EXTENSION.zh_CN.md](./EXTENSION.zh_CN.md) / [AUDIT.zh_CN.md](./AUDIT.zh_CN.md)）；元进度键 `meta` / `ASHM` — [META_PROGRESS.zh_CN.md](./META_PROGRESS.zh_CN.md)；项目约定见根目录 [`PROJECT.md`](../../PROJECT.md)
- USB 存档：设置页「备份存档 / 导入存档」实现 `!EMBACKUP`（与 [AI-Passport_BS 存档管家](https://github.com/Jiaqingrun/AI-Passport_BS) 对接；现行 ZIP：`save-manager-v1.0.1`）
- 下次社区更新说明清单（管家链接、`RESTORED 3`、命名空间）：[COMMUNITY_PUBLISH.zh_CN.md](./COMMUNITY_PUBLISH.zh_CN.md)
- 刷机：日常用保档脚本 `./tools/flash_ashbox_preserve_save.sh <口>`（只写 `0x10000`）；合并镜像 `0x0` 会清空存档，仅空白机或明确清档时用（见 [BUILD.zh_CN.md](./BUILD.zh_CN.md)）。**社区安装/更新须声明会清空进度，不是保档刷机。**
- 内容填充方案（审计 + Phase F / G）：[CONTENT_FILL.zh_CN.md](./CONTENT_FILL.zh_CN.md)
- 内容审计 Phase G（叙事 / UX）：[AUDIT_CONTENT_G.zh_CN.md](./AUDIT_CONTENT_G.zh_CN.md)
- 十轮严格审计（合规 / 内容 / 互动 / 延伸 / 玩法 / 代码 / 功耗 / 优化）：[AUDIT_10R.zh_CN.md](./AUDIT_10R.zh_CN.md)
- 底层代码审计（存档 ABI / 任务与锁 / 深睡 / 音频）：[AUDIT_CODE.zh_CN.md](./AUDIT_CODE.zh_CN.md)
- 画面 / UI / 布局 / 按钮 / 重叠 / 字库三轮审计：[AUDIT_UI_VISUAL.zh_CN.md](./AUDIT_UI_VISUAL.zh_CN.md)
- 家园布局锁（已定死）：[LAYOUT_LOCK.zh_CN.md](./LAYOUT_LOCK.zh_CN.md)
- Flash / 体积无损压缩方案：[FLASH_SIZE_PLAN.zh_CN.md](./FLASH_SIZE_PLAN.zh_CN.md)
- **审计硬轨**（每次审计必含 Flash 体积 + 整体代码优化 + 玩家文案版式；改后严格审计并当场修）：[AUDIT_REQUIREMENTS.zh_CN.md](./AUDIT_REQUIREMENTS.zh_CN.md) · 项目约定见 [`PROJECT.md`](../../PROJECT.md) · 宿主门禁 `tools/check_ashbox_copy_layout.py`
- 严格审计轮 2026-11-06（Flash + 整体代码，两项必做 PASS）：[AUDIT_STRICT.zh_CN.md](./AUDIT_STRICT.zh_CN.md)
- 游戏代码严格 Bug 审计 2026-11-07（Flash + 优化硬轨；bug 总评 FAIL）：[AUDIT_BUG.zh_CN.md](./AUDIT_BUG.zh_CN.md)
- 严格审计 2026-11-07（Flash + 代码 + **持久化**双槽/墙钟/站事落盘）：[AUDIT_20261107.zh_CN.md](./AUDIT_20261107.zh_CN.md) — 断电相关历史快照
- **全量严格审计 2026-11-08**（Flash + 代码 + 文案版式；SAVE_VER 13；栈溢出当场修）：[AUDIT_FULL_20261108.zh_CN.md](./AUDIT_FULL_20261108.zh_CN.md) — 现行审计快照

- 据点枢纽：日勤（醒着可玩）、菜地（两块田地·谷/薯）、仓库可扩容/仓家具、铁棚·渡口交易、线索；选档含猎场（需开闸：打开站门后才能走）
- 玩家可见规划说明：选路副行写明掉落区间/空手而归概率/稀有与补余烬；人选写明加成；建设底栏与「帮助」可翻阅概率、出处与建设效果
- 场景精灵：12 景 ×3 帧、240×176 末世像素；HUD（资源/广播/此刻）在场景下信息带，不压站台；顶/底仅留薄安全边
- 字体子集：新增中文后需用 `main/ashbox_font_symbols.txt` 重生 `assets/fonts/ashbox_font_16.c`

## 深度循环（仅醒着决策）

1. **出发**：选档（近路 / 废街 / 深处 / 猎场·需开闸 / 过夜）、选人（信任 / 疲劳 / 个性）、可选撬棍或手枪+子弹。旧「携带口粮/零件」档已停用。
2. **深睡或等待**：远征、菜地、井产水走墙钟；口粮消耗、零件产出、威胁上涨与**耗水**在深睡段乘 `sleep_factor=0.5`（产水不折）。空闲省电：约 60s 调暗并停电台/动画；站内约 180s 熄屏，**远征中不自动熄屏**（倒计时须可见）；远征中约 5 分钟、站内约 10 分钟无操作自动深睡（战报/危机不自动睡；唤醒按墙钟夹紧补算）。主页仍可长按确定立即入睡。
3. **归来分支**：settle 写入掉落与 `outcome_id` / 分支；未决分支跨深睡保留，仅醒着可选。
4. **站内危机**：威胁 ≥ 80 且无外出、无未决战报时强制守夜（交涉 / 加固 / 躲藏）。在站即可躲或谈；断粮虚弱时仍半产口粮，避免死锁。
5. **调查线**：`铁皮来客`、`余烬低语`、`门后灰潮`，各五步；收束分支已入 outcome/标签池，守夜可在末步直接了结铁皮线；灰潮线挂开闸/铁棚/换种/种菜。
6. **广播日志**：环形纪事；远征途中优先播途中句，否则播最新日志。
7. **过夜站令**：深睡前设定（节食 / 戒备 / 赶工），只影响深睡段产耗。
8. **归来遭遇标签**：成功归来抽取标签，偏向醒着可选的分支池。

建筑升级保留代价：宿舍 L3 歇得更快且不加口粮耗、工坊增产加快威胁、食堂降耗、闸门增强掉落并让加固更便宜。升井只暂停井产水，站内其它行动仍可用。
