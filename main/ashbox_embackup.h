#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** NVS layout for Ashbox / AI Passport (matches save-manager). */
#define ASH_EMBACKUP_NVS_OFFSET 0x9000u
#define ASH_EMBACKUP_NVS_SIZE   0x6000u

typedef enum {
    ASH_EM_UI_IDLE = 0,
    ASH_EM_UI_NO_HOST,
    ASH_EM_UI_BACKUP_BUSY,
    ASH_EM_UI_BACKUP_OK,
    ASH_EM_UI_BACKUP_FAIL,
    ASH_EM_UI_IMPORT_WAIT,
    ASH_EM_UI_IMPORT_CONFIRM,
    ASH_EM_UI_IMPORT_BUSY,
    ASH_EM_UI_IMPORT_DONE_HINT,
} ash_em_ui_t;

/** Flush game+settings into NVS before a snapshot. Called from the backup task. */
typedef void (*ash_em_flush_fn)(void);

/** Start USB RX task and protocol (safe to call once after NVS init). */
void ashbox_embackup_start(ash_em_flush_fn flush);

bool ashbox_embackup_host_ok(void);
bool ashbox_embackup_busy(void);
ash_em_ui_t ashbox_embackup_ui_state(void);
/** 0..100 while transferring; -1 when idle. */
int ashbox_embackup_progress_pct(void);
/** True while import is backing up the live save before upload. */
bool ashbox_embackup_import_preamble(void);
void ashbox_embackup_clear_ui_pulse(void);

/** Settings → 备份存档. */
void ashbox_embackup_request_backup(void);

/** Settings → 导入存档: wait for web OFFER. */
void ashbox_embackup_enter_import(void);
void ashbox_embackup_leave_import(void);

/** Confirm dialog after OFFERED (default focus should be cancel). */
void ashbox_embackup_confirm_overwrite(bool accept);

#ifdef __cplusplus
}
#endif
