#pragma once

#include "ashbox_game.h"
#include "bsp_button.h"
#include <stdbool.h>

void ashbox_ui_start(ash_game_t *game, bool wake_guard_active);
void ashbox_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void ashbox_ui_tick(void); /* light refresh + settle mono */
/** Suggested main-loop delay: shorter while awake/interactive, longer when dim. */
int ashbox_ui_tick_delay_ms(void);
bool ashbox_ui_wants_deep_sleep(void);
void ashbox_ui_clear_deep_sleep_request(void);
/* Restore audio/backlight after a refused or cancelled deep-sleep attempt. */
void ashbox_ui_on_deep_sleep_aborted(void);
bool ashbox_ui_wake_guard_active(void);
/* UI / input-task only: may take LVGL after runtime lock. */
void ashbox_ui_set_wake_guard(bool on);
/* Button-callback safe: clear quiet flag only; defer apply/paint to UI task. */
void ashbox_ui_request_clear_wake_guard(void);
/* Swallow OK briefly after page enter (anti accidental second confirm). */
void ashbox_ui_arm_ok_guard_ms(int ms);
bool ashbox_ui_take_input_drain_request(void);
/** Flush game + settings to NVS (USB backup / import preamble). */
void ashbox_ui_flush_persistent(void);
