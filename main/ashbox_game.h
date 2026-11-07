#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ASHBOX_PERSON_CORE 3 /* mid-save person/equip arrays (ABI frozen) */
#define ASHBOX_PERSON_MAX 6  /* live pop cap; slots 3..5 in v12 person_ext */
#define ASHBOX_REPORT_LINES 3
#define ASHBOX_JOURNAL_N 8
#define ASHBOX_JOURNAL_LEN 36
#define ASHBOX_QUEST_CORE 2 /* quest 0/1 live in mid-save arrays */
#define ASHBOX_QUEST_N 3    /* + quest 2 in v4 append fields */
#define ASHBOX_QUEST_STEPS 5
#define ASHBOX_BRANCH_MAX 3
#define ASHBOX_ITEM_N 4
#define ASHBOX_ITEM_CAP 20
#define ASHBOX_FARM_PLOTS 2     /* mid-save farm array size */
#define ASHBOX_FARM_PLOTS_MAX 4 /* with super-core farm_ext */
#define ASHBOX_PARTS_CAP 999
#define ASHBOX_EMBER_CAP 999
#define ASHBOX_MAGIC 0x48534132u /* ASH2 */
#define ASHBOX_SAVE_VER 13
#define ASHBOX_BUILD_LV_BASE 3
#define ASHBOX_JOB_REST 0
#define ASHBOX_JOB_FARM 1
#define ASHBOX_JOB_WORK 2
#define ASHBOX_PACE_LEGACY 0
#define ASHBOX_PACE_V13 1
#define ASHBOX_LABOR_FATIGUE_PER_H 5
#define ASHBOX_LABOR_AUTO_REST 80
#define ASHBOX_LABOR_REHIRE_BELOW 50
#define ASHBOX_FARM_GRAIN_BASE 30
#define ASHBOX_FARM_ROOT_BASE 240
#define ASHBOX_FARM_GRAIN_BONUS_PER_H 2
#define ASHBOX_FARM_ROOT_BONUS_PER_H 8
#define ASHBOX_FARM_GRAIN_BONUS_CAP 24
#define ASHBOX_FARM_ROOT_BONUS_CAP 96
#define ASHBOX_FARM_LABOR_H_CAP 12
#define ASHBOX_EMBER_P_DEEP 35u
#define ASHBOX_EMBER_P_HUNT 20u
#define ASHBOX_EMBER_P_NIGHT 45u
#define ASHBOX_TRAIT_NONE 0
#define ASHBOX_TRAIT_ALL_DOUBLE 1
#define ASHBOX_TRAIT_WORK_PARTS_X2 2
#define ASHBOX_TRAIT_WORK_SPD_60 3
#define ASHBOX_TRAIT_WORK_SPD_40 4
#define ASHBOX_TRAIT_WORK_SPD_20 5
#define ASHBOX_TRAIT_FARM_FAST 6
#define ASHBOX_TRAIT_FARM_YIELD 7
#define ASHBOX_TRAIT_COUNT 8
/** Day-latch sentinel: never used / available today. Day 0 is a valid latch day. */
#define ASHBOX_DAY_LATCH_NEVER 0xFFFFu
#define ASHBOX_WELL_LV_NONE 0
#define ASHBOX_WELL_LV_PIT 1
#define ASHBOX_WELL_LV_WELL 2
#define ASHBOX_PIT_COST_PARTS 10u
#define ASHBOX_WELL_UPGRADE_COST_PARTS 20u
#define ASHBOX_PIT_WATER_PER_H 1u
#define ASHBOX_PIT_WATER_DAY_CAP 12u
#define ASHBOX_WELL_WATER_PER_H 6u
#define ASHBOX_WELL_WATER_DAY_CAP 36u
#define ASHBOX_WELL_UPGRADE_S 86400u
#define ASHBOX_CREW_EXHAUST_S 86400u /* legacy field; no longer used for lock */
#define ASHBOX_WATER_BASE_CAP 50u
#define ASHBOX_WATER_BUCKET_BONUS 18u
#define ASHBOX_WATER_CAP 99u /* legacy clamp ceiling; live cap uses buckets */
#define ASHBOX_WANDERER_JOIN_FOOD 20u
#define ASHBOX_WANDERER_JOIN_WATER 10u
#define ASHBOX_STASH_SLOT_FOOD 300u
#define ASHBOX_STASH_SLOT_IRON 50u
#define ASHBOX_IRON_CAP 999u
#define ASHBOX_CRAFT_BENCH_PARTS 20u
#define ASHBOX_CRAFT_BENCH_IRON 50u
#define ASHBOX_CRAFT_BUCKET_IRON 18u
#define ASHBOX_CRAFT_PISTOL_IRON 100u
#define ASHBOX_CRAFT_AMMO_IRON 2u
#define ASHBOX_CRAFT_CROWBAR_IRON 10u
#define ASHBOX_INFECT_HEAL_S (36u * 3600u)
#define ASHBOX_STOVE_LINING_COST_PARTS 32u
#define ASHBOX_BOX_LAMP_COST_EMBER 6u
#define ASHBOX_ABX_TRADE_PARTS 24u
#define ASHBOX_PAD_STOVE_LINING 0x01u
#define ASHBOX_PAD_BOX_LAMP 0x02u
#define ASHBOX_WELL_DONE_FATIGUE 25
#define ASHBOX_WASTE_FOOD 1u
#define ASHBOX_WASTE_IRON 2u
#define ASHBOX_WASTE_WATER 4u
#define ASHBOX_WASTE_PARTS 8u
#define ASHBOX_CHORE_NONE 0
#define ASHBOX_CHORE_FAN 1
#define ASHBOX_CHORE_STOVE 2
#define ASHBOX_CHORE_BOX 3
#define ASHBOX_CHORE_GATE 4
#define ASHBOX_CHORE_KIND_MAX 4
#define ASHBOX_STASH_LV_MAX 3
#define ASHBOX_FURN_LV_MAX 2
#define ASHBOX_SEED_ROOT_CAP 20
#define ASHBOX_GEAR_LV_MAX 2
#define ASHBOX_REP_MAX 3
#define ASHBOX_EMBER_PITY_SHORT 12
#define ASHBOX_EMBER_PITY_BLOCK 10
#define ASHBOX_EMBER_PITY_HUNT 8
/* NIGHT/DEEP use a separate ember path and do not call soft pity. */
#define ASHBOX_DUTY_MAX 3
#define ASHBOX_DUTY_TUNE 0  /* 听广播 */
#define ASHBOX_DUTY_GATE 1  /* 查看闸门 */
#define ASHBOX_DUTY_TALLY 2 /* 清点仓库 */
#define ASHBOX_DUTY_STOVE 3 /* 看灶火 */
#define ASHBOX_DUTY_KIND_N 4

