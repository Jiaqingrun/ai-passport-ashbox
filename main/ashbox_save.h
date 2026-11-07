#pragma once

#include "ashbox_game.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* NVS namespace for game save + UI settings (not the ASH2 magic word). */
#define ASHBOX_NVS_NS "ashbox"
#define ASHBOX_NVS_SAVE_KEY "save"
/** Mirror of primary save; written first so a torn primary write can fall back. */
#define ASHBOX_NVS_SAVE_BAK_KEY "sv2"
/* Host/x86 and ESP32-C3 agree today; fail the build if the blob layout drifts. */
#define ASHBOX_SAVE_BLOB_SIZE 776u

esp_err_t ashbox_save_init(void);
esp_err_t ashbox_save_load(ash_game_t *g);
/** Write primary NVS key only (periodic flush; lower wear). */
esp_err_t ashbox_save_store(const ash_game_t *g);
/** Write backup `sv2` first, then primary (boot / sleep / urgent / heal). */
esp_err_t ashbox_save_store_mirrored(const ash_game_t *g);
/** Decode/migrate a raw NVS blob into current `ash_game_t` (host-testable; no NVS I/O). */
esp_err_t ashbox_save_decode(const uint8_t *raw, size_t len, ash_game_t *out);
/** Scan a raw NVS partition dump for a CRC-valid ASH2 blob (rescue path; host-testable). */
bool ashbox_save_scan_dump(const uint8_t *dump, size_t dump_len, ash_game_t *out);
/** Legacy blob sizes (include trailing padding); for host migrate fixtures. */
size_t ashbox_save_v3_bytes(void);
size_t ashbox_save_v8_bytes(void);
size_t ashbox_save_v9_bytes(void);
size_t ashbox_save_v10_bytes(void);
size_t ashbox_save_v11_bytes(void);
/** After full-partition import: open NS and confirm the save blob is readable. */
bool ashbox_save_nvs_readable(void);
