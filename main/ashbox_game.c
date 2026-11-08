#include "ashbox_game.h"
#include "ashbox_game_loot.h"
#include "ashbox_content.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define DT_MAX_S (36LL * 3600)
#define SLEEP_NUM 1
#define SLEEP_DEN 2
#define FATIGUE_DEPART_LOCK 85

static void tick_wanderer(ash_game_t *g, int64_t dt_s);
static void roll_wanderer_day_schedule(ash_game_t *g);

static const uint32_t k_tier_s[ASH_TIER_COUNT] = {180, 3600, 14400, 7200, 28800};
static const uint8_t k_beds[6] = {0, 2, 3, 4, 5, 6};
static const uint16_t k_food_cap[6] = {0, 50, 100, 150, 200, 260};
/* 仓库独立档：低于口袋顶，逼出「扩仓」决策（放置三瓶颈） */
static const uint16_t k_stash_cap[6] = {0, 40, 100, 180, 240, 300};
static const uint16_t k_upgrade_cost[5] = {8, 20, 45, 70, 100};
static const uint16_t k_stash_upgrade_cost[3] = {10, 24, 0};
static const uint16_t k_furn_upgrade_cost[2] = {15, 22};
static const uint16_t k_gear_ember_cost[2] = {3, 5};
static const uint16_t k_gear_parts_cost[2] = {4, 8};
static const uint16_t k_quiet_gear_cost = 18;
static const uint16_t k_door_bolt_cost = 24;
static const char *k_names[ASHBOX_PERSON_MAX] = {
    "阿禾", "老阙", "小芒", "灰皮", "夜行", "旧靴",
};
static const char *k_tier_name[ASH_TIER_COUNT] = {"近路", "废街", "深处", "猎场", "过夜"};
static const char *k_place[ASH_TIER_COUNT] = {
    "货亭一带", "废弃商街", "旧地铁", "灰原猎径", "外环长道",
};
static const char *k_time_hint[ASH_TIER_COUNT] = {
    "约3分钟", "约1小时", "约4小时", "约2小时", "约8小时",
};
static const char *k_loot_hint[ASH_TIER_COUNT] = {
    "口粮2-4 · 捡零件概率约15%得1 · 基础空手而归概率约5% · 偶有种子",
    "口粮5-8 · 捡零件概率约22%得1-2 · 基础空手而归概率约12% · 偶有种子",
    "口粮9-14 · 捡零件概率约32%得1-3 · 余烬概率约35%得1 · 基础空手而归概率约18%",
    "口粮4-7干肉1-2 · 捡零件概率约10%得1 · 余烬概率约20%得1 · 基础空手而归概率约20%",
    "口粮16-22 · 捡零件概率约38%得1-4 · 余烬概率约45%得1 · 基础空手而归概率约22%",
};
#define FARM_GROW_GRAIN_S_LEGACY 14400u
#define FARM_GROW_ROOT_S_LEGACY 21600u
#define FARM_GROW_GRAIN_S 86400u
#define FARM_GROW_ROOT_S 259200u
#define STASH_STEP 5u
#define FURN_SHELF_BONUS 20

static void sync_quest2_catchup(ash_game_t *g);
static unsigned ember_pity_need(ash_tier_t tier);
static void add_fatigue(ash_person_t *p, int delta);

static uint16_t day_latch_today(const ash_game_t *g)
{
    if (!g) return ASHBOX_DAY_LATCH_NEVER;
    if (g->day_key >= (uint32_t)ASHBOX_DAY_LATCH_NEVER) {
        return (uint16_t)(ASHBOX_DAY_LATCH_NEVER - 1u);
    }
    return (uint16_t)g->day_key;
}

static uint16_t widen_day8_latch(uint8_t d8, uint32_t day_key)
{
    if (d8 == 0xFFu) return ASHBOX_DAY_LATCH_NEVER;
    if ((uint8_t)(day_key & 0xFFu) == d8) {
        if (day_key >= (uint32_t)ASHBOX_DAY_LATCH_NEVER) {
            return (uint16_t)(ASHBOX_DAY_LATCH_NEVER - 1u);
        }
        return (uint16_t)day_key;
    }
    return (uint16_t)d8;
}

void ash_game_widen_day_latches(ash_game_t *g)
{
    if (!g) return;
    g->day_latch_trade = widen_day8_latch(g->trade_day, g->day_key);
    g->day_latch_trade2 = widen_day8_latch(g->trade2_day, g->day_key);
    g->day_latch_trade_extra = widen_day8_latch(g->trade_extra_day, g->day_key);
    g->day_latch_trade2_extra = widen_day8_latch(g->trade2_extra_day, g->day_key);
    g->day_latch_duty = widen_day8_latch(g->duty_day, g->day_key);
    g->day_latch_ember = widen_day8_latch(g->ember_trade_day, g->day_key);
    g->day_latch_abx = widen_day8_latch(g->short_probe_used_today, g->day_key);
    g->pad_v11 = 0;
}

static uint16_t sat_add_u16(uint16_t a, int delta, uint16_t cap)
{
    int v = (int)a + delta;
    if (v < 0) return 0;
    if (v > (int)cap) return cap;
    return (uint16_t)v;
}

static uint8_t sat_add_u8(uint8_t a, int delta, uint8_t cap)
{
    int v = (int)a + delta;
    if (v < 0) return 0;
    if (v > (int)cap) return cap;
    return (uint8_t)v;
}

/* 在站劳动力：健康=2，虚弱=1（仍产粮，避免断粮后永久锁死） */
static int labor_weight_excluding(const ash_game_t *g, int exclude_id)
{
    int w = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if (i == exclude_id) continue;
        if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_HEALTHY) w += 2;
        else if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_WEAK) w += 1;
    }
    return w;
}

static int labor_weight(const ash_game_t *g)
{
    return labor_weight_excluding(g, -1);
}

static int count_present(const ash_game_t *g)
{
    int n = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if ((*ash_game_person_at(g, (unsigned)(i))).state != ASH_PERSON_AWAY) n++;
    }
    return n;
}

static void clamp_parts(ash_game_t *g)
{
    if (g->parts > ASHBOX_PARTS_CAP) g->parts = ASHBOX_PARTS_CAP;
    if (g->stash_parts > ASHBOX_PARTS_CAP) g->stash_parts = ASHBOX_PARTS_CAP;
}

static void clamp_economy(ash_game_t *g)
{
    int fcap = ash_game_food_cap(g);
    int scap = ash_game_stash_cap(g);
    if ((int)g->food > fcap) g->food = (uint16_t)fcap;
    if ((int)g->stash_food > scap) g->stash_food = (uint16_t)scap;
    if (g->ember > ASHBOX_EMBER_CAP) g->ember = ASHBOX_EMBER_CAP;
    for (int i = 0; i < ASHBOX_ITEM_N; i++) {
        if (g->item[i] > ASHBOX_ITEM_CAP) g->item[i] = ASHBOX_ITEM_CAP;
    }
    if (g->lore_shards > 4) g->lore_shards = 4;
    if (g->lore_shards < g->item[ASH_ITEM_SHARD]) {
        uint8_t sh = g->item[ASH_ITEM_SHARD];
        g->lore_shards = (sh > 4) ? 4 : sh;
    }
    if (g->stash_lv < 1) g->stash_lv = 1;
    if (g->stash_lv > ASHBOX_STASH_LV_MAX) g->stash_lv = ASHBOX_STASH_LV_MAX;
    if (g->furn_lv > ASHBOX_FURN_LV_MAX) g->furn_lv = ASHBOX_FURN_LV_MAX;
    if (g->seed_root > ASHBOX_SEED_ROOT_CAP) g->seed_root = ASHBOX_SEED_ROOT_CAP;
    if (g->gear_lv > ASHBOX_GEAR_LV_MAX) g->gear_lv = ASHBOX_GEAR_LV_MAX;
    if (g->rep_shed > ASHBOX_REP_MAX) g->rep_shed = ASHBOX_REP_MAX;
    if (g->rep_ferry > ASHBOX_REP_MAX) g->rep_ferry = ASHBOX_REP_MAX;
    if (g->quiet_gear > 1) g->quiet_gear = 1;
    if (g->door_bolt > 1) g->door_bolt = 1;
    if (g->lore_twist > 1) g->lore_twist = 1;
    if (g->shovel > 1) g->shovel = 1;
    if (g->well_lv > ASHBOX_WELL_LV_WELL) g->well_lv = ASHBOX_WELL_LV_WELL;
    if (g->craft_bench > 1) g->craft_bench = 1;
    if (g->iron > ASHBOX_IRON_CAP) g->iron = ASHBOX_IRON_CAP;
    {
        int wcap = ash_game_water_cap(g);
        while ((int)g->water + (int)g->stash_water > wcap) {
            if (g->stash_water > 0) g->stash_water--;
            else if (g->water > 0) g->water--;
            else break;
        }
    }
    clamp_parts(g);
    if (g->kitchen_lv >= 2) g->world_flags |= (uint8_t)ASH_WORLD_FARM;
    if ((g->lore_shards >= 4 || g->quest2_done) && !g->lore_twist) g->lore_twist = 1;
}

void ash_game_after_migrate(ash_game_t *g)
{
    if (!g) return;
    g->ver = ASHBOX_SAVE_VER;
    g->magic = ASHBOX_MAGIC;
    /* B14: never remap latch day 0 → never. Stale abx latch clears on day mismatch. */
    if (g->day_latch_abx != ASHBOX_DAY_LATCH_NEVER &&
        g->day_latch_abx != day_latch_today(g)) {
        g->day_latch_abx = ASHBOX_DAY_LATCH_NEVER;
    }
    if (g->stash_lv == 0) g->stash_lv = 1;
    if (g->chore_kind > ASHBOX_CHORE_KIND_MAX) g->chore_kind = ASHBOX_CHORE_NONE;
    if (!g->exp.active) {
        g->chore_kind = ASHBOX_CHORE_NONE;
        g->chore_done_exp = 0;
    }
    if (g->quest2_done) g->world_flags |= (uint8_t)ASH_WORLD_CAMP2;
    /* Old saves: grant starter water if the water system was never used. */
    if (g->water == 0 && g->stash_water == 0 && g->shovel == 0 && g->well_lv == 0 &&
        g->well_upgrade_remain_s == 0) {
        g->water = 40;
    }
    /* 过劳锁已废除：清掉旧倒计时，疲劳可自然回落 */
    if (g->crew_exhaust_remain_s > 0) {
        g->crew_exhaust_remain_s = 0;
        ash_game_log(g, "过劳劲过了 · 可以干活了");
    }
    if (g->farm_plots_unlocked < ASHBOX_FARM_PLOTS) g->farm_plots_unlocked = ASHBOX_FARM_PLOTS;
    if (g->farm_plots_unlocked > ASHBOX_FARM_PLOTS_MAX)
        g->farm_plots_unlocked = ASHBOX_FARM_PLOTS_MAX;
    if (g->build_cap_bonus > 2) g->build_cap_bonus = 2;
    if (g->pop > ASHBOX_PERSON_MAX) g->pop = ASHBOX_PERSON_MAX;
    if (g->pace_era > ASHBOX_PACE_V13) g->pace_era = ASHBOX_PACE_LEGACY;
    for (int qi = 0; qi < ASHBOX_QUEST_N; qi++) {
        if (g->quest_beat_day[qi] == 0) g->quest_beat_day[qi] = ASHBOX_DAY_LATCH_NEVER;
    }
    /* 旧档重伤计时曾是约10分钟自愈(600s)；现改为24小时用药窗口，到期死亡 */
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        ash_person_t *p = ash_game_person_mut(g, (unsigned)i);
        if (!p || p->state != ASH_PERSON_INJURED) continue;
        if (p->injure_remain_s == 0 || p->injure_remain_s <= 600u)
            p->injure_remain_s = ASHBOX_INJURE_DEAD_S;
    }
    clamp_economy(g);
    sync_quest2_catchup(g);
    (void)ash_game_try_clear(g);
    g->crc = ash_game_crc(g);
}

static uint32_t crc32_bytes(const uint8_t *data, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~c;
}

uint32_t ash_game_crc(const ash_game_t *g)
{
    /* Seal pads without a second full ash_game_t on the caller's stack —
     * main task is ~4KiB and load/store already keep a blob buffer. */
    static ash_game_t s_crc_tmp;
    memset(&s_crc_tmp, 0, sizeof(s_crc_tmp));
    if (g) {
        memcpy(&s_crc_tmp, g, offsetof(ash_game_t, crc));
    }
    memset(s_crc_tmp.pad_v12, 0, sizeof(s_crc_tmp.pad_v12));
    s_crc_tmp.pad_v13a = 0;
    memset(s_crc_tmp.pad_v13, 0, sizeof(s_crc_tmp.pad_v13));
    return crc32_bytes((const uint8_t *)&s_crc_tmp, sizeof(s_crc_tmp));
}

uint32_t ash_game_tier_duration(ash_tier_t tier)
{
    if ((unsigned)tier >= ASH_TIER_COUNT) return k_tier_s[0];
    return k_tier_s[tier];
}

const char *ash_game_tier_name(ash_tier_t tier)
{
    if ((unsigned)tier >= ASH_TIER_COUNT) return "?";
    return k_tier_name[tier];
}

const char *ash_game_tier_place(ash_tier_t tier)
{
    if ((unsigned)tier >= ASH_TIER_COUNT) return "?";
    return k_place[tier];
}

const char *ash_game_tier_time_hint(ash_tier_t tier)
{
    if ((unsigned)tier >= ASH_TIER_COUNT) return "?";
    return k_time_hint[tier];
}

const char *ash_game_tier_loot_hint(ash_tier_t tier)
{
    if ((unsigned)tier >= ASH_TIER_COUNT) return "?";
    return k_loot_hint[tier];
}

const char *ash_game_person_name(uint8_t id)
{
    if (id >= ASHBOX_PERSON_MAX) return "?";
    return k_names[id];
}

const char *ash_game_person_trait(uint8_t id)
{
    return ash_content_person_trait(id);
}

const char *ash_game_loadout_name(ash_loadout_t load)
{
    return ash_content_loadout_name(load);
}

const char *ash_game_loadout_hint(ash_loadout_t load)
{
    return ash_content_loadout_hint(load);
}

const char *ash_game_branch_label(uint8_t branch_id)
{
    return ash_content_branch(branch_id)->label;
}

void ash_game_branch_effect(uint8_t branch_id, char *out, size_t out_n)
{
    ash_content_branch_effect(branch_id, out, out_n);
}

const char *ash_game_crisis_label(ash_crisis_opt_t opt)
{
    return ash_content_crisis_label(opt);
}

const char *ash_game_crisis_hint(ash_crisis_opt_t opt)
{
    return ash_content_crisis_hint(opt);
}

static uint8_t quest_step_of(const ash_game_t *g, uint8_t qid)
{
    if (!g || qid >= ASHBOX_QUEST_N) return 0;
    if (qid < ASHBOX_QUEST_CORE) return g->quest_step[qid];
    return g->quest2_step;
}

static uint8_t quest_done_of(const ash_game_t *g, uint8_t qid)
{
    if (!g || qid >= ASHBOX_QUEST_N) return 1;
    if (qid < ASHBOX_QUEST_CORE) return g->quest_done[qid];
    return g->quest2_done;
}

static void quest_set_step(ash_game_t *g, uint8_t qid, uint8_t step)
{
    if (!g || qid >= ASHBOX_QUEST_N) return;
    if (qid < ASHBOX_QUEST_CORE) g->quest_step[qid] = step;
    else g->quest2_step = step;
}

static void quest_set_done(ash_game_t *g, uint8_t qid, uint8_t done)
{
    if (!g || qid >= ASHBOX_QUEST_N) return;
    if (qid < ASHBOX_QUEST_CORE) g->quest_done[qid] = done ? 1 : 0;
    else g->quest2_done = done ? 1 : 0;
}

const char *ash_game_quest_name(uint8_t qid)
{
    return ash_content_quest_name(qid);
}

const char *ash_game_quest_hint(const ash_game_t *g, uint8_t qid)
{
    if (!g || qid >= ASHBOX_QUEST_N) return "";
    uint8_t step = quest_step_of(g, qid);
    if (quest_done_of(g, qid)) step = ASHBOX_QUEST_STEPS;
    return ash_content_quest_step_hint(qid, step);
}

int ash_game_build_lv_max(const ash_game_t *g)
{
    int bonus = g ? (int)g->build_cap_bonus : 0;
    if (bonus < 0) bonus = 0;
    if (bonus > 2) bonus = 2;
    return ASHBOX_BUILD_LV_BASE + bonus;
}

int ash_game_farm_plot_count(const ash_game_t *g)
{
    int n = g ? (int)g->farm_plots_unlocked : ASHBOX_FARM_PLOTS;
    if (n < ASHBOX_FARM_PLOTS) n = ASHBOX_FARM_PLOTS;
    if (n > ASHBOX_FARM_PLOTS_MAX) n = ASHBOX_FARM_PLOTS_MAX;
    return n;
}

int ash_game_beds(const ash_game_t *g)
{
    uint8_t lv = g->dorm_lv;
    int max_lv = ash_game_build_lv_max(g);
    if (lv < 1) lv = 1;
    if ((int)lv > max_lv) lv = (uint8_t)max_lv;
    if (lv > 5) lv = 5;
    return k_beds[lv];
}

int ash_game_food_cap(const ash_game_t *g)
{
    uint8_t lv = g->kitchen_lv;
    int max_lv = ash_game_build_lv_max(g);
    if (lv < 1) lv = 1;
    if ((int)lv > max_lv) lv = (uint8_t)max_lv;
    if (lv > 5) lv = 5;
    return k_food_cap[lv];
}

int ash_game_water_cap(const ash_game_t *g)
{
    int buckets = g ? (int)g->bucket : 0;
    if (buckets < 0) buckets = 0;
    if (buckets > 20) buckets = 20;
    return (int)ASHBOX_WATER_BASE_CAP + (int)ASHBOX_WATER_BUCKET_BONUS * buckets;
}

static uint32_t expedition_hours(ash_tier_t tier)
{
    uint32_t dur = ((unsigned)tier < ASH_TIER_COUNT) ? k_tier_s[tier] : k_tier_s[0];
    uint32_t hours = (dur + 3599u) / 3600u;
    if (hours < 1u) hours = 1u;
    if (hours > 255u) hours = 255u;
    return hours;
}

uint16_t ash_game_expedition_water_cost(ash_tier_t tier)
{
    return (uint16_t)expedition_hours(tier);
}

uint16_t ash_game_expedition_food_cost(ash_tier_t tier)
{
    if (tier == ASH_TIER_SHORT) return 1u;
    return (uint16_t)(expedition_hours(tier) * 2u);
}

static int ceil_div_u(unsigned n, unsigned d)
{
    if (n == 0 || d == 0) return 0;
    return (int)((n + d - 1u) / d);
}

int ash_game_stash_slots_cap(const ash_game_t *g)
{
    static const int k_slots[4] = {0, 8, 14, 22};
    uint8_t lv = g ? g->stash_lv : 1;
    if (lv < 1) lv = 1;
    if (lv > ASHBOX_STASH_LV_MAX) lv = ASHBOX_STASH_LV_MAX;
    int cap = k_slots[lv];
    if (g && g->furn_lv >= 1) cap += 2;
    return cap;
}

int ash_game_stash_slots_used(const ash_game_t *g)
{
    if (!g) return 0;
    unsigned food = (unsigned)g->food + (unsigned)g->stash_food;
    unsigned iron = (unsigned)g->iron;
    return ceil_div_u(food, ASHBOX_STASH_SLOT_FOOD) + ceil_div_u(iron, ASHBOX_STASH_SLOT_IRON) +
           (int)g->bucket + (int)g->backpack;
}

uint16_t ash_game_total_food(const ash_game_t *g)
{
    if (!g) return 0;
    unsigned t = (unsigned)g->food + (unsigned)g->stash_food;
    return (t > 65535u) ? 65535u : (uint16_t)t;
}

uint16_t ash_game_total_water(const ash_game_t *g)
{
    if (!g) return 0;
    unsigned t = (unsigned)g->water + (unsigned)g->stash_water;
    return (t > 65535u) ? 65535u : (uint16_t)t;
}

uint16_t ash_game_total_parts(const ash_game_t *g)
{
    if (!g) return 0;
    unsigned t = (unsigned)g->parts + (unsigned)g->stash_parts;
    return (t > 65535u) ? 65535u : (uint16_t)t;
}