typedef enum {
    ASH_PERSON_HEALTHY = 0,
    ASH_PERSON_WEAK,
    ASH_PERSON_AWAY,
    ASH_PERSON_INJURED,
    ASH_PERSON_INFECTED,
} ash_person_state_t;

typedef enum {
    ASH_TIER_SHORT = 0,
    ASH_TIER_BLOCK,
    ASH_TIER_DEEP,
    ASH_TIER_HUNT,
    ASH_TIER_NIGHT, /* ~8h overnight route for sleepers */
    ASH_TIER_COUNT,
} ash_tier_t;

typedef enum {
    ASH_LOAD_NONE = 0,
    ASH_LOAD_FOOD,
    ASH_LOAD_PARTS,
    ASH_LOAD_COUNT,
} ash_loadout_t;

typedef enum {
    ASH_CRISIS_TALK = 0,
    ASH_CRISIS_FORTIFY,
    ASH_CRISIS_HIDE,
    ASH_CRISIS_COUNT,
} ash_crisis_opt_t;

typedef enum {
    ASH_ORDER_NONE = 0,
    ASH_ORDER_RATION,
    ASH_ORDER_WATCH,
    ASH_ORDER_RUSH,
    ASH_ORDER_COUNT,
} ash_night_order_t;

typedef enum {
    ASH_ITEM_SEED = 0,
    ASH_ITEM_JERKY,
    ASH_ITEM_TOKEN,
    ASH_ITEM_SHARD,
} ash_item_t;

typedef enum {
    ASH_TRADE_JERKY_FOR_SEED = 0,
    ASH_TRADE_JERKY_FOR_ROOT, /* 灰河渡口 */
    ASH_TRADE_PARTS_FOR_ROOT,
    ASH_TRADE_FOOD_FOR_WATER,
    ASH_TRADE_WATER_FOR_FOOD,
    ASH_TRADE_WATER_FOR_IRON,
    ASH_TRADE_IRON_FOR_WATER,
    ASH_TRADE_IRON_FOR_PARTS,
    ASH_TRADE_PARTS_FOR_IRON,
    ASH_TRADE_PARTS_FOR_EMBER,
    ASH_TRADE_PARTS_FOR_STIM, /* 件换肾上腺素（救急） */
    ASH_TRADE_PARTS_FOR_ABX,  /* 件换抗生素（热伤） */
    ASH_TRADE_COUNT,
} ash_trade_t;

