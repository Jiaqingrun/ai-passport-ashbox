#pragma once

#include "ashbox_game.h"

#include <stdint.h>

/** Success-path core loot (food/parts/ember/iron/water + tool return). */
void ash_game_apply_success_core_loot(ash_game_t *g, ash_report_t *r, uint8_t pid, ash_tier_t tier,
                                      ash_loadout_t load, uint8_t carried_crowbar,
                                      uint8_t carried_pistol, uint8_t carried_ammo);
