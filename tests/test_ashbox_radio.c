#include "ashbox_radio.h"
#include "ashbox_game.h"

#include <assert.h>
#include <stdio.h>

static void test_window_and_day_sync(void)
{
    assert(ash_radio_window_open(20));
    assert(ash_radio_window_open(0));
    assert(ash_radio_window_open(9));
    assert(!ash_radio_window_open(15));
    assert(!ash_radio_window_open(17));

    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 4;
    g.sig_day = 3;
    g.sig_bits = 0;
    ash_radio_sync_day(&g);
    assert(g.sig_day == 4);
    assert(ash_radio_listens(&g) == 0);
    assert(ash_radio_can_listen(&g));

    g.sig_bits = (uint16_t)((2u << 10) | (1u << 12) | (2u << 0) | (1u << 8));
    g.day_key = 5;
    ash_radio_sync_day(&g);
    assert(g.sig_day == 5);
    assert(ash_radio_listens(&g) == 0);
    assert(ash_radio_clarity(&g, 0) == 2);
    assert(ash_radio_resonance(&g) == 1);
}

static void test_target_and_signal(void)
{
    for (uint16_t d = 1; d <= 20; d++) {
        for (uint8_t b = 0; b < ASH_RADIO_BANDS; b++) {
            uint8_t t = ash_radio_target(d, b);
            assert(t < ASH_RADIO_NOTCHES);
            assert(ash_radio_signal(d, b, t) == 4);
            assert(ash_radio_signal(d, b, (uint8_t)((t + 1) % ASH_RADIO_NOTCHES)) >= 3 ||
                   t == ASH_RADIO_NOTCHES - 1);
        }
    }
    assert(ash_radio_signal(1, 0, ASH_RADIO_NOTCHES) == 0);
}

static void test_clarity_needs_window(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 8;
    ash_radio_sync_day(&g);
    assert(ash_radio_clarity(&g, 1) == 0);
    /* Two listens in one day cap at two; out-of-window never progress. */
    ash_radio_mark_listen(&g, 1, true, false);
    assert(ash_radio_listens(&g) == 1);
    assert(ash_radio_clarity(&g, 1) == 0);
    ash_radio_mark_listen(&g, 1, true, true);
    assert(ash_radio_listens(&g) == 2);
    assert(ash_radio_clarity(&g, 1) == 0); /* only one in-window success */
    ash_radio_mark_listen(&g, 1, true, true);
    assert(ash_radio_listens(&g) == 2);
    /* New day: third success completes clarity. */
    g.day_key = 9;
    ash_radio_sync_day(&g);
    ash_radio_mark_listen(&g, 1, true, true);
    assert(ash_radio_clarity(&g, 1) == 0);
    g.day_key = 10;
    ash_radio_sync_day(&g);
    ash_radio_mark_listen(&g, 1, true, true);
    assert(ash_radio_clarity(&g, 1) == 1);
}

static void test_apply(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.day_key = 2;
    g.pop = 1;
    g.person[0].state = ASH_PERSON_HEALTHY;
    g.parts = 0;
    g.threat = 1;
    g.person[0].trust = -20;
    g.sig_bits = 0;
    ash_radio_apply(&g, ASH_RADIO_LANDMARK);
    assert(g.parts == 1);
    ash_radio_apply(&g, ASH_RADIO_VOICE);
    assert(g.person[0].trust == -19);
    ash_radio_apply(&g, ASH_RADIO_BLANK);
    assert(ash_radio_resonance(&g) == 1);
    assert(g.threat == 0);
    for (int i = 0; i < 5; i++) ash_radio_apply(&g, ASH_RADIO_BLANK);
    assert(ash_radio_resonance(&g) == 3);
    assert(g.threat == 0);
    for (uint8_t b = 0; b < ASH_RADIO_BANDS; b++) {
        ash_radio_kind_t k = ash_radio_result_kind(&g, b);
        assert(k < ASH_RADIO_KIND_COUNT);
        assert(ash_radio_result_title(k)[0]);
        assert(ash_radio_result_body(k)[0]);
        assert(ash_radio_result_effect(k)[0]);
    }
}

int main(void)
{
    test_window_and_day_sync();
    test_target_and_signal();
    test_clarity_needs_window();
    test_apply();
    puts("test_ashbox_radio: PASS");
    return 0;
}
