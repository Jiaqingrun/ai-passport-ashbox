#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define ASH_GUIDE_DONE 6u

#define ASH_COACH_FARM 0x01u
#define ASH_COACH_BUILD 0x02u
#define ASH_COACH_TRADE 0x04u
#define ASH_COACH_ORDER 0x08u
#define ASH_COACH_WANDER 0x10u

typedef struct {
    uint8_t ver;        /* 4 */
    uint8_t brightness; /* 10..100, step 10 */
    uint8_t volume;     /* 0..100, step 10; 0=静音 */
    uint8_t radio_on;   /* 1=背景电台 */
    uint8_t guide_step; /* 0..ASH_GUIDE_DONE-1 进行中；ASH_GUIDE_DONE=已完成 */
    uint8_t coach_seen; /* ASH_COACH_* bitmask; first-use tips */
} ash_settings_t;

esp_err_t ashbox_settings_load(ash_settings_t *out);
esp_err_t ashbox_settings_save(const ash_settings_t *in);
void ashbox_settings_apply(const ash_settings_t *in);
ash_settings_t ashbox_settings_defaults(void);
uint8_t ashbox_settings_step_up(uint8_t v, uint8_t min_v);
uint8_t ashbox_settings_step_down(uint8_t v, uint8_t min_v);