bool ash_game_take_food(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return amount == 0;
    if (ash_game_total_food(g) < amount) return false;
    uint16_t need = amount;
    if (g->food >= need) {
        g->food = (uint16_t)(g->food - need);
    } else {
        need = (uint16_t)(need - g->food);
        g->food = 0;
        g->stash_food = (uint16_t)(g->stash_food - need);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_take_water(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return amount == 0;
    if (ash_game_total_water(g) < amount) return false;
    uint16_t need = amount;
    if (g->water >= need) {
        g->water = (uint16_t)(g->water - need);
    } else {
        need = (uint16_t)(need - g->water);
        g->water = 0;
        g->stash_water = (uint16_t)(g->stash_water - need);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_take_parts(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return amount == 0;
    if (ash_game_total_parts(g) < amount) return false;
    uint16_t need = amount;
    if (g->parts >= need) {
        g->parts = (uint16_t)(g->parts - need);
    } else {
        need = (uint16_t)(need - g->parts);
        g->parts = 0;
        g->stash_parts = (uint16_t)(g->stash_parts - need);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_take_iron(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return amount == 0;
    if (g->iron < amount) return false;
    g->iron = (uint16_t)(g->iron - amount);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_try_pocket_refill_water(ash_game_t *g)
{
    if (!g || g->water != 0 || g->stash_water == 0) return false;
    int cap = ash_game_water_cap(g);
    int room = cap - (int)g->water;
    if (room <= 0) return false;
    int pull = (int)g->stash_water;
    if (pull > room) pull = room;
    g->stash_water = (uint16_t)((int)g->stash_water - pull);
    g->water = (uint16_t)((int)g->water + pull);
    g->crc = ash_game_crc(g);
    return true;
}

static uint16_t give_into_pocket_stash(uint16_t *pocket, uint16_t *stash, int pocket_cap,
                                       uint16_t amount, int *overflow_out)
{
    int add = (int)amount;
    int p = (int)*pocket + add;
    int ov = 0;
    if (p > pocket_cap) {
        ov = p - pocket_cap;
        p = pocket_cap;
    }
    *pocket = (uint16_t)p;
    if (ov > 0 && stash) {
        int s = (int)*stash + ov;
        *stash = (uint16_t)s;
        ov = 0;
    }
    if (overflow_out) *overflow_out = ov;
    return amount;
}

uint16_t ash_game_give_food(ash_game_t *g, uint16_t amount, bool *wasted)
{
    if (wasted) *wasted = false;
    if (!g || amount == 0) return 0;
    int fcap = ash_game_food_cap(g);
    int before_slots = ash_game_stash_slots_used(g);
    int slot_cap = ash_game_stash_slots_cap(g);
    uint16_t stored = 0;
    for (uint16_t i = 0; i < amount; i++) {
        g->food = (uint16_t)(g->food + 1);
        if ((int)g->food > fcap) {
            g->food = (uint16_t)fcap;
            g->stash_food = (uint16_t)(g->stash_food + 1);
        }
        if (ash_game_stash_slots_used(g) > slot_cap) {
            if (g->stash_food > 0) g->stash_food--;
            else if (g->food > 0) g->food--;
            if (wasted) *wasted = true;
            g->report_waste_flags = (uint8_t)(g->report_waste_flags | ASHBOX_WASTE_FOOD);
            break;
        }
        stored++;
        (void)before_slots;
    }
    g->crc = ash_game_crc(g);
    return stored;
}

uint16_t ash_game_give_water(ash_game_t *g, uint16_t amount, bool *wasted)
{
    if (wasted) *wasted = false;
    if (!g || amount == 0) return 0;
    int cap = ash_game_water_cap(g);
    unsigned total = (unsigned)g->water + (unsigned)g->stash_water;
    unsigned room = (total >= (unsigned)cap) ? 0u : (unsigned)cap - total;
    uint16_t take = amount;
    if ((unsigned)take > room) {
        take = (uint16_t)room;
        if (wasted) *wasted = true;
        g->report_waste_flags = (uint8_t)(g->report_waste_flags | ASHBOX_WASTE_WATER);
    }
    int ov = 0;
    give_into_pocket_stash(&g->water, &g->stash_water, cap, take, &ov);
    (void)ov;
    /* keep pocket within cap; excess already in stash; trim total to cap */
    while ((int)g->water + (int)g->stash_water > cap && g->stash_water > 0) g->stash_water--;
    while ((int)g->water > cap) g->water--;
    g->crc = ash_game_crc(g);
    return take;
}

uint16_t ash_game_give_parts(ash_game_t *g, uint16_t amount, bool *wasted)
{
    if (wasted) *wasted = false;
    if (!g || amount == 0) return 0;
    unsigned total = (unsigned)g->parts + (unsigned)g->stash_parts;
    unsigned room = (total >= ASHBOX_PARTS_CAP) ? 0u : ASHBOX_PARTS_CAP - total;
    uint16_t take = amount;
    if ((unsigned)take > room) {
        take = (uint16_t)room;
        if (wasted) *wasted = true;
        g->report_waste_flags = (uint8_t)(g->report_waste_flags | ASHBOX_WASTE_PARTS);
    }
    for (uint16_t i = 0; i < take; i++) {
        if (g->parts < ASHBOX_PARTS_CAP) g->parts++;
        else if (g->stash_parts < ASHBOX_PARTS_CAP) g->stash_parts++;
    }
    g->crc = ash_game_crc(g);
    return take;
}

uint16_t ash_game_give_iron(ash_game_t *g, uint16_t amount, bool *wasted)
{
    if (wasted) *wasted = false;
    if (!g || amount == 0) return 0;
    int slot_cap = ash_game_stash_slots_cap(g);
    uint16_t stored = 0;
    for (uint16_t i = 0; i < amount; i++) {
        if (g->iron >= ASHBOX_IRON_CAP) {
            if (wasted) *wasted = true;
            g->report_waste_flags = (uint8_t)(g->report_waste_flags | ASHBOX_WASTE_IRON);
            break;
        }
        g->iron++;
        if (ash_game_stash_slots_used(g) > slot_cap) {
            g->iron--;
            if (wasted) *wasted = true;
            g->report_waste_flags = (uint8_t)(g->report_waste_flags | ASHBOX_WASTE_IRON);
            break;
        }
        stored++;
    }
    g->crc = ash_game_crc(g);
    return stored;
}

int ash_game_crowbar_carry_cap(const ash_game_t *g, uint8_t person_id)
{
    if (!g || person_id >= ASHBOX_PERSON_MAX) return 1;
    if ((*ash_game_equip_bp_at(g, (unsigned)(person_id)))) return 2;
    return 1;
}

int ash_game_pop_cap(const ash_game_t *g)
{
    if (!g) return 1;
    if (g->pop == 0) return 1; /* wipeout: always allow first wanderer */
    int cap = 1;
    if (g->kitchen_lv >= 2) cap = 2;
    if (g->workshop_lv >= 2) cap = 3;
    int beds = ash_game_beds(g);
    if (cap > beds) cap = beds;
    return cap;
}

uint16_t ash_game_pit_cost(const ash_game_t *g)
{
    (void)g;
    return ASHBOX_PIT_COST_PARTS;
}

uint16_t ash_game_well_upgrade_cost(const ash_game_t *g)
{
    (void)g;
    return ASHBOX_WELL_UPGRADE_COST_PARTS;
}

bool ash_game_work_locked(const ash_game_t *g)
{
    (void)g;
    /* 升井只停井产水；过劳锁已废除。API 保留给 UI/旧调用。 */
    return false;
}

static void fmt_remain(uint32_t s, char *out, size_t n)
{
    if (!out || n == 0) return;
    if (s >= 3600u) {
        unsigned h = (unsigned)((s + 3599u) / 3600u);
        snprintf(out, n, "约%u小时", h);
    } else {
        unsigned m = (unsigned)((s + 59u) / 60u);
        if (m < 1u) m = 1u;
        snprintf(out, n, "约%u分钟", m);
    }
}

bool ash_game_block_reason(const ash_game_t *g, ash_act_t act, char *out, size_t out_n)
{
    if (!out || out_n == 0) return false;
    out[0] = '\0';
    if (!g) {
        snprintf(out, out_n, "状态异常");
        return true;
    }
    char rem[16];
    if (act == ASH_ACT_WELL_UPGRADE) {
        if (g->well_upgrade_remain_s > 0) {
            fmt_remain(g->well_upgrade_remain_s, rem, sizeof(rem));
            snprintf(out, out_n, "正在升井 · 还要%s", rem);
            return true;
        }
        if (g->exp.active) {
            snprintf(out, out_n, "人在外面时不能升井");
            return true;
        }
        if (g->well_lv != ASHBOX_WELL_LV_PIT) {
            snprintf(out, out_n, "要先挖好深坑");
            return true;
        }
        if (g->parts < ash_game_well_upgrade_cost(g)) {
            snprintf(out, out_n, "零件不够 · 还要%u", (unsigned)ash_game_well_upgrade_cost(g));
            return true;
        }
    }
    if (act == ASH_ACT_DEPART) {
        if (g->food == 0) {
            snprintf(out, out_n, "没有口粮 · 不能出发");
            return true;
        }
        /* water cost checked by caller with tier; generic lack: */
        if (g->water == 0) {
            snprintf(out, out_n, "没有水 · 不能出发");
            return true;
        }
    }
    if (act == ASH_ACT_BUILD && !g->shovel && g->well_lv == 0) {
        snprintf(out, out_n, "先出门带回铁铲");
        return true;
    }
    return false;
}

static int station_pop_for_water(const ash_game_t *g)
{
    int n = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_AWAY) continue;
        n++;
    }
    return n;
}

/* UTC+8 civil hour 0..23 from unix seconds; -1 if clock untrusted. */
static int cst_hour_trusted(int64_t unix_s)
{
    if (unix_s < 1600000000LL) return -1; /* before ~2020: treat untrusted */
    int64_t local = unix_s + 8 * 3600;
    if (local < 0) local = 0;
    return (int)((local % 86400LL) / 3600LL);
}

static bool hour_consumes_water(int hour)
{
    if (hour < 0) return true; /* untrusted: always consume */
    return !(hour >= 22 || hour < 7);
}

static void add_water_with_overflow(ash_game_t *g, int add)
{
    if (add <= 0) return;
    int cap = ash_game_water_cap(g);
    int w = (int)g->water + add;
    if (w > cap) {
        int ov = w - cap;
        w = cap;
        int st = (int)g->stash_water + ov;
        if (st > (int)ASHBOX_WATER_CAP) st = (int)ASHBOX_WATER_CAP;
        g->stash_water = (uint16_t)st;
    }
    if (w < 0) w = 0;
    g->water = (uint16_t)w;
}

static void tick_water_economy(ash_game_t *g, int64_t dt_s, int sleep_num, int sleep_den)
{
    if (!g || dt_s <= 0) return;
    if (sleep_num <= 0) sleep_num = 1;
    if (sleep_den <= 0) sleep_den = 1;

    /* Upgrade timer (wall clock); completion no longer locks the station. */
    if (g->well_upgrade_remain_s > 0) {
        if ((uint32_t)dt_s >= g->well_upgrade_remain_s) {
            g->well_upgrade_remain_s = 0;
            g->well_lv = ASHBOX_WELL_LV_WELL;
            g->crew_exhaust_remain_s = 0;
            for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
                add_fatigue(&(*ash_game_person_mut(g, (unsigned)(i))), (int)ASHBOX_WELL_DONE_FATIGUE);
            }
            ash_game_log(g, "水井完工了 · 大家累一阵还能出门");
        } else {
            g->well_upgrade_remain_s -= (uint32_t)dt_s;
        }
    }
    /* Legacy exhaust field: drain silently if any old save still has it. */
    if (g->crew_exhaust_remain_s > 0) {
        if ((uint32_t)dt_s >= g->crew_exhaust_remain_s) g->crew_exhaust_remain_s = 0;
        else g->crew_exhaust_remain_s -= (uint32_t)dt_s;
    }

    /* Consume: 1 water / present person / 2h outside 22:00-07:00 CST; deep sleep halves. */
    int pop = station_pop_for_water(g);
    if (pop > 0) {
        int64_t consume_s = 0;
        int start_h = cst_hour_trusted(g->last_settle_unix);
        if (start_h < 0) {
            consume_s = dt_s;
        } else {
            int64_t left = dt_s;
            int64_t t = g->last_settle_unix;
            while (left > 0) {
                int h = cst_hour_trusted(t);
                int64_t into = 3600 - (t + 8 * 3600) % 3600;
                if (into <= 0) into = 3600;
                if (into > left) into = left;
                if (hour_consumes_water(h)) consume_s += into;
                t += into;
                left -= into;
            }
        }
        consume_s = consume_s * (int64_t)sleep_num / (int64_t)sleep_den;
        int64_t use = (int64_t)pop * consume_s / (2 * 3600);
        if (use > 0) {
            int w = (int)g->water - (int)use;
            if (w < 0) {
                int need = -w;
                w = 0;
                if ((int)g->stash_water >= need) {
                    g->stash_water = (uint16_t)(g->stash_water - (uint16_t)need);
                } else {
                    need -= (int)g->stash_water;
                    g->stash_water = 0;
                    (void)need;
                }
            }
            g->water = (uint16_t)w;
        }
    }

    /* Produce from pit/well (paused while upgrading); respect per-UTC-day cap. */
    if (g->well_upgrade_remain_s == 0 &&
        (g->well_lv == ASHBOX_WELL_LV_PIT || g->well_lv == ASHBOX_WELL_LV_WELL)) {
        uint16_t rate = (g->well_lv == ASHBOX_WELL_LV_WELL) ? ASHBOX_WELL_WATER_PER_H
                                                            : ASHBOX_PIT_WATER_PER_H;
        uint16_t day_cap = (g->well_lv == ASHBOX_WELL_LV_WELL) ? ASHBOX_WELL_WATER_DAY_CAP
                                                               : ASHBOX_PIT_WATER_DAY_CAP;
        int64_t left = dt_s;
        int64_t t = g->last_settle_unix;
        while (left > 0) {
            int64_t into = 86400 - (t % 86400);
            if (into <= 0) into = 86400;
            if (into > left) into = left;
            /* New UTC day fragment starts: refresh daily counter. */
            if (t % 86400 == 0 && t != g->last_settle_unix) g->water_made_today = 0;
            int64_t made = (int64_t)rate * into / 3600;
            if (made > 0) {
                uint16_t room = (g->water_made_today >= day_cap)
                                    ? 0
                                    : (uint16_t)(day_cap - g->water_made_today);
                if (made > room) made = room;
                if (made > 0) {
                    g->water_made_today =
                        (uint16_t)(g->water_made_today + (uint16_t)made);
                    add_water_with_overflow(g, (int)made);
                }
            }
            t += into;
            left -= into;
            if (left > 0) g->water_made_today = 0; /* crossing into next day */
        }
    }
}

bool ash_game_build_pit(ash_game_t *g)
{
    if (!g || !g->shovel) return false;
    if (g->well_lv != ASHBOX_WELL_LV_NONE) return false;
    if (ash_game_work_locked(g)) return false;
    uint16_t cost = ash_game_pit_cost(g);
    if (g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->well_lv = ASHBOX_WELL_LV_PIT;
    ash_game_log(g, "挖好了深坑 · 开始往下渗水");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_start_well_upgrade(ash_game_t *g)
{
    if (!g) return false;
    if (g->exp.active || g->report.pending || g->crisis.pending) return false;
    if (g->well_lv != ASHBOX_WELL_LV_PIT) return false;
    if (g->well_upgrade_remain_s > 0) return false;
    if (g->crew_exhaust_remain_s > 0) return false;
    uint16_t cost = ash_game_well_upgrade_cost(g);
    if (g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->well_upgrade_remain_s = ASHBOX_WELL_UPGRADE_S;
    ash_game_log(g, "开始升井 · 这口井停产一天");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_stash_deposit_water(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return false;
    if (g->water < amount) return false;
    if ((uint32_t)g->stash_water + amount > ASHBOX_WATER_CAP) return false;
    g->water = (uint16_t)(g->water - amount);
    g->stash_water = (uint16_t)(g->stash_water + amount);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_stash_withdraw_water(ash_game_t *g, uint16_t amount)
{
    if (!g || amount == 0) return false;
    if (g->stash_water < amount) return false;
    int cap = ash_game_water_cap(g);
    if ((int)g->water + (int)amount > cap) return false;
    g->stash_water = (uint16_t)(g->stash_water - amount);
    g->water = (uint16_t)(g->water + amount);
    g->crc = ash_game_crc(g);
    return true;
}

int ash_game_stash_cap(const ash_game_t *g)
{
    uint8_t lv = g ? g->stash_lv : 1;
    if (lv < 1) lv = 1;
    if (lv > ASHBOX_STASH_LV_MAX) lv = ASHBOX_STASH_LV_MAX;
    int cap = (int)k_stash_cap[lv];
    if (g && g->furn_lv >= 1) cap += FURN_SHELF_BONUS;
    if (g && g->stash_food_bonus) cap += (int)g->stash_food_bonus;
    return cap;
}

uint16_t ash_game_upgrade_cost(const ash_game_t *g, int building)
{
    uint8_t lv = 1;
    if (building == 0) lv = g->dorm_lv;
    else if (building == 1) lv = g->kitchen_lv;
    else if (building == 2) lv = g->workshop_lv;
    else return 0;
    int max_lv = ash_game_build_lv_max(g);
    if (lv < 1 || (int)lv >= max_lv) return 0;
    if (lv > 5) return 0;
    return k_upgrade_cost[lv - 1];
}

uint16_t ash_game_stash_upgrade_cost(const ash_game_t *g)
{
    if (!g) return 0;
    uint8_t lv = g->stash_lv;
    if (lv < 1) lv = 1;
    if (lv >= ASHBOX_STASH_LV_MAX) return 0;
    return k_stash_upgrade_cost[lv - 1];
}

uint16_t ash_game_furn_upgrade_cost(const ash_game_t *g)
{
    if (!g || g->furn_lv >= ASHBOX_FURN_LV_MAX) return 0;
    return k_furn_upgrade_cost[g->furn_lv];
}

bool ash_game_root_unlocked(const ash_game_t *g)
{
    return g && (g->world_flags & ASH_WORLD_CAMP2) != 0;
}

const char *ash_game_crop_name(ash_crop_t crop)
{
    if (crop == ASH_CROP_GRAIN) return "稻谷";
    if (crop == ASH_CROP_ROOT) return "红薯";
    return "空田地";
}

bool ash_game_stash_full(const ash_game_t *g)
{
    if (!g) return false;
    return ash_game_stash_slots_used(g) >= ash_game_stash_slots_cap(g);
}

int ash_game_farm_ready_count(const ash_game_t *g)
{
    if (!g) return 0;
    if (!(g->kitchen_lv >= 2 || (g->world_flags & ASH_WORLD_FARM))) return 0;
    int n = 0;
    for (int i = 0; i < ash_game_farm_plot_count(g); i++) {
        if ((*ash_game_farm_at(g, (unsigned)(i))).crop != 0 && (*ash_game_farm_at(g, (unsigned)(i))).remain_s == 0) n++;
    }
    return n;
}

int ash_game_station_healthy(const ash_game_t *g)
{
    int n = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_HEALTHY) n++;
    }
    return n;
}

static bool any_weak_or_no_food(const ash_game_t *g)
{
    /* 仓有粮不算断粮：回补前勿用「灶冷」吓玩家 */
    if (g->food == 0 && g->stash_food == 0) return true;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_WEAK) return true;
    }
    return false;
}

static int farm_empty_count(const ash_game_t *g)
{
    if (!ash_game_farm_unlocked(g)) return 0;
    int n = 0;
    for (int i = 0; i < ash_game_farm_plot_count(g); i++) {
        if ((*ash_game_farm_at(g, (unsigned)(i))).crop == 0) n++;
    }
    return n;
}

static bool can_plant_any(const ash_game_t *g)
{
    if (farm_empty_count(g) == 0) return false;
    if (g->item[ASH_ITEM_SEED] > 0) return true;
    if (ash_game_root_unlocked(g) && g->seed_root > 0) return true;
    return false;
}

static bool need_seed_via_trade(const ash_game_t *g)
{
    if (farm_empty_count(g) == 0) return false;
    if (g->item[ASH_ITEM_SEED] > 0) return false;
    if (ash_game_root_unlocked(g) && g->seed_root > 0) return false;
    if (ash_game_trade_available(g) && ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_SEED))
        return true;
    if (ash_game_trade2_available(g) &&
        (ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_ROOT) ||
         ash_game_trade_can_pay(g, ASH_TRADE_PARTS_FOR_ROOT)))
        return true;
    return false;
}

/* 有货可换才催交易，避免「今日可换」整日盖死电台氛围 */
static bool trade_hook_worth(const ash_game_t *g)
{
    if (need_seed_via_trade(g)) return true;
    if (ash_game_trade_available(g)) {
        if (ash_game_trade_can_pay(g, ASH_TRADE_FOOD_FOR_WATER) ||
            ash_game_trade_can_pay(g, ASH_TRADE_IRON_FOR_PARTS) ||
            ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_SEED))
            return true;
    }
    if (ash_game_trade2_available(g)) {
        if (ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_ROOT) ||
            ash_game_trade_can_pay(g, ASH_TRADE_PARTS_FOR_ROOT))
            return true;
    }
    return false;
}

static bool all_plots_empty(const ash_game_t *g)
{
    return ash_game_farm_unlocked(g) && farm_empty_count(g) == ash_game_farm_plot_count(g);
}

bool ash_game_try_pocket_refill(ash_game_t *g)
{
    bool any = ash_game_try_pocket_refill_water(g);
    if (!g) return any;
    if (g->food != 0 || g->stash_food == 0) return any;
    int cap = ash_game_food_cap(g);
    int pull = (int)g->stash_food;
    if (pull > cap) pull = cap;
    if (pull <= 0) return any;
    g->food = (uint16_t)pull;
    g->stash_food = (uint16_t)((int)g->stash_food - pull);
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_home_nudge(const ash_game_t *g)
{
    static char s_nudge[64];
    if (!g) return "";
    if (g->hide_penalty_h > 0) {
        snprintf(s_nudge, sizeof(s_nudge), "躲藏中 · 产粮减半 · 约还要%u小时",
                 (unsigned)g->hide_penalty_h);
        return s_nudge;
    }
    if (g->well_upgrade_remain_s > 0) {
        snprintf(s_nudge, sizeof(s_nudge), "升井中 · 井先停产 · 别的还能干");
        return s_nudge;
    }
    if (ash_game_total_water(g) == 0) return "没水了 · 先找水再出门";
    if (ash_game_total_food(g) == 0 && g->item[ASH_ITEM_JERKY] > 0) return "干肉还能换成热口粮";
    if (ash_game_total_food(g) == 0) return "先把口粮补回来 · 别硬撑";
    if (g->pop == 0) return "站里空了 · 等过路客上门";
    if (g->wanderer_pending) return "门外有人 · 先留下或打发";
    if (g->pace_era == ASHBOX_PACE_V13 && g->expedition_total < 2 && !g->exp.active)
        return "今天先做：派阿禾走近路";
    if (g->shovel && g->well_lv == ASHBOX_WELL_LV_NONE && g->parts >= ash_game_pit_cost(g))
        return "有铁铲了 · 去建设里挖深坑";
    if (g->well_lv == ASHBOX_WELL_LV_PIT && g->well_upgrade_remain_s == 0 &&
        g->parts >= ash_game_well_upgrade_cost(g) && !g->exp.active)
        return "深坑可以升级成水井 · 升井当天井停产一天";
    if (!g->exp.active && !g->report.pending && !g->crisis.pending && g->food > 0 &&
        !ash_game_can_depart(g))
        return "人太累了 · 先歇一阵再出门";
    if (ash_game_farm_ready_count(g) > 0) {
        snprintf(s_nudge, sizeof(s_nudge), "有%d块田地熟了 · 该收了",
                 ash_game_farm_ready_count(g));
        return s_nudge;
    }
    if (ash_game_stash_full(g) && !g->quiet_gear && g->parts >= ash_game_quiet_gear_cost(g))
        return "零件够了 · 可装静音轮 · 威胁涨得慢";
    if (g->quiet_gear && !g->door_bolt && g->parts >= ash_game_door_bolt_cost(g))
        return "零件够了 · 可装门扣 · 威胁涨得更慢";
    if (g->door_bolt && !ash_game_has_stove_lining(g) &&
        g->parts >= ash_game_stove_lining_cost(g))
        return "零件够了 · 可装灶层 · 口粮更省";
    if (g->gear_lv >= ASHBOX_GEAR_LV_MAX && !ash_game_has_box_lamp(g) &&
        g->ember >= ash_game_box_lamp_cost(g))
        return "余烬够了 · 可装匣灯 · 空手而归时威胁更轻";
    if (g->furn_lv == 1 && g->item[ASH_ITEM_TOKEN] > 0 &&
        g->parts >= ash_game_furn_upgrade_cost(g))
        return "有信物 · 可装干燥架催熟菜地";
    if (g->item[ASH_ITEM_TOKEN] > 0 && g->furn_lv < 1)
        return "有信物可装干燥架 · 要先装货架";
    if (ash_game_stash_full(g)) return "仓满了 · 先清仓或去交易";
    if (g->food <= 6 && g->item[ASH_ITEM_JERKY] > 0) return "口粮见底了 · 干肉可换成口粮";
    if (need_seed_via_trade(g)) return "没有种子了 · 去交易看看";
    if (ash_game_trade2_available(g) && trade_hook_worth(g)) return "渡口今天还能再换";
    if (ash_game_trade_available(g) && trade_hook_worth(g)) return "铁棚今天还能再换";
    if (!quest_done_of(g, 2) && quest_step_of(g, 2) == 2 && g->item[ASH_ITEM_SHARD] >= 2)
        return "灰片够了 · 可交出去推进灰潮";
    if (!ash_game_stash_full(g) && all_plots_empty(g) && can_plant_any(g))
        return "空田地在等你 · 去菜地播种";
    if (!g->gate_unlocked && g->ember >= 3)
        return "余烬攒满3 · 去建设开闸 · 开后可去猎场";
    if (g->gate_unlocked && g->gear_lv < ASHBOX_GEAR_LV_MAX &&
        g->ember >= ash_game_gear_upgrade_cost_ember(g) &&
        g->parts >= ash_game_gear_upgrade_cost_parts(g))
        return "余烬和零件够了 · 可升级猎装";
    if (g->threat >= 60 && g->threat < 80) {
        if (g->night_order == ASH_ORDER_NONE) return "威胁升高了 · 先设好过夜站令";
        return "威胁升高了 · 夜里多留心";
    }
    if (g->pop == 0) return "站里空了 · 等过路客上门";
    if (ash_game_farm_unlocked(g) && farm_empty_count(g) > 0 && g->item[ASH_ITEM_SEED] == 0 &&
        g->seed_root == 0)
        return "空田地还在 · 先去弄种子";
    /* 已推进的线索才抢主页，避免开线文案盖住近路节奏 */
    for (uint8_t q = 0; q < ASHBOX_QUEST_N; q++) {
        if (quest_done_of(g, q)) continue;
        uint8_t step = quest_step_of(g, q);
        if (step == 0) continue;
        const char *hint = ash_content_quest_step_hint(q, step);
        if (hint && hint[0]) {
            snprintf(s_nudge, sizeof(s_nudge), "%s", hint);
            return s_nudge;
        }
    }
    /* 日勤软钩：低于线索与硬经营钩，给醒着玩家可见余次 */
    if (ash_game_duty_left(g) > 0 && !g->exp.active) {
        snprintf(s_nudge, sizeof(s_nudge), "日勤还剩%u次 · 醒着也能忙",
                 ash_game_duty_left(g));
        return s_nudge;
    }
    if (g->wanderer_pending) return "门外有人 · 先留下或打发";
    if (g->pace_era == ASHBOX_PACE_V13 && !quest_done_of(g, 0)) {
        snprintf(s_nudge, sizeof(s_nudge), "三线进度 · 铁皮%u/5",
                 (unsigned)quest_step_of(g, 0));
        return s_nudge;
    }
    return "据点还稳 · 可以再出门一趟";
}

bool ash_game_chore_pending(const ash_game_t *g)
{
    return g && g->exp.active && g->chore_kind >= ASHBOX_CHORE_FAN &&
           g->chore_kind <= ASHBOX_CHORE_KIND_MAX;
}

const char *ash_game_chore_label(const ash_game_t *g)
{
    if (!ash_game_chore_pending(g)) return "";
    const ash_tier_t tier = g->exp.active ? g->exp.tier : ASH_TIER_SHORT;
    switch (g->chore_kind) {
    case ASHBOX_CHORE_FAN:
        if (tier == ASH_TIER_HUNT) return "晾干肉绳";
        if (tier == ASH_TIER_NIGHT) return "听外环潮";
        if (tier == ASH_TIER_DEEP) return "擦匣盖灰";
        return "拧紧风扇";
    case ASHBOX_CHORE_STOVE:
        if (tier == ASH_TIER_HUNT || tier == ASH_TIER_NIGHT) return "温一锅薄汤";
        return "清理灶边";
    case ASHBOX_CHORE_BOX:
        if (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT) return "听匣里声";
        return "按实匣盖";
    case ASHBOX_CHORE_GATE:
        if (tier == ASH_TIER_NIGHT) return "看闸门外环";
        if (tier == ASH_TIER_HUNT) return "闻反潮味";
        return "塞紧门缝";
    default:
        return "站内事";
    }
}

const char *ash_game_chore_hint(const ash_game_t *g)
{
    if (!ash_game_chore_pending(g)) return "";
    const ash_tier_t tier = g->exp.active ? g->exp.tier : ASH_TIER_SHORT;
    switch (g->chore_kind) {
    case ASHBOX_CHORE_FAN:
        if (tier == ASH_TIER_HUNT) return "零件+1 · 干肉绳稳了";
        if (tier == ASH_TIER_NIGHT) return "零件+1 · 潮声远了点";
        if (tier == ASH_TIER_DEEP) return "零件+1 · 匣盖不响";
        return "零件+1 · 站里稳一点";
    case ASHBOX_CHORE_STOVE:
        if (tier == ASH_TIER_HUNT || tier == ASH_TIER_NIGHT) return "口粮+1 · 薄汤还温";
        return "口粮+1 · 灶还温着";
    case ASHBOX_CHORE_BOX:
        if (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT) return "威胁-1 · 匣声静了";
        return "威胁-1 · 匣盖按实";
    case ASHBOX_CHORE_GATE:
        if (tier == ASH_TIER_NIGHT) return "威胁-2 · 外环脚印旧";
        if (tier == ASH_TIER_HUNT) return "威胁-2 · 反潮味轻了";
        return "威胁-2 · 缝里不进风";
    default:
        return "处理站内事";
    }
}

bool ash_game_resolve_chore(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g)) return false;
    if (!ash_game_chore_pending(g)) return false;
    uint8_t kind = g->chore_kind;
    const ash_tier_t tier = g->exp.active ? g->exp.tier : ASH_TIER_SHORT;
    g->chore_kind = ASHBOX_CHORE_NONE;
    if (g->chore_done_exp < 255) g->chore_done_exp++;
    switch (kind) {
    case ASHBOX_CHORE_FAN:
        g->parts = sat_add_u16(g->parts, 1, ASHBOX_PARTS_CAP);
        if (tier == ASH_TIER_HUNT) ash_game_log(g, "干肉绳晾稳 · 风扇不响");
        else if (tier == ASH_TIER_NIGHT) ash_game_log(g, "听外环潮 · 风扇拧稳了");
        else if (tier == ASH_TIER_DEEP) ash_game_log(g, "匣盖灰擦开 · 风扇不响");
        else ash_game_log(g, "风扇拧稳不再响");
        break;
    case ASHBOX_CHORE_STOVE: {
        int cap = ash_game_food_cap(g);
        int scap = ash_game_stash_cap(g);
        int food = (int)g->food + 1;
        if (food > cap) {
            int ov = food - cap;
            food = cap;
            int st = (int)g->stash_food + ov;
            if (st > scap) st = scap;
            g->stash_food = (uint16_t)st;
        }
        g->food = (uint16_t)food;
        if (tier == ASH_TIER_HUNT || tier == ASH_TIER_NIGHT)
            ash_game_log(g, "薄汤温着 · 灶边清开了");
        else
            ash_game_log(g, "灶灰清开 · 汤还能温");
        break;
    }
    case ASHBOX_CHORE_BOX: {
        int th = (int)g->threat - 1;
        if (th < 0) th = 0;
        g->threat = (uint8_t)th;
        if (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT)
            ash_game_log(g, "听过匣声 · 低语小了");
        else
            ash_game_log(g, "匣盖按实 · 低语小了");
        break;
    }
    case ASHBOX_CHORE_GATE: {
        int th = (int)g->threat - 2;
        if (th < 0) th = 0;
        g->threat = (uint8_t)th;
        if (tier == ASH_TIER_NIGHT) ash_game_log(g, "闸外脚印不新鲜");
        else if (tier == ASH_TIER_HUNT) ash_game_log(g, "反潮味轻了 · 缝塞紧");
        else ash_game_log(g, "门缝塞紧 · 风进不来");
        break;
    }
    default:
        break;
    }
    /* 站里有人看见你在做事，微抬在站者信任（有盖） */
    for (uint8_t i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if ((*ash_game_person_mut(g, (unsigned)(i))).state == ASH_PERSON_AWAY) continue;
        if ((*ash_game_person_mut(g, (unsigned)(i))).trust < 3) {
            (*ash_game_person_mut(g, (unsigned)(i))).trust = (int8_t)((*ash_game_person_mut(g, (unsigned)(i))).trust + 1);
            break;
        }
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

static void sync_duty_day(ash_game_t *g)
{
    if (!g) return;
    uint16_t today = day_latch_today(g);
    if (g->day_latch_duty != today) {
        g->day_latch_duty = today;
        g->duty_count = 0;
    }
}

unsigned ash_game_duty_left(const ash_game_t *g)
{
    if (!g) return 0;
    uint16_t today = day_latch_today(g);
    uint8_t count = g->duty_count;
    if (g->day_latch_duty != today) count = 0;
    if (count >= ASHBOX_DUTY_MAX) return 0;
    return (unsigned)(ASHBOX_DUTY_MAX - count);
}

bool ash_game_duty_available(const ash_game_t *g)
{
    if (!g) return false;
    if (g->exp.active || g->report.pending || g->crisis.pending) return false;
    if (ash_game_work_locked(g)) return false;
    return ash_game_duty_left(g) > 0;
}

const char *ash_game_duty_label(uint8_t kind)
{
    switch (kind) {
    case ASHBOX_DUTY_TUNE: return "听广播";
    case ASHBOX_DUTY_GATE: return "查看闸门";
    case ASHBOX_DUTY_TALLY: return "清点仓库";
    case ASHBOX_DUTY_STOVE: return "看灶火";
    default: return "日勤";
    }
}

const char *ash_game_duty_hint(uint8_t kind)
{
    switch (kind) {
    case ASHBOX_DUTY_TUNE: return "纪事一句 · 威胁或静";
    case ASHBOX_DUTY_GATE: return "威胁-1 · 看清外面";
    case ASHBOX_DUTY_TALLY: return "清点仓容 · 或得零件";
    case ASHBOX_DUTY_STOVE: return "口粮+1 · 灶还温";
    default: return "站内日勤";
    }
}

bool ash_game_do_duty(ash_game_t *g, uint8_t kind)
{
    if (!ash_game_duty_available(g)) return false;
    if (kind >= ASHBOX_DUTY_KIND_N) return false;
    sync_duty_day(g);
    if (g->duty_count >= ASHBOX_DUTY_MAX) return false;

    switch (kind) {
    case ASHBOX_DUTY_TUNE: {
        /* G4：三线了结后听广播走守望句 */
        if (g->lore_twist && quest_done_of(g, 0) && quest_done_of(g, 1) &&
            quest_done_of(g, 2)) {
            unsigned wi =
                (g->day_key * 5u + g->duty_count * 3u + g->expedition_total) %
                ash_content_radio_watch_count();
            ash_game_log(g, ash_content_radio_watch(wi));
            break;
        }
        unsigned roll = (g->day_key * 7u + g->duty_count * 3u + g->expedition_total) % 6u;
        if (roll == 0u) {
            int th = (int)g->threat - 1;
            if (th < 0) th = 0;
            g->threat = (uint8_t)th;
            ash_game_log(g, "调到旧声 · 站里静了");
        } else if (roll == 1u) {
            ash_game_log(g, "广播里有人唤搜救");
        } else if (roll == 2u) {
            ash_game_log(g, "频率里全是沙沙");
        } else if (roll == 3u) {
            ash_game_log(g, "听见渡口潮声");
        } else if (roll == 4u) {
            ash_game_log(g, "匣频里余烬跳了一下");
        } else {
            ash_game_log(g, "外环风声贴着铁皮走");
        }
        break;
    }
    case ASHBOX_DUTY_GATE: {
        int th = (int)g->threat - 1;
        if (th < 0) th = 0;
        g->threat = (uint8_t)th;
        ash_game_log(g, "闸外脚印不新鲜");
        break;
    }
    case ASHBOX_DUTY_TALLY: {
        unsigned roll = (g->day_key + g->stash_food + g->duty_count) % 5u;
        if (roll == 0u) {
            g->parts = sat_add_u16(g->parts, 1, ASHBOX_PARTS_CAP);
            ash_game_log(g, "仓边捡回一枚螺丝");
        } else if (ash_game_stash_full(g)) {
            ash_game_log(g, "仓已满了 · 该扩了");
        } else {
            ash_game_log(g, "仓账对过 · 先放下");
        }
        break;
    }
    case ASHBOX_DUTY_STOVE: {
        int cap = ash_game_food_cap(g);
        int scap = ash_game_stash_cap(g);
        int food = (int)g->food + 1;
        if (food > cap) {
            int ov = food - cap;
            food = cap;
            int st = (int)g->stash_food + ov;
            if (st > scap) st = scap;
            g->stash_food = (uint16_t)st;
        }
        g->food = (uint16_t)food;
        ash_game_log(g, "灶里还有余温");
        break;
    }
    default:
        return false;
    }

    g->duty_count = (uint8_t)(g->duty_count + 1u);
    /* 日勤短活：给在站里最不累的人加一点疲劳 */
    {
        int best = -1;
        uint8_t best_f = 255;
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            ash_person_t *p = ash_game_person_mut(g, (unsigned)i);
            if (!p || p->state == ASH_PERSON_AWAY) continue;
            if (p->fatigue < best_f) {
                best_f = p->fatigue;
                best = i;
            }
        }
        if (best >= 0) add_fatigue(ash_game_person_mut(g, (unsigned)best), ASHBOX_LABOR_FATIGUE_PER_H);
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_wait_nudge(const ash_game_t *g)
{
    static char s_wait[48];
    if (!g || !g->exp.active) return "";
    if (ash_game_chore_pending(g)) {
        snprintf(s_wait, sizeof(s_wait), "%s", ash_game_chore_label(g));
        return s_wait;
    }
    if (g->threat >= 50 && g->night_order == ASH_ORDER_NONE) return "先设好过夜站令再睡";
    if (ash_game_farm_ready_count(g) > 0) return "田地熟了 · 去据点收";
    if (ash_game_stash_full(g)) return "仓满了 · 先清仓或去交易";
    if (g->parts >= ash_game_upgrade_cost(g, 0) &&
        g->dorm_lv < ash_game_build_lv_max(g))
        return "零件够了 · 可升宿舍";
    if (g->parts >= ash_game_upgrade_cost(g, 1) &&
        g->kitchen_lv < ash_game_build_lv_max(g))
        return "零件够了 · 可升食堂";
    if (!ash_game_stash_full(g) && all_plots_empty(g) && can_plant_any(g))
        return "空田地还在 · 去菜地播";
    if (g->chore_done_exp == 0) return "站里别干等 · 据点还有活";
    return "等人回来 · 也可以先深睡";
}

static void clamp_trust(ash_person_t *p)
{
    if (p->trust > 3) p->trust = 3;
    if (p->trust < -3) p->trust = -3;
}

static void add_fatigue(ash_person_t *p, int delta)
{
    int f = (int)p->fatigue + delta;
    if (f < 0) f = 0;
    if (f > 100) f = 100;
    p->fatigue = (uint8_t)f;
}

void ash_game_log(ash_game_t *g, const char *line)
{
    if (!g || !line) return;
    char *slot = g->journal[g->journal_head % ASHBOX_JOURNAL_N];
    snprintf(slot, ASHBOX_JOURNAL_LEN, "%s", line);
    g->journal_head = (uint8_t)((g->journal_head + 1u) % ASHBOX_JOURNAL_N);
    if (g->journal_count < ASHBOX_JOURNAL_N) g->journal_count++;
}

static void finish_quest(ash_game_t *g, uint8_t qid)
{
    if (qid >= ASHBOX_QUEST_N || quest_done_of(g, qid)) return;
    quest_set_step(g, qid, ASHBOX_QUEST_STEPS);
    quest_set_done(g, qid, 1);
    if (qid == 0) {
        g->threat_rise_num = 7;
        ash_game_log(g, "铁皮来客了结 · 威胁涨得慢了");
    } else if (qid == 1) {
        g->deep_loot_bonus = 1;
        ash_game_log(g, "余烬低语了结 · 深处口粮约多一成");
    } else if (qid == 2) {
        g->item[ASH_ITEM_TOKEN] = sat_add_u8(g->item[ASH_ITEM_TOKEN], 1, ASHBOX_ITEM_CAP);
        if (g->lore_shards < 4) g->lore_shards = 4;
        if (g->item[ASH_ITEM_SHARD] < 4) {
            g->item[ASH_ITEM_SHARD] = sat_add_u8(g->item[ASH_ITEM_SHARD], 1, ASHBOX_ITEM_CAP);
        }
        g->lore_twist = 1;
        int th = (int)g->threat - 8;
        if (th < 0) th = 0;
        g->threat = (uint8_t)th;
        if (!(g->world_flags & ASH_WORLD_CAMP2)) {
            g->world_flags |= (uint8_t)ASH_WORLD_CAMP2;
            g->seed_root = sat_add_u8(g->seed_root, 1, ASHBOX_SEED_ROOT_CAP);
            ash_game_log(g, "渡口通联 · 可种更饱的红薯");
        }
        ash_game_log(g, "门后灰潮了结 · 守望开始");
    }
}

/* G3：每步推进写一句「世界多了什么」（了结由 finish_quest 写） */
static void log_quest_step_milestone(ash_game_t *g, uint8_t qid, uint8_t reached)
{
    static const char *k_ms[ASHBOX_QUEST_N][4] = {
        {
            "铁皮墙敲声被记进匣",
            "近路脚印摸清了",
            "脚印埋了 · 追线断了",
            "全站戒备撑过一夜",
        },
        {
            "听清了匣里余烬的声音",
            "碎片贴进匣盖内侧",
            "老阙把碎片磨亮了",
            "匣主独自守过一夜",
        },
        {
            "开闸后听清门外风声",
            "铁棚频率通联上了",
            "换种听潮 · 灰潮推进",
            "田地活了 · 灰片可拼",
        },
    };
    if (!g || qid >= ASHBOX_QUEST_N || reached < 1 || reached > 4) return;
    ash_game_log(g, k_ms[qid][reached - 1u]);
}

bool ash_game_quest_beat_available(const ash_game_t *g, uint8_t qid)
{
    if (!g || qid >= ASHBOX_QUEST_N) return false;
    if (g->pace_era != ASHBOX_PACE_V13) return true;
    uint16_t today = day_latch_today(g);
    return g->quest_beat_day[qid] != today;
}

static void try_advance_quest(ash_game_t *g, uint8_t qid, uint8_t need_step)
{
    if (qid >= ASHBOX_QUEST_N || quest_done_of(g, qid)) return;
    if (quest_step_of(g, qid) != need_step) return;
    if (g->pace_era == ASHBOX_PACE_V13) {
        uint16_t today = day_latch_today(g);
        if (g->quest_beat_day[qid] == today) {
            ash_game_log(g, "这条线今天先到这，明天再往下");
            return;
        }
        g->quest_beat_day[qid] = today;
    }
    uint8_t next = (uint8_t)(need_step + 1u);
    quest_set_step(g, qid, next);
    if (next >= ASHBOX_QUEST_STEPS) {
        finish_quest(g, qid);
    } else {
        log_quest_step_milestone(g, qid, next);
    }
}

/* Phase A 已开闸/通联的存档：补齐听闻与通联步，不跳过交易/种地 */
static void sync_quest2_catchup(ash_game_t *g)
{
    if (!g || quest_done_of(g, 2)) return;
    uint8_t before = quest_step_of(g, 2);
    if (quest_step_of(g, 2) == 0 && g->gate_unlocked) try_advance_quest(g, 2, 0);
    if (quest_step_of(g, 2) == 1 && (g->world_flags & ASH_WORLD_CAMP)) try_advance_quest(g, 2, 1);
    /* G3：迁移 catch-up 说明（听闻被跳过） */
    if (before == 0 && quest_step_of(g, 2) > before) {
        ash_game_log(g, "旧档补记 · 跳过灰潮听闻");
    }
}

void ash_game_new(ash_game_t *g)
{
    memset(g, 0, sizeof(*g));
    g->magic = ASHBOX_MAGIC;
    g->ver = ASHBOX_SAVE_VER;
    g->food = 20;
    g->parts = 10;
    g->ember = 0;
    g->dorm_lv = 1;
    g->kitchen_lv = 1;
    g->workshop_lv = 1;
    g->pop = 1;
    (*ash_game_person_mut(g, (unsigned)(0))).id = 0;
    (*ash_game_person_mut(g, (unsigned)(0))).state = ASH_PERSON_HEALTHY;
    g->day_key = 1;
    g->threat_rise_num = 10;
    g->night_order = ASH_ORDER_NONE;
    g->trade_day = 0xFF;
    g->trade2_day = 0xFF;
    g->trade_extra_day = 0xFF;
    g->trade2_extra_day = 0xFF;
    g->short_probe_used_today = 0xFF;
    g->ember_trade_day = 0xFF;
    g->duty_day = 0xFF;
    g->day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_trade2 = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_trade_extra = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_trade2_extra = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_duty = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_ember = ASHBOX_DAY_LATCH_NEVER;
    g->day_latch_abx = ASHBOX_DAY_LATCH_NEVER;
    g->pad_v11 = 0;
    g->farm_plots_unlocked = ASHBOX_FARM_PLOTS;
    g->build_cap_bonus = 0;
    g->loot_bonus_perm_pct = 0;
    g->super_core = 0;
    g->run_cleared = 0;
    g->peace_broken = 0;
    g->wipeout_reject_count = 0;
    g->game_over = 0;
    g->clear_toast_pending = 0;
    g->joined_after_clear = 0;
    g->achieve_toast_mask = 0;
    g->stash_food_bonus = 0;
    g->end_toast_pending = 0;
    memset(g->person_job, 0, sizeof(g->person_job));
    g->pace_era = ASHBOX_PACE_V13;
    g->wanderer_trait = ASHBOX_TRAIT_NONE;
    g->wanderer_fired = 0;
    g->pad_v13a = 0;
    for (int qi = 0; qi < ASHBOX_QUEST_N; qi++) g->quest_beat_day[qi] = ASHBOX_DAY_LATCH_NEVER;
    g->wanderer_sched_day = 0;
    g->wanderer_t0_min = 0;
    g->wanderer_t1_min = 0;
    memset(g->person_trait, 0, sizeof(g->person_trait));
    memset(g->farm_labor_h, 0, sizeof(g->farm_labor_h));
    memset(g->pad_v13, 0, sizeof(g->pad_v13));
    g->duty_count = 0;
    g->world_flags = 0;
    g->stash_lv = 1;
    g->furn_lv = 0;
    g->seed_root = 0;
    g->ember_dry = 0;
    g->gear_lv = 0;
    g->rep_shed = 0;
    g->rep_ferry = 0;
    g->quiet_gear = 0;
    g->door_bolt = 0;
    g->lore_twist = 0;
    g->shovel = 0;
    g->well_lv = 0;
    g->pad_v9 = 0;
    g->water = 40;
    g->stash_water = 0;
    g->water_made_today = 0;
    g->report_water_delta = 0;
    g->well_upgrade_remain_s = 0;
    g->crew_exhaust_remain_s = 0;
    g->iron = 0;
    g->bucket = 0;
    g->backpack = 0;
    g->crowbar = 0;
    g->pistol = 0;
    g->ammo = 0;
    g->craft_bench = 0;
    g->wanderer_next_s = 1800; /* first possible visit ~30m */
    g->crc = ash_game_crc(g);
}

static void set_line(ash_report_t *r, int i, const char *s)
{
    if (i < 0 || i >= ASHBOX_REPORT_LINES) return;
    snprintf(r->lines[i], sizeof(r->lines[i]), "%s", s);
}

static unsigned fail_chance(const ash_game_t *g, ash_tier_t tier, uint8_t pid, ash_loadout_t load)
{
    (void)load; /* 携带档已停用；字段保留存档兼容 */
    unsigned fail_p = (tier == ASH_TIER_SHORT)   ? 5u
                      : (tier == ASH_TIER_BLOCK)  ? 12u
                      : (tier == ASH_TIER_HUNT)   ? 20u
                      : (tier == ASH_TIER_NIGHT)  ? 22u
                                                 : 18u;
    if (pid == 0) {
        if (fail_p >= 5u) fail_p -= 5u;
        else fail_p = 0;
    }
    if (pid < ASHBOX_PERSON_MAX) {
        if ((*ash_game_person_at(g, (unsigned)(pid))).fatigue >= 80) fail_p += 20u;
        else if ((*ash_game_person_at(g, (unsigned)(pid))).fatigue >= 60) fail_p += 10u;
        if ((*ash_game_person_at(g, (unsigned)(pid))).trust > 0) {
            unsigned t = (unsigned)(*ash_game_person_at(g, (unsigned)(pid))).trust;
            fail_p = (fail_p > t) ? (fail_p - t) : 0;
        } else if ((*ash_game_person_at(g, (unsigned)(pid))).trust < 0) {
            fail_p += (unsigned)(-(*ash_game_person_at(g, (unsigned)(pid))).trust);
        }
    }
    /* 手枪带弹：略降空手而归；突然袭击免死另计 */
    if (g->exp_pistol && g->exp_ammo > 0) {
        if (fail_p >= 4u) fail_p -= 4u;
        else fail_p = 0;
    }
    if ((tier == ASH_TIER_HUNT || tier == ASH_TIER_NIGHT) && g->gear_lv > 0) {
        unsigned cut = (unsigned)g->gear_lv * 3u;
        fail_p = (fail_p > cut) ? (fail_p - cut) : 0;
    } else if (tier == ASH_TIER_BLOCK && g->gear_lv > 0) {
        unsigned cut = (unsigned)g->gear_lv * 2u;
        fail_p = (fail_p > cut) ? (fail_p - cut) : 0;
    }
    if (fail_p < 5u && tier != ASH_TIER_SHORT) fail_p = 5u;
    if (fail_p > 90u) fail_p = 90u;
    return fail_p;
}

static uint8_t pick_outcome_id(bool success, ash_tier_t tier, ash_loadout_t load, bool tutorial)
{
    if (tutorial && success) return 0;
    if (tier == ASH_TIER_HUNT) return success ? 9 : 10;
    if (tier == ASH_TIER_NIGHT) return success ? 11 : 12;
    if (success && load == ASH_LOAD_FOOD) return 7;
    if (success && load == ASH_LOAD_PARTS) return 8;
    if (tier == ASH_TIER_SHORT) return success ? 1 : 2;
    if (tier == ASH_TIER_BLOCK) return success ? 3 : 4;
    return success ? 5 : 6; /* deep */
}

static int farm_duty_boost_num(const ash_game_t *g)
{
    /* Rest=0; each farm job at base adds ~25% (weak half). Cap 50%. */
    int boost = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        const ash_person_t *p = ash_game_person_at(g, (unsigned)i);
        if (!p || p->state == ASH_PERSON_AWAY) continue;
        if (g->person_job[i] != ASHBOX_JOB_FARM) continue;
        int add = (p->state == ASH_PERSON_WEAK) ? 12 : 25;
        boost += add;
        if (g->person_trait[i] == ASHBOX_TRAIT_FARM_FAST ||
            g->person_trait[i] == ASHBOX_TRAIT_ALL_DOUBLE) {
            /* 种植加快两成：折成额外生长速度约 +25% */
            boost += 25;
        }
    }
    if (boost > 80) boost = 80;
    return boost;
}

static int work_duty_parts_num(const ash_game_t *g)
{
    int boost = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        const ash_person_t *p = ash_game_person_at(g, (unsigned)i);
        if (!p || p->state == ASH_PERSON_AWAY) continue;
        if (g->person_job[i] != ASHBOX_JOB_WORK) continue;
        boost += (p->state == ASH_PERSON_WEAK) ? 10 : 20;
        uint8_t tr = g->person_trait[i];
        if (tr == ASHBOX_TRAIT_WORK_SPD_60) boost += 60;
        else if (tr == ASHBOX_TRAIT_WORK_SPD_40) boost += 40;
        else if (tr == ASHBOX_TRAIT_WORK_SPD_20) boost += 20;
        else if (tr == ASHBOX_TRAIT_ALL_DOUBLE) boost += 20;
    }
    if (boost > 120) boost = 120;
    return boost;
}

static bool station_has_trait(const ash_game_t *g, uint8_t trait)
{
    if (!g) return false;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        const ash_person_t *p = ash_game_person_at(g, (unsigned)i);
        if (!p || p->state == ASH_PERSON_AWAY) continue;
        if (g->person_trait[i] == trait) return true;
    }
    return false;
}