typedef enum {
    ASH_DEATH_NONE = 0,
    ASH_DEATH_ASSAULT,
    ASH_DEATH_INJURY,
    ASH_DEATH_INFECTION,
} ash_death_kind_t;

typedef enum {
    ASH_CRAFT_BENCH = 0,
    ASH_CRAFT_BUCKET,
    ASH_CRAFT_PISTOL,
    ASH_CRAFT_AMMO,
    ASH_CRAFT_CROWBAR,
    ASH_CRAFT_COUNT,
} ash_craft_t;

typedef enum {
    ASH_WANDER_RATE_FAIR = 0,
    ASH_WANDER_RATE_CHEAP,
    ASH_WANDER_RATE_DEAR,
} ash_wander_rate_t;

typedef enum {
    ASH_WORLD_CAMP = 1u << 0,  /* 铁棚通联 */
    ASH_WORLD_FARM = 1u << 1,  /* 菜地可用（或食堂达标自动） */
    ASH_WORLD_CAMP2 = 1u << 2, /* 灰河渡口 */
} ash_world_flag_t;

typedef enum {
    ASH_CROP_NONE = 0,
    ASH_CROP_GRAIN = 1,
    ASH_CROP_ROOT = 2,
} ash_crop_t;

typedef struct {
    uint8_t id;
    ash_person_state_t state;
    uint32_t injure_remain_s;
    int8_t trust;
    uint8_t fatigue;
    uint8_t flags;
} ash_person_t;

typedef struct {
    bool active;
    uint32_t remaining_s;
    uint32_t duration_s;
    uint8_t person_id;
    ash_tier_t tier;
    ash_loadout_t loadout;
} ash_expedition_t;

typedef struct {
    bool pending;
    int16_t food_delta;
    int16_t parts_delta;
    int16_t ember_delta;
    char lines[ASHBOX_REPORT_LINES][32];
    uint8_t outcome_id;
    uint8_t branch_count;
    uint8_t branch_ids[ASHBOX_BRANCH_MAX];
    uint8_t branch_resolved;
    uint8_t success;
    uint8_t tag_id;
} ash_report_t;

typedef struct {
    bool pending;
    uint8_t reserved;
} ash_crisis_t;

