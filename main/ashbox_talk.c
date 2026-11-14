#include "ashbox_talk.h"

#include "ashbox_phenom.h"

#include <stddef.h>

static const ash_talk_t k_talk[] = {
    {0, ASH_TALK_ANY, "火堆",
     "火堆矮下去了。\n有人把最后一根柴推给你。\n谁都没先开口。",
     {{"添一根", 0, 0, 0, -2, 0, 0, 1}, {"早点睡", 0, 0, 0, 0, 0, -6, 0}}},
    {1, ASH_TALK_FARM, "菜地",
     "菜地边上的土还是湿的。\n她蹲着数叶子，数到一半忘了数到哪。\n你站在后面没出声。",
     {{"陪着数", 0, 0, 0, 0, 0, -2, 1}, {"去修棚", 0, 0, 2, 0, 0, 3, 0}}},
    {2, ASH_TALK_INJURED, "换药",
     "伤员的绷带该换了。\n他咬着木片，额头上全是汗。\n灯芯爆了一下。",
     {{"换药", -1, 0, 0, 0, 0, 3, 2}, {"明天再说", 0, 0, 0, 0, 0, 0, -1}}},
    {3, ASH_TALK_VISITOR, "门外",
     "门外有人咳了一声。\n不是自己人。\n火堆的光只够照到门槛。",
     {{"开门", -2, 0, 0, 0, 2, 0, 1}, {"装作没听见", 0, 0, 0, 0, -1, 0, -1}}},
    {4, ASH_TALK_THREAT, "铁皮声",
     "远处有铁皮被拖过地面的声音。\n一下，又一下。\n像在找什么。",
     {{"轮流守", 0, 0, 0, 0, -4, 6, 0}, {"关门睡", 0, 0, 0, 0, 2, -4, 0}}},
    {5, ASH_TALK_POP2, "梦话",
     "小芒睡着了还在说梦话。\n她念的是一个地名。\n你没听过。",
     {{"叫醒她", 0, 0, 0, 0, 0, 2, 1}, {"让她睡", 0, 0, 0, 0, 0, -4, 0}}},
    {6, ASH_TALK_LOW_FOOD, "半块",
     "口粮袋抖了抖，只掉出半块。\n有人把它推到你面前。\n说今天不饿。",
     {{"分半块", -1, 0, 0, 0, 0, 0, 2}, {"明天再想办法", 0, 0, 0, 0, 1, 0, -1}}},
    {7, ASH_TALK_RAIN, "接雨",
     "雨敲在铁皮上，像有人在数数。\n桶里的水快满了。\n屋里潮得发冷。",
     {{"接雨水", 0, 3, 0, 0, 0, 2, 0}, {"补屋顶", 0, 0, -1, 0, -1, 0, 0}}},
    {8, ASH_TALK_STILL, "静夜",
     "今晚没有风。\n火堆的声音显得太大了。\n远处什么都不响。",
     {{"听一会儿", 0, 0, 0, 0, -2, 1, 0}, {"早点睡", 0, 0, 0, 0, 0, -5, 0}}},
    {9, ASH_TALK_RADIO, "电台",
     "老阙把电台拧到最轻。\n杂音里有人哼歌。\n他抬头看你，没说话。",
     {{"调到人声", 0, 0, 0, -1, -2, 0, 1}, {"关掉", 0, 0, 0, 0, 1, -3, 0}}},
    {10, ASH_TALK_CORE, "发热",
     "超级核心在箱子里发热。\n隔着木板都能感觉到。\n像有什么要醒过来。",
     {{"去看一眼", 0, 0, 0, 1, 2, 0, 0}, {"不去管", 0, 0, 0, 0, -1, -2, 0}}},
    {11, ASH_TALK_CLEAR, "空椅子",
     "火堆边多了一把空椅子。\n没人去坐。\n风从门缝里进来。",
     {{"坐一会儿", 0, 0, 0, 0, 0, 2, 1}, {"去修门", 0, 0, 2, 0, 0, 4, 0}}},
};

#define TALK_N (sizeof(k_talk) / sizeof(k_talk[0]))

static bool any_injured(const ash_game_t *g)
{
    for (uint8_t i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if (g->person[i].state == ASH_PERSON_INJURED ||
            g->person[i].state == ASH_PERSON_INFECTED) return true;
    }
    return false;
}

