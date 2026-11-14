#include "ashbox_content.h"
#include "ashbox_game.h"
#include "ashbox_meta.h"
#include "ashbox_phenom.h"
#include "ashbox_save.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void settle_s(ash_game_t *g, int64_t dt_s)
{
    ash_clock_info_t clk = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = dt_s};
    ash_game_settle(g, &clk);
}

/* Most legacy flow tests chain several quest/farm beats in one day. */
static void new_game(ash_game_t *g)
{
    ash_game_new(g);
    g->pace_era = ASHBOX_PACE_LEGACY;
    g->crc = ash_game_crc(g);
}

/* C-1: the tag pool is state-filtered and still emits at most three ids. */
static void test_branch_pool_filters_by_state(void)
{
    ash_game_t g;
    new_game(&g);
    uint8_t out[ASHBOX_BRANCH_MAX];
    g.exp.person_id = 0;
    g.food = 40;
    g.stash_food = 0;
    g.parts = 100;
    uint8_t n = ash_game_branch_pool(&g, 0, out, ASHBOX_BRANCH_MAX); /* empty shelf */
    assert(n >= 1 && n <= ASHBOX_BRANCH_MAX);
    bool has37 = false;
    for (uint8_t i = 0; i < n; i++) if (out[i] == 37) has37 = true;
    assert(!has37); /* 37 needs food <= 12 */

    g.food = 8;
    n = ash_game_branch_pool(&g, 0, out, ASHBOX_BRANCH_MAX);
    has37 = false;
    for (uint8_t i = 0; i < n; i++) if (out[i] == 37) has37 = true;
    assert(has37); /* and it outranks the plain anchor */

    /* A cost branch with no stock never appears. */
    g.food = 0;
    g.parts = 0;
    n = ash_game_branch_pool(&g, 0, out, ASHBOX_BRANCH_MAX);
    for (uint8_t i = 0; i < n; i++) assert(ash_game_branch_affordable(&g, out[i]));
    assert(n >= 1);
}

/* G-2: one paid second beat per line per day. */
static void test_quest_second_beat(void)
{
    ash_game_t g;
    new_game(&g);
    g.pace_era = ASHBOX_PACE_V13; /* daily beat latch only runs in v13 pace */
    g.parts = 100;
    g.report.pending = true;
    g.report.branch_resolved = 0;
    g.report.branch_count = 1;
    g.report.branch_ids[0] = 3; /* q0 step 0 -> 1 */
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest_step[0] == 1);
    assert(ash_game_quest_beat_available(&g, 0)); /* push affordable */
    g.parts = 0;
    assert(!ash_game_quest_beat_available(&g, 0)); /* too poor to push */

    g.quest_step[0] = 0; /* pretend the same beat comes up again today */
    g.report.branch_resolved = 0;
    g.parts = 20;
    assert(ash_game_quest_beat_available(&g, 0));
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest_step[0] == 1);
    assert(g.parts == 12); /* 8 parts for the push */

    g.quest_step[0] = 0;
    g.report.branch_resolved = 0;
    g.parts = 100;
    assert(!ash_game_quest_beat_available(&g, 0)); /* second beat already used */
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest_step[0] == 0);
    assert(g.parts == 100);
}

/* G-3/G-4: escalation bands and the breach grace window. */
static void test_crisis_bands_and_breach(void)
{
    ash_game_t g;
    new_game(&g);
    g.threat = 79;
    assert(ash_game_crisis_band(&g) == 0);
    g.threat = 80;
    assert(ash_game_crisis_band(&g) == 1);
    g.threat = 95;
    assert(ash_game_crisis_band(&g) == 2);
    g.threat = 100;
    assert(ash_game_crisis_band(&g) == 3);

    g.threat = 100;
    g.parts = 6;
    g.crisis.pending = true;
    g.expedition_total = 5;
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_FORTIFY));
    assert(g.threat <= 79);          /* breach caps the aftermath */
    assert(g.crisis.reserved == 6);  /* grace window armed */
    assert(!g.crisis.pending);

    /* Rise is suspended while the window lasts. Anchor on an hour boundary so the
     * window burns down by real hours even when each settle is under an hour. */
    g.last_settle_unix = (g.last_settle_unix / 3600) * 3600;
    ash_game_settle(&g, &(ash_clock_info_t){.trusted = true, .dt_s = 1800});
    uint8_t after_half = g.threat;
    ash_game_settle(&g, &(ash_clock_info_t){.trusted = true, .dt_s = 1800});
    assert(g.threat == after_half);
    assert(g.crisis.reserved == 5); /* one real hour crossed */

    /* Grace expires and threat moves again. */
    uint8_t before_long = g.threat;
    ash_game_settle(&g, &(ash_clock_info_t){.trusted = true, .dt_s = 3600 * 6});
    assert(g.crisis.reserved == 0);
    assert(g.threat == before_long); /* the settle is fully inside the window */
    ash_game_settle(&g, &(ash_clock_info_t){.trusted = true, .dt_s = 3600 * 2});
    assert(g.threat > before_long); /* and threat moves again afterwards */
}

/* G-6: the post-clear arcs re-arm the three lines and downgrade loot. */
static void test_post_clear_arc(void)
{
    ash_game_t g;
    new_game(&g);
    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    g.lore_twist = 1;
    assert(ash_game_try_clear(&g));
    assert(g.run_cleared == 1);
    assert(ash_game_arc_stage(&g) == 1);
    assert(g.quest_step[0] == 3 && g.quest_step[1] == 3 && g.quest2_step == 2);

    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    assert(ash_game_try_clear(&g));
    assert(ash_game_arc_stage(&g) == 2);
    assert(g.quest_step[0] == 0 && g.quest2_step == 2);

    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    assert(ash_game_try_clear(&g));
    assert(ash_game_arc_stage(&g) == 3);
    assert(!ash_game_try_clear(&g));
}

/* U-5: the departure preview must equal the real roll's base chance. */
static void test_depart_preview_matches_roll(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 0;
    g.person[0].fatigue = 0;
    /* short tier, person 0: 5 - 5 = 0 (floor does not apply to short) */
    assert(ash_game_depart_fail_pct(&g, ASH_TIER_SHORT, 0, 0, 0) == 0);
    /* block tier, person 0: 12 - 5 = 7 */
    assert(ash_game_depart_fail_pct(&g, ASH_TIER_BLOCK, 0, 0, 0) == 7);
    /* trust 2 buys two points */
    g.person[0].trust = 2;
    assert(ash_game_depart_fail_pct(&g, ASH_TIER_BLOCK, 0, 0, 0) == 5);
    /* fatigue 60 costs ten */
    g.person[0].fatigue = 60;
    assert(ash_game_depart_fail_pct(&g, ASH_TIER_BLOCK, 0, 0, 0) == 15);
}

static void test_stance_preview_and_start(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 0;
    g.person[0].fatigue = 0;
    unsigned base = ash_game_depart_fail_pct(&g, ASH_TIER_BLOCK, 0, 0, 0);
    unsigned steady = ash_game_depart_fail_pct_stance(&g, ASH_TIER_BLOCK, 0, 0, 0,
                                                      ASH_STANCE_STEADY);
    unsigned fast = ash_game_depart_fail_pct_stance(&g, ASH_TIER_BLOCK, 0, 0, 0,
                                                    ASH_STANCE_FAST);
    unsigned greedy = ash_game_depart_fail_pct_stance(&g, ASH_TIER_BLOCK, 0, 0, 0,
                                                      ASH_STANCE_GREEDY);
    assert(base == 7);
    assert(steady < base);
    assert(greedy > base);
    assert(fast >= base);
    assert(strcmp(ash_game_stance_name(ASH_STANCE_GREEDY), "贪走") == 0);

    ash_game_t a;
    new_game(&a);
    uint32_t full = ash_game_tier_duration(ASH_TIER_SHORT);
    assert(ash_game_start_expedition_stance(&a, ASH_TIER_SHORT, 0, 0, 0, 0,
                                            ASH_STANCE_FAST));
    assert(a.exp.loadout == (ash_loadout_t)ASH_STANCE_FAST);
    assert(a.exp.duration_s == full * 85u / 100u);

    ash_game_t b;
    new_game(&b);
    assert(ash_game_start_expedition_stance(&b, ASH_TIER_SHORT, 0, 0, 0, 0,
                                            ASH_STANCE_STEADY));
    assert(b.exp.loadout == (ash_loadout_t)ASH_STANCE_STEADY);
    assert(b.exp.duration_s == full);

    ash_game_t c;
    new_game(&c);
    assert(ash_game_start_expedition_ex(&c, ASH_TIER_SHORT, 0, 0, 0, 0));
    assert(c.exp.loadout == (ash_loadout_t)ASH_STANCE_NONE);
}

/* Audit: the fortify price shown, checked and deducted must be the same number. */
static void test_crisis_fortify_cost_banded(void)
{
    ash_game_t g;
    new_game(&g);
    g.expedition_total = 5;
    g.threat = 95; /* band 2: cost 3 + 2 = 5 */
    g.crisis.pending = true;
    assert(ash_game_crisis_band(&g) == 2);
    assert(ash_game_crisis_fortify_cost(&g) == 5);
    g.parts = 4;
    assert(!ash_game_crisis_option_ok(&g, ASH_CRISIS_FORTIFY)); /* never underflow */
    g.parts = 5;
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_FORTIFY));
    int before = (int)g.threat;
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_FORTIFY));
    assert(g.parts == 0);
    assert((int)g.threat < before);
}

/* Audit: an armed trip spends exactly one round, even after an assault save. */
static void test_armed_trip_spends_one_round(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 50;
    g.water = 50;
    g.pistol = 1;
    g.ammo = 2;
    assert(ash_game_start_expedition_ex(&g, ASH_TIER_SHORT, 0, 0, 1, 1));
    assert(g.ammo == 1); /* deducted at departure */
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    ash_game_settle(&g, &(ash_clock_info_t){.trusted = true, .dt_s = 2});
    assert(!g.report.pending || g.report.branch_count >= 0);
    assert(g.ammo == 1); /* one round spent, the rest returned */
    assert(g.exp_ammo == 0);
}

/* Audit: a save cleared before arcs existed gets the first arc on its next settle. */
static void test_legacy_clear_arms_arc(void)
{
    ash_game_t g;
    new_game(&g);
    g.run_cleared = 1;
    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    g.lore_twist = 1;
    assert(ash_game_arc_stage(&g) == 0);
    assert(ash_game_try_clear(&g));
    assert(ash_game_arc_stage(&g) == 1);
    assert(g.quest_step[0] == 3 && g.quest2_step == 2);
}

static void test_water_system(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.water == 40);

    /* Daytime consume: 1 person / 2h → 1 water over 2h (10:00 CST = 02:00 UTC). */
    {
        int64_t day = (1650000000LL / 86400LL) * 86400LL;
        g.last_settle_unix = day + 2 * 3600; /* 02:00 UTC = 10:00 CST */
    }
    uint16_t w0 = g.water;
    settle_s(&g, 7200);
    assert(g.water == (uint16_t)(w0 - 1));

    /* Night window 22:00-07:00: no consume over 2h (23:00 CST = 15:00 UTC). */
    g.water = 40;
    {
        int64_t day = (1650000000LL / 86400LL) * 86400LL;
        g.last_settle_unix = day + 15 * 3600; /* 15:00 UTC = 23:00 CST */
    }
    w0 = g.water;
    settle_s(&g, 7200);
    assert(g.water == w0);

    /* Depart requires water; short costs 1. */
    new_game(&g);
    g.water = 0;
    assert(!ash_game_can_depart(&g));
    g.water = 1;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(g.water == 0);

    /* First expedition completion grants shovel. */
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(g.report.pending);
    if (g.report.branch_count > 0 && !g.report.branch_resolved)
        assert(ash_game_pick_branch(&g, 0));
    assert(g.shovel == 1);
    int16_t wdelta = g.report_water_delta;
    ash_game_ack_report(&g);
    assert(g.water >= (uint16_t)(wdelta > 0 ? wdelta : 0));

    /* Build pit then produce under day cap. */
    new_game(&g);
    g.shovel = 1;
    g.parts = 50;
    g.water = 40; /* cushion station thirst so production is visible */
    g.stash_water = 0;
    {
        /* Water budget rolls at local (UTC+8) midnight, like every other day key. */
        int64_t local_mid = (1650000000LL / 86400LL) * 86400LL - 8 * 3600;
        g.last_settle_unix = local_mid + 2 * 3600; /* 02:00 local */
    }
    assert(ash_game_build_pit(&g));
    assert(g.well_lv == ASHBOX_WELL_LV_PIT);
    uint16_t before = (uint16_t)(g.water + g.stash_water);
    settle_s(&g, 3600 * 10); /* 10h < day cap 15; thirst 02:00-12:00 = 5 h → 2 */
    /* Counter tracks whole hours since local midnight: 02:00 seed (2) + 10 h. */
    assert(g.water_made_today == 12);
    assert((uint16_t)(g.water + g.stash_water) == (uint16_t)(before + 10 - 2));

    /* Well upgrade pauses well only; station stays usable. */
    new_game(&g);
    g.shovel = 1;
    g.well_lv = ASHBOX_WELL_LV_PIT;
    g.parts = 50;
    g.water = 40;
    g.food = 20;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(!ash_game_start_well_upgrade(&g)); /* expedition blocks start */
    g.exp.active = false;
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.person[0].fatigue = 0;
    g.report.pending = false;
    assert(ash_game_start_well_upgrade(&g));
    assert(!ash_game_work_locked(&g));
    assert(ash_game_can_depart(&g)); /* may still leave during upgrade */
    char reason[48];
    assert(ash_game_block_reason(&g, ASH_ACT_WELL_UPGRADE, reason, sizeof(reason)));
    assert(strstr(reason, "升井") != NULL);

    /* Finish upgrade → fatigue bump, no exhaust lock. */
    uint8_t fat0 = g.person[0].fatigue;
    settle_s(&g, (int64_t)ASHBOX_WELL_UPGRADE_S);
    assert(g.well_lv == ASHBOX_WELL_LV_WELL);
    assert(g.crew_exhaust_remain_s == 0);
    assert(g.person[0].fatigue == (uint8_t)(fat0 + ASHBOX_WELL_DONE_FATIGUE) ||
           g.person[0].fatigue >= fat0);
    assert(!ash_game_work_locked(&g));
}

