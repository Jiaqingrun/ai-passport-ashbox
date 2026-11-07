#include "ashbox_game_loot.h"

#include <stddef.h>

static uint8_t sat_add_u8(uint8_t a, int delta, uint8_t cap)
{
    int v = (int)a + delta;
    if (v < 0) return 0;
    if (v > (int)cap) return cap;
    return (uint8_t)v;
}

static void apply_loot_bias(const ash_game_t *g, uint8_t pid, ash_tier_t tier, ash_loadout_t load,
                            int *food, int *parts, int *ember)
{
    (void)parts;
    (void)load; /* 携带档已停用 */
    if (pid == 0) *food = (*food * 12 + 5) / 10;
    if (g->gate_unlocked) {
        *food = (*food * 12 + 5) / 10;
        *ember = (*ember * 12 + 5) / 10;
        /* 旧节奏：开闸后深处/过夜至少1余烬；新节奏只看概率，不保底 */
        if (g->pace_era != ASHBOX_PACE_V13 && *ember < 1 &&
            (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT))
            *ember = 1;
    }
    if (g->deep_loot_bonus && tier == ASH_TIER_DEEP) {
        *food = (*food * 11 + 5) / 10;
    }
}

/* 远征基础零件：难拿。越远命中率越高、上限 n 越大；命中后数量为 1..n。 */
static int parts_loot_roll(const ash_game_t *g, uint8_t pid, ash_tier_t tier, ash_loadout_t load,
                           unsigned seed, bool backpack)
{
    unsigned p = 15u;
    unsigned n_max = 1u;
    if (tier == ASH_TIER_BLOCK) {
        p = 22u;
        n_max = 2u;
    } else if (tier == ASH_TIER_HUNT) {
        p = 10u;
        n_max = 1u;
    } else if (tier == ASH_TIER_DEEP) {
        p = 32u;
        n_max = 3u;
    } else if (tier == ASH_TIER_NIGHT) {
        p = 38u;
        n_max = 4u;
    } else if (tier == ASH_TIER_SHORT) {
        p = 15u;
        n_max = 1u;
    }
    (void)load;
    if (pid == 1) p += 8u;
    if (g->gate_unlocked) p += 5u;
    if (g->deep_loot_bonus && tier == ASH_TIER_DEEP) p += 5u;
    if (backpack) {
        p += 8u;
        n_max += 1u;
    }
    if (p > 55u) p = 55u; /* 故意封顶，保持稀缺 */
    if (n_max > 5u) n_max = 5u;
    if ((seed % 100u) >= p) return 0;
    return 1 + (int)((seed / 11u) % n_max);
}

static int iron_loot_for_tier(ash_tier_t tier, unsigned seed)
{
    int lo = 0, hi = 2;
    if (tier == ASH_TIER_BLOCK) {
        lo = 2;
        hi = 5;
    } else if (tier == ASH_TIER_HUNT) {
        lo = 1;
        hi = 3;
    } else if (tier == ASH_TIER_DEEP) {
        lo = 4;
        hi = 8;
    } else if (tier == ASH_TIER_NIGHT) {
        lo = 5;
        hi = 10;
    } else if (tier == ASH_TIER_SHORT) {
        lo = 0;
        hi = 2;
    }
    int span = hi - lo + 1;
    return lo + (int)((seed / 23u) % (unsigned)span);
}

typedef enum {
    CB_FOOD = 0,
    CB_WATER,
    CB_MILK,
    CB_IRON,
    CB_PARTS,
    CB_PACK,
    CB_STIM,
    CB_ABX,
    CB_KIND_N
} crowbar_kind_t;

/* 撬棍加成权重：粮/水常见，抗生素抬高以便热伤有出口 */
static const uint8_t k_crowbar_w[CB_KIND_N] = {18, 22, 12, 12, 10, 6, 8, 12};