static bool any_farm(const ash_game_t *g)
{
    for (int i = 0; i < ASHBOX_FARM_PLOTS; i++) {
        if (g->farm[i].crop != 0) return true;
    }
    return false;
}

static bool matches(const ash_game_t *g, uint8_t when)
{
    switch (when) {
    case ASH_TALK_ANY: return true;
    case ASH_TALK_FARM: return any_farm(g);
    case ASH_TALK_INJURED: return any_injured(g);
    case ASH_TALK_VISITOR: return ash_game_wanderer_pending(g);
    case ASH_TALK_THREAT: return g->threat >= 50;
    case ASH_TALK_POP2: return g->pop >= 2;
    case ASH_TALK_LOW_FOOD: return g->food <= 3;
    case ASH_TALK_RAIN: {
        const ash_phenom_t *p = ash_phenom_for_day(g->day_key);
        return p && p->id == 1;
    }
    case ASH_TALK_STILL: {
        const ash_phenom_t *p = ash_phenom_for_day(g->day_key);
        return p && p->id == 3;
    }
    case ASH_TALK_RADIO: return g->quest_done[0] != 0;
    case ASH_TALK_CORE: return g->super_core != 0;
    case ASH_TALK_CLEAR: return g->run_cleared != 0;
    default: return false;
    }
}

bool ash_talk_available(const ash_game_t *g)
{
    return g && g->day_key > 0 && (uint16_t)g->day_key != g->talk_day;
}

const ash_talk_t *ash_talk_pick(const ash_game_t *g)
{
    if (!g) return &k_talk[0];
    /* Best matching scene first, then a deterministic day pick within it. */
    const ash_talk_t *fallback = &k_talk[0];
    uint8_t pool[TALK_N];
    uint8_t n = 0;
    uint8_t best = 0;
    for (uint8_t i = 0; i < TALK_N; i++) {
        if (k_talk[i].when == ASH_TALK_ANY) continue;
        if (matches(g, k_talk[i].when)) {
            pool[n++] = i;
            if (k_talk[i].when == ASH_TALK_CLEAR) best = 1;
        }
    }
    if (n == 0) {
        for (uint8_t i = 0; i < TALK_N; i++) {
            if (k_talk[i].when == ASH_TALK_ANY) return &k_talk[i];
        }
        return fallback;
    }
    if (best) {
        for (uint8_t i = 0; i < TALK_N; i++) {
            if (k_talk[i].when == ASH_TALK_CLEAR) return &k_talk[i];
        }
    }
    return &k_talk[pool[(g->day_key * 7u + 3u) % n]];
}

void ash_talk_mark_seen(ash_game_t *g)
{
    if (!g) return;
    g->talk_day = (uint16_t)g->day_key;
}

void ash_talk_apply(ash_game_t *g, const ash_talk_t *t, uint8_t choice)
{
    if (!g || !t || choice > 1) return;
    const ash_talk_opt_t *o = &t->opt[choice];
    int v;

    v = (int)g->food + o->food;
    if (v < 0) v = 0;
    if (v > (int)ash_game_food_cap(g)) v = ash_game_food_cap(g);
    g->food = (uint16_t)v;

    v = (int)g->water + o->water;
    if (v < 0) v = 0;
    if (v > ash_game_water_cap(g)) v = ash_game_water_cap(g);
    g->water = (uint16_t)v;

    v = (int)g->parts + o->parts;
    if (v < 0) v = 0;
    if (v > ASHBOX_PARTS_CAP) v = ASHBOX_PARTS_CAP;
    g->parts = (uint16_t)v;

    v = (int)g->ember + o->ember;
    if (v < 0) v = 0;
    if (v > ASHBOX_EMBER_CAP) v = ASHBOX_EMBER_CAP;
    g->ember = (uint16_t)v;

    v = (int)g->threat + o->threat;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    g->threat = (uint8_t)v;

    for (uint8_t i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if (g->person[i].state == ASH_PERSON_AWAY) continue;
        int f = (int)g->person[i].fatigue + o->fatigue;
        if (f < 0) f = 0;
        if (f > 100) f = 100;
        g->person[i].fatigue = (uint8_t)f;
        int tr = (int)g->person[i].trust + o->trust;
        if (tr < -20) tr = -20;
        if (tr > 20) tr = 20;
        g->person[i].trust = (int8_t)tr;
    }
}