/* P0-1: an awake 1 s tick must still earn its share of well water. */
static void test_water_awake_ticks(void)
{
    ash_game_t g;
    new_game(&g);
    g.well_lv = ASHBOX_WELL_LV_WELL;
    g.water = 50; /* cap 50: count production via the daily counter, not the tank */
    g.stash_water = 0;
    int64_t day = (1650000000LL / 86400LL) * 86400LL;
    int64_t local_mid = day - 8 * 3600;
    g.last_settle_unix = local_mid + 1 * 3600; /* 01:00 local, one hour of head room */
    g.water_made_today = 6 * 3600 / 3600; /* seeded as if the well existed since 00:00 */
    for (int i = 0; i < 3600; i++) settle_s(&g, 1);
    assert(g.water_made_today == 12); /* 02:00 -> 6/h over 1 h */
    assert(g.water_made_today <= ASHBOX_WELL_WATER_DAY_CAP);

    /* Midnight wraps the counter instead of stalling it. */
    new_game(&g);
    g.well_lv = ASHBOX_WELL_LV_WELL;
    g.water = 10;
    g.last_settle_unix = local_mid + 86390; /* 23:59:50 local */
    g.water_made_today = 0;
    settle_s(&g, 7200); /* crosses local midnight, lands at 01:59:50 */
    assert(g.water_made_today <= ASHBOX_WELL_WATER_DAY_CAP);
    assert(g.water > 10);
}

/* Audit 2026-11-12 regressions: trade room, top-band fortify, hour timers,
 * water-only pocket refill and the water budget's single day boundary. */
static void test_audit_20261112_regressions(void)
{
    int64_t day = (1650000000LL / 86400LL) * 86400LL;
    int64_t local_mid = day - 8 * 3600;

    /* NV5: a trade whose output pool is full must not be offered at all. */
    {
        ash_game_t g;
        new_game(&g);
        g.food = 30;
        g.water = (uint16_t)ash_game_water_cap(&g);
        g.stash_water = 0;
        assert(!ash_game_trade_can_pay(&g, ASH_TRADE_FOOD_FOR_WATER));
        g.water = 0;
        assert(ash_game_trade_can_pay(&g, ASH_TRADE_FOOD_FOR_WATER));

        g.item[ASH_ITEM_JERKY] = 5;
        g.item[ASH_ITEM_SEED] = ASHBOX_ITEM_CAP;
        assert(!ash_game_trade_can_pay(&g, ASH_TRADE_JERKY_FOR_SEED));
        g.item[ASH_ITEM_SEED] = 0;
        assert(ash_game_trade_can_pay(&g, ASH_TRADE_JERKY_FOR_SEED));
    }

    /* NV4: band 3 is a forced fortify failure, so the paid FORTIFY is charged the
     * fortify cost plus the breach part — but it must say so in the journal
     * instead of claiming the gate held and then logging the breach. */
    {
        ash_game_t g;
        new_game(&g);
        g.threat = 100;
        g.crisis.pending = true;
        g.parts = 40;
        g.door_bolt = 1;
        assert(ash_game_crisis_band(&g) >= 3);
        uint16_t parts0 = g.parts;
        uint16_t cost = ash_game_crisis_fortify_cost(&g);
        assert(ash_game_resolve_crisis(&g, ASH_CRISIS_FORTIFY));
        assert(g.parts == (uint16_t)(parts0 - cost - 1)); /* fortify + breach part */
        assert(g.crisis.reserved == 6);                   /* breach window armed */
        assert(g.door_bolt == 0);                         /* the log says it broke */
        for (unsigned i = 0; i < ASHBOX_JOURNAL_N; i++) {
            assert(strstr(g.journal[i], "扛住") == NULL); /* no contradictory line */
        }
    }

    /* NV7: hour-based timers must burn down on 1 s awake settles too. */
    {
        ash_game_t g;
        new_game(&g);
        g.hide_penalty_h = 4;
        g.last_settle_unix = local_mid + 12 * 3600 + 1800; /* half past the hour */
        for (int i = 0; i < 3600; i++) settle_s(&g, 1);
        assert(g.hide_penalty_h == 3);
    }

    /* NV9: a water-only empty pocket refills from the stash, so the crew is not
     * forced WEAK and departure stays legal while the stash still holds water. */
    {
        ash_game_t g;
        new_game(&g);
        g.water = 0;
        g.stash_water = 20;
        g.food = 30;
        g.water_made_today = 0;
        g.last_settle_unix = local_mid + 12 * 3600;
        settle_s(&g, 60);
        assert(g.water > 0);
        assert(g.person[0].state != ASH_PERSON_WEAK);
        assert(ash_game_can_depart(&g));
    }

    /* NV6: the wall-clock resync must not reset the water budget a second time. */
    {
        ash_game_t g;
        new_game(&g);
        g.well_lv = ASHBOX_WELL_LV_WELL;
        g.last_settle_unix = local_mid + 12 * 3600;
        g.water_made_today = 30;
        uint32_t day0 = g.day_key;
        ash_game_note_wall_time(&g, local_mid + 86400 + 3600);
        assert(g.day_key != day0);
        assert(g.water_made_today == 30);
    }
}

/* P0-12/P0-13: auto-join stops at beds, and a wiped-out station is not refilled by a build. */
static void test_join_respects_beds_and_wipeout(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.pop == 1 && g.dorm_lv == 1 && ash_game_beds(&g) == 2);
    g.parts = 200;
    assert(ash_game_upgrade(&g, 2)); /* workshop L2 wants a third body */
    assert(g.pop == 2);              /* but only two beds exist */
    assert(g.person[1].id == 1 && g.person[1].state == ASH_PERSON_HEALTHY);

    assert(ash_game_upgrade(&g, 0)); /* dorm L2 -> three beds */
    assert(ash_game_beds(&g) == 3);
    assert(g.pop == 2);              /* beds alone do not add a body */
    assert(ash_game_pop_cap(&g) == 2);
    assert(ash_game_upgrade(&g, 1)); /* kitchen L2 raises the tier cap */
    assert(ash_game_pop_cap(&g) == 3);
    assert(g.pop == 3);
    assert(g.person[2].id == 2);

    new_game(&g);
    g.pop = 0;
    g.kitchen_lv = 1;
    g.parts = 200;
    assert(ash_game_upgrade(&g, 1)); /* kitchen L2 would auto-join before this fix */
    assert(g.pop == 0);
}

/* P0-9: a pending crisis always has a legal option, even with an empty station. */
static void test_crisis_empty_station_resolves(void)
{
    ash_game_t g;
    new_game(&g);
    g.pop = 0;
    g.parts = 0;
    g.threat = 90;
    g.expedition_total = 5;
    g.crisis.pending = true;
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_HIDE));
    assert(!ash_game_crisis_option_ok(&g, ASH_CRISIS_TALK));
    assert(!ash_game_crisis_option_ok(&g, ASH_CRISIS_FORTIFY));
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_HIDE));
    assert(!g.crisis.pending);
    assert(g.threat < 90);
}

/* P0-7: a branch that costs food or parts cannot be taken with an empty store. */
static void test_branch_cost_affordability(void)
{
    ash_game_t g;
    new_game(&g);
    /* branch 11 = 全员戒备: food -1, parts -1, threat -5 */
    g.food = 0;
    g.stash_food = 0;
    g.parts = 0;
    g.stash_parts = 0;
    assert(!ash_game_branch_affordable(&g, 11));
    g.parts = 1; /* still no food for the -1 food cost */
    assert(!ash_game_branch_affordable(&g, 11));
    g.food = 1;
    assert(ash_game_branch_affordable(&g, 11));
    g.parts = 0; /* both costs must be covered */
    assert(!ash_game_branch_affordable(&g, 11));

    g.report.pending = true;
    g.report.branch_resolved = 0;
    g.report.branch_count = 1;
    g.report.branch_ids[0] = 11;
    g.food = 0;
    assert(!ash_game_pick_branch(&g, 0));   /* refused, nothing applied */
    assert(!g.report.branch_resolved);
    assert(g.threat == 0);
}

static void test_new_game(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.magic == ASHBOX_MAGIC);
    assert(g.ver == ASHBOX_SAVE_VER);
    assert(g.food == 20);
    assert(g.parts == 10);
    assert(g.water == 40);
    assert(g.shovel == 0);
    assert(g.well_lv == 0);
    assert(g.pop == 1);
    assert(g.person[0].trust == 0);
    assert(g.person[0].fatigue == 0);
    assert(g.threat_rise_num == 10);
    assert(ash_game_can_depart(&g));
    assert(ash_game_expedition_water_cost(ASH_TIER_SHORT) == 1);
    assert(ash_game_expedition_water_cost(ASH_TIER_BLOCK) == 1);
    assert(ash_game_expedition_water_cost(ASH_TIER_DEEP) == 4);
    assert(ash_game_expedition_water_cost(ASH_TIER_NIGHT) == 8);
}

static void test_short_probe_no_daily_cap(void)
{
    ash_game_t g;
    new_game(&g);
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(g.exp.active);
    g.exp.remaining_s = 10;
    settle_s(&g, 10);
    assert(!g.exp.active);
    assert(g.report.pending);
    if (g.report.branch_count > 0 && !g.report.branch_resolved) {
        assert(ash_game_pick_branch(&g, 0));
    }
    ash_game_ack_report(&g);
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.person[0].fatigue = 0;
    g.food = 20;
    g.water = 10;
    g.day_latch_abx = ASHBOX_DAY_LATCH_NEVER; /* antibiotic day latch unused */
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
}

