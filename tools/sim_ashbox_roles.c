/* Host-only dual-role pacing probe for Phase D planning. Not part of firmware. */
#include "ashbox_content.h"
#include "ashbox_game.h"

#include <stdio.h>
#include <string.h>

static void settle_s(ash_game_t *g, int64_t dt_s)
{
    ash_clock_info_t clk = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = dt_s};
    ash_game_settle(g, &clk);
    if (g->last_settle_unix > 0) {
        ash_game_note_wall_time(g, g->last_settle_unix + dt_s);
    } else {
        ash_game_note_wall_time(g, 1700000000 + dt_s);
    }
}

static void finish_report(ash_game_t *g, int prefer_quest)
{
    if (!g->report.pending) return;
    if (g->report.branch_count > 0 && !g->report.branch_resolved) {
        int pick = 0;
        if (prefer_quest) {
            for (int i = 0; i < g->report.branch_count; i++) {
                const ash_branch_def_t *b = ash_content_branch(g->report.branch_ids[i]);
                if (b->quest_id != 255) {
                    pick = i;
                    break;
                }
            }
        }
        (void)ash_game_pick_branch(g, (uint8_t)pick);
    }
    ash_game_ack_report(g);
}

static void spend_upgrades(ash_game_t *g, int rush_kitchen)
{
    for (int guard = 0; guard < 8; guard++) {
        bool did = false;
        if (rush_kitchen && g->kitchen_lv < 2 && ash_game_upgrade(g, 1)) did = true;
        else if (g->dorm_lv < 2 && ash_game_upgrade(g, 0)) did = true;
        else if (g->kitchen_lv < 3 && ash_game_upgrade(g, 1)) did = true;
        else if (g->workshop_lv < 2 && ash_game_upgrade(g, 2)) did = true;
        else if (g->stash_lv < 2 && ash_game_upgrade_stash(g)) did = true;
        else if (g->furn_lv < 1 && ash_game_upgrade_furn(g)) did = true;
        if (!did) break;
    }
}

static void farm_and_trade(ash_game_t *g)
{
    for (uint8_t p = 0; p < ASHBOX_FARM_PLOTS; p++) {
        if (g->farm[p].crop != 0 && g->farm[p].remain_s == 0) {
            (void)ash_game_farm_harvest(g, p);
        }
    }
    if (ash_game_trade_available(g) && ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_SEED)) {
        (void)ash_game_trade(g, ASH_TRADE_JERKY_FOR_SEED);
    } else if (ash_game_trade_available(g) && ash_game_trade_can_pay(g, ASH_TRADE_FOOD_FOR_WATER) &&
               g->food + g->stash_food < 20) {
        (void)ash_game_trade(g, ASH_TRADE_FOOD_FOR_WATER);
    }
    if (ash_game_trade2_available(g) && ash_game_trade_can_pay(g, ASH_TRADE_JERKY_FOR_ROOT)) {
        (void)ash_game_trade(g, ASH_TRADE_JERKY_FOR_ROOT);
    }
    for (uint8_t p = 0; p < ASHBOX_FARM_PLOTS; p++) {
        if (g->farm[p].crop != 0) continue;
        if (ash_game_root_unlocked(g) && g->seed_root > 0) {
            (void)ash_game_farm_plant(g, p, ASH_CROP_ROOT);
        } else if (g->item[ASH_ITEM_SEED] > 0) {
            (void)ash_game_farm_plant(g, p, ASH_CROP_GRAIN);
        }
    }
    if (g->food < 8 && g->item[ASH_ITEM_JERKY] > 0) (void)ash_game_use_jerky(g);
}

static void resolve_crisis_safe(ash_game_t *g)
{
    if (!ash_game_crisis_pending(g)) return;
    if (ash_game_crisis_option_ok(g, ASH_CRISIS_FORTIFY) &&
        g->parts >= ash_game_crisis_fortify_cost(g)) {
        (void)ash_game_resolve_crisis(g, ASH_CRISIS_FORTIFY);
    } else if (ash_game_crisis_option_ok(g, ASH_CRISIS_TALK)) {
        (void)ash_game_resolve_crisis(g, ASH_CRISIS_TALK);
    } else {
        (void)ash_game_resolve_crisis(g, ASH_CRISIS_HIDE);
    }
}

typedef struct {
    const char *name;
    int prefer_deep;   /* 匣主：优先深处/调查 */
    int prefer_hunt;   /* 猎手：开闸后偏猎场 */
    int rush_kitchen;  /* 先冲食堂开菜地 */
} role_t;