typedef struct {
    uint8_t crop; /* ash_crop_t */
    uint32_t remain_s;
} ash_farm_plot_t;

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today; /* legacy v≤10 抗生素日闩低8；运行时用 day_latch_abx */
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[ASHBOX_QUEST_CORE];
    uint8_t quest_done[ASHBOX_QUEST_CORE];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    /* ---- v4 append (before crc) ---- */
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day; /* legacy v≤10 铁棚日闩低8；运行时用 day_latch_trade */
    uint8_t lore_shards; /* 0..4 mirrored from item shard for radio; kept for clarity */
    uint8_t quest2_step; /* quest id 2 「门后灰潮」— uses former reserved_v4 */
    uint8_t quest2_done;
    /* ---- v5 append (before crc) ---- */
    uint8_t stash_lv; /* 1..3 independent warehouse tier */
    uint8_t chore_done_exp; /* 本趟远征已完成站事次数 */
    uint8_t duty_day;   /* legacy v≤10 日勤日闩低8；运行时用 day_latch_duty */
    uint8_t duty_count; /* 当日已做日勤次数 0..ASHBOX_DUTY_MAX（原 reserved_v5[1]） */
    /* ---- v6 append (before crc) ---- */
    uint8_t furn_lv;     /* 0..2 仓家具：货架/干燥架 */
    uint8_t seed_root;   /* 薯种，与谷种分列有盖 */
    uint8_t trade2_day;  /* legacy v≤10 渡口日闩低8；运行时用 day_latch_trade2 */
    uint8_t chore_kind;  /* 待办站事；0=无（原 reserved_v6） */
    /* ---- v7 append Phase D (before crc) ---- */
    uint8_t ember_dry;       /* 非深处连续未掉烬次数（软保底） */
    uint8_t gear_lv;         /* 0..2 猎装 */
    uint8_t rep_shed;        /* 0..3 铁棚声望 */
    uint8_t rep_ferry;       /* 0..3 渡口声望 */
    uint8_t quiet_gear;      /* 0/1 工坊静音轮 */
    uint8_t lore_twist;      /* 0/1 灾因反转已启 */
    uint8_t trade_extra_day; /* legacy v≤10 铁棚二次；运行时用 day_latch_trade_extra */
    uint8_t trade2_extra_day;/* legacy v≤10 渡口二次；运行时用 day_latch_trade2_extra */
    /* ---- v8 append Phase G late sink (before crc) ---- */
    uint8_t door_bolt;       /* 0/1 外环门扣：静音轮后零件沉淀 */
    /* ---- v9 append water / well (before crc) ----
     * shovel/well_lv/pad occupy the former v8 crc alignment gap so that
     * offsetof(water) == offsetof(ash_game_v8_t, crc). */
    uint8_t shovel;                 /* 0/1 铁铲 */
    uint8_t well_lv;                /* 0无 1深坑 2水井 */
    uint8_t pad_v9;
    uint16_t water;
    uint16_t stash_water;
    uint16_t water_made_today;      /* 当日产水计数 */
    int16_t report_water_delta;     /* 待入账战报水量（不改 ash_report_t 中段布局） */
    uint32_t well_upgrade_remain_s; /* >0 深坑升井中 */
    uint32_t crew_exhaust_remain_s; /* >0 全体过劳禁工 */
    /* ---- v10 append iron / craft / wanderer (before crc) ----
     * offsetof(iron) == offsetof(ash_game_v9_t, crc). */
    uint16_t iron;
    uint8_t bucket;
    uint8_t backpack; /* unequipped count in stash */
    uint8_t crowbar;
    uint8_t pistol; /* unequipped count */
    uint8_t ammo;
    uint8_t craft_bench; /* 0/1 */
    uint8_t equip_backpack[ASHBOX_PERSON_CORE];
    uint8_t equip_pistol[ASHBOX_PERSON_CORE];
    uint8_t exp_crowbar;
    uint8_t exp_pistol;
    uint8_t exp_ammo;
    int16_t report_iron_delta;
    uint8_t report_waste_flags;
    uint8_t report_death_kind; /* ash_death_kind_t */
    uint8_t wanderer_pending;
    uint8_t wanderer_name_id;
    uint8_t wanderer_met_trade; /* 0 = first meet trade still offered */
    uint8_t wanderer_rate_kind; /* ash_wander_rate_t */
    uint8_t wanderer_ember_ok;  /* rare ember trade on this visit */
    uint8_t med_stim; /* 肾上腺素：缓解重伤 */
    uint32_t wanderer_next_s;
    uint8_t ember_trade_day; /* legacy v≤10 余烬日闩低8；运行时用 day_latch_ember */
    uint8_t med_milk; /* 旺仔牛奶：清疲劳，指定角色 */
    uint8_t med_abx;  /* 抗生素：治感染 */
    uint8_t pad_v10b; /* bit0 灶层 · bit1 匣灯（ASHBOX_PAD_*） */
    /* ---- v11 append full day latches (before crc) ----
     * offsetof(day_latch_trade) == offsetof(ash_game_v10_t, crc).
     * 0xFFFF = never; 0 is a valid day (do not clamp to never). */
    uint16_t day_latch_trade;
    uint16_t day_latch_trade2;
    uint16_t day_latch_trade_extra;
    uint16_t day_latch_trade2_extra;
    uint16_t day_latch_duty;
    uint16_t day_latch_ember;
    uint16_t day_latch_abx;
    uint16_t pad_v11;
    /* ---- v12 append meta/duty/clear/super-core (before crc) ----
     * offsetof(person_ext) == former offsetof(crc) on v11. */
    ash_person_t person_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    uint8_t equip_backpack_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    uint8_t equip_pistol_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    ash_farm_plot_t farm_ext[ASHBOX_FARM_PLOTS_MAX - ASHBOX_FARM_PLOTS];
    uint8_t person_job[ASHBOX_PERSON_MAX]; /* ASHBOX_JOB_* */
    uint8_t run_cleared;
    uint8_t peace_broken; /* 1 = carried/equipped pistol or ammo */
    uint8_t wipeout_reject_count;
    uint8_t game_over; /* forced wipeout fail */
    uint8_t build_cap_bonus; /* 0 or 2 after super core */
    uint8_t loot_bonus_perm_pct; /* 0 or 50 */
    uint8_t super_core; /* inventory 0/1 */
    uint8_t farm_plots_unlocked; /* 2 or 4 */
    uint8_t clear_toast_pending;
    uint8_t joined_after_clear;
    uint16_t achieve_toast_mask;
    uint16_t stash_food_bonus; /* pacifist grant */
    uint8_t end_toast_pending; /* forced fail toast */
    /* Named pad absorbs 4-byte alignment before crc on v12 (frozen). */
    uint8_t pad_v12[3];
    /* ---- v13 append (before crc) ----
     * offsetof(pace_era) == former offsetof(crc) on v12. */
    uint8_t pace_era; /* 0=legacy rules; 1=v13 farm/ember/wanderer */
    uint8_t wanderer_trait; /* pending visitor trait */
    uint8_t wanderer_fired; /* bit0/bit1 today slots fired */
    uint8_t pad_v13a;
    uint16_t quest_beat_day[ASHBOX_QUEST_N]; /* day_key of last advance; 0xFFFF never */
    uint16_t wanderer_sched_day; /* day_key schedule rolled for */
    uint16_t wanderer_t0_min; /* local minutes from midnight [480..1319] */
    uint16_t wanderer_t1_min;
    uint8_t person_trait[ASHBOX_PERSON_MAX]; /* work trait; core trio stay 0 */
    uint8_t farm_labor_h[ASHBOX_FARM_PLOTS_MAX]; /* hours toward crop bonus */
    /* 6 bytes: seal + keep sizeof multiple of 8 (int64 inside struct). */
    uint8_t pad_v13[6];
    uint32_t crc;
} ash_game_t;