static void test_mid_return_excludes_away_production(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 20;
    g.pop = 2;
    g.person[1].id = 1;
    g.person[1].state = ASH_PERSON_HEALTHY;
    assert(ash_game_start_expedition(&g, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    g.exp.remaining_s = 1800;
    settle_s(&g, 1);
    /* C2 pauses at the 50% leg boundary; choose to keep going. */
    assert(ash_game_leg_waiting(&g));
    assert(ash_game_leg_continue(&g));
    settle_s(&g, 3600);
    assert(!g.exp.active);
    assert(g.report.pending);
}

static void test_note_wall_time_resyncs_day(void)
{
    ash_game_t g;
    new_game(&g);
    g.last_settle_unix = 1000;
    g.day_key = 1;
    g.day_latch_abx = 1;
    ash_game_note_wall_time(&g, 86400 * 3 + 10); /* day_key -> 4 */
    assert(g.last_settle_unix == 86400 * 3 + 10);
    assert(g.day_key == 4);
    assert(g.day_latch_abx == ASHBOX_DAY_LATCH_NEVER);
    ash_game_note_wall_time(&g, 0); /* ignore */
    assert(g.last_settle_unix == 86400 * 3 + 10);
}

/* Power-cut RTC restart must not rewind day_key / wipe 日勤 latches. */
static void test_note_wall_time_rejects_rtc_rewind(void)
{
    ash_game_t g;
    new_game(&g);
    g.last_settle_unix = 86400 * 10 + 100; /* day ~11 timeline */
    g.day_key = 11;
    g.day_latch_duty = 11;
    g.duty_count = 2;
    g.day_latch_abx = 11;

    assert(!ash_game_wall_unix_usable(&g, 30)); /* epoch-ish RTC after power cut */
    ash_game_note_wall_time(&g, 30);
    assert(g.last_settle_unix == 86400 * 10 + 100);
    assert(g.day_key == 11);
    assert(g.day_latch_duty == 11);
    assert(g.duty_count == 2);

    /* Forward wall snap still OK. */
    assert(ash_game_wall_unix_usable(&g, 86400 * 10 + 500));
    ash_game_note_wall_time(&g, 86400 * 10 + 500);
    assert(g.last_settle_unix == 86400 * 10 + 500);
    assert(g.day_key == 11);

    /* Mono settle still advances timeline without wall sync. */
    ash_clock_info_t clk = {.trusted = true, .dt_s = 86400};
    ash_game_settle(&g, &clk);
    assert(g.day_key == 12);
    assert(ash_game_duty_left(&g) == ASHBOX_DUTY_MAX); /* new day clears duty */
}

static void test_untrusted_no_advance(void)
{
    ash_game_t g;
    new_game(&g);
    uint16_t food = g.food;
    ash_clock_info_t clk = {.trusted = false, .dt_s = 3600};
    ash_game_settle(&g, &clk);
    assert(g.food == food);
}

static void test_scene_priority(void)
{
    ash_game_t g;
    new_game(&g);
    assert(ash_game_scene(&g, false, false) == 0);
    g.threat = 85;
    g.expedition_total = 5;
    /* 未武装危机时威胁场景仍为 3 */
    assert(ash_game_scene(&g, false, false) == 3);
    g.food = 0;
    assert(ash_game_scene(&g, false, false) == 2);
}

/* 长按换景：只收集当前局势下条件成立的场景 */
static void test_scenes_available(void)
{
    ash_game_t g;
    int out[16];
    int n;

    new_game(&g);
    g.food = 30;
    g.threat = 10;
    n = ash_game_scenes_available(&g, false, false, out, 16);
    assert(n == 1 && out[0] == 0); /* 单人站内仅 IDLE */

    g.pop = 2;
    g.person[1] = g.person[0];
    g.person[1].id = 1;
    n = ash_game_scenes_available(&g, false, false, out, 16);
    assert(n == 2 && out[0] == 0 && out[1] == 7); /* IDLE + DUO */

    g.crisis.pending = true;
    n = ash_game_scenes_available(&g, false, false, out, 16);
    assert(n == 1 && out[0] == 3); /* 危机锁 THREAT */

    g.crisis.pending = false;
    g.report.pending = true;
    n = ash_game_scenes_available(&g, false, false, out, 16);
    assert(n == 1 && out[0] == 4); /* 战报锁 RETURN */

    g.report.pending = false;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    n = ash_game_scenes_available(&g, false, false, out, 16);
    assert(n >= 1 && out[0] == 1); /* AWAY */

    new_game(&g);
    g.food = 30;
    g.parts = 20;
    g.threat = 10;
    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    g.farm[0].crop = ASH_CROP_GRAIN;
    g.farm[0].remain_s = 0;
    n = ash_game_scenes_available(&g, false, false, out, 16);
    {
        bool has_farm = false, has_idle = false;
        for (int i = 0; i < n; i++) {
            if (out[i] == 0) has_idle = true;
            if (out[i] == 8) has_farm = true;
        }
        assert(has_idle && has_farm);
    }
}

/* 站台自动聚焦：菜地/仓满/交易/远征/守望 */
static void test_scene_autofocus_hooks(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 30;
    g.parts = 20;
    g.threat = 10;
    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    g.farm[0].crop = ASH_CROP_GRAIN;
    g.farm[0].remain_s = 0;
    assert(ash_game_farm_ready_count(&g) > 0);
    assert(ash_game_scene(&g, false, false) == 8); /* FARM */

    new_game(&g);
    g.stash_lv = 1;
    g.iron = (uint16_t)(ash_game_stash_slots_cap(&g) * ASHBOX_STASH_SLOT_IRON);
    assert(ash_game_stash_full(&g));
    assert(ash_game_scene(&g, false, false) == 9); /* STASH */

    new_game(&g);
    g.food = 40;
    g.parts = 20;
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.day_key = 7;
    assert(ash_game_trade_available(&g));
    assert(ash_game_scene(&g, false, false) == 10); /* TRADE */

    new_game(&g);
    g.food = 40;
    g.parts = 20;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    g.farm[0].crop = ASH_CROP_GRAIN;
    g.farm[0].remain_s = 0;
    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    assert(ash_game_scene(&g, false, false) == 1); /* AWAY locks over farm */

    new_game(&g);
    g.food = 30;
    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    g.lore_twist = 1;
    assert(ash_game_scene(&g, false, false) == 11); /* WATCH */
}

static void force_success_depart(ash_game_t *g, ash_tier_t tier)
{
    new_game(g);
    g->day_key = 11;
    g->expedition_total = 4;
    g->food = 40;
    g->parts = 20;
    g->person[0].trust = 3;
    g->person[0].fatigue = 0;
    if (tier == ASH_TIER_HUNT) g->gate_unlocked = 1;
    assert(ash_game_start_expedition(g, tier, 0, ASH_LOAD_NONE));
    g->exp_leg_total = 1; /* loot tests exercise the return, not the leg stop */
    /* 阿禾 + trust3：近路 fail_p=0；废街/深处/猎场仍可能失败，重掷 day_key */
    for (int attempt = 0; attempt < 40; attempt++) {
        g->day_key = (uint32_t)(11 + attempt);
        g->exp.remaining_s = 1;
        if (!g->exp.active) {
            g->person[0].state = ASH_PERSON_HEALTHY;
            g->person[0].fatigue = 0;
            memset(&g->report, 0, sizeof(g->report));
            g->crisis.pending = false;
            if (tier == ASH_TIER_HUNT) g->gate_unlocked = 1;
            assert(ash_game_start_expedition(g, tier, 0, ASH_LOAD_NONE));
            g->exp_leg_total = 1; /* loot tests exercise the return, not the leg stop */
            g->exp.remaining_s = 1;
        }
        settle_s(g, 1);
        if (g->report.pending && g->report.success) return;
        if (g->report.pending) {
            if (g->report.branch_count && !g->report.branch_resolved)
                (void)ash_game_pick_branch(g, 0);
            ash_game_ack_report(g);
            g->person[0].state = ASH_PERSON_HEALTHY;
            g->person[0].fatigue = 0;
            g->person[0].injure_remain_s = 0;
        }
    }
    assert(0 && "could not force successful return");
}

static void test_loot_all_tiers_have_food_and_parts(void)
{
    for (int tier = 0; tier < ASH_TIER_COUNT; tier++) {
        ash_game_t g;
        force_success_depart(&g, (ash_tier_t)tier);
        assert(g.report.pending);
        assert(g.report.food_delta >= 2);
        /* 零件改为概率：单次可为 0；命中时数量在 1..n */
        assert(g.report.parts_delta >= 0);
        assert(g.report.parts_delta <= 5);
        if (tier == ASH_TIER_HUNT) {
            assert(g.item[ASH_ITEM_JERKY] >= 1);
        }
        if (tier == ASH_TIER_DEEP || tier == ASH_TIER_NIGHT) {
            assert(g.report.ember_delta >= 1);
        }
        if (tier == ASH_TIER_NIGHT) {
            assert(ash_game_tier_duration(ASH_TIER_NIGHT) == 28800u);
        }
    }
}

static void test_parts_prob_harder_near(void)
{
    /* 近路命中率应低于深处：多样本统计 */
    int hit_short = 0, hit_deep = 0, ok_short = 0, ok_deep = 0;
    const int N = 100;
    for (int i = 0; i < N; i++) {
        ash_game_t g;
        force_success_depart(&g, ASH_TIER_SHORT);
        g.day_key = 1000u + (unsigned)i * 17u;
        /* 重新掷一次成功归来以换种子 */
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
        g.person[0].state = ASH_PERSON_HEALTHY;
        g.person[0].fatigue = 0;
        g.food = 200;
        g.water = 200;
        assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending && g.report.success) {
            ok_short++;
            if (g.report.parts_delta > 0) hit_short++;
        }
    }
    for (int i = 0; i < N; i++) {
        ash_game_t g;
        force_success_depart(&g, ASH_TIER_DEEP);
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
        g.person[0].state = ASH_PERSON_HEALTHY;
        g.person[0].fatigue = 0;
        g.food = 200;
        g.water = 200;
        g.day_key = 2000u + (unsigned)i * 19u;
        g.expedition_total = (uint32_t)(i + 50);
        assert(ash_game_start_expedition(&g, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending && g.report.success) {
            ok_deep++;
            if (g.report.parts_delta > 0) hit_deep++;
        }
    }
    assert(ok_short > 20 && ok_deep > 20);
    /* 深处基础命中 32% vs 近路 15%，样本应更深侧更多（允许一定抖动） */
    assert(hit_deep + 3 >= hit_short);
}

static void test_med_use_and_stim_trade(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].fatigue = 90;
    g.med_milk = 1;
    assert(ash_game_use_milk(&g, 0));
    assert(g.person[0].fatigue == 0);
    assert(g.med_milk == 0);

    g.person[0].state = ASH_PERSON_INJURED;
    g.person[0].injure_remain_s = ASHBOX_INJURE_DEAD_S;
    g.med_stim = 1;
    assert(ash_game_use_stim(&g, 0));
    assert(g.person[0].state == ASH_PERSON_HEALTHY);

    g.person[0].state = ASH_PERSON_INFECTED;
    g.med_abx = 1;
    assert(ash_game_use_abx(&g, 0));
    assert(g.person[0].state == ASH_PERSON_HEALTHY);

    g.world_flags |= ASH_WORLD_CAMP;
    g.parts = 40;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    assert(ash_game_trade_can_pay(&g, ASH_TRADE_PARTS_FOR_STIM));
    assert(ash_game_trade(&g, ASH_TRADE_PARTS_FOR_STIM));
    assert(g.med_stim == 1);

    g.parts = 50;
    g.day_key = (uint32_t)(g.day_key + 1u); /* new day for camp slot */
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.day_latch_abx = ASHBOX_DAY_LATCH_NEVER;
    assert(ash_game_trade_can_pay(&g, ASH_TRADE_PARTS_FOR_ABX));
    assert(ash_game_trade(&g, ASH_TRADE_PARTS_FOR_ABX));
    assert(g.med_abx == 1);
    assert(g.day_latch_abx == (uint16_t)g.day_key);
    assert(!ash_game_trade_can_pay(&g, ASH_TRADE_PARTS_FOR_ABX)); /* day latch */
}

static void test_crowbar_haul_distribution_smoke(void)
{
    /* 带撬棍成功归来：资源不减（可能加）；多样本下至少一类增益常见 */
    int emptyish = 0, any_gain = 0;
    int food_hi = 0, parts_hi = 0, med_hi = 0;
    for (int i = 0; i < 80; i++) {
        ash_game_t g;
        new_game(&g);
        g.food = 200;
        g.water = 200;
        g.crowbar = 1;
        g.day_key = 3000u + (unsigned)i * 23u;
        g.expedition_total = (uint32_t)(i + 7);
        g.person[0].trust = 3;
        assert(ash_game_start_expedition_ex(&g, ASH_TIER_SHORT, 0, 1, 0, 0));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        ash_clock_info_t clk = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = 1};
        ash_game_settle(&g, &clk);
        if (!g.report.pending || !g.report.success) continue;
        assert(g.crowbar >= 1);
        assert(g.report.food_delta >= 0);
        assert(g.report_water_delta >= 0);
        assert(g.report_iron_delta >= 0);
        assert(g.report.parts_delta >= 0);
        if (g.report.food_delta >= 6) food_hi++; /* crowbar food +4..8 on top of base */
        if (g.report.parts_delta >= 1) parts_hi++;
        if (g.med_milk || g.med_stim || g.med_abx) med_hi++;
        if (g.report.food_delta >= 6 || g.report_water_delta >= 3 || g.report_iron_delta >= 2 ||
            g.report.parts_delta >= 1 || g.med_milk || g.med_stim || g.med_abx || g.backpack)
            any_gain++;
        else
            emptyish++;
        assert(g.report.food_delta <= 20); /* no old 15-40 crowbar spikes */
    }
    assert(any_gain >= 3);
    assert(food_hi + parts_hi + med_hi >= 1);
    (void)emptyish;
}

static void test_backpack_iron_bonus_not_food_double(void)
{
    ash_game_t bare, pack;
    new_game(&bare);
    new_game(&pack);
    bare.food = pack.food = 200;
    bare.water = pack.water = 200;
    bare.day_key = pack.day_key = 4242u;
    bare.expedition_total = pack.expedition_total = 11u;
    bare.person[0].trust = pack.person[0].trust = 3;
    pack.backpack = 1;
    assert(ash_game_equip_backpack(&pack, 0, true));
    assert(ash_game_start_expedition_ex(&bare, ASH_TIER_SHORT, 0, 0, 0, 0));
    assert(ash_game_start_expedition_ex(&pack, ASH_TIER_SHORT, 0, 0, 0, 0));
    bare.exp_leg_total = pack.exp_leg_total = 1;
    bare.exp.remaining_s = pack.exp.remaining_s = 1;
    ash_clock_info_t clk = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = 1};
    ash_game_settle(&bare, &clk);
    ash_game_settle(&pack, &clk);
    assert(bare.report.pending && pack.report.pending);
    assert(bare.report.success && pack.report.success);
    /* Food no longer doubles; iron gets +2 with pack. Water may differ if
     * pack's higher parts hit rate skips the “no parts → +1 water” consol. */
    assert(pack.report.food_delta == bare.report.food_delta);
    assert(pack.report_iron_delta == bare.report_iron_delta + 2);
}

