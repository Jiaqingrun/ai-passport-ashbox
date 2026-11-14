[English](./AUDIT_CODE_20261112.md) · **简体中文**

# 代码审计 — 2026-11-12（HEAD `0dfba4c`）

范围：余烬/Ashbox 固件当前实现，源码为主，附固件镜像核对。方法：先跑仓库门禁，再把源码切成
十一片只读审计（`ashbox_game.c` ×3、`ashbox_ui.c` ×3、存档/meta/设置、内容/掉落、运行时/休眠/音频、
备份/demo、UI 子页），每条结论附 `文件:行号`。上一轮的
[`AUDIT_GAMEPLAY_20261108.md`](./AUDIT_GAMEPLAY_20261108.md) 中发现的问题在 HEAD 上逐条复核，不再重复列举。

环境：macOS 主机、未激活 ESP-IDF（因此未重新编译）、复用镜像（见 1.1）。
标记：**[V]** = 主审已重读并确认证据；**[S]** = 分片报告，已给出证据位置但未逐行复核。

## 1. 强制项：Flash / 固件尺寸 — PASS

| 检查 | 结果 |
| --- | --- |
| 镜像身份 | 复用 `build/firmware/8a17a64a…/`（app.bin sha256 `8a17a64a…`，整片镜像 `669a48…`/`8a17a64a…`）；镜像内 `version = 0dfba4c` 与 HEAD 一致，`idf_ver = v5.5.3`，构建时间 Nov 9 2026 13:34。未重编原因：无激活的 ESP-IDF，且没有任何源码比镜像更新（`main/*.c` 最新为 11-08 23:16，HEAD 为纯文档提交）。 |
| 预算 | app.bin 2,920,608 B / factory 8,323,072 B = **占用 35.1 %，剩余 64.9 %** |
| 主要占用 | sprites ASR1 1,316,600 B（1.256 MiB）；PCM 631,884 B（炉火 384,000 + 电台 ×5 247,884）；字形位图 106,480 B；`.flash.text` 642,352 B；`.flash.rodata` 2,195,316 B；`.dram0.bss` 147,792 B |
| 计划对齐 | 与 [`FLASH_SIZE_PLAN.md`](./FLASH_SIZE_PLAN.md) 一致；自 11-08 以来未新增 rodata |
| 禁止项重申 | 不删场景/帧、不缩小舞台、不做有损美术、不缩短电台内容 |
| 观察 | 同一提交存在两份归档（`669a48…`、`8a17a64a…`），大小相同但 sha256 不同 —— 构建不可比特复现（内嵌时间戳）。归档应按构建记录，不能用大小推定。 |

## 2. 强制项：全库优化 — PASS（问题见下）

主机门禁（`./tools/validate.sh --static`）：**PASS** —— 仓库检查（433 个文件）、字体字形、
死分支（61/61 可达）、文案布局、术语表、精灵图（36 张 240×176，RLE 省 1,724,680 B）、三个 Ashbox
主机测试、按键守卫、电池、按键、LVGL 初始化、音频、低功耗、BLE/Wi-Fi、布局与 Python 契约测试。

覆盖缺口：`main/demo_*.c`（9 个文件、约 1,100 行）**没有**被 `main/CMakeLists.txt` 编译，
但 `docs/README.md` 与 `tests/test_deep_sleep_contract.py:56` 把它当作已交付能力 —— 死代码被当作功能描述 **[S]**。

## 3. 强制项：玩家可见文案布局 — PASS，但存在文案内容缺陷

`check_ashbox_copy_layout.py` PASS（帮助 94 行、3 行视口、行高 68/70、标签宽 196），
`check_ashbox_glossary.py` PASS，帮助使用 `LV_LABEL_LONG_MODE_WRAP`。审计到的界面未见换行/裁切/截断回归。
一条文案**内容**错误（NV10）与一处定长缓冲越界（NV19）见下；布局门禁两者都抓不到。

