[English](./PROJECT.md) · **简体中文**

# 项目约定 · ai-passport-ashbox（《余烬》/ Ashbox）

> **分层、不混写**：全局个人规范见 `00-personal-standards.mdc`（勿在本文件重复 conda/Git/`~/QR`/`~/.qr` 等全局条文）。  
> 编辑：`qr project-standards --edit ai-passport-ashbox` · 从对话修订：`qr project-standards-revise ai-passport-ashbox`  
> 生成 Cursor 规则：`qr rules --target .` 后**必须** `git checkout -- AGENTS.md AGENTS.zh_CN.md`（见文末）。  
> 英文默认路径：[`PROJECT.md`](./PROJECT.md)（Passport/`check_repo` 对偶）。

**冲突裁决（本仓）：**  
`AGENTS.md` 烧录/安全/UI 重设计硬轨 > 本文件 > [`docs/ashbox/LAYOUT_LOCK.md`](./docs/ashbox/LAYOUT_LOCK.md) / 存档 ABI > [`docs/ashbox/README.md`](./docs/ashbox/README.md) 索引说明 > 运维/审计快照 > `docs/archive/`（若有）。

---

## 用途

FoloToy AI Passport 上的**离线**末世据点放置游戏《余烬》（Ashbox / Ember）：暖炉家园、远征搜刮、据点建设、交易、伤病用药、过路客与 USB 存档管家。  
**不是**硬件测试 demo；**不是**在线服务端游戏。

显示名：余烬 / Ember · 代码目录：`ai-passport-ashbox` · 主开发分支习惯：`feature/ashbox-ember`

---

## 技术栈与目录

