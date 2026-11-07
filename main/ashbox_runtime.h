#pragma once

#include <stdbool.h>

/*
 * Serializes ash_game_t mutations and NVS save against the UI input task and
 * the main tick. Always acquire this lock *before* bsp_lvgl_lock() to avoid
 * ABBA deadlocks with LVGL.
 */
void ashbox_runtime_init(void);
void ashbox_runtime_lock(void);
void ashbox_runtime_unlock(void);