## 4. 新发现

### 4.1 S1 — 内存安全、锁纪律、不可逆确认

| ID | 问题 | 证据 |
| --- | --- | --- |
| NV1 **[V]** | 交易页把 22 行写进 16 行栈数组。`k_camp_trades` = 13 + `k_ferry_trades` = 8 + 潮汐 1 = 22，但 `titles[ASH_LIST_MAX][40]`、`subs[ASH_LIST_MAX][56]`、`enough_a[ASH_LIST_MAX]` 且 `ASH_LIST_MAX 16`，填充循环是 `for (i = 0; i < n; i++)`。两个营地都开放的后期会踩坏栈。 | `ashbox_ui_trade.c:85-88`、`:29-32`；`ashbox_ui_page.h:19` |
| NV2 **[V]** | 应用任务在未持 `bsp_lvgl_lock()` 时写 LVGL。`ashbox_ui_tick()` 由 `main.c:270` 调用，内部 `maybe_show_meta_toasts()` 运行在 `:3452` 加锁**之前**，并对弹窗行调用 `lv_label_set_text`。本文件的约定是 `runtime → LVGL`，`ashbox_ui_on_key` 也加锁（`:3169`），唯独 tick 路径没有。 | `ashbox_ui.c:3406`、`:2914`、`:2936`、`:3452`；`main.c:270` |
| NV3 **[V]** | 超级核心的确认永远确认不了，且「已武装」标记能跨页面存活。武装时置 `s_core_confirm = true; s_block_modal = true` 并显示「长按取消 · 再确定才用」，但下一次确定会落进 block-modal 分支，而该分支把已武装的 `s_core_confirm` 当作**取消**。长按确定在它之前被处理并调用 `go_back()`，`enter_page()` 只隐藏弹窗、不清标记；此后 `activate_build` 能在标记仍在时进入，于是未确认的一次确定就吃掉这个全程一次的核心。流放确认（`:2809-2829`、`:3298-3300`）是同一套反向协议。 | `ashbox_ui.c:2642-2663`、`:3289-3305`、`:3230-3262`、`:2186-2191` |

### 4.2 S2 — 经济、状态机、生命周期