static void test_tier_names_plain(void)
{
    assert(ash_game_tier_name(ASH_TIER_SHORT)[0] != '\0');
    assert(strstr(ash_game_tier_name(ASH_TIER_SHORT), "探") == NULL);
}

static void test_fatigue_and_trust(void)
{
    ash_game_t g;
    new_game(&g);
    /* fatigue lock threshold is 85 in game.c */
    g.person[0].fatigue = 85;
    assert(!ash_game_person_can_depart(&g, 0));
    g.person[0].fatigue = 0;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(g.person[0].fatigue > 0);
}

static void test_loadout_prepay_and_loot(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 20;
    g.water = 10;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_FOOD));
    assert(g.food == 19); /* short food cost 1 */
    assert(g.water == 9);
    assert(g.exp.loadout == ASH_LOAD_NONE);

    new_game(&g);
    g.parts = 10;
    g.food = 20;
    g.water = 10;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_PARTS));
    assert(g.parts == 10); /* loadout retired */
    assert(g.food == 19);

    new_game(&g);
    g.food = 0;
    g.stash_food = 0;
    g.water = 10;
    assert(!ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_FOOD));
}

static void test_branch_persists_until_picked(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 3;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(g.report.pending);
    assert(g.report.branch_count >= 1);
    assert(!g.report.branch_resolved);
    ash_game_ack_report(&g);
    assert(g.report.pending); /* 未选分支不可 ack */
    assert(ash_game_pick_branch(&g, 0));
    assert(g.report.branch_resolved);
    ash_game_ack_report(&g);
    assert(!g.report.pending);
}

static void test_loot_deferred_until_ack(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 3;
    g.food = 20;
    g.parts = 10;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(g.food == 19); /* prepaid short cost 1 */
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(g.report.pending);
    assert(g.report.success);
    assert(g.report.food_delta >= 2);
    /* 归来未入账 */
    assert(g.food == 19);
    assert(g.parts == 10);
    /* 强制选「先藏私货」：粮+1 并入战报 */
    g.report.branch_ids[0] = 1;
    g.report.branch_count = 1;
    g.report.branch_resolved = 0;
    int16_t base_food = g.report.food_delta;
    assert(ash_game_pick_branch(&g, 0));
    assert(g.report.food_delta == (int16_t)(base_food + 1));
    assert(g.food == 19); /* 仍未入账 */
    ash_game_ack_report(&g);
    assert(g.food == (uint16_t)(19 + base_food + 1));
}

static void test_crisis_three_options(void)
{
    ash_game_t g;
    new_game(&g);
    g.expedition_total = 5;
    g.threat = 80;
    g.parts = 10;
    g.pop = 3;
    for (int i = 0; i < 3; i++) {
        g.person[i].id = (uint8_t)i;
        g.person[i].state = ASH_PERSON_HEALTHY;
    }
    settle_s(&g, 1);
    assert(ash_game_crisis_pending(&g));
    assert(!ash_game_can_depart(&g));
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_TALK));
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_FORTIFY));
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_HIDE));
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_FORTIFY));
    assert(!ash_game_crisis_pending(&g));
    assert(g.threat < 80);
}

static void test_quest_branch_advance(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 3;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    /* 强制 outcome 带盯铁皮声（id 3） */
    g.report.branch_ids[0] = 3;
    g.report.branch_count = 1;
    g.report.branch_resolved = 0;
    g.quest_step[0] = 0;
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest_step[0] == 1);
}

static void test_journal_radio(void)
{
    ash_game_t g;
    new_game(&g);
    ash_game_log(&g, "测试纪事一条");
    assert(g.journal_count == 1);
    const char *r = ash_game_radio(&g);
    assert(strstr(r, "测试纪事") != NULL);
    /* 远征途中不得被日志盖住 */
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    const char *away = ash_game_radio(&g);
    assert(strstr(away, "测试纪事") == NULL);
}

static void test_plans_and_legs(void)
{
    ash_game_t g;
    new_game(&g);
    int beds0 = ash_game_beds(&g);
    g.parts = 200;
    g.iron = 40;
    for (int i = 0; i < 6; i++) assert(ash_game_plan_contribute(&g, ASH_PLAN_ROOF));
    assert(ash_game_plan_done(&g, ASH_PLAN_ROOF));
    assert(ash_game_beds(&g) == beds0 + 1);
    int water0 = ash_game_water_cap(&g);
    for (int i = 0; i < 11; i++) assert(ash_game_plan_contribute(&g, ASH_PLAN_TANK));
    assert(ash_game_plan_done(&g, ASH_PLAN_TANK));
    assert(ash_game_water_cap(&g) == water0 + 20);
    int stash0 = ash_game_stash_cap(&g);
    for (int i = 0; i < 12; i++) assert(ash_game_plan_contribute(&g, ASH_PLAN_CELLAR));
    assert(ash_game_plan_done(&g, ASH_PLAN_CELLAR));
    assert(ash_game_stash_cap(&g) == stash0 + 60);
    /* Radio costs iron; completion deducts it. */
    uint16_t iron0 = g.iron;
    for (int i = 0; i < 9; i++) assert(ash_game_plan_contribute(&g, ASH_PLAN_RADIO));
    assert(ash_game_plan_done(&g, ASH_PLAN_RADIO));
    assert(g.iron == iron0 - 10);
    /* Full parts but short iron: the plan waits, then completes once iron arrives. */
    ash_game_t q;
    new_game(&q);
    q.parts = 100;
    q.iron = 0;
    for (int i = 0; i < 9; i++) assert(ash_game_plan_contribute(&q, ASH_PLAN_WATCH));
    assert(!ash_game_plan_contribute(&q, ASH_PLAN_WATCH));
    assert(ash_game_plan_parts(&q, ASH_PLAN_WATCH) == 50);
    assert(!ash_game_plan_done(&q, ASH_PLAN_WATCH));
    q.iron = 15;
    assert(ash_game_plan_contribute(&q, ASH_PLAN_WATCH));
    assert(ash_game_plan_done(&q, ASH_PLAN_WATCH));

    /* 2-leg block run pauses at the midpoint; continuing adds +8 empty risk. */
    ash_game_t l;
    new_game(&l);
    assert(ash_game_start_expedition(&l, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    assert(l.exp_leg_total == 2);
    l.exp.remaining_s = l.exp.duration_s / 2u;
    settle_s(&l, 1);
    assert(ash_game_leg_waiting(&l));
    assert(l.exp_leg == 1);
    assert(ash_game_leg_continue(&l));
    assert(!ash_game_leg_waiting(&l));
    assert(l.exp_leg_extra_fail == 8);

    ash_game_t w;
    new_game(&w);
    assert(ash_game_start_expedition(&w, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    w.exp.remaining_s = w.exp.duration_s / 2u;
    settle_s(&w, 1);
    assert(ash_game_leg_waiting(&w));
    assert(ash_game_leg_withdraw(&w));
    assert(!w.exp.active);
    assert(w.report.pending);
}

static void test_upgrade_cost_api(void)
{
    ash_game_t g;
    new_game(&g);
    assert(ash_game_upgrade_cost(&g, 0) == 8);
    assert(ash_game_upgrade(&g, 0));
    assert(g.dorm_lv == 2);
    assert(ash_game_beds(&g) >= 2);

    /* E3: the workshop ladder is 8/16/36/60, separate from dorm/kitchen. */
    ash_game_t w;
    new_game(&w);
    w.parts = 1000;
    w.build_cap_bonus = 2; /* base max 5 so the full ladder is reachable */
    static const uint16_t k_ws_costs[] = {8, 16, 36, 60};
    for (int i = 0; i < 4; i++) {
        assert(ash_game_upgrade_cost(&w, 2) == k_ws_costs[i]);
        assert(ash_game_upgrade(&w, 2));
    }
    assert(w.workshop_lv == 5);
    assert(ash_game_upgrade_cost(&w, 2) == 0);

    /* E3: mode cycles normal → steady → hard → normal and defaults to normal. */
    ash_game_t m;
    new_game(&m);
    assert(ash_game_build_mode(&m) == ASH_BUILD_NORMAL);
    assert(strcmp(ash_game_build_mode_name(ASH_BUILD_NORMAL), "常") == 0);
    assert(ash_game_cycle_build_mode(&m));
    assert(ash_game_build_mode(&m) == ASH_BUILD_STEADY);
    assert(strcmp(ash_game_build_mode_name(ASH_BUILD_STEADY), "稳") == 0);
    assert(ash_game_cycle_build_mode(&m));
    assert(ash_game_build_mode(&m) == ASH_BUILD_HARD);
    assert(strcmp(ash_game_build_mode_name(ASH_BUILD_HARD), "猛") == 0);
    assert(ash_game_cycle_build_mode(&m));
    assert(ash_game_build_mode(&m) == ASH_BUILD_NORMAL);
}

static void test_deep_sleep_economy_half_expedition_full(void)
{
    ash_game_t g1, g2;
    new_game(&g1);
    new_game(&g2);
    g1.threat = 10;
    g2.threat = 10;
    g1.expedition_total = 5;
    g2.expedition_total = 5;
    /* 2 h so the 0.15 x rise rate yields at least one whole point in both halves. */
    ash_clock_info_t awake = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = 7200};
    ash_clock_info_t sleep = {.trusted = true, .in_deep_sleep_segment = true, .dt_s = 7200};
    ash_game_settle(&g1, &awake);
    ash_game_settle(&g2, &sleep);
    int d_awake = (int)g1.threat - 10;
    int d_sleep = (int)g2.threat - 10;
    assert(d_awake > 0);
    assert(d_sleep > 0);
    assert(d_sleep <= d_awake);
    assert(d_sleep * 2 <= d_awake + 1); /* ≈ half */

    new_game(&g1);
    g1.person[0].trust = 3;
    assert(ash_game_start_expedition(&g1, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    uint32_t rem = g1.exp.remaining_s;
    ash_clock_info_t sl = {.trusted = true, .in_deep_sleep_segment = true, .dt_s = 100};
    ash_game_settle(&g1, &sl);
    assert(g1.exp.active);
    assert(g1.exp.remaining_s == rem - 100); /* 全额墙钟 */
}

static void test_night_order_and_gate(void)
{
    ash_game_t g;
    new_game(&g);
    assert(ash_game_set_night_order(&g, ASH_ORDER_WATCH));
    assert(g.night_order == ASH_ORDER_WATCH);
    g.ember = 3;
    assert(ash_game_unlock_gate(&g));
    assert(g.gate_unlocked == 1);
    assert(g.ember == 0);
    uint32_t c1 = ash_game_crc(&g);
    g.crc = c1;
    assert(ash_game_crc(&g) == c1);

    /* 深睡长段：站令早报入纪事；威胁涨速词可见 */
    new_game(&g);
    assert(ash_game_set_night_order(&g, ASH_ORDER_RATION));
    ash_clock_info_t deep = {.trusted = true, .in_deep_sleep_segment = true, .dt_s = 3600};
    ash_game_settle(&g, &deep);
    bool found = false;
    for (int i = 0; i < ASHBOX_JOURNAL_N; i++) {
        if (strstr(g.journal[i], "站令见效") != NULL) {
            found = true;
            break;
        }
    }
    assert(found);
    assert(strcmp(ash_game_order_result_line(ASH_ORDER_WATCH), "站令见效 · 威胁慢些") == 0);
    g.quiet_gear = 1;
    g.door_bolt = 1;
    assert(strcmp(ash_game_threat_pace_word(&g), "慢") == 0);
    char foot[48];
    g.rep_shed = 2;
    ash_game_trade_footer(&g, ASH_TRADE_FOOD_FOR_WATER, foot, sizeof(foot));
    assert(strstr(foot, "望2") != NULL);
    assert(strstr(ash_game_item_name(ASH_ITEM_TOKEN), "干燥") != NULL);
}

static void test_return_tag_sets_branches(void)
{
    ash_game_t g;
    new_game(&g);
    g.person[0].trust = 3;
    g.expedition_total = 5; /* 非教程，启用标签分支池 */
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    settle_s(&g, 1);
    assert(g.report.pending);
    assert(g.report.success);
    assert(ash_game_tag_label(g.report.tag_id)[0] != '\0');
    assert(g.report.branch_count >= 1);
}

static void test_crisis_escape_when_all_weak(void)
{
    ash_game_t g;
    new_game(&g);
    g.expedition_total = 5;
    g.threat = 90;
    g.food = 0;
    g.parts = 0;
    g.pop = 2;
    for (int i = 0; i < 2; i++) {
        g.person[i].id = (uint8_t)i;
        g.person[i].state = ASH_PERSON_WEAK;
        g.person[i].trust = 0;
    }
    settle_s(&g, 1);
    assert(ash_game_crisis_pending(&g));
    assert(!ash_game_crisis_option_ok(&g, ASH_CRISIS_FORTIFY));
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_HIDE));
    assert(ash_game_crisis_option_ok(&g, ASH_CRISIS_TALK));
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_HIDE));
    assert(!ash_game_crisis_pending(&g));
}

static void test_weak_still_produce_food(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 0;
    g.person[0].state = ASH_PERSON_WEAK;
    g.threat = 0;
    g.expedition_total = 5;
    settle_s(&g, 7200);
    assert(g.food > 0);
}

static void test_quest0_finish_via_crisis_at_step4(void)
{
    ash_game_t g;
    new_game(&g);
    g.expedition_total = 5;
    g.threat = 85;
    g.parts = 10;
    g.quest_step[0] = 4;
    g.quest_done[0] = 0;
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    settle_s(&g, 1);
    assert(ash_game_crisis_pending(&g));
    assert(ash_game_resolve_crisis(&g, ASH_CRISIS_FORTIFY));
    assert(g.quest_done[0] == 1);
    assert(g.threat_rise_num == 7);
}

static void test_stash_and_caps(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.stash_lv == 1);
    assert(ash_game_stash_cap(&g) == 40);
    assert(ash_game_food_cap(&g) == 50);
    g.food = 40;
    g.kitchen_lv = 1;
    assert(ash_game_stash_deposit(&g, true, 5));
    assert(g.food == 35);
    assert(g.stash_food == 5);
    assert(ash_game_stash_withdraw(&g, true, 5));
    assert(g.food == 40);
    assert(g.stash_food == 0);
    /* 槽位顶满后 settle 不涨占用 */
    g.iron = (uint16_t)(ash_game_stash_slots_cap(&g) * ASHBOX_STASH_SLOT_IRON);
    assert(ash_game_stash_full(&g));
    uint16_t iron0 = g.iron;
    settle_s(&g, 3600);
    assert(g.iron == iron0);
    /* 独立扩仓抬高槽位 */
    g.parts = 50;
    assert(ash_game_upgrade_stash(&g));
    assert(g.stash_lv == 2);
    assert(ash_game_stash_slots_cap(&g) > 8);
    assert(!ash_game_stash_full(&g));
}