static inline ash_person_t *ash_game_person_mut(ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->person[i];
    return &g->person_ext[i - ASHBOX_PERSON_CORE];
}

static inline const ash_person_t *ash_game_person_at(const ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->person[i];
    return &g->person_ext[i - ASHBOX_PERSON_CORE];
}

static inline uint8_t *ash_game_equip_bp_mut(ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->equip_backpack[i];
    return &g->equip_backpack_ext[i - ASHBOX_PERSON_CORE];
}

static inline const uint8_t *ash_game_equip_bp_at(const ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->equip_backpack[i];
    return &g->equip_backpack_ext[i - ASHBOX_PERSON_CORE];
}

static inline uint8_t *ash_game_equip_pis_mut(ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->equip_pistol[i];
    return &g->equip_pistol_ext[i - ASHBOX_PERSON_CORE];
}

static inline const uint8_t *ash_game_equip_pis_at(const ash_game_t *g, unsigned i)
{
    if (!g) return NULL;
    if (i >= ASHBOX_PERSON_MAX) i = 0;
    if (i < ASHBOX_PERSON_CORE) return &g->equip_pistol[i];
    return &g->equip_pistol_ext[i - ASHBOX_PERSON_CORE];
}

static inline ash_farm_plot_t *ash_game_farm_mut(ash_game_t *g, unsigned plot)
{
    if (!g) return NULL;
    if (plot >= ASHBOX_FARM_PLOTS_MAX) plot = 0;
    if (plot < ASHBOX_FARM_PLOTS) return &g->farm[plot];
    return &g->farm_ext[plot - ASHBOX_FARM_PLOTS];
}

static inline const ash_farm_plot_t *ash_game_farm_at(const ash_game_t *g, unsigned plot)
{
    if (!g) return NULL;
    if (plot >= ASHBOX_FARM_PLOTS_MAX) plot = 0;
    if (plot < ASHBOX_FARM_PLOTS) return &g->farm[plot];
    return &g->farm_ext[plot - ASHBOX_FARM_PLOTS];
}

typedef enum {
    ASH_ACT_DEPART = 0,
    ASH_ACT_DUTY,
    ASH_ACT_CHORE,
    ASH_ACT_BUILD,
    ASH_ACT_FARM,
    ASH_ACT_TRADE,
    ASH_ACT_WELL_UPGRADE,
    ASH_ACT_COUNT,
} ash_act_t;

typedef struct {
    bool trusted;
    bool in_deep_sleep_segment;
    int64_t dt_s;
} ash_clock_info_t;

