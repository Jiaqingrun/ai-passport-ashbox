#pragma once

#include "ashbox_game.h"

#include <stdbool.h>
#include <stdint.h>

/* M1 return digest: a small NVS snapshot written before deep sleep, compared
 * after the next boot settle. Pure delta rules live here; the NVS wrapper stays
 * behind ESP_PLATFORM so host tests can link this module without IDF. */

#define ASH_DIGEST_MAGIC 0x41574159u /* 'AWAY' */
#define ASH_DIGEST_VER 1u
#define ASH_DIGEST_MIN_AWAY_S (30 * 60)
#define ASH_DIGEST_MAX_AWAY_S (3 * 24 * 3600)

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t bytes;
    int64_t unix; /* wall clock at sleep entry */
    uint16_t food;
    uint16_t parts;
    uint16_t iron;
    uint16_t water;
    uint16_t ember;
    uint8_t threat;
    uint8_t pop;
    uint8_t pad[2];
} ash_digest_snap_t;

typedef struct {
    int64_t away_s;
    int32_t food;
    int32_t parts;
    int32_t iron;
    int32_t water;
    int32_t ember;
    int16_t threat;
} ash_digest_delta_t;

/** True when the snapshot is fresh enough and the current totals are usable. */
bool ash_digest_calc(const ash_digest_snap_t *snap, int64_t now, uint16_t food,
                     uint16_t parts, uint16_t iron, uint16_t water, uint16_t ember,
                     uint8_t threat, ash_digest_delta_t *out);

#ifdef ESP_PLATFORM
#include "esp_err.h"

esp_err_t ashbox_digest_store(const ash_game_t *g, int64_t now);
/** Power-on / reset: a stale pre-sleep blob must not be shown as this wait. */
void ashbox_digest_discard(void);
/** Reads and consumes the snapshot. Returns false when absent or stale. */
bool ashbox_digest_take(const ash_game_t *g, int64_t now, ash_digest_delta_t *out);
#endif