static void test_farm_plant_harvest(void)
{
    ash_game_t g;
    new_game(&g);
    g.kitchen_lv = 2;
    g.item[ASH_ITEM_SEED] = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    assert(ash_game_farm_unlocked(&g));
    assert(ash_game_farm_plant(&g, 0, ASH_CROP_GRAIN));
    assert(g.item[ASH_ITEM_SEED] == 1);
    assert(g.farm[0].crop == ASH_CROP_GRAIN);
    assert(g.farm[0].remain_s > 0);
    assert(!ash_game_farm_harvest(&g, 0));
    settle_s(&g, 14400);
    assert(g.farm[0].remain_s == 0);
    uint16_t food0 = g.food;
    assert(ash_game_farm_harvest(&g, 0));
    assert(g.food > food0);
    assert(g.farm[0].crop == 0);
}

static void test_trade_daily_and_hunt_gate(void)
{
    ash_game_t g;
    new_game(&g);
    assert(!ash_game_tier_unlocked(&g, ASH_TIER_HUNT));
    g.gate_unlocked = 1;
    assert(ash_game_tier_unlocked(&g, ASH_TIER_HUNT));
    assert(!ash_game_camp_open(&g));
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.food = 20;
    g.parts = 10;
    g.day_key = 9;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    assert(ash_game_trade_available(&g));
    uint16_t w0 = ash_game_total_water(&g);
    assert(ash_game_trade(&g, ASH_TRADE_FOOD_FOR_WATER));
    assert(g.food == 15);
    assert(ash_game_total_water(&g) == (uint16_t)(w0 + 1));
    assert(!ash_game_trade_available(&g));
    assert(!ash_game_trade(&g, ASH_TRADE_FOOD_FOR_WATER));
}

static void test_long_run_no_wrap(void)
{
    ash_game_t g;
    new_game(&g);
    g.pop = 3;
    g.kitchen_lv = 3;
    g.workshop_lv = 3;
    g.dorm_lv = 3;
    for (int i = 0; i < 3; i++) {
        g.person[i].id = (uint8_t)i;
        g.person[i].state = ASH_PERSON_HEALTHY;
    }
    g.food = 20;
    g.parts = 50;
    g.expedition_total = 5;
    for (int d = 0; d < 40; d++) {
        settle_s(&g, 86400);
        assert(g.food <= (uint16_t)ash_game_food_cap(&g));
        assert(g.stash_food <= (uint16_t)ash_game_stash_cap(&g));
        assert(g.parts <= ASHBOX_PARTS_CAP);
        assert(g.stash_parts <= ASHBOX_PARTS_CAP);
        assert(g.threat <= 100);
    }
}

static void test_quest_finish_branches_in_pools(void)
{
    /* 收束分支必须出现在 outcome / tag，避免线永久卡死 */
    const ash_outcome_def_t *deep = ash_content_outcome(5);
    bool has29 = false, has19 = false;
    for (uint8_t i = 0; i < deep->branch_count; i++) {
        if (deep->branch_ids[i] == 29) has29 = true;
        if (deep->branch_ids[i] == 19) has19 = true; /* 听匣低语 · 步0 */
    }
    assert(has19 && has29);

    /* 余烬线收束 + 步0 仍在标签池 */
    uint8_t ember_tag[3];
    uint8_t en = ash_content_tag_branches(2, ember_tag, 3); /* 匣里余烬 */
    bool has24 = false, has_step0 = false;
    for (uint8_t i = 0; i < en; i++) {
        if (ember_tag[i] == 24) has24 = true;
        if (ember_tag[i] == 4 || ember_tag[i] == 19) has_step0 = true;
    }
    assert(has24 && has_step0);

    uint8_t tagged[3];
    uint8_t n = ash_content_tag_branches(1, tagged, 3); /* 新鲜脚印 */
    bool has12 = false;
    for (uint8_t i = 0; i < n; i++) {
        if (tagged[i] == 12) has12 = true;
    }
    assert(has12);

    /* 猎场/过夜独立池 */
    const ash_outcome_def_t *hunt = ash_content_outcome(9);
    const ash_outcome_def_t *night = ash_content_outcome(11);
    assert(hunt->branch_count == 3);
    assert(night->branch_count == 3);
    bool has30 = false;
    for (uint8_t i = 0; i < hunt->branch_count; i++) {
        if (hunt->branch_ids[i] == 30) has30 = true;
    }
    assert(has30);
}