static int crowbar_haul_count(unsigned seed)
{
    unsigned r = seed % 100u;
    if (r < 60u) return 1;
    if (r < 85u) return 2;
    if (r < 95u) return 3;
    if (r < 99u) return 4;
    return 5;
}

static crowbar_kind_t crowbar_pick_kind(unsigned seed, uint8_t used_mask)
{
    unsigned total = 0;
    for (int i = 0; i < CB_KIND_N; i++) {
        if (used_mask & (uint8_t)(1u << i)) continue;
        total += k_crowbar_w[i];
    }
    if (total == 0) return CB_FOOD;
    unsigned pick = (seed / 13u) % total;
    unsigned acc = 0;
    for (int i = 0; i < CB_KIND_N; i++) {
        if (used_mask & (uint8_t)(1u << i)) continue;
        acc += k_crowbar_w[i];
        if (pick < acc) return (crowbar_kind_t)i;
    }
    return CB_FOOD;
}

static int crowbar_kind_amount(crowbar_kind_t k, unsigned seed)
{
    switch (k) {
    case CB_FOOD:
        return 4 + (int)(seed % 5u); /* 4-8 */
    case CB_WATER:
        return 2 + (int)(seed % 3u); /* 2-4 */
    case CB_IRON:
        return 2 + (int)(seed % 4u); /* 2-5 */
    case CB_PARTS:
        return 1 + (int)(seed % 2u); /* 1-2 */
    case CB_MILK:
    case CB_PACK:
    case CB_STIM:
    case CB_ABX:
    default:
        return 1;
    }
}

/* 撬棍战利品并入战报资源；背包/药品当场入仓。返回收获条数。 */
static int apply_crowbar_haul(ash_game_t *g, unsigned seed, int *food, int *parts, int *iron,
                              int16_t *water_delta)
{
    int n = crowbar_haul_count(seed);
    if (n <= 0) {
        ash_game_log(g, "撬了空壳");
        return 0;
    }
    uint8_t used = 0;
    unsigned s = seed;
    for (int i = 0; i < n; i++) {
        crowbar_kind_t k = crowbar_pick_kind(s, used);
        used |= (uint8_t)(1u << (unsigned)k);
        int amt = crowbar_kind_amount(k, s / 7u);
        s = s * 1103515245u + 12345u;
        switch (k) {
        case CB_FOOD:
            *food += amt;
            break;
        case CB_WATER:
            *water_delta = (int16_t)(*water_delta + amt);
            break;
        case CB_IRON:
            *iron += amt;
            break;
        case CB_PARTS:
            *parts += amt;
            break;
        case CB_MILK:
            g->med_milk = sat_add_u8(g->med_milk, 1, ASHBOX_ITEM_CAP);
            ash_game_log(g, "撬到旺仔牛奶");
            break;
        case CB_PACK:
            if (g->backpack < 9) g->backpack++;
            ash_game_log(g, "撬到旧背包");
            break;
        case CB_STIM:
            g->med_stim = sat_add_u8(g->med_stim, 1, ASHBOX_ITEM_CAP);
            ash_game_log(g, "撬到肾上腺素");
            break;
        case CB_ABX:
            g->med_abx = sat_add_u8(g->med_abx, 1, ASHBOX_ITEM_CAP);
            ash_game_log(g, "撬到抗生素");
            break;
        default:
            break;
        }
    }
    return n;
}

