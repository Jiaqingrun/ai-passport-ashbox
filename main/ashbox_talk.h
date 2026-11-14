#pragma once

#include "ashbox_game.h"

#include <stdbool.h>
#include <stdint.h>

/* M3 夜谈：每天一段，按营地状态过滤；只做小资源/心情式修正，不改战斗数值。 */
typedef enum {
    ASH_TALK_ANY = 0,
    ASH_TALK_FARM,
    ASH_TALK_INJURED,
    ASH_TALK_VISITOR,
    ASH_TALK_THREAT,
    ASH_TALK_POP2,
    ASH_TALK_LOW_FOOD,
    ASH_TALK_RAIN,
    ASH_TALK_STILL,
    ASH_TALK_RADIO,
    ASH_TALK_CORE,
    ASH_TALK_CLEAR,
} ash_talk_when_t;

typedef struct {
    const char *text;
    int8_t food, water, parts, ember, threat, fatigue, trust;
} ash_talk_opt_t;

typedef struct {
    uint8_t id;
    uint8_t when; /* ash_talk_when_t */
    const char *title;
    const char *body; /* 2-3 lines, '\n' separated */
    ash_talk_opt_t opt[2];
} ash_talk_t;

bool ash_talk_available(const ash_game_t *g);
const ash_talk_t *ash_talk_pick(const ash_game_t *g);
void ash_talk_mark_seen(ash_game_t *g);
void ash_talk_apply(ash_game_t *g, const ash_talk_t *t, uint8_t choice);