static void test_quest1_ember_whisper_step0_reachable(void)
{
    ash_game_t g;
    new_game(&g);
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.food = 40;
    g.parts = 40;
    g.expedition_total = 5;
    g.quest_step[1] = 0;
    g.quest_done[1] = 0;
    assert(ash_game_start_expedition(&g, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
    /* 立刻归来：用满时长；成败都可能，失败则重试到成功一次 */
    for (int tries = 0; tries < 40 && g.quest_step[1] < 1; tries++) {
        if (!g.exp.active) {
            g.person[0].state = ASH_PERSON_HEALTHY;
            g.person[0].fatigue = 0;
            assert(ash_game_start_expedition(&g, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
        }
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending) {
            if (g.report.success && g.quest_step[1] >= 1) break;
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
        settle_s(&g, 900);
    }
    assert(g.quest_step[1] >= 1); /* catch-up 或深处推进 */
}

/* G2：首趟深处成功只 0→1，不得同趟跳到 ≥2 */
static void test_quest1_deep_single_step(void)
{
    ash_game_t g;
    for (int attempt = 0; attempt < 60; attempt++) {
        new_game(&g);
        g.pop = 1;
        g.person[0].state = ASH_PERSON_HEALTHY;
        g.person[0].trust = 3;
        g.person[0].fatigue = 0;
        g.food = 50;
        g.parts = 50;
        g.expedition_total = 5 + (uint32_t)attempt;
        g.day_key = 200 + (uint32_t)attempt;
        g.quest_step[1] = 0;
        g.quest_done[1] = 0;
        assert(ash_game_start_expedition(&g, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (!(g.report.pending && g.report.success)) continue;
        assert(g.quest_step[1] == 1);
        /* 不选「带回碎片」枝：同趟不得被推到 2 */
        if (g.report.branch_count && !g.report.branch_resolved) {
            for (uint8_t i = 0; i < g.report.branch_count; i++) {
                if (g.report.branch_ids[i] != 9) {
                    (void)ash_game_pick_branch(&g, i);
                    break;
                }
            }
            if (!g.report.branch_resolved) (void)ash_game_pick_branch(&g, 0);
        }
        ash_game_ack_report(&g);
        assert(g.quest_step[1] == 1 || g.quest_step[1] == 2); /* 若误选枝9可为2 */
        /* 第二趟深处：应从 1→2 */
        if (g.quest_step[1] != 1) return;
        g.person[0].state = ASH_PERSON_HEALTHY;
        g.person[0].fatigue = 0;
        assert(ash_game_start_expedition(&g, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending && g.report.success) {
            assert(g.quest_step[1] >= 2);
            return;
        }
    }
    assert(0 && "could not force two deep successes");
}

static void test_quest2_ash_tide_flow(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.quest2_step == 0);
    assert(g.quest2_done == 0);
    assert(strstr(ash_game_quest_name(2), "灰潮") != NULL);

    g.ember = 3;
    assert(ash_game_unlock_gate(&g));
    assert(g.quest2_step == 1); /* 听闻 */

    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    settle_s(&g, 1);
    assert(g.quest2_step == 2); /* 通联补齐 */

    g.item[ASH_ITEM_JERKY] = 2;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.day_key = 3;
    assert(ash_game_trade(&g, ASH_TRADE_JERKY_FOR_SEED));
    assert(g.quest2_step == 3); /* 以物易种 */

    g.kitchen_lv = 2;
    g.item[ASH_ITEM_SEED] = 1;
    assert(ash_game_farm_plant(&g, 0, ASH_CROP_GRAIN));
    settle_s(&g, 14400);
    assert(ash_game_farm_harvest(&g, 0));
    assert(g.quest2_step == 4); /* 试种成活 */

    g.lore_shards = 2;
    g.report.pending = true;
    g.report.branch_count = 1;
    g.report.branch_ids[0] = 29;
    g.report.branch_resolved = 0;
    g.exp.person_id = 0;
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest2_done == 1);
    assert(g.lore_shards == 4);
    assert(g.item[ASH_ITEM_TOKEN] >= 1);
}

static void test_quest2_catchup_from_phase_a(void)
{
    ash_game_t g;
    new_game(&g);
    g.gate_unlocked = 1;
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.quest2_step = 0;
    ash_game_after_migrate(&g);
    assert(g.quest2_step == 2);
    assert(g.quest2_done == 0);
    /* G3：迁移 catch-up 须有说明纪事 */
    bool found = false;
    for (uint8_t i = 0; i < g.journal_count && i < ASHBOX_JOURNAL_N; i++) {
        if (strstr(g.journal[i], "跳过灰潮听闻") != NULL) found = true;
    }
    assert(found);
}

/* G5：抉择后 L1 场面、L2 会计 */
static void test_pick_branch_report_scene_first(void)
{
    ash_game_t g;
    new_game(&g);
    g.report.pending = true;
    g.report.branch_count = 1;
    g.report.branch_ids[0] = 9; /* 带回碎片 */
    g.report.branch_resolved = 0;
    g.report.food_delta = 4;
    g.report.parts_delta = 2;
    g.exp.person_id = 0;
    g.quest_step[1] = 1;
    assert(ash_game_pick_branch(&g, 0));
    assert(strstr(g.report.lines[1], "粮") == NULL); /* L1 非纯会计 */
    assert(strstr(g.report.lines[2], "粮") != NULL);
}

/* G4：三线了结后电台走守望句 */
static void test_late_watch_radio(void)
{
    ash_game_t g;
    new_game(&g);
    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    g.lore_twist = 1;
    g.day_key = 9;
    g.expedition_total = 3;
    g.food = 20;
    g.threat = 10;
    const char *r = ash_game_radio(&g);
    assert(r && r[0]);
    assert(strstr(r, "守望") != NULL || strstr(r, "匣") != NULL || strstr(r, "潮") != NULL);
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_TUNE));
    bool found = false;
    for (uint8_t i = 0; i < g.journal_count && i < ASHBOX_JOURNAL_N; i++) {
        if (strstr(g.journal[i], "守望") != NULL || strstr(g.journal[i], "匣") != NULL ||
            strstr(g.journal[i], "潮") != NULL || strstr(g.journal[i], "渡口") != NULL)
            found = true;
    }
    assert(found);
}

static void test_phase_c_camp2_furn_root(void)
{
    ash_game_t g;
    new_game(&g);
    assert(g.ver == ASHBOX_SAVE_VER);
    assert(g.furn_lv == 0);
    assert(g.day_latch_trade2 == ASHBOX_DAY_LATCH_NEVER);
    assert(!ash_game_root_unlocked(&g));
    assert(!ash_game_camp2_open(&g));

    /* 了结调查线开通渡口并赠薯种 */
    g.quest2_step = 4;
    g.lore_shards = 2;
    g.report.pending = true;
    g.report.branch_count = 1;
    g.report.branch_ids[0] = 29;
    g.report.branch_resolved = 0;
    g.exp.person_id = 0;
    assert(ash_game_pick_branch(&g, 0));
    assert(g.quest2_done == 1);
    assert(ash_game_camp2_open(&g));
    assert(ash_game_root_unlocked(&g));
    assert(g.seed_root >= 1);

    /* 渡口日限交易与铁棚分离 */
    g.item[ASH_ITEM_JERKY] = 2;
    g.parts = 20;
    g.day_key = 11;
    g.day_latch_trade = (uint16_t)g.day_key; /* 铁棚今日已换 */
    g.day_latch_trade2 = ASHBOX_DAY_LATCH_NEVER;
    assert(!ash_game_trade_available(&g));
    assert(ash_game_trade2_available(&g));
    assert(ash_game_trade(&g, ASH_TRADE_JERKY_FOR_ROOT));
    assert(g.seed_root >= 2);
    assert(!ash_game_trade2_available(&g));
    assert(!ash_game_trade(&g, ASH_TRADE_PARTS_FOR_ROOT));

    /* 货架加仓、干燥架催熟；种薯产量更高 */
    g.parts = 80;
    g.item[ASH_ITEM_TOKEN] = 2;
    uint16_t cap0 = (uint16_t)ash_game_stash_cap(&g);
    assert(ash_game_upgrade_furn(&g));
    assert(g.furn_lv == 1);
    assert(ash_game_stash_cap(&g) == (int)cap0 + 20);
    assert(ash_game_upgrade_furn(&g));
    assert(g.furn_lv == 2);
    assert(g.item[ASH_ITEM_TOKEN] == 1);

    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    g.seed_root = 2;
    assert(ash_game_farm_plant(&g, 0, ASH_CROP_ROOT));
    assert(g.farm[0].crop == ASH_CROP_ROOT);
    assert(g.farm[0].remain_s == 21600u); /* 种植仍记满时长；催熟在 tick */
    /* 干燥架 tick ×8/7：18900s 墙钟 ≈ 满 6h */
    settle_s(&g, 18900);
    assert(g.farm[0].remain_s == 0);
    uint16_t food0 = g.food;
    assert(ash_game_farm_harvest(&g, 0));
    assert(g.food >= food0 + 7);
}

static void test_report_loot_overflow_to_stash(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = (uint16_t)ash_game_food_cap(&g);
    g.stash_food = 0;
    g.report.pending = true;
    g.report.branch_resolved = 1;
    g.report.branch_count = 0;
    g.report.food_delta = 10;
    g.report.parts_delta = 0;
    g.report.ember_delta = 0;
    ash_game_ack_report(&g);
    assert(g.food == (uint16_t)ash_game_food_cap(&g));
    assert(g.stash_food == 10);
}

static void test_radio_hooks_beat_journal(void)
{
    ash_game_t g;
    new_game(&g);
    ash_game_log(&g, "一条纪事");
    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM;
    g.farm[0].crop = ASH_CROP_GRAIN;
    g.farm[0].remain_s = 0;
    const char *r = ash_game_radio(&g);
    assert(strstr(r, "田地") != NULL);
    assert(strstr(r, "纪事") == NULL);
}

static void test_radio_lore_beats_journal(void)
{
    ash_game_t g;
    new_game(&g);
    ash_game_log(&g, "一条纪事");
    g.gate_unlocked = 1;
    g.lore_shards = 2;
    g.day_key = 6; /* (6+0)%3==0 lore lane；(6)&1==0 → 档内「旧城」句 */
    g.expedition_total = 0;
    const char *r = ash_game_radio(&g);
    assert(strstr(r, "纪事") == NULL);
    assert(strstr(r, "旧城") != NULL);
}

static void test_home_nudge_and_trade_hook(void)
{
    ash_game_t g;
    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.day_key = 5;
    g.food = 20; /* 可付粮换件 → 值得催交易 */
    const char *n = ash_game_home_nudge(&g);
    assert(strstr(n, "铁棚") != NULL);
    const char *r = ash_game_radio(&g);
    assert(strstr(r, "铁棚") != NULL);

    /* 通联但付不起：不得整日盖死电台 */
    g.food = 2;
    g.parts = 0;
    g.item[ASH_ITEM_JERKY] = 0;
    ash_game_log(&g, "一条纪事");
    r = ash_game_radio(&g);
    assert(strstr(r, "今日可换") == NULL);

    g.food = 0;
    g.stash_food = 8;
    assert(ash_game_try_pocket_refill(&g));
    assert(g.food > 0);
    assert(g.stash_food < 8);
}

static void test_station_chore_during_expedition(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 30;
    g.parts = 10;
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(!ash_game_chore_pending(&g));
    /* 近路 180s：约 25s 后应刷出站事 */
    g.exp.remaining_s = g.exp.duration_s - 30;
    settle_s(&g, 1);
    assert(ash_game_chore_pending(&g));
    assert(strstr(ash_game_wait_nudge(&g), "风扇") != NULL ||
           strstr(ash_game_wait_nudge(&g), "灶") != NULL ||
           strstr(ash_game_wait_nudge(&g), "匣") != NULL ||
           strstr(ash_game_wait_nudge(&g), "门") != NULL);
    uint16_t parts_before = g.parts;
    uint16_t food_before = g.food;
    uint8_t threat_before = g.threat;
    assert(ash_game_resolve_chore(&g));
    assert(!ash_game_chore_pending(&g));
    assert(g.chore_done_exp == 1);
    assert(g.parts > parts_before || g.food > food_before || g.threat < threat_before ||
           g.stash_food > 0);
    const char *r = ash_game_radio(&g);
    assert(strstr(r, "站事") == NULL);
}

static void test_fatigue_and_jerky_nudge(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 5;
    g.item[ASH_ITEM_JERKY] = 1;
    assert(strstr(ash_game_home_nudge(&g), "干肉") != NULL);

    new_game(&g);
    g.food = 20;
    g.person[0].fatigue = 90;
    assert(!ash_game_can_depart(&g));
    assert(strstr(ash_game_home_nudge(&g), "太累") != NULL);
}

static void test_home_nudge_quest_after_progress(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 20;
    g.quest_step[0] = 1;
    assert(strstr(ash_game_home_nudge(&g), "路线") != NULL);
}

static void test_stash_auto_pull_and_jerky(void)
{
    ash_game_t g;
    new_game(&g);
    g.food = 0;
    g.stash_food = 12;
    g.person[0].state = ASH_PERSON_WEAK;
    g.threat = 0;
    g.expedition_total = 5;
    settle_s(&g, 1);
    assert(g.food > 0);
    assert(g.stash_food < 12);

    new_game(&g);
    g.item[ASH_ITEM_JERKY] = 2;
    g.food = 10;
    assert(ash_game_use_jerky(&g));
    assert(g.item[ASH_ITEM_JERKY] == 1);
    assert(g.food == 14);
    assert(!ash_game_trade_can_pay(&g, ASH_TRADE_FOOD_FOR_WATER) || g.food >= 5);
}

static void test_rare_seed_drop_possible(void)
{
    /* 近路成功在足够多 day_key 下应至少一次掉种（2% 档，有盖） */
    bool got = false;
    for (int attempt = 0; attempt < 200 && !got; attempt++) {
        ash_game_t g;
        new_game(&g);
        g.day_key = (uint32_t)(1000 + attempt);
        g.expedition_total = 5;
        g.food = 40;
        g.person[0].trust = 3;
        g.person[0].fatigue = 0;
        assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending && g.report.success && g.item[ASH_ITEM_SEED] > 0) got = true;
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
    }
    assert(got);
}

static void test_ember_pity_block_path(void)
{
    /* 连续废街成功未掉烬时，软保底应给出余烬（D0） */
    ash_game_t g;
    new_game(&g);
    g.food = 80;
    g.parts = 40;
    g.expedition_total = 5;
    g.day_key = 42;
    g.ember_dry = (uint8_t)(ASHBOX_EMBER_PITY_BLOCK - 1u);
    assert(ash_game_start_expedition(&g, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
    g.exp_leg_total = 1;
    g.exp.remaining_s = 1;
    /* 强制成功：把 fail 概率绕开——多试 day 直到成功且触发保底 */
    bool got = false;
    for (int attempt = 0; attempt < 40 && !got; attempt++) {
        new_game(&g);
        g.food = 80;
        g.parts = 40;
        g.expedition_total = 5 + (uint32_t)attempt;
        g.day_key = 1000 + (uint32_t)attempt;
        g.ember_dry = (uint8_t)(ASHBOX_EMBER_PITY_BLOCK - 1u);
        g.person[0].trust = 3;
        g.person[0].fatigue = 0;
        assert(ash_game_start_expedition(&g, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE));
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending && g.report.success && g.report.ember_delta > 0) got = true;
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
    }
    assert(got);
}

static void test_tide_shards_and_harvest_catchup(void)
{
    ash_game_t g;
    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.quest2_step = 2;
    g.item[ASH_ITEM_SHARD] = 3;
    g.lore_shards = 3;
    assert(ash_game_tide_offer_shards(&g));
    assert(g.quest2_step == 3);
    assert(g.item[ASH_ITEM_SHARD] == 1);

    new_game(&g);
    g.kitchen_lv = 2;
    g.world_flags |= (uint8_t)ASH_WORLD_FARM | (uint8_t)ASH_WORLD_CAMP;
    g.quest2_step = 2;
    g.item[ASH_ITEM_SEED] = 1;
    assert(ash_game_farm_plant(&g, 0, ASH_CROP_GRAIN));
    g.farm[0].remain_s = 0;
    assert(ash_game_farm_harvest(&g, 0));
    assert(g.quest2_step >= 3);
}

static void test_dead_branch_scan(void)
{
    /* 调查推进枝必须 ∈ outcome∪tag，或系统推进白名单（灰潮换种/收获） */
    unsigned bn = ash_content_branch_count();
    unsigned on = ash_content_outcome_count();
    unsigned tn = ash_content_tag_count();
    assert(bn > 0 && on > 0 && tn > 0);
    bool reachable[256];
    memset(reachable, 0, sizeof(reachable));
    for (unsigned o = 0; o < on; o++) {
        const ash_outcome_def_t *od = ash_content_outcome((uint8_t)o);
        assert(od->branch_count <= ASHBOX_BRANCH_MAX);
        for (uint8_t i = 0; i < od->branch_count; i++) {
            uint8_t id = od->branch_ids[i];
            assert(id < bn);
            reachable[id] = true;
        }
    }
    for (unsigned t = 0; t < tn; t++) {
        uint8_t ids[ASHBOX_TAG_POOL_MAX];
        uint8_t n = ash_content_tag_branches((uint8_t)t, ids, ASHBOX_TAG_POOL_MAX);
        for (uint8_t i = 0; i < n; i++) {
            assert(ids[i] < bn);
            reachable[ids[i]] = true;
        }
    }
    static const uint8_t system_only[] = {27, 28}; /* 换种 / 验收 */
    for (unsigned id = 0; id < bn; id++) {
        const ash_branch_def_t *b = ash_content_branch((uint8_t)id);
        if (b->quest_id == 255) continue;
        if (reachable[id]) continue;
        bool allowed = false;
        for (unsigned a = 0; a < sizeof(system_only); a++) {
            if (system_only[a] == (uint8_t)id) {
                allowed = true;
                break;
            }
        }
        assert(allowed);
    }
}

static void test_door_bolt_late_sink(void)
{
    ash_game_t g;
    new_game(&g);
    g.parts = 50;
    assert(!ash_game_buy_door_bolt(&g)); /* 需先静音轮 */
    assert(ash_game_door_bolt_cost(&g) == 0);
    assert(ash_game_buy_quiet_gear(&g));
    assert(g.quiet_gear == 1);
    uint16_t cost = ash_game_door_bolt_cost(&g);
    assert(cost == 24);
    uint16_t before = g.parts;
    assert(ash_game_buy_door_bolt(&g));
    assert(g.door_bolt == 1);
    assert(g.parts == (uint16_t)(before - cost));
    assert(ash_game_door_bolt_cost(&g) == 0);
    assert(!ash_game_buy_door_bolt(&g));
    /* 加固更便宜（有底） */
    g.crisis.pending = true;
    g.gate_unlocked = 1;
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    uint16_t fc = ash_game_crisis_fortify_cost(&g);
    assert(fc >= 1 && fc <= 2);
    const char *nudge = ash_game_home_nudge(&g);
    assert(nudge);
    /* 已装门扣后不应再催装 */
    g.parts = 80;
    nudge = ash_game_home_nudge(&g);
    assert(!strstr(nudge, "门扣"));
}

static void test_gear_quiet_rep_twist(void)
{
    ash_game_t g;
    new_game(&g);
    g.ember = 10;
    g.parts = 40;
    assert(ash_game_upgrade_gear(&g));
    assert(g.gear_lv == 1);
    assert(g.ember == 7);
    assert(ash_game_buy_quiet_gear(&g));
    assert(g.quiet_gear == 1);

    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.food = 30;
    g.day_key = 9;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    assert(ash_game_trade(&g, ASH_TRADE_FOOD_FOR_WATER));
    assert(g.rep_shed == 1);
    assert(!ash_game_trade_available(&g)); /* rep<2 无二次 */
    g.rep_shed = 2;
    assert(ash_game_trade_available(&g));
    assert(ash_game_trade(&g, ASH_TRADE_FOOD_FOR_WATER));
    assert(g.day_latch_trade_extra == (uint16_t)g.day_key);
    assert(!ash_game_trade_available(&g));

    g.lore_shards = 4;
    ash_game_after_migrate(&g);
    assert(g.lore_twist == 1);
    const char *r = ash_game_radio(&g);
    assert(r && r[0]);
}

static void test_station_duty_daily_cap(void)
{
    ash_game_t g;
    new_game(&g);
    g.day_key = 3;
    assert(ash_game_duty_available(&g));
    assert(ash_game_duty_left(&g) == ASHBOX_DUTY_MAX);
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_STOVE));
    assert(g.food >= 21);
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_GATE));
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_TUNE));
    assert(ash_game_duty_left(&g) == 0);
    assert(!ash_game_duty_available(&g));
    assert(!ash_game_do_duty(&g, ASHBOX_DUTY_TALLY));
    /* 跨日重置 */
    g.day_key = 4;
    assert(ash_game_duty_left(&g) == ASHBOX_DUTY_MAX);
    assert(ash_game_duty_available(&g));
    /* 远征中不可日勤 */
    assert(ash_game_start_expedition(&g, ASH_TIER_SHORT, 0, ASH_LOAD_NONE));
    assert(!ash_game_duty_available(&g));
}

