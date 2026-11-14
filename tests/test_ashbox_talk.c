#include "ashbox_talk.h"
#include "ashbox_game.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_available_and_mark(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 7;
    g.talk_day = 0;
    assert(ash_talk_available(&g));
    ash_talk_mark_seen(&g);
    assert(g.talk_day == 7);
    assert(!ash_talk_available(&g));
    g.day_key = 8;
    assert(ash_talk_available(&g));
}

static void test_pick_deterministic(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 3;
    g.pop = 2;
    g.person[1].state = ASH_PERSON_HEALTHY;
    const ash_talk_t *a = ash_talk_pick(&g);
    const ash_talk_t *b = ash_talk_pick(&g);
    assert(a != NULL && a == b);
    assert(a->title && a->title[0] && a->body && a->body[0]);
    assert(a->opt[0].text[0] && a->opt[1].text[0]);
    /* Different days eventually pick different scenes. */
    bool varied = false;
    for (uint32_t d = 1; d <= 16; d++) {
        g.day_key = d;
        const ash_talk_t *p = ash_talk_pick(&g);
        if (p && p->id != a->id) varied = true;
    }
    assert(varied);
}

static void test_apply_clamps(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 5;
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.food = 0;
    g.water = 0;
    g.parts = 0;
    g.ember = 0;
    g.threat = 0;
    g.person[0].fatigue = 100;
    g.person[0].trust = -20;
    const ash_talk_t *t = ash_talk_pick(&g);
    assert(t != NULL);
    for (uint8_t c = 0; c < 2; c++) {
        ash_talk_apply(&g, t, c);
        assert(g.food <= ash_game_food_cap(&g));
        assert(g.water <= ash_game_water_cap(&g));
        assert(g.parts <= ASHBOX_PARTS_CAP);
        assert(g.ember <= ASHBOX_EMBER_CAP);
        assert(g.threat <= 100);
        assert(g.person[0].fatigue <= 100);
        assert(g.person[0].trust >= -20 && g.person[0].trust <= 20);
    }
    ash_talk_apply(&g, t, 2); /* invalid choice is ignored */
    assert(t->id < 12);
}

int main(void)
{
    test_available_and_mark();
    test_pick_deterministic();
    test_apply_clamps();
    puts("test_ashbox_talk: PASS");
    return 0;
}