void ash_game_new(ash_game_t *g);
void ash_game_after_migrate(ash_game_t *g); /* clamp + defaults for older saves */
/** Expand legacy uint8 day latches into v11 uint16 fields (migrators). */
void ash_game_widen_day_latches(ash_game_t *g);
void ash_game_settle(ash_game_t *g, const ash_clock_info_t *clk);
/** True when `unix_now` may safely replace the game wall anchor (not a power-loss RTC rewind). */
bool ash_game_wall_unix_usable(const ash_game_t *g, int64_t unix_now);
/* Anchor last_settle_unix / day_key to wall clock after a mono-dt settle.
 * Ignores snaps that rewind past the saved timeline (ESP32 RTC resets on power cut). */
void ash_game_note_wall_time(ash_game_t *g, int64_t unix_now);
bool ash_game_can_depart(const ash_game_t *g);
bool ash_game_person_can_depart(const ash_game_t *g, uint8_t person_id);
bool ash_game_tier_unlocked(const ash_game_t *g, ash_tier_t tier);
bool ash_game_start_expedition(ash_game_t *g, ash_tier_t tier, uint8_t person_id,
                               ash_loadout_t loadout);
/* New depart path: food = hours*3; optional crowbar/pistol/ammo carried. */
bool ash_game_start_expedition_ex(ash_game_t *g, ash_tier_t tier, uint8_t person_id,
                                  uint8_t crowbar_n, uint8_t take_pistol, uint8_t ammo_n);
uint16_t ash_game_expedition_water_cost(ash_tier_t tier);
uint16_t ash_game_expedition_food_cost(ash_tier_t tier);
int ash_game_crowbar_carry_cap(const ash_game_t *g, uint8_t person_id);
bool ash_game_work_locked(const ash_game_t *g);
bool ash_game_block_reason(const ash_game_t *g, ash_act_t act, char *out, size_t out_n);
bool ash_game_build_pit(ash_game_t *g);
bool ash_game_start_well_upgrade(ash_game_t *g);
uint16_t ash_game_pit_cost(const ash_game_t *g);
uint16_t ash_game_well_upgrade_cost(const ash_game_t *g);
int ash_game_water_cap(const ash_game_t *g);
int ash_game_stash_slots_used(const ash_game_t *g);
int ash_game_stash_slots_cap(const ash_game_t *g);
uint16_t ash_game_total_food(const ash_game_t *g);
uint16_t ash_game_total_water(const ash_game_t *g);
uint16_t ash_game_total_parts(const ash_game_t *g);
bool ash_game_take_food(ash_game_t *g, uint16_t amount);
bool ash_game_take_water(ash_game_t *g, uint16_t amount);
bool ash_game_take_parts(ash_game_t *g, uint16_t amount);
bool ash_game_take_iron(ash_game_t *g, uint16_t amount);
/* Returns amount actually stored; sets waste flags on overflow. */
uint16_t ash_game_give_food(ash_game_t *g, uint16_t amount, bool *wasted);
uint16_t ash_game_give_water(ash_game_t *g, uint16_t amount, bool *wasted);
uint16_t ash_game_give_parts(ash_game_t *g, uint16_t amount, bool *wasted);
uint16_t ash_game_give_iron(ash_game_t *g, uint16_t amount, bool *wasted);
bool ash_game_try_pocket_refill_water(ash_game_t *g);
bool ash_game_stash_deposit_water(ash_game_t *g, uint16_t amount);
bool ash_game_stash_withdraw_water(ash_game_t *g, uint16_t amount);
bool ash_game_craft(ash_game_t *g, ash_craft_t kind);
bool ash_game_craft_can(const ash_game_t *g, ash_craft_t kind);
const char *ash_game_craft_label(ash_craft_t kind);
const char *ash_game_craft_hint(ash_craft_t kind);
bool ash_game_equip_backpack(ash_game_t *g, uint8_t person_id, bool on);
bool ash_game_equip_pistol(ash_game_t *g, uint8_t person_id, bool on);
int ash_game_pop_cap(const ash_game_t *g);
bool ash_game_wanderer_pending(const ash_game_t *g);
const char *ash_game_wanderer_name(const ash_game_t *g);
bool ash_game_wanderer_can_join(const ash_game_t *g);
const char *ash_game_wanderer_join_block_reason(const ash_game_t *g);
uint8_t ash_game_wanderer_trait(const ash_game_t *g);
const char *ash_game_work_trait_name(uint8_t trait);
const char *ash_game_work_trait_hint(uint8_t trait);
uint8_t ash_game_person_work_trait(const ash_game_t *g, uint8_t person_id);
bool ash_game_wanderer_join(ash_game_t *g);
bool ash_game_wanderer_dismiss(ash_game_t *g);
bool ash_game_wanderer_trade_available(const ash_game_t *g);
bool ash_game_wanderer_trade(ash_game_t *g, ash_trade_t kind);
/** Seconds until next wanderer visit (0 if pending/soon); UINT32_MAX if none. */
uint32_t ash_game_wanderer_next_wake_s(const ash_game_t *g);
bool ash_game_exile_person(ash_game_t *g, uint8_t person_id);
bool ash_game_exile_blocked_reason(const ash_game_t *g, uint8_t person_id, char *out,
                                   size_t out_n);