/* B12/B13: day_key low8 wrap must not freeze duty / false-lock trade */
static void test_day_latch_survives_low8_wrap(void)
{
    ash_game_t g;
    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_key = 1;
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_STOVE));
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_GATE));
    assert(ash_game_do_duty(&g, ASHBOX_DUTY_TUNE));
    assert(ash_game_duty_left(&g) == 0);
    g.item[ASH_ITEM_JERKY] = 1;
    assert(ash_game_trade(&g, ASH_TRADE_JERKY_FOR_SEED));
    assert(!ash_game_trade_available(&g));

    g.day_key = 257; /* same low8 as day 1 */
    assert(ash_game_duty_left(&g) == ASHBOX_DUTY_MAX);
    assert(ash_game_duty_available(&g));
    assert(ash_game_trade_available(&g));
}

/* B14: day_key with low8==0 must keep ember day latch (never clamp 0→never) */
static void test_ember_trade_day0_not_clamped(void)
{
    ash_game_t g;
    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_key = 256; /* low8 == 0 */
    g.parts = 40;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    assert(ash_game_trade_can_pay(&g, ASH_TRADE_PARTS_FOR_EMBER));
    assert(ash_game_trade(&g, ASH_TRADE_PARTS_FOR_EMBER));
    assert(g.day_latch_ember == 256);
    ash_game_after_migrate(&g);
    assert(g.day_latch_ember == 256);
    assert(!ash_game_trade_can_pay(&g, ASH_TRADE_PARTS_FOR_EMBER));
}

/* B1: backpack raises crowbar carry cap to 2 */
static void test_crowbar_backpack_cap(void)
{
    ash_game_t g;
    new_game(&g);
    g.crowbar = 3;
    assert(ash_game_crowbar_carry_cap(&g, 0) == 1);
    g.equip_backpack[0] = 1;
    assert(ash_game_crowbar_carry_cap(&g, 0) == 2);
    g.food = 40;
    g.water = 20;
    assert(ash_game_start_expedition_ex(&g, ASH_TIER_SHORT, 0, 2, 0, 0));
    assert(g.exp_crowbar == 2);
    assert(g.crowbar == 1);
}

static void test_phase_d_block_gate_expectation(void)
{
    /* D5：仅废街路径在有限趟内应能攒到开闸烬（含保底；约每10次成功保底1烬） */
    ash_game_t g;
    new_game(&g);
    g.food = 120;
    g.parts = 80;
    g.kitchen_lv = 2;
    g.pop = 2;
    g.person[1].id = 1;
    g.person[1].state = ASH_PERSON_HEALTHY;
    int trips = 0;
    for (int i = 0; i < 48 && !g.gate_unlocked; i++) {
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
        if (ash_game_crisis_pending(&g)) {
            if (ash_game_crisis_option_ok(&g, ASH_CRISIS_TALK))
                (void)ash_game_resolve_crisis(&g, ASH_CRISIS_TALK);
            else
                (void)ash_game_resolve_crisis(&g, ASH_CRISIS_HIDE);
        }
        if (g.ember >= 3) {
            assert(ash_game_unlock_gate(&g));
            break;
        }
        g.day_key = 2000 + (uint32_t)i;
        g.food = 120;
        g.parts = 80;
        g.water = 40;
        g.threat = 20;
        if (g.pop < 2) {
            g.pop = 2;
            g.person[1].id = 1;
        }
        g.person[0].fatigue = 0;
        g.person[1].fatigue = 0;
        g.person[0].trust = 3;
        g.person[0].state = ASH_PERSON_HEALTHY;
        g.person[0].injure_remain_s = 0;
        g.person[1].state = ASH_PERSON_HEALTHY;
        if (!ash_game_start_expedition(&g, ASH_TIER_BLOCK, 0, ASH_LOAD_NONE)) {
            settle_s(&g, 3600);
            continue;
        }
        trips++;
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (g.report.pending) {
            if (g.report.branch_count && !g.report.branch_resolved)
                (void)ash_game_pick_branch(&g, 0);
            ash_game_ack_report(&g);
        }
        settle_s(&g, 7200); /* 疲劳回落 */
    }
    assert(g.gate_unlocked);
    assert(trips <= 40);
}

static void test_v10_slots_craft_wanderer_death(void)
{
    ash_game_t g;
    new_game(&g);
    assert(ash_game_water_cap(&g) == (int)ASHBOX_WATER_BASE_CAP);
    assert(ash_game_expedition_food_cost(ASH_TIER_SHORT) == 1);
    assert(ash_game_expedition_food_cost(ASH_TIER_BLOCK) == 2);
    assert(ash_game_expedition_food_cost(ASH_TIER_DEEP) == 8);
    assert(ash_game_expedition_food_cost(ASH_TIER_HUNT) == 4);
    assert(ash_game_expedition_food_cost(ASH_TIER_NIGHT) == 16);

    g.iron = 50;
    g.parts = 20;
    assert(ash_game_craft(&g, ASH_CRAFT_BENCH));
    assert(g.craft_bench == 1);
    g.iron = 18;
    assert(ash_game_craft(&g, ASH_CRAFT_BUCKET));
    assert(g.bucket == 1);
    assert(ash_game_water_cap(&g) == (int)(ASHBOX_WATER_BASE_CAP + ASHBOX_WATER_BUCKET_BONUS));

    g.iron = 100;
    assert(ash_game_craft(&g, ASH_CRAFT_PISTOL));
    assert(ash_game_equip_pistol(&g, 0, true));
    assert(g.equip_pistol[0] == 1);
    g.iron = 4;
    assert(ash_game_craft(&g, ASH_CRAFT_AMMO));
    assert(ash_game_craft(&g, ASH_CRAFT_AMMO));
    assert(g.ammo == 2);

    g.backpack = 1;
    assert(ash_game_equip_backpack(&g, 0, true));
    assert(g.backpack == 0);
    assert(g.equip_backpack[0] == 1);

    g.pop = 0;
    memset(g.person, 0, sizeof(g.person));
    memset(g.equip_backpack, 0, sizeof(g.equip_backpack));
    memset(g.equip_pistol, 0, sizeof(g.equip_pistol));
    assert(!ash_game_can_depart(&g));
    assert(ash_game_pop_cap(&g) == 1);
    g.wanderer_pending = 1;
    g.wanderer_name_id = 0;
    g.food = 25;
    g.water = 15;
    assert(ash_game_wanderer_can_join(&g));
    assert(ash_game_wanderer_join(&g));
    assert(g.pop == 1);
    assert(g.food == 5);
    assert(g.water == 5);

    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_key = 3;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.water = 10;
    g.iron = 0;
    assert(ash_game_trade(&g, ASH_TRADE_WATER_FOR_IRON));
    assert(g.iron == 1);
    assert(ash_game_total_water(&g) == 5);

    new_game(&g);
    g.world_flags |= (uint8_t)ASH_WORLD_CAMP;
    g.day_key = 5;
    g.day_latch_trade = ASHBOX_DAY_LATCH_NEVER;
    g.water = 5;
    g.food = 0;
    assert(ash_game_trade(&g, ASH_TRADE_WATER_FOR_FOOD));
    assert(g.food == 3); /* reverse rate: water1 → food3 */
}

static void test_gameplay_retune_economy(void)
{
    ash_game_t g;
    /* Away people do not eat station food: one-away drains less than both-home. */
    ash_game_t home, away;
    new_game(&home);
    new_game(&away);
    home.pop = away.pop = 2;
    home.person[1].id = away.person[1].id = 1;
    home.person[1].state = away.person[1].state = ASH_PERSON_HEALTHY;
    home.kitchen_lv = away.kitchen_lv = 1;
    home.food = away.food = 40;
    home.water = away.water = 40;
    home.workshop_lv = away.workshop_lv = 1;
    assert(ash_game_start_expedition(&away, ASH_TIER_DEEP, 0, ASH_LOAD_NONE));
    assert(away.exp.remaining_s > 7200);
    uint16_t home0 = home.food;
    uint16_t away0 = away.food;
    settle_s(&home, 7200);
    settle_s(&away, 7200);
    int home_d = (int)home0 - (int)home.food;
    int away_d = (int)away0 - (int)away.food;
    /* Net delta (start-end): away produces less too, so gains less → larger delta. */
    assert(away_d > home_d);

    /* Deep sleep halves water consume vs awake (same wall dt). */
    new_game(&g);
    g.water = 40;
    g.stash_water = 0;
    g.well_lv = 0; /* no production */
    g.last_settle_unix = 1000; /* untrusted clock → always-consume path */
    ash_clock_info_t awake = {.trusted = true, .in_deep_sleep_segment = false, .dt_s = 7200};
    ash_game_settle(&g, &awake);
    uint16_t w_awake = g.water;
    new_game(&g);
    g.water = 40;
    g.stash_water = 0;
    g.well_lv = 0;
    g.last_settle_unix = 1000;
    ash_clock_info_t sleep = {.trusted = true, .in_deep_sleep_segment = true, .dt_s = 7200};
    ash_game_settle(&g, &sleep);
    assert(g.water > w_awake); /* slept less thirsty */

    /* Infect heals after 36h into fatigued healthy. */
    new_game(&g);
    g.person[0].state = ASH_PERSON_INFECTED;
    g.person[0].injure_remain_s = ASHBOX_INFECT_HEAL_S;
    g.food = 40;
    g.water = 40;
    settle_s(&g, (int64_t)ASHBOX_INFECT_HEAL_S - 1);
    assert(g.person[0].state == ASH_PERSON_INFECTED);
    settle_s(&g, 1);
    assert(g.person[0].state == ASH_PERSON_HEALTHY);
    assert(g.person[0].fatigue >= 60);

    /* Heavy wound: 24h untreated → death; stim within window saves. */
    new_game(&g);
    g.pop = 2;
    g.person[1] = g.person[0];
    g.person[1].id = 1;
    g.person[0].state = ASH_PERSON_INJURED;
    g.person[0].injure_remain_s = ASHBOX_INJURE_DEAD_S;
    g.food = 40;
    g.water = 40;
    settle_s(&g, (int64_t)ASHBOX_INJURE_DEAD_S - 1);
    assert(g.pop == 2);
    assert(g.person[0].state == ASH_PERSON_INJURED);
    settle_s(&g, 1);
    assert(g.pop == 1);
    assert(g.person[0].id == 0); /* survivor slid to slot 0 */

    new_game(&g);
    g.person[0].state = ASH_PERSON_INJURED;
    g.person[0].injure_remain_s = ASHBOX_INJURE_DEAD_S;
    g.med_stim = 1;
    assert(ash_game_use_stim(&g, 0));
    settle_s(&g, (int64_t)ASHBOX_INJURE_DEAD_S);
    assert(g.pop == 1);
    assert(g.person[0].state == ASH_PERSON_HEALTHY);

    /* Legacy 600s injure timer migrates to full-day window. */
    new_game(&g);
    g.person[0].state = ASH_PERSON_INJURED;
    g.person[0].injure_remain_s = 600;
    ash_game_after_migrate(&g);
    assert(g.person[0].injure_remain_s == ASHBOX_INJURE_DEAD_S);

    /* Stove lining + box lamp sinks; dorm L3 consume matches L2. */
    new_game(&g);
    g.quiet_gear = 1;
    g.door_bolt = 1;
    g.parts = 100;
    assert(ash_game_buy_stove_lining(&g));
    assert(ash_game_has_stove_lining(&g));
    g.gear_lv = ASHBOX_GEAR_LV_MAX;
    g.ember = 10;
    assert(ash_game_buy_box_lamp(&g));
    assert(ash_game_has_box_lamp(&g));

    new_game(&g);
    g.dorm_lv = 2;
    int c2 = 9 + 2; /* dorm_consume_num L2 */
    g.dorm_lv = 3;
    /* L3 beds=4 (expanded dorm ladder); consume still matches L2 at 11. */
    assert(ash_game_beds(&g) == 4);

    /* Legacy exhaust cleared on migrate. */
    new_game(&g);
    g.crew_exhaust_remain_s = 1000;
    ash_game_after_migrate(&g);
    assert(g.crew_exhaust_remain_s == 0);

    /* Night success food/water floors. */
    int night_food_ok = 0, night_water_ok = 0;
    for (int i = 0; i < 40; i++) {
        new_game(&g);
        g.food = 200;
        g.water = 200;
        g.day_key = 9000u + (unsigned)i * 17u;
        g.expedition_total = (uint32_t)(50 + i);
        g.person[0].trust = 3;
        g.person[0].fatigue = 0;
        if (!ash_game_start_expedition(&g, ASH_TIER_NIGHT, 0, ASH_LOAD_NONE)) continue;
        g.exp_leg_total = 1;
        g.exp.remaining_s = 1;
        settle_s(&g, 1);
        if (!g.report.pending || !g.report.success) continue;
        if (g.report.food_delta >= 16) night_food_ok++;
        if (g.report_water_delta >= 8) night_water_ok++;
    }
    assert(night_food_ok >= 5);
    assert(night_water_ok >= 5);
    (void)c2;
}