static int farm_workers_present(const ash_game_t *g)
{
    int n = 0;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        const ash_person_t *p = ash_game_person_at(g, (unsigned)i);
        if (!p || p->state == ASH_PERSON_AWAY) continue;
        if (g->person_job[i] == ASHBOX_JOB_FARM) n++;
    }
    return n;
}

static void tick_farm(ash_game_t *g, int64_t dt_s)
{
    if (dt_s <= 0 || !ash_game_farm_unlocked(g)) return;
    /* 干燥架：生长加速约 ×8/7（中途装上亦生效） */
    int64_t step = dt_s;
    if (g->furn_lv >= 2) {
        step = dt_s * 8 / 7;
        if (step < dt_s) step = dt_s;
    }
    int farm_boost = farm_duty_boost_num(g);
    if (farm_boost > 0) {
        step = step * (100 + farm_boost) / 100;
    }
    int workers = farm_workers_present(g);
    int labor_add = (int)(dt_s * workers / 3600);
    if (labor_add <= 0 && dt_s >= 1800 && workers > 0) labor_add = 1;
    int plots = ash_game_farm_plot_count(g);
    for (int i = 0; i < plots; i++) {
        ash_farm_plot_t *fp = ash_game_farm_mut(g, (unsigned)i);
        if (!fp || fp->crop == 0) continue;
        if (labor_add > 0 && g->pace_era == ASHBOX_PACE_V13) {
            int h = (int)g->farm_labor_h[i] + labor_add;
            if (h > ASHBOX_FARM_LABOR_H_CAP) h = ASHBOX_FARM_LABOR_H_CAP;
            g->farm_labor_h[i] = (uint8_t)h;
        }
        if (fp->remain_s > (uint32_t)step) {
            fp->remain_s -= (uint32_t)step;
        } else {
            fp->remain_s = 0;
        }
    }
}

