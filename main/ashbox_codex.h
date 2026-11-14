#pragma once

#include "ashbox_game.h"

#include <stdbool.h>
#include <stdint.h>

/* M3 observation log: five volumes, twenty entries at v1. Unlocks are derived
 * from persistent state, so old saves fill in as soon as they meet a condition. */
#define ASH_CODEX_COUNT 30u
#define ASH_CODEX_PER_VOL 6u
#define ASH_CODEX_VOL_COUNT 5u

typedef struct {
    const char *title;
    const char *body;
    const char *source;
} ash_codex_entry_t;

const ash_codex_entry_t *ash_codex_entry(uint8_t idx);
const char *ash_codex_vol_name(uint8_t vol);
uint8_t ash_codex_vol_of(uint8_t idx);

bool ash_codex_has(const ash_game_t *g, uint8_t idx);
bool ash_codex_unlock(ash_game_t *g, uint8_t idx); /* true when newly recorded */
void ash_codex_sweep(ash_game_t *g);               /* apply every met condition */

uint8_t ash_codex_count(const ash_game_t *g);
uint8_t ash_codex_vol_count(const ash_game_t *g, uint8_t vol);
const char *ash_codex_rank(uint8_t count);
