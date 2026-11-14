#include "ashbox_codex.h"
#include "ashbox_game.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_table_shape(void)
{
    for (uint8_t i = 0; i < ASH_CODEX_COUNT; i++) {
        const ash_codex_entry_t *e = ash_codex_entry(i);
        assert(e != NULL);
        assert(e->title && e->title[0]);
        assert(e->body && strlen(e->body) > 10);
        assert(e->source && e->source[0]);
        assert(ash_codex_vol_of(i) < ASH_CODEX_VOL_COUNT);
    }
    assert(ash_codex_entry(ASH_CODEX_COUNT) == NULL);
    for (uint8_t v = 0; v < ASH_CODEX_VOL_COUNT; v++) {
        assert(ash_codex_vol_name(v) && ash_codex_vol_name(v)[0]);
    }
}

static void test_start_and_full_sweep(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.codex_seen = 0;
    g.codex_points = 0;
    ash_codex_sweep(&g);
    /* A fresh station knows the opening world entry and阿禾 by being there. */
    assert(ash_codex_has(&g, 0));
    assert(ash_codex_has(&g, 12));
    assert(ash_codex_count(&g) >= 2);
    assert(g.codex_points == ash_codex_count(&g));

    g.pop = 5;
    g.expedition_total = 3;
    g.quest_done[0] = 1;
    g.gate_unlocked = 1;
    g.lore_twist = 1;
    g.world_flags |= (uint8_t)(ASH_WORLD_CAMP | ASH_WORLD_CAMP2);
    g.crowbar = 1;
    g.exp_crowbar = 1;
    g.pistol = 1;
    g.lore_shards = 1;
    g.super_core = 1;
    g.run_cleared = 1;
    g.sig_day = 1;
    g.talk_day = 1;
    ash_codex_sweep(&g);
    assert(ash_codex_count(&g) == ASH_CODEX_COUNT);
    assert(g.codex_points == ASH_CODEX_COUNT);
    for (uint8_t v = 0; v < ASH_CODEX_VOL_COUNT; v++) {
        assert(ash_codex_vol_count(&g, v) == ASH_CODEX_PER_VOL);
    }
    assert(strcmp(ash_codex_rank(ASH_CODEX_COUNT), "见证者") == 0);

    /* Unlock is idempotent and sweep does not disturb a complete book. */
    uint32_t before = g.codex_seen;
    ash_codex_sweep(&g);
    assert(g.codex_seen == before);
    assert(!ash_codex_unlock(&g, 0));
}

static void test_bounds_and_ranks(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.codex_seen = 0;
    assert(!ash_codex_has(&g, ASH_CODEX_COUNT));
    assert(!ash_codex_unlock(&g, ASH_CODEX_COUNT));
    assert(ash_codex_vol_count(&g, ASH_CODEX_VOL_COUNT) == 0);
    assert(strcmp(ash_codex_rank(0), "") == 0);
    assert(strcmp(ash_codex_rank(4), "") == 0);
    assert(strcmp(ash_codex_rank(5), "守夜人") == 0);
    assert(strcmp(ash_codex_rank(10), "听风者") == 0);
    assert(strcmp(ash_codex_rank(15), "记录者") == 0);
    assert(strcmp(ash_codex_rank(20), "见证者") == 0);
}

int main(void)
{
    test_table_shape();
    test_start_and_full_sweep();
    test_bounds_and_ranks();
    puts("test_ashbox_codex: PASS");
    return 0;
}