static void clear_person_equip(ash_game_t *g, uint8_t pid)
{
    if (!g || pid >= ASHBOX_PERSON_MAX) return;
    (*ash_game_equip_bp_mut(g, (unsigned)(pid))) = 0;
    (*ash_game_equip_pis_mut(g, (unsigned)(pid))) = 0;
}

static void remove_person_at(ash_game_t *g, uint8_t pid)
{
    if (!g || pid >= g->pop || pid >= ASHBOX_PERSON_MAX) return;
    clear_person_equip(g, pid);
    for (uint8_t i = pid; i + 1 < g->pop && i + 1 < ASHBOX_PERSON_MAX; i++) {
        (*ash_game_person_mut(g, (unsigned)(i))) = (*ash_game_person_mut(g, (unsigned)(i + 1)));
        (*ash_game_person_mut(g, (unsigned)(i))).id = i;
        (*ash_game_equip_bp_mut(g, (unsigned)(i))) = (*ash_game_equip_bp_mut(g, (unsigned)(i + 1)));
        (*ash_game_equip_pis_mut(g, (unsigned)(i))) = (*ash_game_equip_pis_mut(g, (unsigned)(i + 1)));
        g->person_job[i] = g->person_job[i + 1];
        g->person_trait[i] = g->person_trait[i + 1];
    }
    if (g->pop > 0) g->pop--;
    if (g->pop < ASHBOX_PERSON_MAX) {
        memset(&(*ash_game_person_mut(g, (unsigned)(g->pop))), 0, sizeof((*ash_game_person_mut(g, (unsigned)(0)))));
        (*ash_game_equip_bp_mut(g, (unsigned)(g->pop))) = 0;
        (*ash_game_equip_pis_mut(g, (unsigned)(g->pop))) = 0;
        g->person_job[g->pop] = ASHBOX_JOB_REST;
        g->person_trait[g->pop] = ASHBOX_TRAIT_NONE;
    }
    g->exp_crowbar = 0;
    g->exp_pistol = 0;
    g->exp_ammo = 0;
}

static ash_death_kind_t roll_death_kind(ash_tier_t tier, unsigned seed, uint8_t threat)
{
    unsigned lethal = (tier == ASH_TIER_SHORT)   ? 2u
                      : (tier == ASH_TIER_BLOCK)  ? 8u
                      : (tier == ASH_TIER_HUNT)   ? 12u
                      : (tier == ASH_TIER_NIGHT)  ? 22u
                                                 : 18u;
    lethal += (unsigned)threat / 10u;
    if (lethal > 45u) lethal = 45u;
    if ((seed % 100u) >= lethal) return ASH_DEATH_NONE;
    unsigned kind_roll = (seed / 7u) % 100u;
    if (kind_roll < 45u) return ASH_DEATH_ASSAULT;
    if (kind_roll < 75u) return ASH_DEATH_INJURY;
    return ASH_DEATH_INFECTION;
}

static void resolve_return(ash_game_t *g, bool success)
{
    ash_expedition_t *e = &g->exp;
    uint8_t pid = e->person_id;
    if (pid >= ASHBOX_PERSON_MAX) pid = 0;
    ash_tier_t tier = e->tier;
    ash_loadout_t load = e->loadout;
    uint8_t carried_crowbar = g->exp_crowbar;
    uint8_t carried_pistol = g->exp_pistol;
    uint8_t carried_ammo = g->exp_ammo;
    e->active = false;
    e->remaining_s = 0;
    g->chore_kind = ASHBOX_CHORE_NONE;

    ash_report_t *r = &g->report;
    memset(r, 0, sizeof(*r));
    r->pending = true;
    r->success = success ? 1 : 0;
    r->branch_resolved = 0;
    g->report_waste_flags = 0;
    g->report_iron_delta = 0;
    g->report_death_kind = ASH_DEATH_NONE;

    int fatigue_add = (tier == ASH_TIER_SHORT)  ? 12
                      : (tier == ASH_TIER_BLOCK) ? 22
                      : (tier == ASH_TIER_HUNT)  ? 28
                      : (tier == ASH_TIER_NIGHT) ? 40
                                                : 35;
    if (pid < g->pop) add_fatigue(&(*ash_game_person_mut(g, (unsigned)(pid))), fatigue_add);

    if (success) {
        unsigned seed = g->day_key * 17u + g->expedition_total * 13u + (unsigned)load * 7u;
        ash_game_apply_success_core_loot(g, r, pid, tier, load, carried_crowbar, carried_pistol,
                                carried_ammo);
        int food = (int)r->food_delta;
        int parts = (int)r->parts_delta;
        int ember = (int)r->ember_delta;
        {
            unsigned tag_n = ash_content_tag_count();
            unsigned tag = tag_n ? ((seed * 11u + (unsigned)pid * 5u) % tag_n) : 0u;
            /* 小芒偏线索；老阙偏轴承/修理；猎场/过夜强制气味标签 */
            if (tier == ASH_TIER_HUNT && tag_n > 9) {
                tag = 9; /* 干肉香 */
            } else if (tier == ASH_TIER_NIGHT && tag_n > 10) {
                tag = 10; /* 长夜潮声 */
            } else if (pid == 1 && tag_n > 11 && (seed % 5u) < 3u) {
                tag = 11; /* 轴承盒 */
            } else if (pid == 2 && tag_n > 0) {
                static const uint8_t k_clue_tags[] = {1, 3, 6, 7};
                if ((seed % 5u) < 3u) {
                    tag = k_clue_tags[(seed / 17u) % 4u];
                    if (tag >= tag_n) tag = tag % tag_n;
                }
            }
            r->tag_id = (uint8_t)tag;
            g->last_tag_id = (uint8_t)tag;
        }
        {
            char buf[40];
            if (tier == ASH_TIER_NIGHT) {
                snprintf(buf, sizeof(buf), "%s走完长夜", k_names[pid]);
            } else if (tier == ASH_TIER_HUNT) {
                snprintf(buf, sizeof(buf), "%s自猎径归来", k_names[pid]);
            } else if (pid == 0) {
                snprintf(buf, sizeof(buf), "阿禾稳稳回来了");
            } else if (pid == 1) {
                snprintf(buf, sizeof(buf), "老阙带着灰回来");
            } else if (pid == 2) {
                snprintf(buf, sizeof(buf), "小芒带回了线索");
            } else {
                snprintf(buf, sizeof(buf), "%s回来了", k_names[pid]);
            }
            set_line(r, 0, buf);
            /* 稀疏掉落：谷种 / 非深处余烬（有盖，不改变主掉落曲线） */
            int rare_seed = 0;
            unsigned seed_p = (tier == ASH_TIER_SHORT)   ? 2u
                              : (tier == ASH_TIER_BLOCK) ? 4u
                              : (tier == ASH_TIER_HUNT)  ? 5u
                              : (tier == ASH_TIER_NIGHT) ? 6u
                                                        : 7u;
            if ((seed % 100u) < seed_p) {
                uint8_t before = g->item[ASH_ITEM_SEED];
                g->item[ASH_ITEM_SEED] =
                    sat_add_u8(g->item[ASH_ITEM_SEED], 1, ASHBOX_ITEM_CAP);
                if (g->item[ASH_ITEM_SEED] > before) rare_seed = 1;
            }
            if (g->pace_era != ASHBOX_PACE_V13 && tier != ASH_TIER_DEEP &&
                tier != ASH_TIER_NIGHT) {
                /* 旧节奏：近路/废街/猎场稀疏余烬 + 软保底 */
                unsigned ember_p = (tier == ASH_TIER_SHORT)   ? 1u
                                   : (tier == ASH_TIER_BLOCK) ? 2u
                                                             : 3u;
                bool got_ember = false;
                if (((seed / 11u) % 100u) < ember_p) {
                    ember += 1;
                    r->ember_delta = (int16_t)ember;
                    got_ember = true;
                } else {
                    unsigned need = ember_pity_need(tier);
                    if (g->ember_dry + 1u >= need) {
                        ember += 1;
                        r->ember_delta = (int16_t)ember;
                        got_ember = true;
                        g->ember_dry = 0;
                    } else if (g->ember_dry < 250) {
                        g->ember_dry = (uint8_t)(g->ember_dry + 1u);
                    }
                }
                if (got_ember) g->ember_dry = 0;
            } else {
                g->ember_dry = 0;
            }
            if (tier == ASH_TIER_HUNT) {
                int jerky = 1 + (int)((seed / 5u) % 2u);
                if (g->gear_lv >= 1) jerky += 1;
                if (jerky > 3) jerky = 3;
                g->item[ASH_ITEM_JERKY] =
                    sat_add_u8(g->item[ASH_ITEM_JERKY], jerky, ASHBOX_ITEM_CAP);
                /* 铁棚已通联时猎场偶遇开通渡口（约 1/4） */
                if ((g->world_flags & ASH_WORLD_CAMP) && !(g->world_flags & ASH_WORLD_CAMP2) &&
                    (seed % 4u) == 0u) {
                    g->world_flags |= (uint8_t)ASH_WORLD_CAMP2;
                    g->seed_root = sat_add_u8(g->seed_root, 1, ASHBOX_SEED_ROOT_CAP);
                    ash_game_log(g, "渡口通联 · 红薯种可种更饱");
                }
                if (((seed / 13u) % 100u) < 4u) {
                    g->seed_root = sat_add_u8(g->seed_root, 1, ASHBOX_SEED_ROOT_CAP);
                }
                if (rare_seed) snprintf(buf, sizeof(buf), "口粮+%d 肉+%d 种+1", food, jerky);
                else snprintf(buf, sizeof(buf), "口粮+%d 干肉+%d", food, jerky);
                set_line(r, 1, buf);
                if (ember > 0) set_line(r, 2, "猎径有获 · 余烬微亮");
                else set_line(r, 2, "干肉香还挂在袖口");
            } else {
                if (rare_seed && parts > 0)
                    snprintf(buf, sizeof(buf), "口粮+%d 零件+%d 种+1", food, parts);
                else if (rare_seed)
                    snprintf(buf, sizeof(buf), "口粮+%d 种+1", food);
                else if (parts > 0)
                    snprintf(buf, sizeof(buf), "口粮+%d 零件+%d", food, parts);
                else {
                    snprintf(buf, sizeof(buf), "口粮带回了 · 零件没有");
                    g->report_water_delta = (int16_t)(g->report_water_delta + 1);
                }
                set_line(r, 1, buf);
                if (ember > 0 && tier != ASH_TIER_DEEP && tier != ASH_TIER_NIGHT) {
                    set_line(r, 2, "口袋里余烬亮了一下");
                } else if (tier == ASH_TIER_NIGHT) {
                    set_line(r, 2, "潮声一整夜 · 匣还暖");
                } else if (tier == ASH_TIER_DEEP) {
                    set_line(r, 2, "地铁风干 · 碎片入匣");
                } else {
                    snprintf(buf, sizeof(buf), "遭遇 · %s", ash_content_tag_label(r->tag_id));
                    set_line(r, 2, buf);
                }
            }
        }
        (*ash_game_person_mut(g, (unsigned)(pid))).state = ASH_PERSON_HEALTHY;
        if (tier == ASH_TIER_DEEP) {
            /* G2：同趟只推进一步——步0 catch-up，或已在步1再推到步2 */
            if (!quest_done_of(g, 1) && quest_step_of(g, 1) == 0) {
                try_advance_quest(g, 1, 0);
            } else if (!quest_done_of(g, 1) && quest_step_of(g, 1) == 1) {
                try_advance_quest(g, 1, 1);
            }
            if (!quest_done_of(g, 1) && quest_step_of(g, 1) >= 4) finish_quest(g, 1);
            /* 灰片 + 铁棚通联 */
            if ((seed % 3u) == 0u) {
                g->item[ASH_ITEM_SHARD] =
                    sat_add_u8(g->item[ASH_ITEM_SHARD], 1, ASHBOX_ITEM_CAP);
                if (g->lore_shards < 4) g->lore_shards = (uint8_t)(g->lore_shards + 1u);
            }
            if (!(g->world_flags & ASH_WORLD_CAMP)) {
                g->world_flags |= (uint8_t)ASH_WORLD_CAMP;
                g->item[ASH_ITEM_SEED] = sat_add_u8(g->item[ASH_ITEM_SEED], 2, ASHBOX_ITEM_CAP);
                ash_game_log(g, "铁棚有人回应 · 得了稻谷种");
                try_advance_quest(g, 2, 1);
            } else {
                try_advance_quest(g, 2, 1);
            }
            if (!quest_done_of(g, 2) && quest_step_of(g, 2) >= 4 && g->lore_shards >= 2) {
                finish_quest(g, 2);
            }
        }
        if (pid == 2 && (tier == ASH_TIER_SHORT || tier == ASH_TIER_BLOCK)) {
            try_advance_quest(g, 0, 0);
        }
        {
            char logb[ASHBOX_JOURNAL_LEN];
            snprintf(logb, sizeof(logb), "%s自%s带回", k_names[pid], k_tier_name[tier]);
            ash_game_log(g, logb);
        }
    } else {
        r->food_delta = 0;
        r->parts_delta = 0;
        r->ember_delta = 0;
        g->report_iron_delta = 0;
        unsigned seed = g->day_key * 17u + g->expedition_total * 13u + (unsigned)load * 7u;
        /* Failure: about half a success roll, floored (may be 0). */
        {
            int wlo = 1, whi = 2;
            if (tier == ASH_TIER_BLOCK) {
                wlo = 2;
                whi = 3;
            } else if (tier == ASH_TIER_HUNT) {
                wlo = 3;
                whi = 4;
            } else if (tier == ASH_TIER_DEEP) {
                wlo = 4;
                whi = 6;
            } else if (tier == ASH_TIER_NIGHT) {
                wlo = 5;
                whi = 8;
            }
            int span = whi - wlo + 1;
            int wfind = (wlo + (int)((seed / 19u) % (unsigned)span)) / 2;
            g->report_water_delta = (int16_t)wfind;
        }
        ash_death_kind_t death = roll_death_kind(tier, seed, g->threat);
        bool died = false;
        if (death == ASH_DEATH_ASSAULT && carried_pistol && carried_ammo > 0) {
            carried_ammo--;
            g->exp_ammo = carried_ammo;
            death = ASH_DEATH_NONE;
            ash_game_log(g, "手枪挡住突然袭击 · 捡回一条命");
        }
        g->report_death_kind = (uint8_t)death;
        if (death != ASH_DEATH_NONE) {
            died = true;
            char buf[32];
            const char *nm = (pid < 3) ? k_names[pid] : "有人";
            snprintf(buf, sizeof(buf), "%s没能回来", nm);
            set_line(r, 0, buf);
            if (death == ASH_DEATH_ASSAULT) set_line(r, 1, "遭突然袭击没回来");
            else if (death == ASH_DEATH_INFECTION) set_line(r, 1, "热伤加重未回");
            else set_line(r, 1, "意外重伤未回");
            set_line(r, 2, "装备与携带尽失");
            remove_person_at(g, pid);
            r->branch_count = 0;
            r->branch_resolved = 1;
            ash_game_log(g, "有人倒下 · 远征结束");
        } else {
            bool got_fever = false;
            if (pid < g->pop) {
                /* 空手而归存活：约 35% 热伤(36h 自退)；其余重伤，24h 内须肾上腺素否则死亡 */
                if (((seed / 3u) % 100u) < 35u) {
                    (*ash_game_person_mut(g, (unsigned)(pid))).state = ASH_PERSON_INFECTED;
                    (*ash_game_person_mut(g, (unsigned)(pid))).injure_remain_s = ASHBOX_INFECT_HEAL_S;
                    got_fever = true;
                } else {
                    (*ash_game_person_mut(g, (unsigned)(pid))).state = ASH_PERSON_INJURED;
                    (*ash_game_person_mut(g, (unsigned)(pid))).injure_remain_s = ASHBOX_INJURE_DEAD_S;
                }
            }
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s空手而归", k_names[pid]);
                set_line(r, 0, buf);
                if (got_fever) {
                    set_line(r, 1, "空手 · 还带了热伤");
                    set_line(r, 2, "先退烧再出门");
                } else {
                    snprintf(buf, sizeof(buf), "%s只摸到空壳", k_place[tier]);
                    set_line(r, 1, buf);
                    if (pid == 0) set_line(r, 2, "阿禾喘着 · 一日内要用药");
                    else if (pid == 1) set_line(r, 2, "老阙喘着 · 一日内要用药");
                    else if (pid == 2) set_line(r, 2, "小芒喘着 · 一日内要用药");
                    else set_line(r, 2, "重伤 · 一日内要用药");
                }
            }
            /* Return surviving carry (ammo may be spent). */
            g->crowbar = sat_add_u8(g->crowbar, carried_crowbar, 20);
            if (carried_pistol && !(pid < ASHBOX_PERSON_MAX && (*ash_game_equip_pis_mut(g, (unsigned)(pid))))) {
                g->pistol = sat_add_u8(g->pistol, 1, 20);
            }
            g->ammo = sat_add_u8(g->ammo, carried_ammo, 99);
            g->exp_crowbar = 0;
            g->exp_pistol = 0;
            g->exp_ammo = 0;
            if (got_fever) {
                if (pid == 0) ash_game_log(g, "阿禾空手而归还带热伤");
                else if (pid == 1) ash_game_log(g, "老阙空手而归还带热伤");
                else if (pid == 2) ash_game_log(g, "小芒空手而归还带热伤");
                else ash_game_log(g, "空手而归 · 还带了热伤");
            } else if (pid == 0) {
                ash_game_log(g, "阿禾重伤 · 一日内要用药");
            } else if (pid == 1) {
                ash_game_log(g, "老阙重伤 · 一日内要用药");
            } else if (pid == 2) {
                ash_game_log(g, "小芒重伤 · 一日内要用药");
            } else {
                ash_game_log(g, "有人重伤 · 一日内要用药");
            }
        }
        {
            int rise = 10 - (int)g->gear_lv * 2;
            if (died) rise += 6;
            if (ash_game_has_box_lamp(g)) rise -= 2;
            if (rise < 4) rise = 4;
            int th = (int)g->threat + rise;
            if (th > 100) th = 100;
            g->threat = (uint8_t)th;
        }
    }

    bool died_out = (g->report_death_kind != ASH_DEATH_NONE);
    bool tutorial = g->expedition_total < 3;
    if (!died_out) {
        r->outcome_id = pick_outcome_id(success, tier, load, tutorial);
        {
            const ash_outcome_def_t *oc = ash_content_outcome(r->outcome_id);
            r->branch_count = oc->branch_count;
            if (r->branch_count > ASHBOX_BRANCH_MAX) r->branch_count = ASHBOX_BRANCH_MAX;
            for (uint8_t i = 0; i < r->branch_count; i++) {
                r->branch_ids[i] = oc->branch_ids[i];
            }
            if (success && !tutorial) {
                uint8_t tagged[ASHBOX_BRANCH_MAX];
                uint8_t tn = ash_content_tag_branches(r->tag_id, tagged, ASHBOX_BRANCH_MAX);
                if (tn > 0) {
                    r->branch_count = tn;
                    for (uint8_t i = 0; i < tn; i++) r->branch_ids[i] = tagged[i];
                }
            }
        }
        if (pid < g->pop && (*ash_game_person_mut(g, (unsigned)(pid))).fatigue >= 70) {
            uint8_t w = 0;
            for (uint8_t i = 0; i < r->branch_count; i++) {
                uint8_t id = r->branch_ids[i];
                if (id == 1 || id == 16) continue;
                r->branch_ids[w++] = id;
            }
            r->branch_count = w;
        }
    } else {
        r->branch_count = 0;
        r->branch_resolved = 1;
    }
    if (r->branch_count == 0) r->branch_resolved = 1;

    /* First expedition completion grants the shovel (success or fail). */
    if (!g->shovel && g->expedition_total == 0) {
        g->shovel = 1;
        ash_game_log(g, "捡到铁铲 · 可挖深坑");
    }

    g->expedition_total++;
}

