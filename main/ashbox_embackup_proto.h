#pragma once

/**
 * Host-testable !EMBACKUP line protocol (no ESP-IDF).
 * Flash / USB / reboot are injected via callbacks.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ashbox_embackup.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Return false if the line did not fully reach the host (do not advance state). */
typedef bool (*ash_em_emit_fn)(const char *line, void *user);
typedef bool (*ash_em_snapshot_fn)(uint8_t *out, size_t len, void *user);
typedef bool (*ash_em_commit_fn)(const uint8_t *data, size_t len, uint32_t crc,
                                 void *user);
typedef void (*ash_em_reboot_fn)(void *user);

typedef struct {
    ash_em_emit_fn emit;
    ash_em_snapshot_fn snapshot;
    ash_em_commit_fn commit;
    ash_em_reboot_fn reboot;
    ash_em_flush_fn flush;
    void *user;
    char device_id[13];
    char firmware[65];
    unsigned save_version;
    uint32_t (*now_ms)(void *user);
} ash_em_hooks_t;

typedef struct ash_em_proto ash_em_proto_t;

size_t ash_em_proto_bytes(void);
void ash_em_proto_init(ash_em_proto_t *p, const ash_em_hooks_t *hooks);
void ash_em_proto_feed(ash_em_proto_t *p, char c);
void ash_em_proto_tick(ash_em_proto_t *p);
void ash_em_proto_request_export(ash_em_proto_t *p, bool for_import_preamble);
void ash_em_proto_set_import_mode(ash_em_proto_t *p, bool on);
void ash_em_proto_confirm_import(ash_em_proto_t *p, bool accept);
bool ash_em_proto_host_ok(const ash_em_proto_t *p);
bool ash_em_proto_busy(const ash_em_proto_t *p);
ash_em_ui_t ash_em_proto_ui(const ash_em_proto_t *p);
int ash_em_proto_progress_pct(const ash_em_proto_t *p);
/** True while import is still exporting the live save before upload. */
bool ash_em_proto_import_preamble(const ash_em_proto_t *p);
void ash_em_proto_clear_ui_pulse(ash_em_proto_t *p);

/** After reboot: queue RESTORED for the next HELLO. result=1/2/3. */
void ash_em_proto_set_pending_restored(ash_em_proto_t *p, unsigned result,
                                       uint32_t crc);

/** Scratch / import image buffer (must be >= ASH_EMBACKUP_NVS_SIZE). */
void ash_em_proto_set_recv_buf(ash_em_proto_t *p, uint8_t *buf, size_t cap);
void ash_em_proto_set_flush(ash_em_proto_t *p, ash_em_flush_fn fn);

uint32_t ash_em_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
