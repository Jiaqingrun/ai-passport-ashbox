#pragma once

#include "ashbox_game.h"

#include <stdbool.h>
#include <stdint.h>

/* M3 守听：四个频段、每日两次、窗口内才涨清晰度。
 * 状态打包在 v15 的 sig_bits：
 *   [1:0] [3:2] [5:4] [7:6]  四段清晰度 0-3
 *   [9:8]   共振 0-3
 *   [11:10] 今日已听 0-2
 *   [13:12] 当前段清晰度进度 0-2
 *   [15:14] 保留 */
#define ASH_RADIO_BANDS 4
#define ASH_RADIO_NOTCHES 7
#define ASH_RADIO_DAILY 2
#define ASH_RADIO_CLARITY_MAX 3

typedef enum {
    ASH_RADIO_LORE = 0,
    ASH_RADIO_LANDMARK,
    ASH_RADIO_VOICE,
    ASH_RADIO_BLANK,
    ASH_RADIO_KIND_COUNT
} ash_radio_kind_t;

typedef struct {
    const char *name;
    const char *freq;
} ash_radio_band_t;

const ash_radio_band_t *ash_radio_band(uint8_t band);
void ash_radio_sync_day(ash_game_t *g);
bool ash_radio_window_open(int hour);
uint8_t ash_radio_listens(const ash_game_t *g);
uint8_t ash_radio_clarity(const ash_game_t *g, uint8_t band);
uint8_t ash_radio_resonance(const ash_game_t *g);
uint8_t ash_radio_progress(const ash_game_t *g);
uint8_t ash_radio_target(uint16_t day, uint8_t band);
uint8_t ash_radio_signal(uint16_t day, uint8_t band, uint8_t pos);
bool ash_radio_can_listen(const ash_game_t *g);
void ash_radio_mark_listen(ash_game_t *g, uint8_t band, bool success, bool in_window);
ash_radio_kind_t ash_radio_result_kind(const ash_game_t *g, uint8_t band);
const char *ash_radio_result_title(ash_radio_kind_t kind);
const char *ash_radio_result_body(ash_radio_kind_t kind);
const char *ash_radio_result_effect(ash_radio_kind_t kind);
void ash_radio_apply(ash_game_t *g, ash_radio_kind_t kind);