static int dorm_consume_num(const ash_game_t *g)
{
    /* 宿舍：L1:10 L2:11 L3+：仍11（歇得快，口粮不加） */
    uint8_t lv = g->dorm_lv < 1 ? 1 : (g->dorm_lv > 5 ? 5 : g->dorm_lv);
    if (lv >= 3) return 11;
    return 9 + (int)lv;
}

static int workshop_threat_num(const ash_game_t *g)
{
    /* 工坊增产但威胁上涨加快；静音轮/门扣软降（有底） */
    int max_lv = ash_game_build_lv_max(g);
    uint8_t lv = g->workshop_lv < 1 ? 1 : ((int)g->workshop_lv > max_lv ? (uint8_t)max_lv : g->workshop_lv);
    int n = (int)g->threat_rise_num + (int)(lv - 1) * 2;
    if (g->quiet_gear) n -= 2;
    if (g->door_bolt) n -= 1;
    /* 静音轮底 5；门扣后再松一档底 4 */
    int floor = g->door_bolt ? 4 : 5;
    if (n < floor) n = floor;
    return n;
}

static unsigned ember_pity_need(ash_tier_t tier)
{
    if (tier == ASH_TIER_SHORT) return ASHBOX_EMBER_PITY_SHORT;
    if (tier == ASH_TIER_BLOCK) return ASHBOX_EMBER_PITY_BLOCK;
    if (tier == ASH_TIER_HUNT) return ASHBOX_EMBER_PITY_HUNT;
    /* DEEP/NIGHT: caller skips soft pity; 255 disables accidental use. */
    return 255u;
}

static void maybe_arm_crisis(ash_game_t *g)
{
    if (g->crisis.pending) return;
    if (g->exp.active || g->report.pending) return;
    bool tutorial = g->expedition_total < 3;
    if (tutorial) return;
    if (g->threat < 80) return;
    g->crisis.pending = true;
    ash_game_log(g, "威胁临门 · 守夜抉择");
}

static void apply_economy(ash_game_t *g, int64_t dt_s, int sleep_num, int sleep_den,
                          int64_t station_healthy_s, int64_t station_present_s)
{
    if (dt_s <= 0) return;
    /* 无人：站内口粮/威胁消耗为 0；零件不产；菜地/产水另算 */
    if (g->pop == 0) {
        if (g->hide_penalty_h > 0) {
            int64_t hours = dt_s / 3600;
            if (hours > g->hide_penalty_h) g->hide_penalty_h = 0;
            else g->hide_penalty_h = (uint8_t)(g->hide_penalty_h - (uint8_t)hours);
        }
        return;
    }
    if (station_present_s < 0) station_present_s = 0;
    if (station_present_s > dt_s * ASHBOX_PERSON_MAX)
        station_present_s = dt_s * ASHBOX_PERSON_MAX;
    int max_lv = ash_game_build_lv_max(g);
    int ws = g->workshop_lv < 1 ? 1 : ((int)g->workshop_lv > max_lv ? max_lv : (int)g->workshop_lv);
    int parts_mult_num = (ws <= 1) ? 10 : (ws == 2) ? 15 : (ws == 3) ? 20 : (ws == 4) ? 24 : 28;
    parts_mult_num = parts_mult_num * (100 + work_duty_parts_num(g)) / 100;
    int kitchen = g->kitchen_lv < 1 ? 1 : ((int)g->kitchen_lv > max_lv ? max_lv : (int)g->kitchen_lv);
    int consume_num = (kitchen >= 3) ? 9 : 10; /* 食堂降耗 */
    consume_num = consume_num * dorm_consume_num(g) / 10;
    if (ash_game_has_stove_lining(g)) consume_num -= 1;
    if (consume_num < 8) consume_num = 8;
    bool deep = (sleep_num != sleep_den);
    if (deep && g->night_order == ASH_ORDER_RATION) consume_num = (consume_num * 7) / 10;
    if (deep && g->night_order == ASH_ORDER_RUSH) parts_mult_num = (parts_mult_num * 13) / 10;

    int64_t food_gain = station_healthy_s * 4 * sleep_num / (3600 * (int64_t)sleep_den);
    if (g->hide_penalty_h > 0) {
        /* 躲藏后若干小时产粮减半 */
        food_gain /= 2;
    }
    int64_t parts_gain =
        station_healthy_s * 2 * parts_mult_num * sleep_num / (3600 * 10 * (int64_t)sleep_den);
    if (station_has_trait(g, ASHBOX_TRAIT_WORK_PARTS_X2) ||
        station_has_trait(g, ASHBOX_TRAIT_ALL_DOUBLE)) {
        parts_gain *= 2;
    }
    /* 外出者不吃站内粮（出发口粮已预扣）；按在站人·秒计 */
    int64_t food_use =
        station_present_s * 3 * sleep_num * consume_num / (2 * 3600 * (int64_t)sleep_den * 10);

    int cap = ash_game_food_cap(g);
    int scap = ash_game_stash_cap(g);
    int food = (int)g->food + (int)food_gain - (int)food_use;
    if (food < 0) food = 0;
    if (food > cap) {
        int overflow = food - cap;
        food = cap;
        int st = (int)g->stash_food + overflow;
        if (st > scap) st = scap;
        if (st < 0) st = 0;
        g->stash_food = (uint16_t)st;
    }
    g->food = (uint16_t)food;
    /* 口袋空而仓有粮：自动回补，避免「有仓粮却断粮锁出发」 */
    if (g->food == 0 && g->stash_food > 0) {
        (void)ash_game_try_pocket_refill(g);
        food = (int)g->food;
    }
    g->food = (uint16_t)food;
    int parts = (int)g->parts + (int)parts_gain;
    if (parts > ASHBOX_PARTS_CAP) {
        int overflow = parts - ASHBOX_PARTS_CAP;
        parts = ASHBOX_PARTS_CAP;
        int st = (int)g->stash_parts + overflow;
        if (st > ASHBOX_PARTS_CAP) st = ASHBOX_PARTS_CAP;
        g->stash_parts = (uint16_t)st;
    }
    if (parts < 0) parts = 0;
    g->parts = (uint16_t)parts;

    if (g->food == 0 || g->water == 0) {
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_mut(g, (unsigned)(i))).state == ASH_PERSON_HEALTHY) {
                (*ash_game_person_mut(g, (unsigned)(i))).state = ASH_PERSON_WEAK;
            }
        }
    } else {
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_mut(g, (unsigned)(i))).state == ASH_PERSON_WEAK) {
                (*ash_game_person_mut(g, (unsigned)(i))).state = ASH_PERSON_HEALTHY;
            }
        }
    }

    int rise = workshop_threat_num(g);
    if (deep && g->night_order == ASH_ORDER_WATCH) rise = (rise * 7) / 10;
    if (deep && g->night_order == ASH_ORDER_RUSH) rise = (rise * 12) / 10;
    /* 约每小时 +3 * (rise/10)；工坊抬高 rise，Q0 完成后 threat_rise_num 降为 7 */
    int threat_add = (int)(dt_s * 3 * rise * sleep_num / (3600 * (int64_t)sleep_den * 10));
    int th = (int)g->threat + threat_add;
    if (th > 100) th = 100;
    g->threat = (uint8_t)th;

    if (g->hide_penalty_h > 0) {
        int64_t hours = dt_s / 3600;
        if (hours > g->hide_penalty_h) g->hide_penalty_h = 0;
        else if (hours > 0) g->hide_penalty_h = (uint8_t)(g->hide_penalty_h - (uint8_t)hours);
    }

    maybe_arm_crisis(g);
}

static void tick_injure(ash_game_t *g, int64_t dt_s)
{
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX;) {
        ash_person_t *p = ash_game_person_mut(g, (unsigned)i);
        if (!p) {
            i++;
            continue;
        }
        ash_person_state_t st = p->state;
        if (st != ASH_PERSON_INJURED && st != ASH_PERSON_INFECTED) {
            i++;
            continue;
        }
        if (p->injure_remain_s == 0) {
            /* 旧档无计时：热伤补 36h 自退；重伤补 24h 用药窗口 */
            if (st == ASH_PERSON_INFECTED) p->injure_remain_s = ASHBOX_INFECT_HEAL_S;
            else p->injure_remain_s = ASHBOX_INJURE_DEAD_S;
        }
        if (p->injure_remain_s > (uint32_t)dt_s) {
            p->injure_remain_s -= (uint32_t)dt_s;
            i++;
            continue;
        }
        p->injure_remain_s = 0;
        if (st == ASH_PERSON_INFECTED) {
            /* 无抗生素也会退：健康但累，避免永久减员 */
            p->state = ASH_PERSON_HEALTHY;
            if (p->fatigue < 60) p->fatigue = 60;
            ash_game_log(g, "热伤退了 · 还累着");
            i++;
        } else {
            /* 重伤满一日未用药 → 死亡 */
            char buf[ASHBOX_JOURNAL_LEN];
            snprintf(buf, sizeof(buf), "%s重伤一日未用药 · 没了", ash_game_person_name((uint8_t)i));
            ash_game_log(g, buf);
            remove_person_at(g, (uint8_t)i);
            /* 下标前移，勿 i++ */
        }
    }
}

static void tick_fatigue_rest(ash_game_t *g, int64_t dt_s)
{
    /* 休整：基础 -12/时，宿舍每级 +2，硬顶 -24。
     * 田作/工坊：每小时疲劳 +ASHBOX_LABOR_FATIGUE_PER_H，满 80 自动歇。 */
    int dorm = g->dorm_lv < 1 ? 1 : (g->dorm_lv > 5 ? 5 : g->dorm_lv);
    int base_rate = 12 + (dorm - 1) * 2;
    if (base_rate > 24) base_rate = 24;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        ash_person_t *p = ash_game_person_mut(g, (unsigned)i);
        if (!p || p->state == ASH_PERSON_AWAY) continue;
        uint8_t job = g->person_job[i];
        if (job == ASHBOX_JOB_FARM || job == ASHBOX_JOB_WORK) {
            int add = (int)(dt_s * ASHBOX_LABOR_FATIGUE_PER_H / 3600);
            if (add <= 0 && dt_s >= 900) add = 1;
            if (add > 0) add_fatigue(p, add);
            if (p->fatigue >= ASHBOX_LABOR_AUTO_REST) {
                g->person_job[i] = ASHBOX_JOB_REST;
                {
                    char buf[ASHBOX_JOURNAL_LEN];
                    snprintf(buf, sizeof(buf), "%s太累了 · 先歇着", ash_game_person_name((uint8_t)i));
                    ash_game_log(g, buf);
                }
            }
            continue;
        }
        int drop = (int)(dt_s * base_rate / 3600);
        if (drop <= 0 && dt_s >= 900 && base_rate > 0) drop = 1;
        if (drop > 0) add_fatigue(p, -drop);
    }
}

/* 远征等待：按时段抛出站事，避免主页只剩倒计时 */
static void tick_chore(ash_game_t *g)
{
    if (!g->exp.active || g->chore_kind != ASHBOX_CHORE_NONE) return;
    unsigned chore_cap = (g->exp.duration_s >= 14400u) ? 3u : 2u; /* 深处/过夜第三件 */
    if (g->chore_done_exp >= chore_cap) return;
    if (g->exp.duration_s == 0 || g->exp.remaining_s > g->exp.duration_s) return;
    uint32_t elapsed = g->exp.duration_s - g->exp.remaining_s;
    uint32_t first_at = g->exp.duration_s / 6u;
    if (first_at < 25u) first_at = 25u;
    if (first_at > 90u) first_at = 90u;
    uint32_t second_at = g->exp.duration_s / 2u;
    if (second_at < first_at + 40u) second_at = first_at + 40u;
    uint32_t third_at = (g->exp.duration_s * 3u) / 4u;
    if (third_at < second_at + 40u) third_at = second_at + 40u;
    uint32_t need = first_at;
    if (g->chore_done_exp == 1) need = second_at;
    else if (g->chore_done_exp >= 2) need = third_at;
    if (elapsed < need) return;
    unsigned pick =
        (g->day_key * 5u + g->expedition_total * 3u + (unsigned)g->chore_done_exp * 7u +
         (unsigned)g->exp.tier) %
        (unsigned)ASHBOX_CHORE_KIND_MAX;
    g->chore_kind = (uint8_t)(pick + 1u);
}

void ash_game_settle(ash_game_t *g, const ash_clock_info_t *clk)
{
    if (!clk || !clk->trusted || clk->dt_s <= 0) {
        return;
    }
    int64_t dt = clk->dt_s;
    if (dt > DT_MAX_S) dt = DT_MAX_S;
    int sn = clk->in_deep_sleep_segment ? SLEEP_NUM : 1;
    int sd = clk->in_deep_sleep_segment ? SLEEP_DEN : 1;
    /* 深睡醒来整段落盘：写站令早报，让隐性选择被感觉到（P0-1） */
    if (clk->in_deep_sleep_segment && dt >= 1800 &&
        g->night_order != ASH_ORDER_NONE) {
        char logb[ASHBOX_JOURNAL_LEN];
        snprintf(logb, sizeof(logb), "%s",
                 ash_game_order_result_line((ash_night_order_t)g->night_order));
        ash_game_log(g, logb);
    }

    /* labor_s：健康按满工、虚弱按半工，再折成「等效健康秒」；present_s：在站人·秒 */
    int64_t labor_s = 0;
    int64_t present_s = 0;
    if (g->exp.active && g->exp.remaining_s <= (uint32_t)dt) {
        int64_t returned_at = (int64_t)g->exp.remaining_s;
        int eid = (int)g->exp.person_id;
        int labor_before = labor_weight_excluding(g, eid);
        int present_before = count_present(g); /* away already excluded */
        unsigned roll = (unsigned)(g->day_key * 17u + g->expedition_total * 13u +
                                   (unsigned)g->exp.loadout * 9u) %
                        100u;
        unsigned fail_p = fail_chance(g, g->exp.tier, g->exp.person_id, g->exp.loadout);
        resolve_return(g, roll >= fail_p);
        int labor_after = labor_weight(g);
        int present_after = count_present(g);
        labor_s = (returned_at * labor_before + (dt - returned_at) * labor_after) / 2;
        present_s = returned_at * present_before + (dt - returned_at) * present_after;
    } else if (g->exp.active) {
        g->exp.remaining_s -= (uint32_t)dt;
        labor_s = dt * labor_weight_excluding(g, (int)g->exp.person_id) / 2;
        present_s = dt * count_present(g); /* away still AWAY */
    } else {
        labor_s = dt * labor_weight(g) / 2;
        present_s = dt * count_present(g);
    }

    if (!g->game_over) {
        apply_economy(g, dt, sn, sd, labor_s, present_s);
        tick_water_economy(g, dt, sn, sd);
        tick_farm(g, dt);
        tick_injure(g, dt);
        tick_fatigue_rest(g, dt);
        tick_chore(g);
        tick_wanderer(g, dt);
    }
    if (g->kitchen_lv >= 2) g->world_flags |= (uint8_t)ASH_WORLD_FARM;
    clamp_economy(g);
    sync_quest2_catchup(g);
    (void)ash_game_try_clear(g);
    g->last_settle_unix += dt;
    if (g->last_settle_unix > 0) {
        uint32_t day = (uint32_t)(g->last_settle_unix / 86400) + 1u;
        if (day != g->day_key) {
            g->day_key = day;
            g->day_latch_abx = ASHBOX_DAY_LATCH_NEVER; /* 抗生素日闩重置 */
            if (g->pace_era == ASHBOX_PACE_V13) roll_wanderer_day_schedule(g);
            /* water_made_today already rolled inside tick_water_economy */
        }
    }
    g->crc = ash_game_crc(g);
}

bool ash_game_wall_unix_usable(const ash_game_t *g, int64_t unix_now)
{
    if (unix_now <= 0) return false;
    if (!g || g->last_settle_unix <= 0) return true;
    /* ~2 min skew OK; larger backward jumps are typical after a power-cut RTC reset. */
    if (unix_now + 120 < g->last_settle_unix) return false;
    uint32_t day = (uint32_t)(unix_now / 86400) + 1u;
    /* Never let a wall snap pull day_key backwards (wipes day latches / 日勤). */
    if (day < g->day_key) return false;
    return true;
}

void ash_game_note_wall_time(ash_game_t *g, int64_t unix_now)
{
    if (!g || !ash_game_wall_unix_usable(g, unix_now)) return;
    uint32_t day = (uint32_t)(unix_now / 86400) + 1u;
    if (day != g->day_key) {
        g->day_key = day;
        g->day_latch_abx = ASHBOX_DAY_LATCH_NEVER;
        g->water_made_today = 0;
    }
    g->last_settle_unix = unix_now;
    g->crc = ash_game_crc(g);
}

bool ash_game_person_can_depart(const ash_game_t *g, uint8_t person_id)
{
    if (!g || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if (ash_game_work_locked(g)) return false;
    if ((*ash_game_person_at(g, (unsigned)(person_id))).state != ASH_PERSON_HEALTHY) return false;
    if ((*ash_game_person_at(g, (unsigned)(person_id))).fatigue >= FATIGUE_DEPART_LOCK) return false;
    return true;
}

bool ash_game_can_depart(const ash_game_t *g)
{
    if (!g || g->pop == 0) return false;
    if (g->exp.active || g->report.pending || g->crisis.pending) return false;
    if (ash_game_work_locked(g)) return false;
    if (ash_game_total_food(g) == 0) return false;
    if (ash_game_total_water(g) == 0) return false;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if (ash_game_person_can_depart(g, (uint8_t)i)) return true;
    }
    return false;
}

bool ash_game_tier_unlocked(const ash_game_t *g, ash_tier_t tier)
{
    if (!g || (unsigned)tier >= ASH_TIER_COUNT) return false;
    if (tier == ASH_TIER_HUNT) return g->gate_unlocked != 0;
    return true;
}

