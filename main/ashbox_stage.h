#pragma once

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

/* M1 stage overlay: tint, hearth glow, rising embers, threat vignette and four
 * corner chips. Pure LVGL drawing; rules live in ashbox_ambience.c. */

typedef struct {
    int hour;
    int minute;
    uint16_t ember;
    uint8_t threat;
    const char *phenom; /* M2 weather name; NULL falls back to the day phase */
    bool phenom_danger;
    uint32_t exp_remaining_s;
    uint8_t pop;
    uint8_t farm_ready;
    uint8_t duty_left;
    bool exp_active;
    bool report;
    bool crisis;
    bool wanderer;
    bool stash_full;
    bool radio_on;
} ashbox_stage_view_t;

/** Creates overlays inside the 240x176 stage parent, above the sprite. */
void ashbox_stage_create(lv_obj_t *parent, int32_t w, int32_t h);

/** now_us is monotonic; animate=false freezes particles but still updates text. */
void ashbox_stage_update(int64_t now_us, const ashbox_stage_view_t *v, bool animate);