static void milestone(const char *tag, const ash_game_t *g, int64_t t_h)
{
    printf("  t=%3lldh %-16s food=%u+%u parts=%u ember=%u threat=%u "
           "gate=%u camp=%u camp2=%u farm=%u q2=%u/%u stash_lv=%u furn=%u "
           "seed=%u root=%u jerky=%u shard=%u pop=%u\n",
           (long long)t_h, tag, g->food, g->stash_food, g->parts, g->ember, g->threat,
           g->gate_unlocked, (g->world_flags & ASH_WORLD_CAMP) != 0,
           (g->world_flags & ASH_WORLD_CAMP2) != 0, ash_game_farm_unlocked(g), g->quest2_step,
           g->quest2_done, g->stash_lv, g->furn_lv, g->item[ASH_ITEM_SEED], g->seed_root,
           g->item[ASH_ITEM_JERKY], g->item[ASH_ITEM_SHARD], g->pop);
}

static void run_role(const role_t *role)
{
    ash_game_t g;
    ash_game_new(&g);
    ash_game_note_wall_time(&g, 1700000000);
    int64_t t = 0;
    int short_n = 0;
    printf("\n=== ROLE %s ===\n", role->name);
    milestone("start", &g, 0);

    for (int step = 0; step < 220 && t < 96 * 3600; step++) {
        finish_report(&g, 1);
        resolve_crisis_safe(&g);
        spend_upgrades(&g, role->rush_kitchen);
        farm_and_trade(&g);

        if (!g.gate_unlocked && g.ember >= 3) {
            (void)ash_game_unlock_gate(&g);
            milestone("gate_open", &g, t / 3600);
        }

        if (g.threat >= 50 && g.night_order == ASH_ORDER_NONE) {
            (void)ash_game_set_night_order(&g, ASH_ORDER_WATCH);
        }

        if (!g.exp.active && !g.report.pending && !ash_game_crisis_pending(&g)) {
            ash_tier_t tier = ASH_TIER_SHORT;
            ash_loadout_t load = ASH_LOAD_NONE;
            uint8_t who = 0;
            if (g.pop >= 2 && g.person[1].state == ASH_PERSON_HEALTHY &&
                g.person[1].fatigue < 60)
                who = 1;
            if (g.pop >= 3 && g.person[2].state == ASH_PERSON_HEALTHY &&
                g.person[2].fatigue < 50)
                who = 2;

            /* 深处不需开闸；闸门只锁猎场。匣主尽早下地铁换余烬/铁棚。 */
            if (role->prefer_deep && short_n >= 2 && g.food >= 14 &&
                ash_game_person_can_depart(&g, who)) {
                tier = ASH_TIER_DEEP;
                load = ASH_LOAD_PARTS;
            } else if (role->prefer_hunt && g.gate_unlocked &&
                       ash_game_tier_unlocked(&g, ASH_TIER_HUNT) &&
                       ash_game_person_can_depart(&g, who) && g.food >= 10) {
                tier = ASH_TIER_HUNT;
                load = ASH_LOAD_FOOD;
            } else if (short_n >= 2 && g.food >= 10) {
                tier = ASH_TIER_BLOCK;
                load = ASH_LOAD_NONE;
                /* 猎径在未开闸时用废街刷余烬（软保底） */
            } else {
                tier = ASH_TIER_SHORT;
                load = ASH_LOAD_NONE;
                short_n++;
            }

            if (!ash_game_start_expedition(&g, tier, who, load)) {
                settle_s(&g, 1800);
                t += 1800;
                continue;
            }
            /* do chores while away */
            uint32_t dur = g.exp.remaining_s;
            uint32_t chunk = 30;
            while (g.exp.active && g.exp.remaining_s > 0) {
                uint32_t step_s = chunk;
                if (step_s > g.exp.remaining_s) step_s = g.exp.remaining_s;
                settle_s(&g, (int64_t)step_s);
                t += step_s;
                if (ash_game_chore_pending(&g)) (void)ash_game_resolve_chore(&g);
            }
            if (g.report.pending) {
                char tag[48];
                snprintf(tag, sizeof(tag), "ret_%s", ash_game_tier_name(tier));
                milestone(tag, &g, t / 3600);
            }
            (void)dur;
        } else {
            settle_s(&g, 900);
            t += 900;
        }

        if (g.quest2_done && ash_game_farm_unlocked(&g) && g.furn_lv >= 1) {
            milestone("mid_stable", &g, t / 3600);
            break;
        }
        if (!g.quest2_done && t >= 72 * 3600 && (step % 20) == 0) {
            milestone("72h_check", &g, t / 3600);
        }
    }
    milestone("end", &g, t / 3600);
    printf("  nudge: %s\n", ash_game_home_nudge(&g));
    printf("  radio: %s\n", ash_game_radio(&g));
}

int main(void)
{
    role_t a = {.name = "匣主·调查优先", .prefer_deep = 1, .prefer_hunt = 0, .rush_kitchen = 1};
    role_t b = {.name = "猎径·吞吐优先", .prefer_deep = 0, .prefer_hunt = 1, .rush_kitchen = 1};
    run_role(&a);
    run_role(&b);
    return 0;
}
