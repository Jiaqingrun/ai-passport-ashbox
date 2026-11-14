#include "ashbox_phenom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int utf8_len(const char *s)
{
    int n = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    }
    return n;
}

/* M2: one full 16-day cycle covers every id exactly once, in a fixed order. */
static void test_cycle_is_a_permutation(void)
{
    unsigned seen[ASH_PHENOM_COUNT] = {0};
    for (uint32_t day = 1; day <= ASH_PHENOM_COUNT; day++) {
        const ash_phenom_t *p = ash_phenom_for_day(day);
        assert(p != NULL);
        assert(p->id < ASH_PHENOM_COUNT);
        seen[p->id]++;
    }
    for (int i = 0; i < ASH_PHENOM_COUNT; i++) assert(seen[i] == 1);
}

/* M2: no reload/reboot reroll, and the cycle boundary is not a repeat. */
static void test_deterministic_and_no_boundary_repeat(void)
{
    for (uint32_t day = 1; day <= 32; day++) {
        const ash_phenom_t *a = ash_phenom_for_day(day);
        const ash_phenom_t *b = ash_phenom_for_day(day);
        assert(a == b);
        assert(a->id == b->id);
        const ash_phenom_t *next = ash_phenom_for_day(day + 1);
        assert(a->id != next->id);
    }
    assert(ash_phenom_for_day(1) == ash_phenom_for_day(17));
    assert(ash_phenom_for_day(3) == ash_phenom_for_day(19));
}

static void test_lookup_bounds_and_day_zero(void)
{
    for (uint8_t id = 0; id < ASH_PHENOM_COUNT; id++) {
        const ash_phenom_t *p = ash_phenom_by_id(id);
        assert(p != NULL);
        assert(p->id == id);
    }
    assert(ash_phenom_by_id(ASH_PHENOM_COUNT) == NULL);
    assert(ash_phenom_for_day(0) != NULL); /* epoch/unknown day still renders */
}

static void test_table_text_and_effects(void)
{
    bool danger_seen = false;
    for (uint8_t id = 0; id < ASH_PHENOM_COUNT; id++) {
        const ash_phenom_t *p = ash_phenom_by_id(id);
        assert(p->name && p->name[0]);
        assert(p->effect && p->effect[0]);
        assert(p->advice && p->advice[0]);
        assert(utf8_len(p->name) == 2); /* the stage chip is only 52 px wide */
        assert(utf8_len(p->advice) >= 2);
        assert(p->water >= -3 && p->water <= 3);
        assert(p->food >= -2 && p->food <= 2);
        assert(p->parts >= -2 && p->parts <= 2);
        assert(p->threat >= -8 && p->threat <= 15);
        if (p->danger) {
            danger_seen = true;
            assert(p->id == 7 || p->id == 8 || p->id == 14);
        }
    }
    assert(danger_seen);
}

int main(void)
{
    test_cycle_is_a_permutation();
    test_deterministic_and_no_boundary_repeat();
    test_lookup_bounds_and_day_zero();
    test_table_text_and_effects();
    puts("test_ashbox_phenom: OK");
    return 0;
}