bool ash_game_quest_beat_available(const ash_game_t *g, uint8_t qid);
bool ash_game_upgrade(ash_game_t *g, int building);
bool ash_game_upgrade_stash(ash_game_t *g);
bool ash_game_upgrade_furn(ash_game_t *g);
bool ash_game_upgrade_gear(ash_game_t *g);
bool ash_game_buy_quiet_gear(ash_game_t *g);
bool ash_game_buy_door_bolt(ash_game_t *g);
bool ash_game_buy_stove_lining(ash_game_t *g);
bool ash_game_buy_box_lamp(ash_game_t *g);
bool ash_game_unlock_gate(ash_game_t *g);
bool ash_game_tide_offer_shards(ash_game_t *g); /* 灰潮步2：交2灰片等价推进 */
void ash_game_ack_report(ash_game_t *g);
bool ash_game_pick_branch(ash_game_t *g, uint8_t branch_slot);
bool ash_game_crisis_pending(const ash_game_t *g);
bool ash_game_crisis_option_ok(const ash_game_t *g, ash_crisis_opt_t opt);
uint16_t ash_game_crisis_fortify_cost(const ash_game_t *g);
bool ash_game_resolve_crisis(ash_game_t *g, ash_crisis_opt_t opt);
void ash_game_log(ash_game_t *g, const char *line);
int ash_game_station_healthy(const ash_game_t *g);
int ash_game_beds(const ash_game_t *g);
int ash_game_food_cap(const ash_game_t *g);
int ash_game_stash_cap(const ash_game_t *g);
uint16_t ash_game_upgrade_cost(const ash_game_t *g, int building);
uint16_t ash_game_stash_upgrade_cost(const ash_game_t *g);
uint16_t ash_game_furn_upgrade_cost(const ash_game_t *g);
uint16_t ash_game_gear_upgrade_cost_ember(const ash_game_t *g);
uint16_t ash_game_gear_upgrade_cost_parts(const ash_game_t *g);
uint16_t ash_game_quiet_gear_cost(const ash_game_t *g);
uint16_t ash_game_door_bolt_cost(const ash_game_t *g);
uint16_t ash_game_stove_lining_cost(const ash_game_t *g);
uint16_t ash_game_box_lamp_cost(const ash_game_t *g);
bool ash_game_has_stove_lining(const ash_game_t *g);
bool ash_game_has_box_lamp(const ash_game_t *g);
bool ash_game_stash_full(const ash_game_t *g);
const char *ash_game_gear_hint(const ash_game_t *g);
int ash_game_farm_ready_count(const ash_game_t *g);
bool ash_game_root_unlocked(const ash_game_t *g);
const char *ash_game_crop_name(ash_crop_t crop);
uint32_t ash_game_tier_duration(ash_tier_t tier);
const char *ash_game_tier_name(ash_tier_t tier);
const char *ash_game_tier_place(ash_tier_t tier);
const char *ash_game_tier_time_hint(ash_tier_t tier);
const char *ash_game_tier_loot_hint(ash_tier_t tier);
const char *ash_game_loadout_name(ash_loadout_t load);
const char *ash_game_loadout_hint(ash_loadout_t load);
const char *ash_game_person_name(uint8_t id);
const char *ash_game_person_trait(uint8_t id);
const char *ash_game_branch_label(uint8_t branch_id);
void ash_game_branch_effect(uint8_t branch_id, char *out, size_t out_n);
const char *ash_game_crisis_label(ash_crisis_opt_t opt);
const char *ash_game_crisis_hint(ash_crisis_opt_t opt);
const char *ash_game_quest_name(uint8_t qid);
const char *ash_game_quest_hint(const ash_game_t *g, uint8_t qid);
const char *ash_game_radio(const ash_game_t *g);
/* 主页副文/电台软钩：一眼下一步，不含危机/战报/远征途中 */
const char *ash_game_home_nudge(const ash_game_t *g);
/* 远征等待时的站内下一步（含待办站事） */
const char *ash_game_wait_nudge(const ash_game_t *g);
bool ash_game_chore_pending(const ash_game_t *g);
const char *ash_game_chore_label(const ash_game_t *g);
const char *ash_game_chore_hint(const ash_game_t *g);
bool ash_game_resolve_chore(ash_game_t *g);
/* 站内日勤：非远征时可选加成，每日最多 ASHBOX_DUTY_MAX 次（非门槛） */
unsigned ash_game_duty_left(const ash_game_t *g);
bool ash_game_duty_available(const ash_game_t *g);
const char *ash_game_duty_label(uint8_t kind);
const char *ash_game_duty_hint(uint8_t kind);
bool ash_game_do_duty(ash_game_t *g, uint8_t kind);
/* 口袋空且仓有粮时即时回补（settle 与主页刷新共用） */
bool ash_game_try_pocket_refill(ash_game_t *g);
int ash_game_scene(const ash_game_t *g, bool wake_guard, bool night_aftermath);
/** Fill out[] with scene ids currently valid to browse (stable id order). Returns count. */
int ash_game_scenes_available(const ash_game_t *g, bool wake_guard, bool night_aftermath,
                              int *out, int cap);
