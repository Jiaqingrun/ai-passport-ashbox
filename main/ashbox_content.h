#pragma once

#include "ashbox_game.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *label;
    int8_t trust_delta; /* applied to returning person */
    int8_t food_delta;
    int8_t parts_delta;
    int8_t threat_delta;
    uint8_t quest_id;   /* 255 = none */
    uint8_t quest_need; /* required step before advance; 255 = any */
    const char *log;
} ash_branch_def_t;

typedef struct {
    uint8_t branch_ids[ASHBOX_BRANCH_MAX];
    uint8_t branch_count;
} ash_outcome_def_t;

const ash_branch_def_t *ash_content_branch(uint8_t id);
unsigned ash_content_branch_count(void);
/* Short effect line for UI, e.g. "信任+1 · 威胁-2" */
void ash_content_branch_effect(uint8_t id, char *out, size_t out_n);
const ash_outcome_def_t *ash_content_outcome(uint8_t id);
unsigned ash_content_outcome_count(void);
const char *ash_content_away_line(unsigned idx);
unsigned ash_content_away_count(void);
const char *ash_content_quest_name(uint8_t qid);
const char *ash_content_quest_step_hint(uint8_t qid, uint8_t step);
const char *ash_content_crisis_label(ash_crisis_opt_t opt);
const char *ash_content_crisis_hint(ash_crisis_opt_t opt);
const char *ash_content_loadout_name(ash_loadout_t load);
const char *ash_content_loadout_hint(ash_loadout_t load);
const char *ash_content_person_trait(uint8_t id);
const char *ash_content_order_name(ash_night_order_t order);
const char *ash_content_order_hint(ash_night_order_t order);
const char *ash_content_tag_label(uint8_t tag_id);
unsigned ash_content_tag_count(void);
/* Prefer these branch ids for the tag; returns count written (≤ max_n). */
uint8_t ash_content_tag_branches(uint8_t tag_id, uint8_t *out, uint8_t max_n);

/* Radio / atmosphere lines (moved out of ashbox_game.c). */
const char *ash_content_radio_idle(unsigned idx);
unsigned ash_content_radio_idle_count(void);
const char *ash_content_radio_lore(unsigned idx);
unsigned ash_content_radio_lore_count(void);
const char *ash_content_radio_twist(unsigned idx);
unsigned ash_content_radio_twist_count(void);
const char *ash_content_radio_watch(unsigned idx);
unsigned ash_content_radio_watch_count(void);
const char *ash_content_med_name_stim(void);
const char *ash_content_med_name_milk(void);
const char *ash_content_med_name_abx(void);
