[简体中文](./AUDIT_CODE.zh_CN.md) · **English**

# Ashbox — low-level code audit

Subject: `feature/ashbox-ember` working tree (`main/ashbox_*.c`, `main/main.c`, save / deep-sleep / audio paths).  
Audit time: 2026-11-05 13:22 (Asia/Shanghai).  
Method: source walkthrough + host layout probe + `./tools/validate.sh --static`; **host PASS does not prove concurrency or device stress**.

```text
Gate snapshot: ./tools/validate.sh --static → PASS (incl. 47 ashbox host tests)
Host sizeof(ash_game_t)=584 (x86_64; device ABI may differ — see C-03)
```

Complements [AUDIT_10R.md](./AUDIT_10R.md): this round covers **runtime correctness / save ABI / tasks & locks / deep sleep**, not gameplay or community compliance.

---

## Verdict

| Layer | Result |
| --- | --- |
| Pure rules / saturation | **Mostly healthy** (caps, `sat_add`, fail floor) |
| Save migrate / CRC | **Usable, ABI-fragile** |
| Tasks & shared state | **Open High items** (no mutex on game + NVS) |
| Deep-sleep path | **Sound design** (refuse → soft mute → irreversible teardown) |
| Audio task | **Medium races** |

No proven Critical crash; top open risk is **lock-free cross-task access to `ash_game_t` / concurrent `ashbox_save_store`**.

---

## Findings (by severity)

| ID | Sev | Where | Finding |
| --- | --- | --- | --- |
| C-01 | **High→fixed** | `ashbox_runtime` + UI/main | **`ashbox_runtime_lock`** serializes settle/save vs input; order = runtime then LVGL. |
| C-02 | **High→fixed** | `ashbox_ui_request_clear_wake_guard` | Button callback clears flags only; apply/paint deferred under locks. |
| C-03 | **Medium→fixed** | `ashbox_save.c` | `_Static_assert` on crc ordering + v4–v7 migrate offsetof canaries. |
| C-04 | **Medium→partial** | `ashbox_save_init` | Loud `ESP_LOGE` before erase; IDF still requires erase (no in-app backup). |
| C-05 | **Medium→fixed** | `ASH_SFX_CMD_STOP` | `stop_all` queues task-owned stop; pointer clear on `sfx_task`. Light `set_radio` races remain. |
| C-06 | **Medium→fixed** | `boot_settle` | On wall rewind / `dt<0`, re-anchor `last_settle_unix` (still no invented progress). |
| C-07 | **Low→fixed** | Input queue | Depth 8→16. |
| E-01 | **High→fixed** | `ashbox_embackup` | Post-import verify opened NVS NS `ASH2` (magic); now `ashbox`/`save` via `ashbox_save_nvs_readable`. |
| C-08 | **Low** | Night ember pity | Overnight success always grants ember; pity constant nearly dead. |
| C-09 | **Info** | `ash_game_crc` | CRC covers padding; OK if fully initialized, amplifies C-03. |
| C-10 | **Info** | `ashbox_enter_deep_sleep` | Holds LVGL lock into sleep; restart on failure. Acceptable. |

---

## Suggested fix order (not implemented here)

1. Mutex (or single-writer command queue) for game + save — C-01  
2. Wake-guard: no LVGL in button callback — C-02  
3. Device `_Static_assert` + offsetof canaries — C-03  
4. NVS major-version policy / backup — C-04  
5. SFX stop as task-owned command — C-05  

---

## Gate fields

```text
Build: NOT RUN
Host tests: PASS
Device tests: NOT RUN (no concurrency stress this round)
Unverified: C-01/C-02 reproduce rate, NVS wear, wall rewind, sfx race
Compliance: see AUDIT_10R
```