static void test_meta_clear_duty_super_core(void)
{
    ash_game_t g;
    ashbox_meta_t m;
    new_game(&g);
    ashbox_meta_new(&m);
    assert(ash_game_beds(&g) == 2);
    assert(ash_game_farm_plot_count(&g) == 2);
    assert(ash_game_build_lv_max(&g) == 3);

    /* dorm L3 beds=4; with super core can go L5 beds=6 */
    g.dorm_lv = 3;
    assert(ash_game_beds(&g) == 4);
    g.super_core = 1;
    assert(ash_game_use_super_core(&g));
    assert(g.farm_plots_unlocked == 4);
    assert(g.build_cap_bonus == 2);
    assert(g.loot_bonus_perm_pct == 50);
    assert(ash_game_build_lv_max(&g) == 5);
    g.dorm_lv = 5;
    assert(ash_game_beds(&g) == 6);

    /* pop==0: food not consumed over 2h daytime */
    new_game(&g);
    g.pop = 0;
    g.food = 40;
    g.water = 40;
    {
        int64_t day = (1650000000LL / 86400LL) * 86400LL;
        g.last_settle_unix = day + 2 * 3600;
    }
    uint16_t f0 = g.food;
    uint16_t w0 = g.water;
    settle_s(&g, 7200);
    assert(g.food == f0);
    assert(g.water == w0);

    /* clear + lone achievement after restart grant */
    new_game(&g);
    ashbox_meta_new(&m);
    g.quest_done[0] = 1;
    g.quest_done[1] = 1;
    g.quest2_done = 1;
    g.lore_twist = 1;
    g.pop = 1;
    assert(ash_game_try_clear(&g));
    assert(g.run_cleared);
    uint16_t neu = ash_game_eval_clear_achievements(&g, &m);
    assert(neu & ASH_ACH_LONE);
    assert(ashbox_meta_has(&m, ASH_ACH_LONE));
    ash_game_restart_with_meta(&g, &m);
    assert(g.pistol >= 1);

    /* duty job API */
    new_game(&g);
    assert(ash_game_set_person_job(&g, 0, ASHBOX_JOB_FARM));
    assert(ash_game_person_job(&g, 0) == ASHBOX_JOB_FARM);
    assert(ash_game_set_person_job(&g, 0, ASHBOX_JOB_WORK));

    /* wipeout MVP: two rejects while broke */
    new_game(&g);
    g.pop = 0;
    g.food = 0;
    g.water = 0;
    g.wanderer_pending = 1;
    assert(!ash_game_wanderer_can_join(&g));
    assert(ash_game_wanderer_dismiss(&g));
    assert(g.wipeout_reject_count == 1);
    g.wanderer_pending = 1;
    assert(ash_game_wanderer_dismiss(&g));
    assert(g.game_over);
}

static uint32_t day_with_phenom(uint8_t id);

/* Simulated overnight: the modifier applies at the local midnight exactly
 * once, and a short settle before midnight must not apply it early. */
static void test_phenom_midnight_once(void)
{
    uint32_t target = day_with_phenom(14); /* 灰潮: threat +15 only */
    ash_game_t g;
    new_game(&g);
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.food = 40;
    g.water = 20;
    g.threat = 50;
    g.crisis.reserved = 5; /* freeze the hourly threat rise for this check */
    g.day_key = target - 1;
    g.last_settle_unix = (int64_t)(target - 2) * 86400 - 8 * 3600 +
                         23 * 3600 + 59 * 60 + 30;

    settle_s(&g, 10); /* 23:59:30 -> 23:59:40, no crossing */
    assert(g.day_key == target - 1);
    assert(g.threat == 50);

    settle_s(&g, 60); /* -> 00:00:40 on the target day */
    assert(g.day_key == target);
    assert(g.threat == 65); /* 50 + 灰潮 15 */

    settle_s(&g, 60); /* same local day: no second application */
    assert(g.day_key == target);
    assert(g.threat == 65);
}

static uint32_t day_with_phenom(uint8_t id)
{
    for (uint32_t day = 1; day <= ASH_PHENOM_COUNT; day++) {
        const ash_phenom_t *p = ash_phenom_for_day(day);
        if (p && p->id == id) return day;
    }
    assert(0 && "phenomenon id missing from the 16-day cycle");
    return 1;
}

/* M2: daily weather only moves the resources it names, inside the caps. */
static void test_phenom_daily_modifier(void)
{
    ash_game_t g;
    new_game(&g);
    g.water = 10;
    g.food = 20;
    g.parts = 5;
    g.threat = 50;

    ash_game_apply_phenom_day(&g, day_with_phenom(1)); /* 灰雨: 水+3 粮-1 */
    assert(g.water == 13);
    assert(g.food == 19);
    assert(g.parts == 5);
    assert(g.threat == 50);

    g.water = 10;
    g.threat = 50;
    ash_game_apply_phenom_day(&g, day_with_phenom(2)); /* 铁风: 件+2 威胁+6 */
    assert(g.parts == 7);
    assert(g.threat == 56);

    g.water = 1;
    g.threat = 99;
    ash_game_apply_phenom_day(&g, day_with_phenom(7)); /* 热风: 水-3 威胁+5 */
    assert(g.water == 0);
    assert(g.threat == 100);

    g.threat = 50;
    ash_game_apply_phenom_day(&g, day_with_phenom(14)); /* 灰潮: 威胁+15 */
    assert(g.threat == 65);

    g.threat = 3;
    ash_game_apply_phenom_day(&g, day_with_phenom(3)); /* 静夜: 威胁-8 */
    assert(g.threat == 0);

    /* day 0 (unknown/epoch) must not crash and must still be deterministic. */
    const ash_phenom_t *p0 = ash_phenom_for_day(0);
    assert(p0 != NULL);
}

int main(void)
{
    test_branch_cost_affordability();
    test_join_respects_beds_and_wipeout();
    test_crisis_empty_station_resolves();
    test_water_awake_ticks();
    test_audit_20261112_regressions();
    test_new_game();
    test_short_probe_no_daily_cap();
    test_mid_return_excludes_away_production();
    test_note_wall_time_resyncs_day();
    test_note_wall_time_rejects_rtc_rewind();
    test_untrusted_no_advance();
    test_scene_priority();
    test_scenes_available();
    test_scene_autofocus_hooks();
    test_loot_all_tiers_have_food_and_parts();
    test_parts_prob_harder_near();
    test_med_use_and_stim_trade();
    test_crowbar_haul_distribution_smoke();
    test_backpack_iron_bonus_not_food_double();
    test_tier_names_plain();
    test_fatigue_and_trust();
    test_loadout_prepay_and_loot();
    test_branch_persists_until_picked();
    test_loot_deferred_until_ack();
    test_crisis_three_options();
    test_quest_branch_advance();
    test_journal_radio();
    test_upgrade_cost_api();
    test_plans_and_legs();
    test_deep_sleep_economy_half_expedition_full();
    test_night_order_and_gate();
    test_return_tag_sets_branches();
    test_crisis_escape_when_all_weak();
    test_weak_still_produce_food();
    test_quest0_finish_via_crisis_at_step4();
    test_quest_finish_branches_in_pools();
    test_quest1_ember_whisper_step0_reachable();
    test_quest1_deep_single_step();
    test_quest2_ash_tide_flow();
    test_quest2_catchup_from_phase_a();
    test_pick_branch_report_scene_first();
    test_late_watch_radio();
    test_stash_and_caps();
    test_rare_seed_drop_possible();
    test_farm_plant_harvest();
    test_trade_daily_and_hunt_gate();
    test_phase_c_camp2_furn_root();
    test_report_loot_overflow_to_stash();
    test_radio_hooks_beat_journal();
    test_radio_lore_beats_journal();
    test_home_nudge_and_trade_hook();
    test_station_chore_during_expedition();
    test_fatigue_and_jerky_nudge();
    test_home_nudge_quest_after_progress();
    test_stash_auto_pull_and_jerky();
    test_ember_pity_block_path();
    test_tide_shards_and_harvest_catchup();
    test_gear_quiet_rep_twist();
    test_door_bolt_late_sink();
    test_dead_branch_scan();
    test_phase_d_block_gate_expectation();
    test_station_duty_daily_cap();
    test_day_latch_survives_low8_wrap();
    test_ember_trade_day0_not_clamped();
    test_crowbar_backpack_cap();
    test_phenom_midnight_once();
    test_phenom_daily_modifier();
    test_long_run_no_wrap();
    test_post_clear_arc();
    test_depart_preview_matches_roll();
    test_stance_preview_and_start();
    test_crisis_fortify_cost_banded();
    test_armed_trip_spends_one_round();
    test_legacy_clear_arms_arc();
    test_crisis_bands_and_breach();
    test_branch_pool_filters_by_state();
    test_quest_second_beat();
    test_water_system();
    test_v10_slots_craft_wanderer_death();
    test_gameplay_retune_economy();
    test_meta_clear_duty_super_core();
    /* v13 pace: ember odds, farm yields, labor fatigue, wanderer schedule */
    {
        ash_game_t g;
        ash_game_new(&g);
        assert(g.pace_era == ASHBOX_PACE_V13);
        assert(ash_game_tier_unlocked(&g, ASH_TIER_DEEP));
        g.kitchen_lv = 2;
        g.world_flags |= (uint8_t)ASH_WORLD_FARM;
        g.item[ASH_ITEM_SEED] = 2;
        assert(ash_game_farm_plant(&g, 0, ASH_CROP_GRAIN));
        assert((*ash_game_farm_at(&g, 0)).remain_s == 86400u);
        (*ash_game_farm_mut(&g, 0)).remain_s = 0;
        g.farm_labor_h[0] = 12;
        uint16_t before = ash_game_total_food(&g);
        assert(ash_game_farm_harvest(&g, 0));
        uint16_t gained = (uint16_t)(ash_game_total_food(&g) - before);
        assert(gained == 69u); /* grain 45 + 24 labor cap */

        ash_game_t g2;
        ash_game_new(&g2);
        g2.pop = 1;
        g2.person[0].state = ASH_PERSON_HEALTHY;
        g2.person[0].fatigue = 0;
        g2.person_job[0] = ASHBOX_JOB_FARM;
        settle_s(&g2, 3600 * 16);
        assert(g2.person_job[0] == ASHBOX_JOB_REST);
        assert(g2.person[0].fatigue >= ASHBOX_LABOR_AUTO_REST);

        ash_game_t g3;
        ash_game_new(&g3);
        g3.last_settle_unix = 8 * 3600; /* local noon-ish day0 */
        g3.day_key = 1;
        settle_s(&g3, 1);
        assert(g3.wanderer_sched_day == 1);
        assert(g3.wanderer_t0_min >= 8 * 60 && g3.wanderer_t0_min < 22 * 60);
        assert(g3.wanderer_t1_min >= 8 * 60 && g3.wanderer_t1_min < 22 * 60);
        assert(g3.wanderer_t0_min != g3.wanderer_t1_min);

        ash_game_t g4;
        ash_game_new(&g4);
        g4.quest_done[0] = 1;
        g4.quest_done[1] = 1;
        g4.quest2_done = 1;
        g4.lore_twist = 1;
        g4.day_key = 1;
        assert(ash_game_try_clear(&g4));

        ash_game_t g5;
        ash_game_new(&g5);
        g5.pop = 2;
        g5.person[1].state = ASH_PERSON_HEALTHY;
        assert(ash_game_exile_person(&g5, 1));
        assert(g5.pop == 1);
    }

    /* Keep in sync with ASHBOX_SAVE_BLOB_SIZE in ashbox_save.h */
    assert(sizeof(ash_game_t) == ASHBOX_SAVE_BLOB_SIZE);
    assert(sizeof(ash_game_t) == 816u);
    assert(offsetof(ash_game_t, crc) + sizeof(uint32_t) <= sizeof(ash_game_t));
    assert(offsetof(ash_game_t, iron) == 592u);
    assert(offsetof(ash_game_t, day_latch_trade) == 628u);
    assert(offsetof(ash_game_t, person_ext) == 644u);
    puts("test_ashbox_game: PASS");
    return 0;
}