bool ash_game_start_expedition_ex(ash_game_t *g, ash_tier_t tier, uint8_t person_id,
                                  uint8_t crowbar_n, uint8_t take_pistol, uint8_t ammo_n)
{
    if (!ash_game_can_depart(g)) return false;
    if ((unsigned)tier >= ASH_TIER_COUNT) return false;
    if (!ash_game_tier_unlocked(g, tier)) return false;
    if (!ash_game_person_can_depart(g, person_id)) return false;

    uint16_t water_cost = ash_game_expedition_water_cost(tier);
    uint16_t food_cost = ash_game_expedition_food_cost(tier);
    if (ash_game_total_water(g) < water_cost) return false;
    if (ash_game_total_food(g) < food_cost) return false;

    int crow_cap = ash_game_crowbar_carry_cap(g, person_id);
    if ((int)crowbar_n > crow_cap) crowbar_n = (uint8_t)crow_cap;
    if (crowbar_n > g->crowbar) return false;

    bool has_equip_gun = person_id < ASHBOX_PERSON_MAX && (*ash_game_equip_pis_mut(g, (unsigned)(person_id)));
    if (take_pistol) {
        if (!has_equip_gun && g->pistol == 0) return false;
        if (ammo_n > g->ammo) return false;
    } else {
        ammo_n = 0;
    }

    if (!ash_game_take_water(g, water_cost)) return false;
    if (!ash_game_take_food(g, food_cost)) {
        (void)ash_game_give_water(g, water_cost, NULL);
        return false;
    }
    g->crowbar = (uint8_t)(g->crowbar - crowbar_n);
    if (take_pistol) {
        if (!has_equip_gun) g->pistol = (uint8_t)(g->pistol - 1);
        g->ammo = (uint8_t)(g->ammo - ammo_n);
        g->peace_broken = 1;
    } else if (has_equip_gun) {
        g->peace_broken = 1;
    }

    (*ash_game_person_mut(g, (unsigned)(person_id))).state = ASH_PERSON_AWAY;
    g->exp.active = true;
    g->exp.tier = tier;
    g->exp.person_id = person_id;
    g->exp.loadout = ASH_LOAD_NONE; /* legacy field unused */
    g->exp.duration_s = ash_game_tier_duration(tier);
    g->exp.remaining_s = g->exp.duration_s;
    g->exp_crowbar = crowbar_n;
    g->exp_pistol = take_pistol ? 1 : 0;
    g->exp_ammo = ammo_n;
    g->chore_kind = ASHBOX_CHORE_NONE;
    g->chore_done_exp = 0;
    {
        char logb[ASHBOX_JOURNAL_LEN];
        snprintf(logb, sizeof(logb), "%s出发%s·口粮%u", k_names[person_id], k_tier_name[tier],
                 (unsigned)food_cost);
        ash_game_log(g, logb);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_start_expedition(ash_game_t *g, ash_tier_t tier, uint8_t person_id,
                               ash_loadout_t loadout)
{
    (void)loadout;
    return ash_game_start_expedition_ex(g, tier, person_id, 0, 0, 0);
}

static void try_join_people(ash_game_t *g)
{
    if (g->kitchen_lv >= 2 && g->pop < 2) {
        g->pop = 2;
        (*ash_game_person_mut(g, (unsigned)(1))).id = 1;
        (*ash_game_person_mut(g, (unsigned)(1))).state = ASH_PERSON_HEALTHY;
        (*ash_game_person_mut(g, (unsigned)(1))).injure_remain_s = 0;
        (*ash_game_person_mut(g, (unsigned)(1))).trust = 0;
        (*ash_game_person_mut(g, (unsigned)(1))).fatigue = 0;
        ash_game_log(g, "老阙进站了");
    }
    if (g->workshop_lv >= 2 && g->pop < 3) {
        g->pop = 3;
        (*ash_game_person_mut(g, (unsigned)(2))).id = 2;
        (*ash_game_person_mut(g, (unsigned)(2))).state = ASH_PERSON_HEALTHY;
        (*ash_game_person_mut(g, (unsigned)(2))).injure_remain_s = 0;
        (*ash_game_person_mut(g, (unsigned)(2))).trust = 0;
        (*ash_game_person_mut(g, (unsigned)(2))).fatigue = 0;
        ash_game_log(g, "小芒进站了");
    }
}

bool ash_game_upgrade(ash_game_t *g, int building)
{
    if (!g || ash_game_work_locked(g)) return false;
    uint8_t *lv = NULL;
    if (building == 0) lv = &g->dorm_lv;
    else if (building == 1) lv = &g->kitchen_lv;
    else if (building == 2) lv = &g->workshop_lv;
    else return false;
    int max_lv = ash_game_build_lv_max(g);
    if ((int)*lv >= max_lv) return false;
    if (*lv < 1 || *lv > 5) return false;
    uint16_t cost = k_upgrade_cost[*lv - 1];
    if (g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    (*lv)++;
    try_join_people(g);
    clamp_parts(g);
    {
        char logb[ASHBOX_JOURNAL_LEN];
        if (building == 0 && (int)*lv >= max_lv)
            snprintf(logb, sizeof(logb), "宿舍升满 · 歇得快不加粮");
        else if (building == 2)
            snprintf(logb, sizeof(logb), "工坊%u级 · 更快也更吵", (unsigned)*lv);
        else {
            const char *n = (building == 0) ? "宿舍" : "食堂";
            snprintf(logb, sizeof(logb), "%s升到%u级", n, (unsigned)*lv);
        }
        ash_game_log(g, logb);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_upgrade_stash(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g)) return false;
    uint8_t lv = g->stash_lv < 1 ? 1 : g->stash_lv;
    if (lv >= ASHBOX_STASH_LV_MAX) return false;
    uint16_t cost = ash_game_stash_upgrade_cost(g);
    if (cost == 0 || g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->stash_lv = (uint8_t)(lv + 1u);
    {
        char logb[ASHBOX_JOURNAL_LEN];
        snprintf(logb, sizeof(logb), "仓库扩到%u级", (unsigned)g->stash_lv);
        ash_game_log(g, logb);
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_upgrade_furn(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || g->furn_lv >= ASHBOX_FURN_LV_MAX) return false;
    uint16_t cost = ash_game_furn_upgrade_cost(g);
    if (cost == 0 || g->parts < cost) return false;
    if (g->furn_lv == 1 && g->item[ASH_ITEM_TOKEN] < 1) return false; /* 干燥架耗信物 */
    g->parts = (uint16_t)(g->parts - cost);
    if (g->furn_lv == 1) g->item[ASH_ITEM_TOKEN]--;
    g->furn_lv = (uint8_t)(g->furn_lv + 1u);
    ash_game_log(g, g->furn_lv == 1 ? "货架装好 · 仓容+20" : "干燥架装好 · 菜地催熟");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

uint16_t ash_game_gear_upgrade_cost_ember(const ash_game_t *g)
{
    if (!g || g->gear_lv >= ASHBOX_GEAR_LV_MAX) return 0;
    return k_gear_ember_cost[g->gear_lv];
}

uint16_t ash_game_gear_upgrade_cost_parts(const ash_game_t *g)
{
    if (!g || g->gear_lv >= ASHBOX_GEAR_LV_MAX) return 0;
    return k_gear_parts_cost[g->gear_lv];
}

uint16_t ash_game_quiet_gear_cost(const ash_game_t *g)
{
    if (!g || g->quiet_gear) return 0;
    return k_quiet_gear_cost;
}

uint16_t ash_game_door_bolt_cost(const ash_game_t *g)
{
    if (!g || !g->quiet_gear || g->door_bolt) return 0;
    return k_door_bolt_cost;
}

const char *ash_game_gear_hint(const ash_game_t *g)
{
    if (!g || g->gear_lv == 0) return "猎装降低空手而归概率 · 猎场干肉+1 · 建设可升";
    if (g->gear_lv == 1) return "1级 · 猎场过夜空手而归概率-3 · 废街-2 · 干肉+1";
    return "2级 · 猎场过夜空手而归概率-6 · 废街-4 · 干肉再+1";
}

bool ash_game_upgrade_gear(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || g->gear_lv >= ASHBOX_GEAR_LV_MAX) return false;
    uint16_t ce = ash_game_gear_upgrade_cost_ember(g);
    uint16_t cp = ash_game_gear_upgrade_cost_parts(g);
    if (ce == 0 || g->ember < ce || g->parts < cp) return false;
    g->ember = (uint16_t)(g->ember - ce);
    g->parts = (uint16_t)(g->parts - cp);
    g->gear_lv = (uint8_t)(g->gear_lv + 1u);
    {
        char logb[ASHBOX_JOURNAL_LEN];
        snprintf(logb, sizeof(logb), "猎装升到%u级", (unsigned)g->gear_lv);
        ash_game_log(g, logb);
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_buy_quiet_gear(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || g->quiet_gear) return false;
    uint16_t cost = ash_game_quiet_gear_cost(g);
    if (cost == 0 || g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->quiet_gear = 1;
    ash_game_log(g, "静音轮装好 · 威胁涨得慢了");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_buy_door_bolt(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || !g->quiet_gear || g->door_bolt) return false;
    uint16_t cost = ash_game_door_bolt_cost(g);
    if (cost == 0 || g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->door_bolt = 1;
    ash_game_log(g, "门扣装好 · 威胁更慢修门更省");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_has_stove_lining(const ash_game_t *g)
{
    return g && (g->pad_v10b & ASHBOX_PAD_STOVE_LINING) != 0;
}

bool ash_game_has_box_lamp(const ash_game_t *g)
{
    return g && (g->pad_v10b & ASHBOX_PAD_BOX_LAMP) != 0;
}

uint16_t ash_game_stove_lining_cost(const ash_game_t *g)
{
    if (!g || !g->door_bolt || ash_game_has_stove_lining(g)) return 0;
    return ASHBOX_STOVE_LINING_COST_PARTS;
}

uint16_t ash_game_box_lamp_cost(const ash_game_t *g)
{
    if (!g || g->gear_lv < ASHBOX_GEAR_LV_MAX || ash_game_has_box_lamp(g)) return 0;
    return ASHBOX_BOX_LAMP_COST_EMBER;
}

bool ash_game_buy_stove_lining(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || !g->door_bolt || ash_game_has_stove_lining(g))
        return false;
    uint16_t cost = ash_game_stove_lining_cost(g);
    if (cost == 0 || g->parts < cost) return false;
    g->parts = (uint16_t)(g->parts - cost);
    g->pad_v10b = (uint8_t)(g->pad_v10b | ASHBOX_PAD_STOVE_LINING);
    ash_game_log(g, "灶层装好 · 口粮更省");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_buy_box_lamp(ash_game_t *g)
{
    if (!g || ash_game_work_locked(g) || g->gear_lv < ASHBOX_GEAR_LV_MAX ||
        ash_game_has_box_lamp(g))
        return false;
    uint16_t cost = ash_game_box_lamp_cost(g);
    if (cost == 0 || g->ember < cost) return false;
    g->ember = (uint16_t)(g->ember - cost);
    g->pad_v10b = (uint8_t)(g->pad_v10b | ASHBOX_PAD_BOX_LAMP);
    ash_game_log(g, "匣灯亮了 · 空手而归时威胁更轻");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_tide_offer_shards(ash_game_t *g)
{
    if (!g || quest_done_of(g, 2)) return false;
    if (quest_step_of(g, 2) != 2) return false;
    if (g->item[ASH_ITEM_SHARD] < 2) return false;
    g->item[ASH_ITEM_SHARD] = (uint8_t)(g->item[ASH_ITEM_SHARD] - 2);
    if (g->lore_shards > 2) g->lore_shards = (uint8_t)(g->lore_shards - 2u);
    else g->lore_shards = 0;
    try_advance_quest(g, 2, 2);
    ash_game_log(g, "灰片交潮商 · 记下潮因");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_unlock_gate(ash_game_t *g)
{
    if (g->gate_unlocked) return true;
    if (g->ember < 3) return false;
    g->ember = (uint16_t)(g->ember - 3);
    g->gate_unlocked = 1;
    ash_game_log(g, "开闸成功 · 猎场可走了");
    try_advance_quest(g, 2, 0);
    g->crc = ash_game_crc(g);
    return true;
}

static void apply_report_loot(ash_game_t *g)
{
    g->report_waste_flags = 0;
    if (g->report.food_delta > 0) {
        (void)ash_game_give_food(g, (uint16_t)g->report.food_delta, NULL);
    } else if (g->report.food_delta < 0) {
        (void)ash_game_take_food(g, (uint16_t)(-g->report.food_delta));
    }
    if (g->report.parts_delta > 0) {
        (void)ash_game_give_parts(g, (uint16_t)g->report.parts_delta, NULL);
    } else if (g->report.parts_delta < 0) {
        (void)ash_game_take_parts(g, (uint16_t)(-g->report.parts_delta));
    }
    int ember = (int)g->ember + (int)g->report.ember_delta;
    if (ember < 0) ember = 0;
    if (ember > ASHBOX_EMBER_CAP) ember = ASHBOX_EMBER_CAP;
    g->ember = (uint16_t)ember;

    if (g->report_water_delta > 0) {
        (void)ash_game_give_water(g, (uint16_t)g->report_water_delta, NULL);
    }
    g->report_water_delta = 0;
    if (g->report_iron_delta > 0) {
        (void)ash_game_give_iron(g, (uint16_t)g->report_iron_delta, NULL);
    }
    g->report_iron_delta = 0;
    clamp_economy(g);
}

void ash_game_ack_report(ash_game_t *g)
{
    if (!g->report.pending) return;
    if (g->report.branch_count > 0 && !g->report.branch_resolved) return;
    apply_report_loot(g);
    memset(&g->report, 0, sizeof(g->report));
    g->report_water_delta = 0;
    g->report_iron_delta = 0;
    g->report_waste_flags = 0;
    g->report_death_kind = ASH_DEATH_NONE;
    maybe_arm_crisis(g);
    g->crc = ash_game_crc(g);
}

bool ash_game_pick_branch(ash_game_t *g, uint8_t branch_slot)
{
    if (!g->report.pending || g->report.branch_resolved) return false;
    if (branch_slot >= g->report.branch_count) return false;
    uint8_t bid = g->report.branch_ids[branch_slot];
    const ash_branch_def_t *b = ash_content_branch(bid);
    /* exp.person_id 在 settle 归来后仍保留，指向本次归来者 */
    uint8_t target = g->exp.person_id;
    if (target >= g->pop || target >= ASHBOX_PERSON_MAX) target = 0;
    (*ash_game_person_mut(g, (unsigned)(target))).trust = (int8_t)((*ash_game_person_mut(g, (unsigned)(target))).trust + b->trust_delta);
    clamp_trust(&(*ash_game_person_mut(g, (unsigned)(target))));

    /* 资源变动并入待收取战报，确认收下时一次入账 */
    g->report.food_delta = (int16_t)(g->report.food_delta + b->food_delta);
    g->report.parts_delta = (int16_t)(g->report.parts_delta + b->parts_delta);

    int th = (int)g->threat + b->threat_delta;
    if (th < 0) th = 0;
    if (th > 100) th = 100;
    g->threat = (uint8_t)th;

    if (b->quest_id != 255) {
        try_advance_quest(g, b->quest_id, b->quest_need);
    }
    if (b->log && b->log[0]) ash_game_log(g, b->log);

    {
        /* G5：L1 保留场面；会计进 L2 */
        char buf[48];
        if (b->log && b->log[0]) set_line(&g->report, 1, b->log);
        else set_line(&g->report, 1, "抉择已定");
        if (g->report.ember_delta != 0) {
            snprintf(buf, sizeof(buf), "口粮%+d 零件%+d 余烬%+d", (int)g->report.food_delta,
                     (int)g->report.parts_delta, (int)g->report.ember_delta);
        } else {
            snprintf(buf, sizeof(buf), "口粮%+d 零件%+d", (int)g->report.food_delta,
                     (int)g->report.parts_delta);
        }
        set_line(&g->report, 2, buf);
    }

    g->report.branch_resolved = 1;
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_crisis_pending(const ash_game_t *g)
{
    return g && g->crisis.pending;
}

static int fortify_cost(const ash_game_t *g)
{
    int cost = 3;
    for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
        if (i == 1 && (*ash_game_person_at(g, (unsigned)(i))).state != ASH_PERSON_AWAY) cost = 2;
    }
    if (g->gate_unlocked) cost = (cost > 1) ? cost - 1 : 1;
    if (g->door_bolt) cost = (cost > 1) ? cost - 1 : 1;
    return cost;
}

bool ash_game_crisis_option_ok(const ash_game_t *g, ash_crisis_opt_t opt)
{
    if (!g || !g->crisis.pending) return false;
    if (g->exp.active || g->report.pending) return false;
    switch (opt) {
    case ASH_CRISIS_TALK: {
        /* 阿禾在站（含虚弱），或任意在站且信任>=2；无人匹配时仍可用「有人开口」兜底 */
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_at(g, (unsigned)(i))).state == ASH_PERSON_AWAY) continue;
            if (i == 0 || (*ash_game_person_at(g, (unsigned)(i))).trust >= 2) return true;
        }
        return count_present(g) >= 1;
    }
    case ASH_CRISIS_FORTIFY:
        return g->parts >= (uint16_t)fortify_cost(g);
    case ASH_CRISIS_HIDE:
        /* 在站即可躲（含虚弱/受伤），保证危机始终至少有一条出路 */
        return count_present(g) >= 1;
    default:
        return false;
    }
}

uint16_t ash_game_crisis_fortify_cost(const ash_game_t *g)
{
    if (!g) return 0;
    return (uint16_t)fortify_cost(g);
}

bool ash_game_resolve_crisis(ash_game_t *g, ash_crisis_opt_t opt)
{
    if (!ash_game_crisis_option_ok(g, opt)) return false;
    switch (opt) {
    case ASH_CRISIS_TALK: {
        int drop = 20;
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_mut(g, (unsigned)(i))).state == ASH_PERSON_AWAY) continue;
            if (i == 0 && (*ash_game_person_mut(g, (unsigned)(i))).state == ASH_PERSON_HEALTHY) drop = 35;
            else if (i == 0) drop = 28;
        }
        int th = (int)g->threat - drop;
        if (th < 15) th = 15;
        g->threat = (uint8_t)th;
        ash_game_log(g, "阿禾把话摊开 · 门外退了");
        try_advance_quest(g, 0, 3);
        if (!quest_done_of(g, 0) && quest_step_of(g, 0) >= 4) finish_quest(g, 0);
        break;
    }
    case ASH_CRISIS_FORTIFY: {
        int cost = fortify_cost(g);
        g->parts = (uint16_t)(g->parts - (uint16_t)cost);
        int th = (int)g->threat - 40;
        if (th < 10) th = 10;
        g->threat = (uint8_t)th;
        ash_game_log(g, "锁扣敲死 · 闸门扛住了");
        try_advance_quest(g, 0, 3);
        if (!quest_done_of(g, 0) && quest_step_of(g, 0) >= 4) finish_quest(g, 0);
        break;
    }
    case ASH_CRISIS_HIDE: {
        int drop = 15;
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if (i == 2 && (*ash_game_person_mut(g, (unsigned)(i))).state != ASH_PERSON_AWAY) drop = 28;
        }
        int th = (int)g->threat - drop;
        if (th < 20) th = 20;
        g->threat = (uint8_t)th;
        g->hide_penalty_h = 4;
        ash_game_log(g, "小芒捂住呼吸 · 一夜未出");
        try_advance_quest(g, 0, 3);
        if (!quest_done_of(g, 0) && quest_step_of(g, 0) >= 4) finish_quest(g, 0);
        break;
    }
    default:
        return false;
    }
    g->crisis.pending = false;
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_radio(const ash_game_t *g)
{
    static char s_line[48];

    if (g->crisis.pending) {
        return "威胁临门 · 守夜抉择";
    }
    if (g->report.pending) {
        if (!g->report.branch_resolved && g->report.branch_count > 0) {
            return "战报待选";
        }
        if (g->report.food_delta > 0 || g->report.parts_delta > 0) {
            snprintf(s_line, sizeof(s_line), "带回 口粮%d 零件%d", (int)g->report.food_delta,
                     (int)g->report.parts_delta);
        } else {
            snprintf(s_line, sizeof(s_line), "%s", g->report.lines[0]);
        }
        return s_line;
    }

    /* 远征途中：站事优先，其次站令提醒，再分档灰句 */
    if (g->exp.active && g->exp.duration_s > 0) {
        if (ash_game_chore_pending(g)) {
            snprintf(s_line, sizeof(s_line), "站事 · %s", ash_game_chore_label(g));
            return s_line;
        }
        uint32_t elapsed = 0;
        if (g->exp.duration_s > g->exp.remaining_s) {
            elapsed = g->exp.duration_s - g->exp.remaining_s;
        }
        if (g->threat >= 50 && g->night_order == ASH_ORDER_NONE &&
            ((elapsed / 30u) % 4u) == 1u) {
            return "深睡前先设站令";
        }
        unsigned n = ash_content_away_count();
        if (n == 0) return "远征途中";
        /* 分段：近路 0-9 · 废街 10-19 · 深/猎/夜 20..n-1 */
        unsigned base = 0, span = n;
        if (g->exp.tier == ASH_TIER_SHORT) {
            base = 0;
            span = (n > 10u) ? 10u : n;
        } else if (g->exp.tier == ASH_TIER_BLOCK) {
            base = (n > 10u) ? 10u : 0u;
            span = (n > 20u) ? 10u : (n - base);
        } else {
            base = (n > 20u) ? 20u : 0u;
            span = n - base;
        }
        if (span == 0) {
            base = 0;
            span = n;
        }
        unsigned phase = (unsigned)((elapsed * (uint32_t)span) / g->exp.duration_s);
        if (phase >= span) phase = span - 1u;
        unsigned idx = base + ((phase + g->exp.person_id) % span);
        return ash_content_away_line(idx);
    }

    /* 硬钩：断粮/满仓/可收/高压 — 须压过纪事 */
    if (g->food == 0 && g->stash_food > 0) {
        return "仓有粮 · 回补口袋";
    }
    if (any_weak_or_no_food(g)) {
        return "灶冷了 · 先补口粮";
    }
    if (ash_game_stash_full(g)) {
        return "仓满了 · 先扩仓库";
    }
    if (ash_game_farm_ready_count(g) > 0) {
        return "田地熟了 · 去据点收";
    }
    if (g->threat >= 80) {
        return "铁轨有脚步 · 守住闸门";
    }

    /* 软钩：只在「值得动手」时插，避免整日盖死末世广播 */
    if (need_seed_via_trade(g)) return "无种 · 去交易换";
    if (ash_game_trade2_available(g) && trade_hook_worth(g)) return "渡口今日可换";
    if (ash_game_trade_available(g) && trade_hook_worth(g)) return "铁棚今日可换";
    if (all_plots_empty(g) && can_plant_any(g)) return "空田地可种 · 去菜地";
    if (g->food <= 6 && g->item[ASH_ITEM_JERKY] > 0) return "干肉可换口粮";
    if (!g->gate_unlocked && g->ember >= 3) return "余烬满3 · 可开闸走猎场";
    if (g->threat >= 60 && g->night_order == ASH_ORDER_NONE) {
        return "威胁升高 · 先设站令";
    }

    /* G4 守望层：三线了结后压过 twist/idle，硬钩仍优先 */
    if (g->lore_twist && quest_done_of(g, 0) && quest_done_of(g, 1) && quest_done_of(g, 2)) {
        unsigned wi = (g->day_key * 3u + g->expedition_total) % ash_content_radio_watch_count();
        return ash_content_radio_watch(wi);
    }

    /* 氛围：纪事不得永久盖住灰潮/营地广播；反转层优先于旧 lore */
    unsigned lane = (g->day_key + g->expedition_total) % 3u;
    if (lane == 0u && g->lore_twist) {
        unsigned ti = (g->day_key + g->expedition_total) % ash_content_radio_twist_count();
        return ash_content_radio_twist(ti);
    }
    if (lane == 0u && g->gate_unlocked && g->lore_shards > 0) {
        /* 每档灰片两句变体，避免同档永远同一句 */
        unsigned band = (unsigned)(g->lore_shards - 1u);
        if (band > 5u) band = 5u;
        unsigned li = band * 2u + ((g->day_key + g->expedition_total) & 1u);
        unsigned lore_n = ash_content_radio_lore_count();
        if (li >= lore_n) li = lore_n - 1u;
        return ash_content_radio_lore(li);
    }
    if (lane == 1u) {
        if (g->world_flags & ASH_WORLD_CAMP2) {
            if (g->rep_ferry > 0) {
                snprintf(s_line, sizeof(s_line), "渡口认得你 · 望%u", (unsigned)g->rep_ferry);
                return s_line;
            }
            return "渡口有红薯种 · 慢收更饱";
        }
        if (g->world_flags & ASH_WORLD_CAMP) {
            if (g->rep_shed > 0) {
                snprintf(s_line, sizeof(s_line), "铁棚频率 · 望%u", (unsigned)g->rep_shed);
                return s_line;
            }
            return "铁棚频率还在";
        }
        if (g->night_order != ASH_ORDER_NONE) {
            return ash_content_order_name((ash_night_order_t)g->night_order);
        }
    }
    if (g->journal_count > 0) {
        unsigned idx = (g->journal_head + ASHBOX_JOURNAL_N - 1u) % ASHBOX_JOURNAL_N;
        return g->journal[idx];
    }
    if (g->night_order != ASH_ORDER_NONE) {
        return ash_content_order_name((ash_night_order_t)g->night_order);
    }
    unsigned idx = (g->day_key * 3u + g->expedition_total) % ash_content_radio_idle_count();
    return ash_content_radio_idle(idx);
}

bool ash_game_set_night_order(ash_game_t *g, ash_night_order_t order)
{
    if (!g || (unsigned)order >= ASH_ORDER_COUNT) return false;
    g->night_order = (uint8_t)order;
    {
        char logb[ASHBOX_JOURNAL_LEN];
        snprintf(logb, sizeof(logb), "站令 · %s", ash_content_order_name(order));
        ash_game_log(g, logb);
    }
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_order_name(ash_night_order_t order)
{
    return ash_content_order_name(order);
}

const char *ash_game_order_hint(ash_night_order_t order)
{
    return ash_content_order_hint(order);
}

const char *ash_game_order_result_line(ash_night_order_t order)
{
    switch (order) {
    case ASH_ORDER_RATION:
        return "站令见效 · 口粮省了";
    case ASH_ORDER_WATCH:
        return "站令见效 · 威胁慢些";
    case ASH_ORDER_RUSH:
        return "站令见效 · 零件多了";
    case ASH_ORDER_NONE:
    default:
        return "不设站令";
    }
}

const char *ash_game_threat_pace_word(const ash_game_t *g)
{
    if (!g) return "";
    int n = workshop_threat_num(g);
    int base = (int)g->threat_rise_num;
    if (base < 1) base = 10;
    /* 相对未改装基准：静音/门扣压低=慢；工坊抬高=快 */
    if (n < base) return "慢";
    if (n > base) return "快";
    return "";
}

const char *ash_game_tag_label(uint8_t tag_id)
{
    return ash_content_tag_label(tag_id);
}

const char *ash_game_journal_line(const ash_game_t *g, unsigned age)
{
    if (!g || g->journal_count == 0 || age >= g->journal_count) return "";
    unsigned idx = (g->journal_head + ASHBOX_JOURNAL_N - 1u - age) % ASHBOX_JOURNAL_N;
    return g->journal[idx];
}

int ash_game_scene(const ash_game_t *g, bool wake_guard, bool night_aftermath)
{
    /* 自动聚焦（与 ashbox_scene_t 同序，避免 game↔LVGL 头文件耦合） */
    if (g->crisis.pending) return 3; /* THREAT */
    if (g->report.pending) return 4; /* RETURN */
    if (any_weak_or_no_food(g)) return 2; /* WEAK */
    if (g->threat >= 80) return 3; /* THREAT */
    /* 站内可行动：压过开闸氛围；远征中仍锁 AWAY */
    if (!g->exp.active) {
        if (ash_game_farm_ready_count(g) > 0) return 8; /* FARM */
        if (ash_game_stash_full(g)) return 9; /* STASH */
        if ((ash_game_trade_available(g) || ash_game_trade2_available(g)) &&
            trade_hook_worth(g))
            return 10; /* TRADE */
    }
    if (g->exp.active) return 1; /* AWAY */
    if (g->lore_twist && quest_done_of(g, 0) && quest_done_of(g, 1) &&
        quest_done_of(g, 2))
        return 11; /* WATCH */
    if (g->gate_unlocked) return 6; /* GATE */
    {
        int in_station = 0;
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_at(g, (unsigned)(i))).state != ASH_PERSON_AWAY) in_station++;
        }
        if (in_station >= 2) return 7; /* DUO */
    }
    if (wake_guard || night_aftermath) return 5; /* NIGHT */
    return 0; /* IDLE */
}

int ash_game_scenes_available(const ash_game_t *g, bool wake_guard, bool night_aftermath,
                              int *out, int cap)
{
    /* 当前局势下可浏览的场景集合（条件真才进表；危机/战报锁单景） */
    if (!g || !out || cap <= 0) return 0;
    int n = 0;
#define ASH_SCENE_PUSH(sc)                                                                 \
    do {                                                                                   \
        if (n < cap) out[n++] = (int)(sc);                                                 \
    } while (0)

    if (g->crisis.pending) {
        ASH_SCENE_PUSH(3); /* THREAT */
        return n;
    }
    if (g->report.pending) {
        ASH_SCENE_PUSH(4); /* RETURN */
        return n;
    }
    if (g->exp.active) {
        ASH_SCENE_PUSH(1); /* AWAY */
        if (any_weak_or_no_food(g)) ASH_SCENE_PUSH(2); /* WEAK */
        return n;
    }

    /* 站内：按场景 id 升序收集当前条件成立的景 */
    ASH_SCENE_PUSH(0); /* IDLE 常在 */
    if (any_weak_or_no_food(g)) ASH_SCENE_PUSH(2);
    if (g->threat >= 80) ASH_SCENE_PUSH(3);
    if (wake_guard || night_aftermath) ASH_SCENE_PUSH(5);
    if (g->gate_unlocked) ASH_SCENE_PUSH(6);
    {
        int in_station = 0;
        for (int i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if ((*ash_game_person_at(g, (unsigned)(i))).state != ASH_PERSON_AWAY) in_station++;
        }
        if (in_station >= 2) ASH_SCENE_PUSH(7);
    }
    if (ash_game_farm_ready_count(g) > 0) ASH_SCENE_PUSH(8);
    if (ash_game_stash_full(g)) ASH_SCENE_PUSH(9);
    if ((ash_game_trade_available(g) || ash_game_trade2_available(g)) && trade_hook_worth(g))
        ASH_SCENE_PUSH(10);
    if (g->lore_twist && quest_done_of(g, 0) && quest_done_of(g, 1) && quest_done_of(g, 2))
        ASH_SCENE_PUSH(11);
#undef ASH_SCENE_PUSH
    return n;
}

bool ash_game_camp_open(const ash_game_t *g)
{
    return g && (g->world_flags & ASH_WORLD_CAMP) != 0;
}

bool ash_game_camp2_open(const ash_game_t *g)
{
    return g && (g->world_flags & ASH_WORLD_CAMP2) != 0;
}

bool ash_game_farm_unlocked(const ash_game_t *g)
{
    return g && (g->kitchen_lv >= 2 || (g->world_flags & ASH_WORLD_FARM) != 0);
}

static const char *k_item_name[ASHBOX_ITEM_N] = {"稻谷种", "干肉", "信物·干燥架", "灰片"};

const char *ash_game_item_name(ash_item_t id)
{
    if ((unsigned)id >= ASHBOX_ITEM_N) return "?";
    return k_item_name[id];
}

uint8_t ash_game_item_count(const ash_game_t *g, ash_item_t id)
{
    if (!g || (unsigned)id >= ASHBOX_ITEM_N) return 0;
    return g->item[id];
}

bool ash_game_stash_deposit(ash_game_t *g, bool food_not_parts, uint16_t amount)
{
    if (!g || amount == 0) return false;
    if (amount > STASH_STEP) amount = STASH_STEP;
    if (food_not_parts) {
        int scap = ash_game_stash_cap(g);
        uint16_t room = (g->stash_food < (uint16_t)scap) ? (uint16_t)(scap - g->stash_food) : 0;
        if (room == 0 || g->food == 0) return false;
        if (amount > room) amount = room;
        if (amount > g->food) amount = g->food;
        g->food = (uint16_t)(g->food - amount);
        g->stash_food = (uint16_t)(g->stash_food + amount);
    } else {
        uint16_t room = (g->stash_parts < ASHBOX_PARTS_CAP) ? (uint16_t)(ASHBOX_PARTS_CAP - g->stash_parts) : 0;
        if (room == 0 || g->parts == 0) return false;
        if (amount > room) amount = room;
        if (amount > g->parts) amount = g->parts;
        g->parts = (uint16_t)(g->parts - amount);
        g->stash_parts = (uint16_t)(g->stash_parts + amount);
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_stash_withdraw(ash_game_t *g, bool food_not_parts, uint16_t amount)
{
    if (!g || amount == 0) return false;
    if (amount > STASH_STEP) amount = STASH_STEP;
    if (food_not_parts) {
        int cap = ash_game_food_cap(g);
        uint16_t room = (g->food < (uint16_t)cap) ? (uint16_t)(cap - g->food) : 0;
        if (room == 0 || g->stash_food == 0) return false;
        if (amount > room) amount = room;
        if (amount > g->stash_food) amount = g->stash_food;
        g->stash_food = (uint16_t)(g->stash_food - amount);
        g->food = (uint16_t)(g->food + amount);
    } else {
        uint16_t room = (g->parts < ASHBOX_PARTS_CAP) ? (uint16_t)(ASHBOX_PARTS_CAP - g->parts) : 0;
        if (room == 0 || g->stash_parts == 0) return false;
        if (amount > room) amount = room;
        if (amount > g->stash_parts) amount = g->stash_parts;
        g->stash_parts = (uint16_t)(g->stash_parts - amount);
        g->parts = (uint16_t)(g->parts + amount);
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_farm_plant(ash_game_t *g, uint8_t plot, ash_crop_t crop)
{
    if (!g || ash_game_work_locked(g) || !ash_game_farm_unlocked(g) ||
        plot >= (uint8_t)ash_game_farm_plot_count(g))
        return false;
    if ((*ash_game_farm_mut(g, (unsigned)(plot))).crop != 0) return false;
    if (crop != ASH_CROP_GRAIN && crop != ASH_CROP_ROOT) return false;
    if (crop == ASH_CROP_ROOT && !ash_game_root_unlocked(g)) return false;
    uint32_t grow;
    if (g->pace_era == ASHBOX_PACE_V13) {
        grow = (crop == ASH_CROP_ROOT) ? FARM_GROW_ROOT_S : FARM_GROW_GRAIN_S;
    } else {
        grow = (crop == ASH_CROP_ROOT) ? FARM_GROW_ROOT_S_LEGACY : FARM_GROW_GRAIN_S_LEGACY;
    }
    if (crop == ASH_CROP_GRAIN) {
        if (g->item[ASH_ITEM_SEED] == 0) return false;
        g->item[ASH_ITEM_SEED]--;
        ash_game_log(g, g->pace_era == ASHBOX_PACE_V13 ? "播下稻谷 · 约1天基础30口粮"
                                                         : "播下稻谷 · 收得快些粮少些");
    } else {
        if (g->seed_root == 0) return false;
        g->seed_root--;
        ash_game_log(g, g->pace_era == ASHBOX_PACE_V13 ? "埋下红薯 · 约3天基础240口粮"
                                                         : "埋下红薯 · 收得慢些更饱");
    }
    if (station_has_trait(g, ASHBOX_TRAIT_FARM_FAST)) {
        grow = (grow * 8u) / 10u;
        if (grow < 3600u) grow = 3600u;
    }
    (*ash_game_farm_mut(g, (unsigned)(plot))).crop = (uint8_t)crop;
    (*ash_game_farm_mut(g, (unsigned)(plot))).remain_s = grow;
    if (plot < ASHBOX_FARM_PLOTS_MAX) g->farm_labor_h[plot] = 0;
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_farm_harvest(ash_game_t *g, uint8_t plot)
{
    if (!g || ash_game_work_locked(g) || plot >= (uint8_t)ash_game_farm_plot_count(g))
        return false;
    if ((*ash_game_farm_mut(g, (unsigned)(plot))).crop == 0 || (*ash_game_farm_mut(g, (unsigned)(plot))).remain_s > 0) return false;
    unsigned seed = g->day_key * 5u + plot * 3u + g->expedition_total;
    int gain;
    uint8_t crop = (*ash_game_farm_mut(g, (unsigned)(plot))).crop;
    uint8_t labor_h = (plot < ASHBOX_FARM_PLOTS_MAX) ? g->farm_labor_h[plot] : 0;
    if (g->pace_era == ASHBOX_PACE_V13) {
        if (crop == ASH_CROP_ROOT) {
            int bonus = (int)labor_h * ASHBOX_FARM_ROOT_BONUS_PER_H;
            if (bonus > ASHBOX_FARM_ROOT_BONUS_CAP) bonus = ASHBOX_FARM_ROOT_BONUS_CAP;
            gain = ASHBOX_FARM_ROOT_BASE + bonus;
        } else {
            int bonus = (int)labor_h * ASHBOX_FARM_GRAIN_BONUS_PER_H;
            if (bonus > ASHBOX_FARM_GRAIN_BONUS_CAP) bonus = ASHBOX_FARM_GRAIN_BONUS_CAP;
            gain = ASHBOX_FARM_GRAIN_BASE + bonus;
        }
    } else {
        if (crop == ASH_CROP_ROOT) gain = 7 + (int)(seed % 4u);
        else gain = 4 + (int)(seed % 3u);
    }
    if (station_has_trait(g, ASHBOX_TRAIT_FARM_YIELD)) {
        gain = gain + (gain * 25) / 100;
    }
    if (station_has_trait(g, ASHBOX_TRAIT_ALL_DOUBLE)) {
        gain *= 2;
    }
    bool wasted = false;
    uint16_t stored = ash_game_give_food(g, (uint16_t)gain, &wasted);
    (*ash_game_farm_mut(g, (unsigned)(plot))).crop = 0;
    (*ash_game_farm_mut(g, (unsigned)(plot))).remain_s = 0;
    if (plot < ASHBOX_FARM_PLOTS_MAX) g->farm_labor_h[plot] = 0;
    if (wasted || stored < (uint16_t)gain) {
        ash_game_log(g, "装不下的口粮坏了");
    } else if (crop == ASH_CROP_ROOT) {
        ash_game_log(g, "红薯收成 · 更饱入库");
    } else {
        ash_game_log(g, "稻谷收成入库");
    }
    /* 已种活一畦：若仍卡在换种步，允许补推进（无干肉亦可） */
    if (quest_step_of(g, 2) == 2) try_advance_quest(g, 2, 2);
    try_advance_quest(g, 2, 3);
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_farm_status(const ash_game_t *g, uint8_t plot, char *buf, size_t n)
{
    if (!buf || n == 0) return "";
    if (!g || plot >= (uint8_t)ash_game_farm_plot_count(g)) {
        snprintf(buf, n, "—");
        return buf;
    }
    if ((*ash_game_farm_at(g, (unsigned)(plot))).crop == 0) {
        snprintf(buf, n, "空田地 · 可种");
    } else if ((*ash_game_farm_at(g, (unsigned)(plot))).remain_s == 0) {
        snprintf(buf, n, "%s熟 · 可收", ash_game_crop_name((ash_crop_t)(*ash_game_farm_at(g, (unsigned)(plot))).crop));
    } else {
        unsigned h = ((*ash_game_farm_at(g, (unsigned)(plot))).remain_s + 3599u) / 3600u;
        snprintf(buf, n, "%s · 约%u时", ash_game_crop_name((ash_crop_t)(*ash_game_farm_at(g, (unsigned)(plot))).crop), h);
    }
    return buf;
}

bool ash_game_trade_available(const ash_game_t *g)
{
    if (!ash_game_camp_open(g)) return false;
    uint16_t today = day_latch_today(g);
    if (g->day_latch_trade != today) return true;
    /* 声望≥2：同日可再换一次（小额二次） */
    return g->rep_shed >= 2 && g->day_latch_trade_extra != today;
}

bool ash_game_trade2_available(const ash_game_t *g)
{
    if (!ash_game_camp2_open(g)) return false;
    uint16_t today = day_latch_today(g);
    if (g->day_latch_trade2 != today) return true;
    return g->rep_ferry >= 2 && g->day_latch_trade2_extra != today;
}

bool ash_game_trade_can_pay(const ash_game_t *g, ash_trade_t kind)
{
    if (!g || (unsigned)kind >= ASH_TRADE_COUNT) return false;
    switch (kind) {
    case ASH_TRADE_JERKY_FOR_SEED:
    case ASH_TRADE_JERKY_FOR_ROOT:
        return g->item[ASH_ITEM_JERKY] >= 1;
    case ASH_TRADE_PARTS_FOR_ROOT:
        return ash_game_total_parts(g) >= 5;
    case ASH_TRADE_FOOD_FOR_WATER:
        return ash_game_total_food(g) >= 5;
    case ASH_TRADE_WATER_FOR_FOOD:
        return ash_game_total_water(g) >= 1;
    case ASH_TRADE_WATER_FOR_IRON:
        return ash_game_total_water(g) >= 5;
    case ASH_TRADE_IRON_FOR_WATER:
        return g->iron >= 1;
    case ASH_TRADE_IRON_FOR_PARTS:
        return g->iron >= 5;
    case ASH_TRADE_PARTS_FOR_IRON:
        return ash_game_total_parts(g) >= 1;
    case ASH_TRADE_PARTS_FOR_EMBER:
        return ash_game_total_parts(g) >= 20 &&
               g->day_latch_ember != day_latch_today(g);
    case ASH_TRADE_PARTS_FOR_STIM:
        return ash_game_total_parts(g) >= 30 && g->med_stim < ASHBOX_ITEM_CAP;
    case ASH_TRADE_PARTS_FOR_ABX:
        return ash_game_total_parts(g) >= ASHBOX_ABX_TRADE_PARTS &&
               g->med_abx < ASHBOX_ITEM_CAP &&
               g->day_latch_abx != day_latch_today(g);
    default:
        return false;
    }
}

bool ash_game_use_jerky(ash_game_t *g)
{
    if (!g || g->item[ASH_ITEM_JERKY] < 1) return false;
    g->item[ASH_ITEM_JERKY]--;
    (void)ash_game_give_food(g, 4, NULL);
    ash_game_log(g, "干肉换成口粮");
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_person_state_label(ash_person_state_t st)
{
    switch (st) {
    case ASH_PERSON_HEALTHY: return "健康";
    case ASH_PERSON_WEAK: return "虚弱";
    case ASH_PERSON_AWAY: return "在外";
    case ASH_PERSON_INJURED: return "重伤";
    case ASH_PERSON_INFECTED: return "热伤";
    default: return "?";
    }
}

bool ash_game_use_milk(ash_game_t *g, uint8_t person_id)
{
    if (!g || g->med_milk < 1 || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if ((*ash_game_person_mut(g, (unsigned)(person_id))).state == ASH_PERSON_AWAY) return false;
    g->med_milk--;
    (*ash_game_person_mut(g, (unsigned)(person_id))).fatigue = 0;
    {
        char buf[36];
        snprintf(buf, sizeof(buf), "%s喝了旺仔牛奶 · 不累了", k_names[person_id]);
        ash_game_log(g, buf);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_use_stim(ash_game_t *g, uint8_t person_id)
{
    if (!g || g->med_stim < 1 || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if ((*ash_game_person_mut(g, (unsigned)(person_id))).state != ASH_PERSON_INJURED) return false;
    g->med_stim--;
    (*ash_game_person_mut(g, (unsigned)(person_id))).state = ASH_PERSON_HEALTHY;
    (*ash_game_person_mut(g, (unsigned)(person_id))).injure_remain_s = 0;
    {
        char buf[36];
        snprintf(buf, sizeof(buf), "%s打了肾上腺素 · 伤压住了", k_names[person_id]);
        ash_game_log(g, buf);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_use_abx(ash_game_t *g, uint8_t person_id)
{
    if (!g || g->med_abx < 1 || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if ((*ash_game_person_mut(g, (unsigned)(person_id))).state != ASH_PERSON_INFECTED) return false;
    g->med_abx--;
    (*ash_game_person_mut(g, (unsigned)(person_id))).state = ASH_PERSON_HEALTHY;
    (*ash_game_person_mut(g, (unsigned)(person_id))).injure_remain_s = 0;
    {
        char buf[36];
        snprintf(buf, sizeof(buf), "%s吃了抗生素 · 热伤退了", k_names[person_id]);
        ash_game_log(g, buf);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_trade(ash_game_t *g, ash_trade_t kind)
{
    if (!g || ash_game_work_locked(g) || (unsigned)kind >= ASH_TRADE_COUNT) return false;
    bool camp2 = (kind == ASH_TRADE_JERKY_FOR_ROOT || kind == ASH_TRADE_PARTS_FOR_ROOT);
    bool chain = (kind >= ASH_TRADE_FOOD_FOR_WATER);
    if (camp2) {
        if (!ash_game_trade2_available(g)) return false;
    } else if (kind == ASH_TRADE_JERKY_FOR_SEED) {
        if (!ash_game_trade_available(g)) return false;
    } else if (chain) {
        /* Resource chain available at either open camp. */
        if (!ash_game_trade_available(g) && !ash_game_trade2_available(g)) return false;
    } else {
        if (!ash_game_trade_available(g)) return false;
    }
    if (!ash_game_trade_can_pay(g, kind)) return false;
    bool ok = false;
    if (kind == ASH_TRADE_JERKY_FOR_SEED) {
        g->item[ASH_ITEM_JERKY]--;
        g->item[ASH_ITEM_SEED] = sat_add_u8(g->item[ASH_ITEM_SEED], 1, ASHBOX_ITEM_CAP);
        ok = true;
    } else if (kind == ASH_TRADE_JERKY_FOR_ROOT) {
        g->item[ASH_ITEM_JERKY]--;
        g->seed_root = sat_add_u8(g->seed_root, 1, ASHBOX_SEED_ROOT_CAP);
        ok = true;
    } else if (kind == ASH_TRADE_PARTS_FOR_ROOT) {
        ok = ash_game_take_parts(g, 5);
        if (ok) g->seed_root = sat_add_u8(g->seed_root, 1, ASHBOX_SEED_ROOT_CAP);
    } else if (kind == ASH_TRADE_FOOD_FOR_WATER) {
        ok = ash_game_take_food(g, 5);
        if (ok) (void)ash_game_give_water(g, 1, NULL);
    } else if (kind == ASH_TRADE_WATER_FOR_FOOD) {
        ok = ash_game_take_water(g, 1);
        if (ok) (void)ash_game_give_food(g, 3, NULL);
    } else if (kind == ASH_TRADE_WATER_FOR_IRON) {
        ok = ash_game_take_water(g, 5);
        if (ok) (void)ash_game_give_iron(g, 1, NULL);
    } else if (kind == ASH_TRADE_IRON_FOR_WATER) {
        ok = ash_game_take_iron(g, 1);
        if (ok) (void)ash_game_give_water(g, 3, NULL);
    } else if (kind == ASH_TRADE_IRON_FOR_PARTS) {
        ok = ash_game_take_iron(g, 5);
        if (ok) (void)ash_game_give_parts(g, 1, NULL);
    } else if (kind == ASH_TRADE_PARTS_FOR_IRON) {
        ok = ash_game_take_parts(g, 1);
        if (ok) (void)ash_game_give_iron(g, 3, NULL);
    } else if (kind == ASH_TRADE_PARTS_FOR_EMBER) {
        ok = ash_game_take_parts(g, 20);
        if (ok) {
            g->ember = sat_add_u16(g->ember, 1, ASHBOX_EMBER_CAP);
            g->day_latch_ember = day_latch_today(g);
        }
    } else if (kind == ASH_TRADE_PARTS_FOR_STIM) {
        ok = ash_game_take_parts(g, 30);
        if (ok) g->med_stim = sat_add_u8(g->med_stim, 1, ASHBOX_ITEM_CAP);
    } else if (kind == ASH_TRADE_PARTS_FOR_ABX) {
        ok = ash_game_take_parts(g, ASHBOX_ABX_TRADE_PARTS);
        if (ok) {
            g->med_abx = sat_add_u8(g->med_abx, 1, ASHBOX_ITEM_CAP);
            g->day_latch_abx = day_latch_today(g);
        }
    }
    if (!ok) return false;
    {
        uint16_t today = day_latch_today(g);
        if (camp2) {
            if (g->day_latch_trade2 != today) g->day_latch_trade2 = today;
            else g->day_latch_trade2_extra = today;
            if (g->rep_ferry < ASHBOX_REP_MAX) g->rep_ferry = (uint8_t)(g->rep_ferry + 1u);
            ash_game_log(g, "渡口成交");
        } else if (chain && ash_game_trade2_available(g) && !ash_game_trade_available(g)) {
            if (g->day_latch_trade2 != today) g->day_latch_trade2 = today;
            else g->day_latch_trade2_extra = today;
            if (g->rep_ferry < ASHBOX_REP_MAX) g->rep_ferry = (uint8_t)(g->rep_ferry + 1u);
            ash_game_log(g, "渡口换资源");
        } else {
            if (g->day_latch_trade != today) g->day_latch_trade = today;
            else g->day_latch_trade_extra = today;
            if (g->rep_shed < ASHBOX_REP_MAX) g->rep_shed = (uint8_t)(g->rep_shed + 1u);
            if (!chain) {
                g->item[ASH_ITEM_TOKEN] = sat_add_u8(g->item[ASH_ITEM_TOKEN], 1, ASHBOX_ITEM_CAP);
            }
            ash_game_log(g, chain ? "铁棚换资源" : "铁棚成交 · 种子");
            if (kind == ASH_TRADE_JERKY_FOR_SEED) try_advance_quest(g, 2, 2);
        }
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

const char *ash_game_trade_label(ash_trade_t kind)
{
    switch (kind) {
    case ASH_TRADE_JERKY_FOR_SEED: return "干肉换稻谷种";
    case ASH_TRADE_JERKY_FOR_ROOT: return "干肉换红薯种";
    case ASH_TRADE_PARTS_FOR_ROOT: return "零件5换红薯种";
    case ASH_TRADE_FOOD_FOR_WATER: return "口粮5换水1";
    case ASH_TRADE_WATER_FOR_FOOD: return "水1换口粮3";
    case ASH_TRADE_WATER_FOR_IRON: return "水5换铁1";
    case ASH_TRADE_IRON_FOR_WATER: return "铁1换水3";
    case ASH_TRADE_IRON_FOR_PARTS: return "铁5换零件1";
    case ASH_TRADE_PARTS_FOR_IRON: return "零件1换铁3";
    case ASH_TRADE_PARTS_FOR_EMBER: return "零件20换余烬1";
    case ASH_TRADE_PARTS_FOR_STIM: return "零件30换肾上腺素";
    case ASH_TRADE_PARTS_FOR_ABX: return "零件24换抗生素";
    default: return "?";
    }
}

const char *ash_game_trade_hint(ash_trade_t kind)
{
    switch (kind) {
    case ASH_TRADE_JERKY_FOR_SEED: return "铁棚 · 稻谷快收粮少";
    case ASH_TRADE_JERKY_FOR_ROOT: return "渡口 · 红薯慢收更饱";
    case ASH_TRADE_PARTS_FOR_ROOT: return "渡口 · 红薯慢收更饱";
    case ASH_TRADE_FOOD_FOR_WATER:
    case ASH_TRADE_WATER_FOR_FOOD:
    case ASH_TRADE_WATER_FOR_IRON:
    case ASH_TRADE_IRON_FOR_WATER:
    case ASH_TRADE_IRON_FOR_PARTS:
    case ASH_TRADE_PARTS_FOR_IRON: return "回换更少 · 应急对换";
    case ASH_TRADE_PARTS_FOR_EMBER: return "每日每站限换1烬";
    case ASH_TRADE_PARTS_FOR_STIM: return "救急压重伤 · 极贵";
    case ASH_TRADE_PARTS_FOR_ABX: return "退热伤 · 每日限1";
    default: return "";
    }
}

/* 交易底栏：声望可见 + 二次门槛（P0-3） */
void ash_game_trade_footer(const ash_game_t *g, ash_trade_t kind, char *out, size_t out_n)
{
    if (!out || out_n == 0) return;
    out[0] = '\0';
    if (!g) return;
    bool ferry =
        (kind == ASH_TRADE_JERKY_FOR_ROOT || kind == ASH_TRADE_PARTS_FOR_ROOT);
    unsigned rep = ferry ? (unsigned)g->rep_ferry : (unsigned)g->rep_shed;
    const char *camp = ferry ? "渡口" : "铁棚";
    snprintf(out, out_n, "%s望%u · 满2可同日二次", camp, rep);
}

bool ash_game_craft_can(const ash_game_t *g, ash_craft_t kind)
{
    if (!g || (unsigned)kind >= ASH_CRAFT_COUNT) return false;
    if (kind != ASH_CRAFT_BENCH && !g->craft_bench) return false;
    switch (kind) {
    case ASH_CRAFT_BENCH:
        return !g->craft_bench && ash_game_total_parts(g) >= ASHBOX_CRAFT_BENCH_PARTS &&
               g->iron >= ASHBOX_CRAFT_BENCH_IRON;
    case ASH_CRAFT_BUCKET:
        return g->iron >= ASHBOX_CRAFT_BUCKET_IRON &&
               ash_game_stash_slots_used(g) + 1 <= ash_game_stash_slots_cap(g);
    case ASH_CRAFT_PISTOL:
        return g->iron >= ASHBOX_CRAFT_PISTOL_IRON;
    case ASH_CRAFT_AMMO:
        return g->iron >= ASHBOX_CRAFT_AMMO_IRON;
    case ASH_CRAFT_CROWBAR:
        return g->iron >= ASHBOX_CRAFT_CROWBAR_IRON;
    default:
        return false;
    }
}

const char *ash_game_craft_label(ash_craft_t kind)
{
    switch (kind) {
    case ASH_CRAFT_BENCH: return "做工作台";
    case ASH_CRAFT_BUCKET: return "做水罐";
    case ASH_CRAFT_PISTOL: return "做手枪";
    case ASH_CRAFT_AMMO: return "做子弹";
    case ASH_CRAFT_CROWBAR: return "做撬棍";
    default: return "?";
    }
}

const char *ash_game_craft_hint(ash_craft_t kind)
{
    switch (kind) {
    case ASH_CRAFT_BENCH: return "20零件+50铁 · 解锁制作";
    case ASH_CRAFT_BUCKET: return "18铁 · 存水多18 · 占1仓位";
    case ASH_CRAFT_PISTOL: return "100铁 · 做好可装备";
    case ASH_CRAFT_AMMO: return "2铁 · 可挡突然袭击";
    case ASH_CRAFT_CROWBAR: return "10铁 · 出门可携带";
    default: return "";
    }
}

bool ash_game_craft(ash_game_t *g, ash_craft_t kind)
{
    if (!ash_game_craft_can(g, kind)) return false;
    switch (kind) {
    case ASH_CRAFT_BENCH:
        if (!ash_game_take_parts(g, ASHBOX_CRAFT_BENCH_PARTS)) return false;
        if (!ash_game_take_iron(g, ASHBOX_CRAFT_BENCH_IRON)) {
            (void)ash_game_give_parts(g, ASHBOX_CRAFT_BENCH_PARTS, NULL);
            return false;
        }
        g->craft_bench = 1;
        ash_game_log(g, "工作台支好了");
        break;
    case ASH_CRAFT_BUCKET:
        if (!ash_game_take_iron(g, ASHBOX_CRAFT_BUCKET_IRON)) return false;
        g->bucket = (uint8_t)(g->bucket + 1);
        ash_game_log(g, "水罐做好 · 能多存水");
        break;
    case ASH_CRAFT_PISTOL:
        if (!ash_game_take_iron(g, ASHBOX_CRAFT_PISTOL_IRON)) return false;
        g->pistol = (uint8_t)(g->pistol + 1);
        ash_game_log(g, "手枪做好了");
        break;
    case ASH_CRAFT_AMMO:
        if (!ash_game_take_iron(g, ASHBOX_CRAFT_AMMO_IRON)) return false;
        if (g->ammo < 250) g->ammo = (uint8_t)(g->ammo + 1);
        ash_game_log(g, "子弹+1");
        break;
    case ASH_CRAFT_CROWBAR:
        if (!ash_game_take_iron(g, ASHBOX_CRAFT_CROWBAR_IRON)) return false;
        if (g->crowbar < 20) g->crowbar = (uint8_t)(g->crowbar + 1);
        ash_game_log(g, "撬棍做好了");
        break;
    default:
        return false;
    }
    clamp_economy(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_equip_backpack(ash_game_t *g, uint8_t person_id, bool on)
{
    if (!g || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if (on) {
        if ((*ash_game_equip_bp_mut(g, (unsigned)(person_id)))) return true;
        if (g->backpack == 0) return false;
        g->backpack--;
        (*ash_game_equip_bp_mut(g, (unsigned)(person_id))) = 1;
    } else {
        if (!(*ash_game_equip_bp_mut(g, (unsigned)(person_id)))) return true;
        if (ash_game_stash_slots_used(g) + 1 > ash_game_stash_slots_cap(g)) return false;
        (*ash_game_equip_bp_mut(g, (unsigned)(person_id))) = 0;
        g->backpack = (uint8_t)(g->backpack + 1);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_equip_pistol(ash_game_t *g, uint8_t person_id, bool on)
{
    if (!g || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if (on) {
        if ((*ash_game_equip_pis_mut(g, (unsigned)(person_id)))) return true;
        if (g->pistol == 0) return false;
        g->pistol--;
        (*ash_game_equip_pis_mut(g, (unsigned)(person_id))) = 1;
        g->peace_broken = 1;
    } else {
        if (!(*ash_game_equip_pis_mut(g, (unsigned)(person_id)))) return true;
        (*ash_game_equip_pis_mut(g, (unsigned)(person_id))) = 0;
        g->pistol = (uint8_t)(g->pistol + 1);
    }
    g->crc = ash_game_crc(g);
    return true;
}

static const char *const k_wander_names[] = {
    "灰皮", "夜行", "旧靴", "哑口", "过路客", "外环人", "半罐", "干口",
};

bool ash_game_wanderer_pending(const ash_game_t *g)
{
    return g && g->wanderer_pending;
}

const char *ash_game_wanderer_name(const ash_game_t *g)
{
    if (!g) return "过路客";
    unsigned n = (unsigned)(sizeof(k_wander_names) / sizeof(k_wander_names[0]));
    return k_wander_names[g->wanderer_name_id % n];
}

bool ash_game_wanderer_can_join(const ash_game_t *g)
{
    if (!g || !g->wanderer_pending) return false;
    if (g->pop >= ash_game_beds(g)) return false;
    if (g->pop >= ash_game_pop_cap(g)) return false;
    return ash_game_total_food(g) >= ASHBOX_WANDERER_JOIN_FOOD &&
           ash_game_total_water(g) >= ASHBOX_WANDERER_JOIN_WATER;
}

const char *ash_game_wanderer_join_block_reason(const ash_game_t *g)
{
    if (!g || !g->wanderer_pending) return "门外没人";
    if (g->pop >= ash_game_beds(g) || g->pop >= ash_game_pop_cap(g)) return "床不够";
    if (ash_game_total_food(g) < ASHBOX_WANDERER_JOIN_FOOD ||
        ash_game_total_water(g) < ASHBOX_WANDERER_JOIN_WATER)
        return "口粮和水不够";
    return "";
}

uint8_t ash_game_wanderer_trait(const ash_game_t *g)
{
    return g ? g->wanderer_trait : ASHBOX_TRAIT_NONE;
}

const char *ash_game_work_trait_name(uint8_t trait)
{
    switch (trait) {
    case ASHBOX_TRAIT_ALL_DOUBLE:
        return "收获全部翻倍";
    case ASHBOX_TRAIT_WORK_PARTS_X2:
        return "工坊零件翻倍";
    case ASHBOX_TRAIT_WORK_SPD_60:
        return "工坊速度加60%";
    case ASHBOX_TRAIT_WORK_SPD_40:
        return "工坊速度加40%";
    case ASHBOX_TRAIT_WORK_SPD_20:
        return "工坊速度加20%";
    case ASHBOX_TRAIT_FARM_FAST:
        return "种田加快两成";
    case ASHBOX_TRAIT_FARM_YIELD:
        return "种田产量加25%";
    default:
        return "没有别的本事";
    }
}

const char *ash_game_work_trait_hint(uint8_t trait)
{
    switch (trait) {
    case ASHBOX_TRAIT_ALL_DOUBLE:
        return "远征菜地工坊带回各翻一倍";
    case ASHBOX_TRAIT_WORK_PARTS_X2:
        return "在工坊时零件产量翻倍";
    case ASHBOX_TRAIT_WORK_SPD_60:
        return "在工坊时生产更快";
    case ASHBOX_TRAIT_WORK_SPD_40:
        return "在工坊时生产更快";
    case ASHBOX_TRAIT_WORK_SPD_20:
        return "在工坊时生产更快";
    case ASHBOX_TRAIT_FARM_FAST:
        return "这一轮剩余时间按八折";
    case ASHBOX_TRAIT_FARM_YIELD:
        return "收成在基础和田作加成后再加";
    default:
        return "没有别的本事";
    }
}

uint8_t ash_game_person_work_trait(const ash_game_t *g, uint8_t person_id)
{
    if (!g || person_id >= ASHBOX_PERSON_MAX) return ASHBOX_TRAIT_NONE;
    return g->person_trait[person_id];
}

static void note_wipeout_reject(ash_game_t *g)
{
    if (!g || g->pop != 0 || g->game_over) return;
    if (g->wipeout_reject_count < 255) g->wipeout_reject_count++;
    if (g->wipeout_reject_count >= 2) {
        g->game_over = 1;
        g->end_toast_pending = 1;
        ash_game_log(g, "据点空了 · 撑不下去");
        if (g->run_cleared && !g->joined_after_clear) {
            /* nobody achievement path after clear handled by UI/meta eval */
        }
    }
}

bool ash_game_wanderer_dismiss(ash_game_t *g)
{
    if (!g || !g->wanderer_pending) return false;
    bool broke = (g->pop == 0 && !ash_game_wanderer_can_join(g));
    g->wanderer_pending = 0;
    g->wanderer_trait = ASHBOX_TRAIT_NONE;
    g->wanderer_next_s = (g->pop == 0) ? 900u : 3600u + (g->day_key % 1800u);
    ash_game_log(g, "打发走了门外的人");
    if (broke) note_wipeout_reject(g);
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_wanderer_join(ash_game_t *g)
{
    if (!g || !g->wanderer_pending) return false;
    if (!ash_game_wanderer_can_join(g)) {
        if (g->pop == 0) note_wipeout_reject(g);
        g->crc = ash_game_crc(g);
        return false;
    }
    if (!ash_game_take_food(g, ASHBOX_WANDERER_JOIN_FOOD)) return false;
    if (!ash_game_take_water(g, ASHBOX_WANDERER_JOIN_WATER)) {
        (void)ash_game_give_food(g, ASHBOX_WANDERER_JOIN_FOOD, NULL);
        return false;
    }
    uint8_t slot = g->pop;
    if (slot >= ASHBOX_PERSON_MAX) return false;
    (*ash_game_person_mut(g, (unsigned)(slot))).id = slot;
    (*ash_game_person_mut(g, (unsigned)(slot))).state = ASH_PERSON_HEALTHY;
    (*ash_game_person_mut(g, (unsigned)(slot))).injure_remain_s = 0;
    (*ash_game_person_mut(g, (unsigned)(slot))).trust = 0;
    (*ash_game_person_mut(g, (unsigned)(slot))).fatigue = 20;
    (*ash_game_person_mut(g, (unsigned)(slot))).flags = 0;
    (*ash_game_equip_bp_mut(g, (unsigned)(slot))) = 0;
    (*ash_game_equip_pis_mut(g, (unsigned)(slot))) = 0;
    g->person_job[slot] = ASHBOX_JOB_REST;
    g->person_trait[slot] = g->wanderer_trait;
    g->pop = (uint8_t)(g->pop + 1);
    g->wipeout_reject_count = 0;
    if (g->run_cleared) g->joined_after_clear = 1;
    g->wanderer_pending = 0;
    g->wanderer_trait = ASHBOX_TRAIT_NONE;
    g->wanderer_next_s = 7200u;
    {
        char buf[ASHBOX_JOURNAL_LEN];
        snprintf(buf, sizeof(buf), "%s留下了 · %s", ash_game_person_name(slot),
                 ash_game_work_trait_name(g->person_trait[slot]));
        ash_game_log(g, buf);
    }
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_wanderer_trade_available(const ash_game_t *g)
{
    return g && g->wanderer_pending && !g->wanderer_met_trade;
}

bool ash_game_wanderer_trade(ash_game_t *g, ash_trade_t kind)
{
    if (!ash_game_wanderer_trade_available(g)) return false;
    if (kind == ASH_TRADE_PARTS_FOR_EMBER && !g->wanderer_ember_ok) return false;
    /* Apply rate bias: cheap = better for player give less / get more conceptually
     * by adjusting amounts ±1 step on chain trades. */
    int food_n = 5, water_n = 5, iron_n = 5, parts_n = 1;
    if (g->wanderer_rate_kind == ASH_WANDER_RATE_CHEAP) {
        food_n = 4;
        water_n = 4;
        iron_n = 4;
    } else if (g->wanderer_rate_kind == ASH_WANDER_RATE_DEAR) {
        food_n = 6;
        water_n = 6;
        iron_n = 6;
    }
    bool ok = false;
    if (kind == ASH_TRADE_FOOD_FOR_WATER) {
        ok = ash_game_take_food(g, (uint16_t)food_n);
        if (ok) (void)ash_game_give_water(g, 1, NULL);
    } else if (kind == ASH_TRADE_WATER_FOR_IRON) {
        ok = ash_game_take_water(g, (uint16_t)water_n);
        if (ok) (void)ash_game_give_iron(g, 1, NULL);
    } else if (kind == ASH_TRADE_IRON_FOR_PARTS) {
        ok = ash_game_take_iron(g, (uint16_t)iron_n);
        if (ok) (void)ash_game_give_parts(g, (uint16_t)parts_n, NULL);
    } else if (kind == ASH_TRADE_PARTS_FOR_EMBER && g->wanderer_ember_ok) {
        ok = ash_game_take_parts(g, 20);
        if (ok) g->ember = sat_add_u16(g->ember, 1, ASHBOX_EMBER_CAP);
    } else {
        return false;
    }
    if (!ok) return false;
    g->wanderer_met_trade = 1;
    ash_game_log(g, "与过路客成交");
    g->crc = ash_game_crc(g);
    return true;
}

static uint8_t roll_wanderer_trait(unsigned seed)
{
    unsigned r = seed % 100u;
    if (r < 1u) return ASHBOX_TRAIT_ALL_DOUBLE;
    if (r < 3u) return ASHBOX_TRAIT_WORK_PARTS_X2;
    if (r < 11u) return ASHBOX_TRAIT_WORK_SPD_60;
    if (r < 25u) return ASHBOX_TRAIT_WORK_SPD_40;
    if (r < 50u) return ASHBOX_TRAIT_WORK_SPD_20;
    if (r < 75u) return ASHBOX_TRAIT_FARM_FAST;
    return ASHBOX_TRAIT_FARM_YIELD;
}

static int64_t local_day_index(int64_t unix_s)
{
    return (unix_s + 8 * 3600) / 86400;
}

static int64_t visit_unix_for_min(int64_t day_idx, unsigned min_of_day)
{
    return day_idx * 86400 - 8 * 3600 + (int64_t)min_of_day * 60;
}

static void roll_wanderer_day_schedule(ash_game_t *g)
{
    unsigned seed = g->day_key * 29u + g->expedition_total * 11u + g->threat * 3u + 7u;
    unsigned span = (22u * 60u) - (8u * 60u); /* 840 minutes */
    unsigned a = 8u * 60u + (seed % span);
    unsigned b = 8u * 60u + ((seed / 7u) % span);
    if (b == a) b = 8u * 60u + ((a + 97u) % span);
    if (a > b) {
        unsigned t = a;
        a = b;
        b = t;
    }
    g->wanderer_t0_min = (uint16_t)a;
    g->wanderer_t1_min = (uint16_t)b;
    g->wanderer_fired = 0;
    g->wanderer_sched_day = (uint16_t)(g->day_key > 0xFFFFu ? 0xFFFFu : g->day_key);
}

static void appear_wanderer(ash_game_t *g, unsigned seed)
{
    g->wanderer_pending = 1;
    g->wanderer_name_id = (uint8_t)(seed % 8u);
    g->wanderer_met_trade = 0;
    g->wanderer_rate_kind =
        ((seed / 3u) % 2u) ? ASH_WANDER_RATE_CHEAP : ASH_WANDER_RATE_DEAR;
    g->wanderer_ember_ok = (((seed / 11u) % 100u) < 2u) ? 1 : 0;
    g->wanderer_trait = roll_wanderer_trait(seed / 13u);
    ash_game_log(g, "门外有人来访");
}

static void tick_wanderer_legacy(ash_game_t *g, int64_t dt_s)
{
    if (g->wanderer_pending) return;
    if (g->wanderer_next_s > (uint32_t)dt_s) {
        g->wanderer_next_s -= (uint32_t)dt_s;
        return;
    }
    g->wanderer_next_s = 0;
    unsigned seed = g->day_key * 19u + g->expedition_total * 7u + g->threat;
    unsigned chance = (g->pop == 0) ? 70u : 18u;
    if ((seed % 100u) >= chance) {
        g->wanderer_next_s = (g->pop == 0) ? 600u : 2400u;
        return;
    }
    appear_wanderer(g, seed);
}

static void tick_wanderer_v13(ash_game_t *g, int64_t dt_s)
{
    if (g->last_settle_unix <= 0) return;
    if (g->wanderer_sched_day != (uint16_t)(g->day_key > 0xFFFFu ? 0xFFFFu : g->day_key)) {
        roll_wanderer_day_schedule(g);
    }
    int64_t t0 = g->last_settle_unix;
    int64_t t1 = t0 + dt_s;
    int64_t day_idx = local_day_index(t1);
    unsigned mins[2] = {g->wanderer_t0_min, g->wanderer_t1_min};
    for (int slot = 0; slot < 2; slot++) {
        if (g->wanderer_fired & (uint8_t)(1u << slot)) continue;
        int64_t v = visit_unix_for_min(day_idx, mins[slot]);
        /* Also catch visits if settle window sits past them today. */
        if (v > t1) continue;
        if (v <= t0 && local_day_index(t0) == day_idx && v < t0) {
            /* Already passed earlier today before this settle — still fire once. */
        } else if (v <= t0 && local_day_index(t0) != day_idx) {
            continue;
        }
        if (g->wanderer_pending) {
            /* 顺延：人不走不叠第二个弹窗，也不标记已响 */
            continue;
        }
        unsigned seed = g->day_key * 19u + g->expedition_total * 7u + (unsigned)slot * 41u +
                        (unsigned)mins[slot];
        appear_wanderer(g, seed);
        g->wanderer_fired = (uint8_t)(g->wanderer_fired | (uint8_t)(1u << slot));
    }
}

static void tick_wanderer(ash_game_t *g, int64_t dt_s)
{
    if (!g || dt_s <= 0) return;
    if (g->pace_era == ASHBOX_PACE_V13) tick_wanderer_v13(g, dt_s);
    else tick_wanderer_legacy(g, dt_s);
}

bool ash_game_try_clear(ash_game_t *g)
{
    if (!g || g->run_cleared) return false;
    if (!quest_done_of(g, 0) || !quest_done_of(g, 1) || !quest_done_of(g, 2)) return false;
    if (!g->lore_twist) return false;
    g->run_cleared = 1;
    g->clear_toast_pending = 1;
    ash_game_log(g, "三线了结 · 据点还在");
    return true;
}

bool ash_game_set_person_job(ash_game_t *g, uint8_t person_id, uint8_t job)
{
    if (!g || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) return false;
    if (job > ASHBOX_JOB_WORK) return false;
    if ((*ash_game_person_at(g, person_id)).state == ASH_PERSON_AWAY) return false;
    if ((job == ASHBOX_JOB_FARM || job == ASHBOX_JOB_WORK) &&
        (*ash_game_person_at(g, person_id)).fatigue >= ASHBOX_LABOR_REHIRE_BELOW) {
        ash_game_log(g, "还累着 · 先歇到不那么累");
        return false;
    }
    g->person_job[person_id] = job;
    g->crc = ash_game_crc(g);
    return true;
}

uint8_t ash_game_person_job(const ash_game_t *g, uint8_t person_id)
{
    if (!g || person_id >= ASHBOX_PERSON_MAX) return ASHBOX_JOB_REST;
    if (person_id < g->pop &&
        (*ash_game_person_at(g, person_id)).state == ASH_PERSON_AWAY) {
        return 255; /* away sentinel for UI */
    }
    return g->person_job[person_id];
}

const char *ash_game_job_name(uint8_t job)
{
    if (job == 255) return "在外";
    if (job == ASHBOX_JOB_FARM) return "田作";
    if (job == ASHBOX_JOB_WORK) return "工坊";
    return "休整";
}

bool ash_game_can_use_super_core(const ash_game_t *g)
{
    return g && g->super_core > 0 && g->build_cap_bonus == 0;
}

bool ash_game_use_super_core(ash_game_t *g)
{
    if (!ash_game_can_use_super_core(g)) return false;
    g->super_core = 0;
    g->build_cap_bonus = 2;
    g->loot_bonus_perm_pct = 50;
    g->farm_plots_unlocked = ASHBOX_FARM_PLOTS_MAX;
    ash_game_log(g, "用了超级核心 · 田地建档远征全变强");
    g->crc = ash_game_crc(g);
    return true;
}

bool ash_game_is_game_over(const ash_game_t *g)
{
    return g && g->game_over;
}

uint32_t ash_game_wanderer_next_wake_s(const ash_game_t *g)
{
    if (!g || g->pace_era != ASHBOX_PACE_V13) return UINT32_MAX;
    if (g->wanderer_pending) return 0;
    if (g->last_settle_unix <= 0) return UINT32_MAX;
    if (g->wanderer_sched_day != (uint16_t)(g->day_key > 0xFFFFu ? 0xFFFFu : g->day_key))
        return 60u; /* schedule will roll on next settle */
    int64_t now = g->last_settle_unix;
    int64_t day_idx = local_day_index(now);
    unsigned mins[2] = {g->wanderer_t0_min, g->wanderer_t1_min};
    uint32_t best = UINT32_MAX;
    for (int slot = 0; slot < 2; slot++) {
        if (g->wanderer_fired & (uint8_t)(1u << slot)) continue;
        int64_t v = visit_unix_for_min(day_idx, mins[slot]);
        if (v <= now) continue;
        int64_t d = v - now;
        if (d > 0 && d < (int64_t)best) best = (uint32_t)d;
    }
    return best;
}

bool ash_game_exile_blocked_reason(const ash_game_t *g, uint8_t person_id, char *out,
                                   size_t out_n)
{
    if (!out || out_n == 0) return true;
    out[0] = '\0';
    if (!g || person_id >= g->pop || person_id >= ASHBOX_PERSON_MAX) {
        snprintf(out, out_n, "没这个人");
        return true;
    }
    if ((*ash_game_person_at(g, person_id)).state == ASH_PERSON_AWAY ||
        (g->exp.active && g->exp.person_id == person_id)) {
        snprintf(out, out_n, "还在外面 · 回来再说");
        return true;
    }
    return false;
}

bool ash_game_exile_person(ash_game_t *g, uint8_t person_id)
{
    char reason[40];
    if (ash_game_exile_blocked_reason(g, person_id, reason, sizeof(reason))) return false;
    char buf[ASHBOX_JOURNAL_LEN];
    snprintf(buf, sizeof(buf), "%s离开营地", ash_game_person_name(person_id));
    remove_person_at(g, person_id);
    ash_game_log(g, buf);
    g->crc = ash_game_crc(g);
    return true;
}
