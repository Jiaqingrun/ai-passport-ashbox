#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ASH_SFX_CLICK = 0,
    ASH_SFX_MOVE,
    ASH_SFX_CONFIRM,
    ASH_SFX_BACK,
    ASH_SFX_DEPART,
    ASH_SFX_REWARD,
    ASH_SFX_WARN,
    ASH_SFX_SLEEP,
    ASH_SFX_BELL, /* 过路客上门：两下偏暖敲击 */
    ASH_SFX_CMD_STOP, /* task-owned soft stop; not a tone */
} ash_sfx_id_t;

esp_err_t ashbox_sfx_init(void);
void ashbox_sfx_set_enabled(bool on);
void ashbox_sfx_set_radio(bool on);
void ashbox_sfx_set_volume(uint8_t percent); /* 0..100 master */
bool ashbox_sfx_enabled(void);
bool ashbox_sfx_radio_on(void);
void ashbox_sfx_play(ash_sfx_id_t id);
void ashbox_sfx_stop_all(void); /* soft mute before deep sleep; hardware teardown is in ashbox_enter_deep_sleep */