/* Success-path loot through tool return (C-GAME1: keep resolve_return readable). */
void ash_game_apply_success_core_loot(ash_game_t *g, ash_report_t *r, uint8_t pid, ash_tier_t tier,
                                    ash_loadout_t load, uint8_t carried_crowbar,
                                    uint8_t carried_pistol, uint8_t carried_ammo)
{
    int food = 0, parts = 0, ember = 0;
    unsigned seed = g->day_key * 17u + g->expedition_total * 13u + (unsigned)load * 7u;
    bool has_pack = (pid < ASHBOX_PERSON_MAX && (*ash_game_equip_bp_mut(g, (unsigned)(pid))));
    if (tier == ASH_TIER_SHORT) {
        food = 2 + (int)(seed % 3u);
    } else if (tier == ASH_TIER_BLOCK) {
        food = 5 + (int)(seed % 4u);
    } else if (tier == ASH_TIER_HUNT) {
        food = 4 + (int)(seed % 4u); /* 4-7 */
    } else if (tier == ASH_TIER_NIGHT) {
        food = 16 + (int)(seed % 7u); /* 16-22 */
        if (g->pace_era != ASHBOX_PACE_V13) ember = 1;
    } else {
        food = 9 + (int)(seed % 6u);
        if (g->pace_era != ASHBOX_PACE_V13) ember = 1;
    }
    if (g->pace_era == ASHBOX_PACE_V13) {
        unsigned ember_p = 0u;
        if (tier == ASH_TIER_DEEP) ember_p = ASHBOX_EMBER_P_DEEP;
        else if (tier == ASH_TIER_HUNT) ember_p = ASHBOX_EMBER_P_HUNT;
        else if (tier == ASH_TIER_NIGHT) ember_p = ASHBOX_EMBER_P_NIGHT;
        if (ember_p && ((seed / 11u) % 100u) < ember_p) ember = 1;
    }
    apply_loot_bias(g, pid, tier, load, &food, &parts, &ember);
    parts = parts_loot_roll(g, pid, tier, load, seed ^ 0xA5u, has_pack);
    int iron = iron_loot_for_tier(tier, seed);
    if (has_pack) iron += 2; /* 背包：铁加法，不翻倍粮水 */
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
            wlo = 8;
            whi = 11; /* ≥ 过夜水成本 8 */
        } else if (tier == ASH_TIER_SHORT) {
            wlo = 1;
            whi = 2;
        }
        int span = whi - wlo + 1;
        int wfind = wlo + (int)((seed / 19u) % (unsigned)span);
        g->report_water_delta = (int16_t)wfind;
    }
    if (carried_crowbar > 0) {
        (void)apply_crowbar_haul(g, seed ^ (0xC30Bu * (unsigned)carried_crowbar), &food, &parts,
                                 &iron, &g->report_water_delta);
    }
    if (g->loot_bonus_perm_pct > 0) {
        unsigned pct = 100u + (unsigned)g->loot_bonus_perm_pct;
        food = (int)((unsigned)food * pct / 100u);
        parts = (int)((unsigned)parts * pct / 100u);
        iron = (int)((unsigned)iron * pct / 100u);
    }
    {
        uint8_t tr = (pid < ASHBOX_PERSON_MAX) ? g->person_trait[pid] : ASHBOX_TRAIT_NONE;
        if (tr == ASHBOX_TRAIT_ALL_DOUBLE) {
            food *= 2;
            parts *= 2;
            if (ember > 0) ember *= 2;
        }
    }
    r->food_delta = (int16_t)food;
    r->parts_delta = (int16_t)parts;
    r->ember_delta = (int16_t)ember;
    g->report_iron_delta = (int16_t)iron;
    g->crowbar = sat_add_u8(g->crowbar, carried_crowbar, 20);
    if (carried_pistol && !(pid < ASHBOX_PERSON_MAX && (*ash_game_equip_pis_mut(g, (unsigned)(pid))))) {
        g->pistol = (uint8_t)(g->pistol + 1);
    }
    g->ammo = (uint8_t)(g->ammo + carried_ammo);
    g->exp_crowbar = 0;
    g->exp_pistol = 0;
    g->exp_ammo = 0;
    if (carried_crowbar == 0 &&
        (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT || tier == ASH_TIER_BLOCK) &&
        ((seed / 29u) % 100u) < 4u) {
        if (g->backpack < 9) g->backpack++;
        ash_game_log(g, "捡到旧背包 · 可装备");
    }
}