| 层 | 位置 |
| --- | --- |
| 目标板 | ESP32-C3，**8 MB Flash，无 PSRAM**，ESP-IDF **5.5.3** |
| 规则（可宿主测） | `main/ashbox_game.{c,h}`、`main/ashbox_content.{c,h}` |
| 存档 / 设置 / 深睡 / 音效 | `main/ashbox_save.*`、`ashbox_settings.*`、`ashbox_sleep.*`、`ashbox_sfx.*` |
| USB 存档协议 | `main/ashbox_embackup*.`（`!EMBACKUP` ↔ [AI-Passport_BS](https://github.com/Jiaqingrun/AI-Passport_BS) 管家 v1.0.1） |
| UI（LVGL） | `main/ashbox_ui.{c,h}`；入口接线 `main/main.c` |
| 精灵 | `main/ashbox_sprites.*` ← `assets/ashbox/sprites/`（**12 景 ×3 帧 ×240×176**） |
| 字库 | `assets/fonts/ashbox_font_16.c` ← `main/ashbox_font_symbols.txt` |
| 电台/暖炉 PCM | `assets/ashbox/sfx/` |
| BSP | `components/bsp`（可复用）；页面/状态机/动画属 `main` |
| 产品文档索引 | [`docs/ashbox/README.md`](./docs/ashbox/README.md)（中英配对，现行文必须入索引） |
| 上游通用文档 | `docs/development/`、`docs/hardware-design/`（板级事实优先级见 `AGENTS.md`） |

QR 项目 ID：`ai-passport-ashbox`（路径 `~/QR/dev/ai-passport-ashbox`）。MCP / `qr ask` / `qr log -p` 用此名或 `dev/ai-passport-ashbox`。

---

## 会话阅读顺序

1. 本文件 `PROJECT.md`  
2. [`AGENTS.md`](./AGENTS.md)（技能安装、烧录政策、UI 重设计禁令）  
3. [`docs/ashbox/README.md`](./docs/ashbox/README.md) → 任务相关现行文（布局锁 / 延伸 / 构建 / 审计清单）  
4. 改代码时：邻近头文件与实现；硬件细节以规格与 `bsp_pins.h` 为准，缺则问用户  

---

## 构建 · 测试 · 刷机

| 目的 | 命令 |
| --- | --- |
| 仓库静态 + 宿主测 | `./tools/validate.sh --static` |
| 固件 + 合并镜像校验 | 激活 IDF 5.5.3 后 `./tools/validate.sh --firmware` |
| 全门禁 | `./tools/validate.sh` |
| 复核归档 | `python3 tools/archive_firmware.py verify build/firmware/<sha>/` |
| 字形 | `python3 tools/check_ashbox_font_glyphs.py`（改中文后必跑） |
| **日常刷机（默认）** | `./tools/flash_ashbox_preserve_save.sh /dev/cu.usbmodemXXXX [归档目录]` — 只写 `0x10000`，保留 NVS |
| 整机合并刷 | 合并 `full.bin` @ `0x0` — **清空存档**；仅空白机或用户点名清档 |

交付必须分列：`Build` / `Host tests` / `Device tests` / `Unverified`。构建成功 ≠ 真机验收；刷机须用户明确同意（连接设备 ≠ 授权）。

---

## 存档与版本

- NVS：命名空间 `ashbox`，键 `save`，magic `ASH2`  
- 现行：`ASHBOX_SAVE_VER=13`（`main/ashbox_game.h`）；blob `ASHBOX_SAVE_BLOB_SIZE=776`（`ashbox_save.h`，宿主测断言）；元进度键 `meta` / `ASHM` — [docs/ashbox/META_PROGRESS.zh_CN.md](docs/ashbox/META_PROGRESS.zh_CN.md)  
- **只追加字段**：新字段插在 `crc` 前；提供旧版 migrator；禁止静默打断旧档可读性  
- 远征进行中醒着推进时约每 15 s 落盘剩余时间（出发/战报/危机/站事立即写）  
- 社区商店安装/更新会**整机清空**进度；对外文案禁止承诺「更新不丢档」。清单见 [`docs/ashbox/COMMUNITY_PUBLISH.md`](./docs/ashbox/COMMUNITY_PUBLISH.md)

---

## 布局锁（摘要 · 细节以 LAYOUT_LOCK 为准）

未经理明确新设计决策，禁止改：

- 屏 240×320；精灵 **240×176**、12×3  
- 家园纵向：`SAFE_TOP=2`、`HDR_H=18`、`STAGE_H=176`、`NOW_BAND_H=52`、底键条几何  
- 不显示「人 x/y」；远征/危机/战报以「此刻」表达；电池为电芯匣（勿标「烬」）  
- 权威常量：`main/ashbox_ui.c`；精灵编码：`tools/ashbox_sprites_encode.py`

体积方案见 [`docs/ashbox/FLASH_SIZE_PLAN.md`](./docs/ashbox/FLASH_SIZE_PLAN.md)。**禁止**有感砍减：砍景/砍帧、缩小分辨率、有损图、缩电台内容（除非用户当场改口）。

---

## 玩法与业务边界

**核循环（保留）：** 出发 → 时钟/深睡 → 归来抉择 → 危机/调查。新系统挂 `ash_game_settle` 与据点页，不另开第二套时钟。

**现行能力（摘要）：** 五档远征（近路/废街/深处/猎场·需开闸/过夜）；携带撬棍/手枪/子弹；零件概率与撬棍收割；铁与工作台合成；装备背包/手枪；药品（旺仔/肾上腺素/抗生素）；铁棚·渡口交易；菜地谷/薯；过路客；日勤/站事；过夜站令。

**合规硬轨（任何切片不得违反，详见 EXTENSION）：**

1. 无内购 / 广告 / 抽卡 / 付费加速  
2. 无清档转生、无强制日活门槛（日勤等为加成有盖）  
3. 玩家可见文案不泄漏芯片/框架/总线名  
4. 离线/深睡按现有折算；禁止「必须在线才能涨」的新资源  
5. 一切增长有盖；加减饱和；顶满停产不回绕  

**禁止擅自：**

- 复用 demo 测试菜单/壳做产品 UI（重命名换色不算重设计）  
- 无 LVGL 线程外无锁碰 UI 对象  
- 按钮回调里做音频/存储/网络等慢操作（进 worker）  
- 猜测未文档化的引脚/外设  
- 把密钥、QR 私密、未脱敏日志提交进仓  
- 默认 `git commit` / `push` / 社区发布 / 整片擦除 / 合并 `0x0` 刷写  

---

## 玩家文案与游戏内容 · 大白话（写死）

凡玩家可见中文必须按本节执行；与「写得短」冲突时**以能看懂为准**。本节同时约束 **UI 用语** 与 **游戏内容文案**（叙事/机制讲解），缺一不可。

**目标：** 普通人不用猜黑话也能看懂在说什么、在发生什么；帮助与商店说明宜细、宜吸引人；游戏内剧情与提示宜像人说话，但不得编造未实现功能。

### A. 强制用语（玩家侧正式名）

| 禁止（黑话/缩写/别名） | 必须写成（唯一正式名） |
| --- | --- |
| 短火、枪（单指武器时） | 手枪 |
| 火子、单独一个「子」指弹药 | 子弹 |
| 空回 | 空手而归 |
| 铁条（指出门工具时） | 撬棍 |
| 工作站（指制作台时） | 工作台 |
| 水桶（指存水器时） | 水罐 |
| 墙钟 | 时钟 |
| 件（指零件时） | 零件 |
| 烬（指余烬时） | 余烬 |
| 粮（指口粮时，易混处） | 口粮 |

**一件一物一名：** 同一道具/建筑/事件在玩家可见文案里只许用一个正式中文名，禁止「手枪/枪/短火」或「撬棍/铁条」混用。

**机制说明要白话算账：** 仓位等规则须写成「口粮300占1仓位 · 铁50占1仓位 · 水罐/背包各占1仓位」这类人话，禁止「口粮按300 · 罐背用仓」式黑话压缩。

**概率：** 凡向玩家交代成功率、空手而归率、掉落几率等，文案须出现「概率」二字（或等价完整说法如「空手而归的概率约…」），禁止只甩「约12%」却不说是什么概率。

**正式名优先：** 不要用过度简化词顶替本应有的正常名称（例：不要为了省字把「手枪」写成「短火」）。专有设定名（余烬、匣、铁皮、灰潮等）可保留，首次出现宜带一句白话解释。

### B. 游戏内容文案（写死）

下列内容一律算「游戏内容」，必须大白话，不得另立「文艺黑话」口径：

| 类别 | 主要落点 |
| --- | --- |
| 帮助与教学 | 据点「怎么玩」、开局引导、主页提示/软钩 |
| 出门与掉落说明 | 选路名/副行、人选特性、携带说明、战报结算句 |
| 据点操作 | 建设/交易/装备/菜地/站令/日勤/危机选项的标题与副文 |
| 叙事与调查 | 归来分支标签与选项、途中句、线索步骤、纪事、电台 idle/lore/twist/watch |
| 系统反馈 | 升级/制作/用药/过路客/守夜等 `ash_game_log` 玩家可见句 |

**写法要求：**

1. **说清因果：** 玩家读完应知道「发生了什么 / 我能做什么 / 大概有什么后果」，少用只有作者懂的隐喻堆砌。  
2. **机制句要人话：** 掉落、概率、消耗、冷却、限制用口语写全；需要数字就写数字并标明含义。  
3. **叙事句要口语：** 分支、电台、途中句可以有气氛，但句子应能念出声、像站里人在说话；禁止为炫文采发明玩家看不到定义的黑话。  
4. **与用语表一致：** 游戏内容里同样禁止「短火/火子/空回」等；新增道具/建筑/状态先定正式中文名再写进内容表。  
5. **帮助优先讲透：** 新玩法上线或改规则时，同步改帮助与选路副行；禁止只改数值不改说明。  

### C. 范围与工程约束

- 机内：`ashbox_ui.c`、`ashbox_game.c` 玩家串、`ashbox_content.c`（分支/途中/线索/危机/站令/标签/电台等）、帮助页全文  
- 社区：`description_*` / `instructions_*` / `changelog_*`（play-first，仍禁工程词；**自 1.7 起商店简介／说明只按本版更新做增量修改，禁止无授权整篇大改**，见 [`COMMUNITY_PUBLISH.zh_CN.md`](./docs/ashbox/COMMUNITY_PUBLISH.zh_CN.md)）  
- 代码标识符、注释、审计文档可用英文或旧称，但**不得**漏进玩家可见串  

**字库：** 新中文必须进 `main/ashbox_font_symbols.txt` 并重生 `assets/fonts/ashbox_font_16.c`；改完跑 `python3 tools/check_ashbox_font_glyphs.py`。`snprintf` 缓冲区与 `ASHBOX_JOURNAL_LEN`（36，存档 ABI）装不下时先缩短白话句，禁止静默截断关键名。

### D. 验收

抽查至少四类：① 帮助「怎么玩」；② 选路副行/装备制作名；③ 一条归来分支或电台句；④ 商店说明/更新说明。  
应能直接读出「手枪 / 子弹 / 空手而归 / 概率」及明白的机制句；不应再出现「短火 / 火子 / 空回」作玩家用语；内容句不应让人觉得「像黑话诗」。

---

## 审计硬轨（写死 · 每次必做）

凡本项目的**审计**（十轮 / 代码 / 内容 / UI / 发布前 / 对话中明确称为审计的任务），交付物**必须**含：

1. **Flash 体积** — 近期 `--firmware` 镜像身份、`app` vs factory 余量、精灵/字库/PCM/代码拆分、对照 `FLASH_SIZE_PLAN`、可做与禁做  
2. **整体代码优化** — 规则/UI 边界、过大单元、重复、可测未测、锁与任务、存档 ABI、死枝、构建脂肪；每条有证据与可执行下一步  
3. **玩家文案版式 + 用语表** — 帮助／引导／弹窗必须 WRAP；一名一物；仓位等机制白话算账；宿主门禁 `tools/check_ashbox_copy_layout.py` + `tools/check_ashbox_glossary.py`

缺一则审计未完成。清单：[`docs/ashbox/AUDIT_REQUIREMENTS.md`](./docs/ashbox/AUDIT_REQUIREMENTS.md)。

### 改后严格审计（写死）

凡会话中改了《余烬》产品代码／UI／玩家文案／帮助／设置，交付前 Agent **必须**：

1. 跑 `./tools/validate.sh --ashbox`（含字形、死枝、**文案版式**、精灵、宿主测）  
2. 按 `AUDIT_REQUIREMENTS` 适用行过一遍（改 UI／文案必查文案版式；重编固件查 Flash；动结构／ABI／热点查代码优化）  
3. **当场修掉 FAIL** — 禁止把已知换行遮挡、截断或门禁失败留到「下次专门审计」

---

## 文档治理（本产品树）

- 唯一权威索引：`docs/ashbox/README.md`（及 `.zh_CN.md`）  
- 布局/体积/审计完成态以各自现行文为准，问题清单不得另立「当前进度 ✅」并行真相源  
- 现行 Markdown：**英文默认路径 + 简体中文 `.zh_CN.md` 配对**，互链保持同步  
- 过时里程碑进 `docs/archive/`（或等价），正文标明历史归档  

---

## 社区与发布

- 技能：`folotoy-ai-passport-publisher`；目标站默认 `https://ai-passport.folotoy.cn`  
- 玩法项目 ID（现行）：`1943` / slug `community-c919dfca`（以 `publisher.py projects` 为准）  
- 对外文案 play-first：禁止 ESP/LVGL/分辨率等工程词进入标题与简介；**同时遵守上文「玩家文案与游戏内容 · 大白话」**  
- 游戏介绍／说明以现行商店（或最近送审）稿为基线：**只随本版玩法／规则更新做对应修改**，不再每次大幅重写；细则见 [`docs/ashbox/COMMUNITY_PUBLISH.zh_CN.md`](./docs/ashbox/COMMUNITY_PUBLISH.zh_CN.md)  
- 更新说明（changelog）只写本版已落地的改动  
- 更新必须带中英 changelog；社区安装须声明会清空进度  
- 一次性授权用尽后不得硬重试；请用户重新下发官方更新提示  

---

## AI 协作（本项目）

- 必备技能：`passport-develop`、`passport-setup`、`passport-build`、`passport-device-test`、`passport-debug`；缺则自行安装后再开工  
- 实现后主动询问是否刷机；检测端口 ≠ 同意刷写  
- 交付前遵守上文「改后严格审计」  
- 里程碑决策：`qr log -p ai-passport-ashbox --type decision "…"`  
- **`qr rules`：** 只采纳 `.cursor/rules/00-personal-standards.mdc` 与 `10-project.mdc`。本仓 `AGENTS.md` / `AGENTS.zh_CN.md` 是上游 Passport 权威入口，**禁止**被汇总稿覆盖后提交；若被改写立即 `git checkout -- AGENTS.md AGENTS.zh_CN.md`  

---

## 当前阶段（摘要）

| 项 | 状态 |
| --- | --- |
| 可玩闭环 | 已上线社区；本机持续迭代 |
| 存档 | v10（铁/制造/装备/药品/过路客等） |
| 布局 | LAYOUT_LOCK 已定死 |
| Flash | 见 FLASH_SIZE_PLAN；审计每次复核 |
| 社区公开名 | 以商店当前通过版为准（1.4 大白话说明已送审；以审核结果为准） |