| ID | 问题 | 证据 |
| --- | --- | --- |
| NV4 **[V]** | 顶档破门惩罚在**每个**危机选项后都执行，包括已经付过零件、已写「闸门扛住了」的加固：band 3 时又把威胁压到 79、再扣一个零件、加 10 疲劳并写「闸门被顶开 · 门扣坏了」。`door_bolt` 永不清除，门扣的 −1 上涨 / −1 加固成本一直有效。 | `ashbox_game.c:3353`、`:3380-3385`、`:3416-3425` |
| NV5 **[V]** | 交易只校验付出侧，产出满了也照扣：`ash_game_trade_can_pay` 只测付出侧，`ash_game_give_water/parts/…` 的返回值被 `(void)` 丢弃，水/零件/物品到顶时食物、余烬、铁凭空消失。同文件的合成则在到顶时拒绝。 | `ashbox_game.c:4034-4099`、`:604-623`、`:4275` |
| NV6 **[V]** | 水的日配额一天重置**两次**：`tick_water_economy` 在 UTC 零点重置，`ash_game_note_wall_time` 又在 UTC+8 零点重置，一天最多可拿到两倍日配额。另外 `seed_water_made_today` 会把「水源存在之前」已过的小时数计入，导致 UTC 零点之后建成的坑/井一直不产水，却照写「开始渗水」。 | `ashbox_game.c:918`、`:2825-2830`、`:830-840`、`:940` |
| NV7 **[V]** | `hide_penalty_h` 与 `crisis.reserved` 用 `dt_s / 3600` 递减，而醒着的 tick 只带约 1 秒，于是 4 小时的躲藏惩罚在一次会话内永不消退（只有跨睡眠才减），首页状态一直显示「躲藏约4时」。 | `ashbox_game.c:2610-2617`、`:2626-2630`；`ashbox_ui.c:1071` |
| NV8 **[V]** | 1 秒级的醒着结算仍会把口粮/零件产出与饮水消耗向下取整为 0（`* 4 / 3600`、`pop * consume_s / 7200`），玩家盯着屏幕时既不产出也不消耗。威胁值为此加了余数累加器，这三处没有。 | `ashbox_game.c:2533-2546`、`:890`、`:2604-2609` |
| NV9 **[V]** | 只有水掏空的口袋不会自动回补：`apply_economy` 仅在 `food == 0 && stash_food > 0` 时调 `ash_game_try_pocket_refill`（该函数其实也补水）；随后 `:2578` 把全员打成 WEAK，仓库里还有水也发不了队。 | `ashbox_game.c:2562`、`:540-552`、`:1129-1131`、`:2578` |
| NV10 **[V]** | 帮助文案仍是改数值前的内容：「稻谷约1天基础30口粮 · 红薯约3天基础240」，而常量是稻谷基础 45、红薯基础 150、红薯生长 172,800 秒（2 天）。P1-3 改了代码没改文案。 | `ashbox_help_copy.c:61`；`ashbox_game.h:34-35`；`ashbox_game.c:48-49` |
| NV11 **[S]** | 出发选择页显示的是固定槽位性格，不是队员真实的工作特质；掉落用的是真实特质，预览会误导。 | `ashbox_content.c:187`、`ashbox_game.c:298`、`ashbox_ui_pick.c:76`、`ashbox_game_loot.c:279` |
| NV12 **[S]** | 备份生命周期的三个洞：`ST_EXPORT_SEND` 没有超时（链路卡住会一直 busy 到重连）、主机 PULL 备份不先冲刷内存存档、导入提交时在 15 秒自动存档仍在写的情况下 deinit/擦除 NVS。 | `ashbox_embackup_proto.c:467`、`:301-307`；`ashbox_embackup.c:284`、`:162-164`；`ashbox_ui.c:2181` |
| NV13 **[S]** | 读档不比较槽位新旧：可解但更旧的主槽会压过更新的备份槽。旧版修补在现行版本读档时也会重跑，于是每次重启都重新送 `water = 40`、把 ≤600 秒的受伤重置成 24 小时。 | `ashbox_save.c:1390`、`:1192-1200`；`ashbox_game.c:201-203`、`:223` |
| NV14 **[S]** | 归来报告到达时无条件 `s_list_sel = -1`，与当前页面无关；在装备页 `s_list_sel / 2` 把 −1 映射成 0 号人，一次误按会翻错人的背包。 | `ashbox_ui.c:3420`、`:2391` |
| NV15 **[S]** | 通关/灭站提示调了 `set_modal_visible(true)` 却没置 `s_block_modal`，下层页面继续收按键，提示会被下一次重绘抹掉。 | `ashbox_ui.c:2913`、`:2935`、`:3289` |
| NV16 **[V]** | 开机时若 LVGL 锁超时，`ashbox_ui_start` 在 `build_screen()` 前就返回，而 `s_page` 已是 `UI_HOME`；下一次 tick 会往 NULL 控件上画（`lv_obj_clear_flag(s_water, …)`）。既没有重试也没有 `s_ui_built` 守卫。 | `ashbox_ui.c:2048`、`:2079`、`:3452-3458`、`:120` |
| NV17 **[S]** | 布局上限：人员选择页按 `12 + i * 52` 排，第 5 行落在 y=272，超出 228 px 裁剪层（该步没有滚动）；农场页循环 `plot < ASHBOX_FARM_PLOTS`（2），可超级核心解锁的是 4 块地。 | `ashbox_ui_pick.c:96`；`ashbox_ui_farm.c:32`；`ashbox_game.c:4748` |
| NV18 **[S]** | 威胁徽标右对齐压在铁的格子上，威胁 ≥ 80 时「铁%u」正好被盖住。 | `ashbox_ui.c:1907`、`:1899-1900`、`:1046` |
| NV19 **[V]** | 四效果分支文案需要超过 40 B，却渲染进 `char effect[40]`，威胁词被从中间切断（分支 20）。 | `ashbox_content.c:238`；`ashbox_ui.c:1697` |
| NV20 **[S]** | 危机页预选第 0 行，一次确定即执行；归来报告页正是为同样的原因故意从 −1 起。 | `ashbox_ui.c:2227`、`:2893` |

