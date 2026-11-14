/* Host-only dual-role pacing probe for Phase D planning. Not part of firmware. */
#include "ashbox_content.h"
#include "ashbox_game.h"

#include <assert.h>
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

static int g_leg_continues;
static int g_leg_withdraws;

static int plan_count(const ash_game_t *g)
{
    int n = 0;
    for (int i = 0; i < ASH_PLAN_COUNT; i++) n += ash_game_plan_done(g, (uint8_t)i) ? 1 : 0;
    return n;
}

static void spend_plans(ash_game_t *g)
{
    static const uint8_t order[] = {
        ASH_PLAN_ROOF, ASH_PLAN_STOVE, ASH_PLAN_RADIO,
        ASH_PLAN_CELLAR, ASH_PLAN_WATCH, ASH_PLAN_TANK,
    };
    for (size_t i = 0; i < sizeof(order); i++) {
        uint8_t p = order[i];
        if (ash_game_plan_done(g, p)) continue;
        if (g->parts == 0 || g->iron < ash_game_plan_cost_iron(p)) continue;
        for (int guard = 0; guard < 20 && !ash_game_plan_done(g, p); guard++) {
            if (!ash_game_plan_contribute(g, p)) break;
        }
    }
}

static void check_invariants(const ash_game_t *g, const char *where)
{
    assert(g->food <= ash_game_food_cap(g));
    assert(g->water <= ash_game_water_cap(g));
    assert(g->parts <= ASHBOX_PARTS_CAP);
    assert(g->stash_food <= ash_game_stash_cap(g));
    assert(g->pop <= ASHBOX_PERSON_MAX);
    assert(g->threat <= 100);
    assert(g->exp_leg <= g->exp_leg_total);
    assert(!g->exp_leg_waiting || g->exp.active);
    for (int p = 0; p < ASH_PLAN_COUNT; p++) {
        assert(ash_game_plan_parts(g, (uint8_t)p) <= ash_game_plan_cost_parts((uint8_t)p));
    }
    (void)where;
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

static void run_plan_scenario(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.parts = 500;
    g.iron = 100;
    int beds0 = ash_game_beds(&g);
    int water0 = ash_game_water_cap(&g);
    int stash0 = ash_game_stash_cap(&g);
    spend_plans(&g);
    assert(plan_count(&g) == ASH_PLAN_COUNT);
    for (int p = 0; p < ASH_PLAN_COUNT; p++) assert(ash_game_plan_done(&g, (uint8_t)p));
    assert(ash_game_beds(&g) == beds0 + 1);
    assert(ash_game_water_cap(&g) == water0 + 20);
    assert(ash_game_stash_cap(&g) == stash0 + 60);
    check_invariants(&g, "plans");
    printf("PLAN PASS: all six plans, beds/water/stash effects\n");
}

static void run_leg_scenario(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.food = 40;
    g.water = 40;
    assert(ash_game_start_expedition(&g, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    assert(g.exp_leg_total == 2);
    g.exp.remaining_s = g.exp.duration_s / 2u;
    settle_s(&g, 1);
    assert(ash_game_leg_waiting(&g));
    assert(ash_game_leg_continue(&g));
    g_leg_continues++;
    assert(g.exp_leg_extra_fail == 8);
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(!g.exp.active);
    assert(g.report.pending);
    check_invariants(&g, "legs");

    ash_game_t w;
    ash_game_new(&w);
    w.food = 40;
    w.water = 40;
    assert(ash_game_start_expedition(&w, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    w.exp.remaining_s = w.exp.duration_s / 2u;
    settle_s(&w, 1);
    assert(ash_game_leg_waiting(&w));
    assert(ash_game_leg_withdraw(&w));
    g_leg_withdraws++;
    assert(!w.exp.active);
    assert(w.report.pending);
    assert(!w.report.success || w.report.food_delta >= 0);
    check_invariants(&w, "withdraw");
    printf("  withdraw: %s | %s\n", w.report.lines[0], w.report.lines[1]);
    printf("LEG PASS: pause/continue/extra-fail/withdraw report\n");
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
        check_invariants(&g, "step");

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
                if (ash_game_leg_waiting(&g)) {
                    assert(ash_game_leg_continue(&g));
                    g_leg_continues++;
                }
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
    check_invariants(&g, "end");
    milestone("end", &g, t / 3600);
    printf("  plans=%d legs+%d withdraw=%d\n", plan_count(&g), g_leg_continues,
           g_leg_withdraws);
    printf("  nudge: %s\n", ash_game_home_nudge(&g));
    printf("  radio: %s\n", ash_game_radio(&g));
}

int main(void)
{
    role_t a = {.name = "匣主·调查优先", .prefer_deep = 1, .prefer_hunt = 0, .rush_kitchen = 1};
    role_t b = {.name = "猎径·吞吐优先", .prefer_deep = 0, .prefer_hunt = 1, .rush_kitchen = 1};
    run_plan_scenario();
    run_leg_scenario();
    run_role(&a);
    run_role(&b);
    printf("\nSIM PASS: 96h x 2 roles, invariants and leg boundaries held\n");
    return 0;
}
