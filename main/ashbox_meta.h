#pragma once

#include "ashbox_game.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#define ASHBOX_NVS_META_KEY "meta"
#define ASHBOX_META_MAGIC 0x4D485341u /* ASHM */
#define ASHBOX_META_VER 1
#define ASHBOX_META_BLOB_SIZE 32u

#define ASH_ACH_LONE (1u << 0)
#define ASH_ACH_TOGETHER (1u << 1)
#define ASH_ACH_CROWD (1u << 2)
#define ASH_ACH_PACIFIST (1u << 3)
#define ASH_ACH_NOBODY (1u << 4)
#define ASH_ACH_ALL_MASK 0x1Fu

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t unlocked; /* ASH_ACH_* bitmask */
    uint8_t super_core_consumed; /* 1 = career core already used */
    uint8_t super_core_granted;  /* 1 = inventory grant already issued once */
    uint16_t clear_count;
    uint8_t pad[16];
    uint32_t crc;
} ashbox_meta_t;

_Static_assert(sizeof(ashbox_meta_t) == ASHBOX_META_BLOB_SIZE, "meta blob size");

void ashbox_meta_new(ashbox_meta_t *m);
uint32_t ashbox_meta_crc(const ashbox_meta_t *m);
esp_err_t ashbox_meta_load(ashbox_meta_t *m);
esp_err_t ashbox_meta_store(const ashbox_meta_t *m);

bool ashbox_meta_has(const ashbox_meta_t *m, uint16_t bit);
/** Merge newly unlocked bits into meta; returns bits that were newly set. */
uint16_t ashbox_meta_unlock(ashbox_meta_t *m, uint16_t bits);

/** Evaluate clear-time achievements from run state; may set run toast fields. */
uint16_t ash_game_eval_clear_achievements(ash_game_t *g, ashbox_meta_t *m);
/** Pacifist break: call when pistol/ammo equipped or carried out. */
void ash_game_mark_peace_broken(ash_game_t *g);
/** Apply permanent meta grants into a freshly created run (restart / new after wipe). */
void ash_game_apply_meta_grants(ash_game_t *g, ashbox_meta_t *m);
/** Restart run: wipe save state, keep meta, apply grants. */
void ash_game_restart_with_meta(ash_game_t *g, ashbox_meta_t *m);

const char *ash_ach_name(uint16_t bit);
const char *ash_ach_cond(uint16_t bit);
const char *ash_ach_grant(uint16_t bit);