uint32_t ash_game_crc(const ash_game_t *g);
bool ash_game_set_night_order(ash_game_t *g, ash_night_order_t order);
const char *ash_game_order_name(ash_night_order_t order);
const char *ash_game_order_hint(ash_night_order_t order);
/* Short effect line for wake-after-sleep journal / radio. */
const char *ash_game_order_result_line(ash_night_order_t order);
/* Threat rise feel for HUD: "" / "慢" / "快" relative to baseline floor. */
const char *ash_game_threat_pace_word(const ash_game_t *g);
const char *ash_game_tag_label(uint8_t tag_id);
const char *ash_game_journal_line(const ash_game_t *g, unsigned age);

bool ash_game_camp_open(const ash_game_t *g);
bool ash_game_camp2_open(const ash_game_t *g);
bool ash_game_farm_unlocked(const ash_game_t *g);
const char *ash_game_item_name(ash_item_t id);
uint8_t ash_game_item_count(const ash_game_t *g, ash_item_t id);
bool ash_game_stash_deposit(ash_game_t *g, bool food_not_parts, uint16_t amount);
bool ash_game_stash_withdraw(ash_game_t *g, bool food_not_parts, uint16_t amount);
bool ash_game_farm_plant(ash_game_t *g, uint8_t plot, ash_crop_t crop);
bool ash_game_farm_harvest(ash_game_t *g, uint8_t plot);
const char *ash_game_farm_status(const ash_game_t *g, uint8_t plot, char *buf, size_t n);
bool ash_game_trade_available(const ash_game_t *g);
bool ash_game_trade2_available(const ash_game_t *g);
bool ash_game_trade_can_pay(const ash_game_t *g, ash_trade_t kind);
bool ash_game_trade(ash_game_t *g, ash_trade_t kind);
bool ash_game_use_jerky(ash_game_t *g); /* 1干肉 → +4口粮（有盖/溢仓），仓库页可换 */
bool ash_game_use_milk(ash_game_t *g, uint8_t person_id);
bool ash_game_use_stim(ash_game_t *g, uint8_t person_id);
bool ash_game_use_abx(ash_game_t *g, uint8_t person_id);
bool ash_game_set_person_job(ash_game_t *g, uint8_t person_id, uint8_t job);
uint8_t ash_game_person_job(const ash_game_t *g, uint8_t person_id);
const char *ash_game_job_name(uint8_t job);
int ash_game_build_lv_max(const ash_game_t *g);
int ash_game_farm_plot_count(const ash_game_t *g);
bool ash_game_try_clear(ash_game_t *g); /* sets run_cleared when conditions met */
bool ash_game_use_super_core(ash_game_t *g);
bool ash_game_can_use_super_core(const ash_game_t *g);
bool ash_game_is_game_over(const ash_game_t *g);
const char *ash_game_person_state_label(ash_person_state_t st);
const char *ash_game_trade_label(ash_trade_t kind);
const char *ash_game_trade_hint(ash_trade_t kind);
void ash_game_trade_footer(const ash_game_t *g, ash_trade_t kind, char *out, size_t out_n);
