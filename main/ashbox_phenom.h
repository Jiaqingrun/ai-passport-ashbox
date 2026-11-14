#pragma once

/* M2 weather (天象): a deterministic 16-day world layer.
 *
 * Pure data + lookup only: no LVGL, no ESP-IDF, no save fields. The same
 * day_key always returns the same phenomenon, so a reload never re-rolls the
 * weather and the host tests can cover the whole 16-day cycle. */

#include <stdbool.h>
#include <stdint.h>

#define ASH_PHENOM_COUNT 16

typedef struct {
    uint8_t id;
    const char *name;   /* two characters, fits the stage chip */
    const char *effect; /* short resource summary, e.g. "水+3 粮-1" */
    const char *advice; /* one actionable line */
    bool danger;
    int8_t water;
    int8_t food;
    int8_t parts;
    int8_t threat;
} ash_phenom_t;

/** Phenomenon for a game day (1-based, matching g->day_key). */
const ash_phenom_t *ash_phenom_for_day(uint32_t day_key);

/** Phenomenon by stable id (0..ASH_PHENOM_COUNT-1); NULL when out of range. */
const ash_phenom_t *ash_phenom_by_id(uint8_t id);

/** 0..15 slot inside the 16-day permutation. */
uint32_t ash_phenom_cycle_slot(uint32_t day_key);