### 4.3 S3 — 死状态、反馈、测试缺口

`report_waste_flags` 只写不读（`main/`、`tests/` 都没有读者）；`clear_count` 只在成就解锁时自增、
从不被读取；meta 解锁返回未掩码的位（`ashbox_meta.c:96`），掩码外的位会永远重复提示；
`ASHBOX_PACE_V13` 下 `wanderer_next_s` 的写入是死代码、`ash_game_wanderer_next_wake_s` 无调用者；
`note_wipeout_reject` 里留有空 `if` 块；人员槽位左移时 `exp.person_id` 不重映射，分支信任会加到
无关的人身上；三条药品日志超过 `ASHBOX_JOURNAL_LEN`，尾部留下悬空 UTF-8 首字节；第二把撬棍只是
重置收获种子；吉祥物连按跳跃会持续上飘；`ui_pixel_math.c` 只有测试在用；帮助的长按分节跳转不可达
（`ashbox_ui.c:3194-3213` 先把事件吃掉，`:3217` 永远进不去）；`ashbox_save.c:1193` 在 CRC 校验前就
把数据 memcpy 进调用者的结构；救援扫描只白名单 v10+，而解码本可迁移 v3 **[S]**。

## 5. 上一轮发现在 HEAD 上的状态

| 状态 | 编号 |
| --- | --- |
| 已修复 | C01、C02、C03、C04、C06、C12、C19、C20、C22、C23、C25、C28、C29、C31、C32、C33、C34 |
| 部分修复 | C07（费用在确认时扣、未在选行时冻结）、C08（生死投点仍无人物项）、C09（余烬已可在营地兑换；终生消耗仍是 17）、C11（结果 7/8 已按状态路由；装备仍是恒 NONE）、C14（躲藏加强，但惩罚不消退 → NV7）、C15（档位数值会升级；文案与选项仍每次相同）、C17（日界已改本地零点；加速步价格仍不显示）、C18（弧线会重开；`clear_count` 无意义）、C24（子页闲置时仍不刷新）、C26（有出口了；自动睡眠仍被挡 → 见 `power_blocks_autosleep`）、C27（交易页仍不显示库存）、C30（帮助有分节，但跳转不可达） |
| 仍开放 | C05（余烬为 1 时开闸 ×1.2 仍是空操作）、C13（后期投入仍回不了本） |
| 有争议 | C16 —— 11-08 的数值（30/240）已改成 45/150，引用行号不再成立；剩下的不对称（同样种子价下红薯约 75 vs 稻谷 45 口粮/天）属数值取舍，不是缺陷 |

## 6. 未验证项 / 需上机

- 全部 S1/S2 均为源码级确认，没有一条在真机复现。NV3、NV6、NV9、NV15、NV18 与下面的功耗规则需要走屏验证。
- `power_blocks_autosleep()` 对 `UI_REPORT`/`UI_CRISIS` 返回 true，而它自己的注释说的是相反的意思，
  结果是停在危机页会挡住 300/600 秒的深睡请求（只黑屏不休眠）。两者必有一错，修前请上机确认 **[V]**
  （`ashbox_ui.c:851-859`、`:886-889`）。
