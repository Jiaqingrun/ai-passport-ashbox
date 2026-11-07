**简体中文** · [English](./AUDIT_CODE.md)

# 《余烬》底层代码审计

审计对象：`feature/ashbox-ember` 工作树（`main/ashbox_*.c`、`main/main.c`、存档/深睡/音频路径）。  
审计时刻：2026-11-05 13:22（Asia/Shanghai）。  
方法：源码走读 + 宿主布局探测 + `./tools/validate.sh --static`；**不以宿主 PASS 代替并发/真机压力结论**。

```text
门禁快照：./tools/validate.sh --static → PASS（含 test_ashbox_game 47）
宿主 sizeof(ash_game_t)=584（x86_64；设备 ABI 可能不同，见 C-03）
```

与 [AUDIT_10R.zh_CN.md](./AUDIT_10R.zh_CN.md) 分工：本轮只盯**运行时正确性 / 存档 ABI / 任务与锁 / 深睡与外设**，不重做玩法与社区合规。

---

## 总判

| 层 | 结论 |
| --- | --- |
| 规则纯函数 / 饱和 | **大体健康**（有盖、`sat_add`、失败率地板） |
| 存档迁移 / CRC | **可用，有 ABI 脆弱点** |
| 任务与共享状态 | **有 High 打开项**（游戏态与 NVS 无互斥） |
| 深睡路径 | **设计合理**（拒睡→软静音→不可逆拆线） |
| 音频任务 | **有 Medium 竞态** |

**无 Critical「必崩」铁证**；最高打开项为 **跨任务无锁读写 `ash_game_t` / 并发 `ashbox_save_store`**。

---

## 发现列表（按严重度）

| ID | 级 | 位置 | 发现 |
| --- | --- | --- | --- |
| C-01 | **High→已修** | `ashbox_runtime` + UI/main | **`ashbox_runtime_lock`**：游戏态 settle/存档与按键路径互斥；约定 **先 runtime 再 LVGL**。 |
| C-02 | **High→已修** | `ashbox_ui_request_clear_wake_guard` | 按键回调只清标志；`apply`/paint 延到持锁的 `ash_input` / tick。 |
| C-03 | **Medium→已修** | `ashbox_save.c` | `_Static_assert`：crc 收尾 + v4–v7 迁移 `offsetof` 与现行字段对齐。 |
| C-04 | **Medium→部分** | `ashbox_save_init` | 擦除前 `ESP_LOGE` 明示会丢档；IDF 仍要求 erase，无应用内备份。 |
| C-05 | **Medium→已修** | `ASH_SFX_CMD_STOP` | `stop_all` 投递任务内停止；指针清理由 `sfx_task` 执行。`set_radio` 仍有轻量竞态（可接受）。 |
| C-06 | **Medium→已修** | `boot_settle` | 墙钟回拨/`dt<0` 时重锚定 `last_settle_unix`，避免永久不推进（仍不虚构进度）。 |
| C-07 | **Low→已修** | `main.c` 输入队列 | 深度 8→16，降低爆发丢键。 |
| E-01 | **High→已修** | `ashbox_embackup` | 导入后校验误开 NVS 名 `ASH2`；改为 `ashbox`/`save`（`ashbox_save_nvs_readable`）。 |
| C-08 | **Low→已修** | `ember_pity_need` | 移除无用的过夜 pity 常量；DEEP/NIGHT 不走软保底。 |
| C-09 | **Info** | `ash_game_crc` | CRC 覆盖含填充字节的整结构；依赖写入前稳定填充。当前路径多 `memset`/整读整写，尚可，但与 C-03 叠加放大 ABI 风险。 |
| C-10 | **Info** | `ashbox_enter_deep_sleep` | 持 LVGL 锁后不解锁即深睡；失败则 `esp_restart`。可接受。按钮移交失败亦重启，避免无输入僵尸态。 |

---

## 分层结论

### 1. 存档

- 现行 `ASH2` / VER=8；v1/v3–v7 有迁移；CRC 失败安全新建。  
- `store`：拷贝 → 写 CRC → NVS blob + commit，逻辑清晰。  
- **缺口**：无布局金丝雀；无跨任务存档锁；NVS 格式升级整擦。

### 2. 规则 / 经济

- `apply_economy` 用 `int64_t` 做产耗再夹到盖；零件/粮有仓溢流。  
- `fail_chance` 有非近路地板；猎场跳过 `apply_loot_bias`（与文案审计已对齐）。  
- 宿主 47 测覆盖烬补、灰潮、猎装等；**不覆盖并发**。

### 3. 运行时 / 功耗

- 空闲 60s / 180s / 5–10min 深睡阶梯完整；战报/危机挡自动睡。  
- 深睡：按键拒睡双检 → 定时唤醒 → 外设 prepare → 深睡；拒睡后软静音可恢复。  
- Wi-Fi 应用不 init；电流未测（同 10R）。

### 4. UI 锁

- 正常按键经队列进 `ash_input` 再 `bsp_lvgl_lock`：**主路径正确**。  
- **例外**：唤醒守卫结束在按键回调里直接碰 UI（C-02）。

---

## 建议修复顺序（本审计不实施）

| 序 | 动作 | 针对 |
| --- | --- | --- |
| 1 | 为 `ash_game_t` + `ashbox_save_store` 加同一把 mutex（或「仅主任务改状态，输入只投递命令」） | C-01 |
| 2 | `on_key` 唤醒消抖只置原子/队列标志，禁止回调内 `lvgl_lock`/paint | C-02 |
| 3 | 设备侧 `_Static_assert(sizeof(ash_game_t)==…)` + 迁移 offsetof 金丝雀；文档记录金丝雀值 | C-03 |
| 4 | NVS 大版本：擦前尝试导出/告警；或独立命名空间策略评估 | C-04 |
| 5 | sfx 停止用队列命令，由 `sfx_task` 单线程清状态 | C-05 |

---

## 门禁字段

```text
Build: PASS（修正环后固件门禁；archive 5ba95bc0…）
Host tests: PASS
Device tests: NOT RUN（待确认是否保档刷机验收 C-01/C-02）
Unverified: 真机按键/唤醒/深睡拒睡手感；墙钟回拨；NVS 大版本擦除仍不可恢复
Compliance: 见 AUDIT_10R（本轮不重复）
```

---

## 附录 · 关键尺寸（宿主 x86_64，仅供对照）

| 符号 | 值 |
| --- | --- |
| `sizeof(ash_game_t)` | 584 |
| `offsetof(..., stash_lv)` | 556 |
| `offsetof(..., door_bolt)` | 572 |
| `offsetof(..., crc)` | 576 |