- 建议补的主机测试（目前没有）：跨本地零点的水日配额；1 秒 tick 的产出/消耗表；产出到顶时交易被拒；
  band 3 加固的日志；超级核心与流放确认的完整往返。

## 7. 修复轮（2026-11-12，当日）

只改源码，不动存档 ABI（`ASHBOX_SAVE_VER` 仍为 13，`sizeof(ash_game_t) == 776`，pad 未动）。

| 编号 | 改动 |
| --- | --- |
| NV1 | 交易页只物化可见窗口（`TRADE_VIEW` 行），22 行不再去索引 16 格数组。 |
| NV2 | `maybe_show_meta_toasts()` 移进 tick 的 `bsp_lvgl_lock()` 区块，且放在页面重绘之后，应用任务不再无锁写 LVGL。 |
| NV3 | 第二次确定现在真的执行确认并消耗核心；长按退出与 `enter_page()` 都会清 `s_core_confirm`/`s_exile_confirm`，武装状态不会跨页面存活。 |
| NV4 | 原结论需修正：band 3 本来就是**加固必失败**（P1-4 设计），所以二次扣费是设计如此，原测试成立。真正的缺陷是自相矛盾的日志：band 3 现在提示「锁扣敲死 · 门被顶开」而不是「闸门扛住了」，并且真的清掉 `door_bolt`，让文案与状态一致。 |
| NV5 | `ash_game_trade_can_pay` 增加产出侧空间校验（水、零件、口粮、铁、余烬、种子），产出到顶时不再白白吞掉投入。 |
| NV6 | 水的日界统一到本地（UTC+8）零点：生产与播种都用它，`ash_game_note_wall_time` 不再第二次清零配额。 |
| NV7 | 小时级计时器（`crisis.reserved`、`hide_penalty_h`）按真实小时边界递减，醒着玩也会到期。 |
| NV9 | 口袋回补改为「粮或水任一为空」即触发，不再只看粮。 |
| NV10 | 帮助里菜地产量改正为 45 / 150、两天。 |
| NV15 | 通关/灭站提示会置 `s_block_modal`、占住屏幕直到关闭，也不会被同一次 tick 的页面重绘抹掉。 |
| NV16 | 新增 `s_ui_built` 守卫：开机 LVGL 锁超时时丢弃输入，并由 tick 重试 `build_screen()`。 |
| NV19 | 报告效果缓冲 40 → 56 B。 |

带理由延后（不是无声丢弃）：**NV8**（口粮/零件/饮水的逐次余数需要新字段，唯一空间是 `pad_v13`，属于要同步更新冻结样例与 pad 断言的 v13.1 改动）、**NV11–NV14、NV17、NV18、NV20**（需要上机走屏的行为与布局工作）、**C05、C13**。

测试（`tests/test_ashbox_game.c`）：新增 `test_audit_20261112_regressions`，覆盖交易空间、band 3 加固日志/门扣、1 秒 tick 下的小时计时器、仅缺水时的回补、水的单次重置；水的测试基准点改到本地日界，破门窗口的期望改为真实小时数。

修复后门禁：`./tools/validate.sh --static` PASS（全部主机测试、文案布局、术语表、死分支、精灵图）；`./tools/validate.sh --firmware` PASS —— app.bin 2,921,488 B（+880 B），整片镜像 2,987,024 B，归档 `build/firmware/b447b700…/`，版本 `0dfba4c-dirty`。

## 8. 交付头（修复后）

```text
Build: PASS (./tools/validate.sh --firmware；app 2,921,488 / 8,323,072 B = 占 35.1 %，余 65 %)
Host tests: PASS
Device tests: NOT RUN
Unverified: NV3/NV6/NV9/NV15/NV18 走屏、自动睡眠策略、提示关闭、两营地全开后的交易页
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS（12 条已修，7 条带理由延后）
Audit mandatory: Copy layout = PASS（host gate），copy accuracy = PASS（NV10 修复后）
```
