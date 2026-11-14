#include "ashbox_ambience.h"
#include "ashbox_digest.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_day_phase_and_tint(void)
{
    ash_ambience_t a;
    ash_ambience_for(6, 0, 4, &a);
    assert(a.phase == ASH_AMB_DAWN);
    assert(a.tint_opa == 50);

    ash_ambience_for(12, 0, 4, &a);
    assert(a.phase == ASH_AMB_DAY);
    assert(a.tint_opa == 8);

    ash_ambience_for(18, 0, 4, &a);
    assert(a.phase == ASH_AMB_DUSK);
    assert(a.tint_opa == 60);

    ash_ambience_for(23, 0, 4, &a);
    assert(a.phase == ASH_AMB_NIGHT);
    assert(a.tint_opa == 95);

    /* Wrap-around must normalise, not index out of bounds. */
    ash_ambience_for(-1, 0, 4, &a);
    assert(a.phase == ASH_AMB_NIGHT);
    ash_ambience_for(28, 0, 4, &a);
    assert(a.phase == ASH_AMB_NIGHT);
    assert(strcmp(ash_ambience_phase_name(a.phase), "夜里") == 0);
}

static void test_threat_vignette_and_embers(void)
{
    ash_ambience_t a;
    ash_ambience_for(12, 49, 0, &a);
    assert(a.vignette == 0);
    assert(a.ember_count == 6);

    ash_ambience_for(12, 50, 5, &a);
    assert(a.vignette == 55);
    assert(a.ember_count == 10);

    ash_ambience_for(12, 80, 10, &a);
    assert(a.vignette == 90);
    assert(a.ember_count == 14);

    ash_ambience_for(12, 200, 99, &a);
    assert(a.vignette == 90);
    assert(a.ember_count == 14);
}

static ash_digest_snap_t snap_at(int64_t unix_s)
{
    ash_digest_snap_t s;
    memset(&s, 0, sizeof(s));
    s.magic = ASH_DIGEST_MAGIC;
    s.ver = ASH_DIGEST_VER;
    s.bytes = (uint16_t)sizeof(s);
    s.unix = unix_s;
    s.food = 100;
    s.parts = 40;
    s.iron = 2;
    s.water = 20;
    s.ember = 1;
    s.threat = 10;
    s.pop = 3;
    return s;
}

static void test_digest_rejects_stale_or_bad(void)
{
    /* The wire struct is shared with NVS; keep it at one 32-byte blob. */
    assert(sizeof(ash_digest_snap_t) == 32);
    const int64_t base = 1700000000LL;
    ash_digest_snap_t s = snap_at(base);
    ash_digest_delta_t d;

    assert(!ash_digest_calc(&s, base + 60, 1, 1, 1, 1, 1, 1, &d));
    assert(!ash_digest_calc(&s, base + ASH_DIGEST_MAX_AWAY_S + 1, 1, 1, 1, 1, 1, 1, &d));
    assert(!ash_digest_calc(&s, base - 1, 1, 1, 1, 1, 1, 1, &d));

    s.magic = 0;
    assert(!ash_digest_calc(&s, base + 3600, 1, 1, 1, 1, 1, 1, &d));
    s = snap_at(base);
    s.bytes = 3;
    assert(!ash_digest_calc(&s, base + 3600, 1, 1, 1, 1, 1, 1, &d));
    s = snap_at(base);
    s.unix = 100; /* epoch clock: away is far past the 3-day cap */
    assert(!ash_digest_calc(&s, base + 3600, 1, 1, 1, 1, 1, 1, &d));

    /* A device without a set RTC runs on epoch+uptime; a 1 h wait is still valid. */
    s = snap_at(100000);
    assert(ash_digest_calc(&s, 100000 + 3600, 1, 1, 1, 1, 1, 1, &d));
    assert(d.away_s == 3600);
}

static void test_digest_signed_net(void)
{
    const int64_t base = 1700000000LL;
    ash_digest_snap_t s = snap_at(base);
    ash_digest_delta_t d;
    /* Food was eaten, parts produced, water rose, threat rose. */
    assert(ash_digest_calc(&s, base + 7 * 3600 + 12 * 60, 88, 52, 3, 26, 1, 12, &d));
    assert(d.away_s == 7 * 3600 + 12 * 60);
    assert(d.food == -12);
    assert(d.parts == 12);
    assert(d.iron == 1);
    assert(d.water == 6);
    assert(d.ember == 0);
    assert(d.threat == 2);
}

int main(void)
{
    test_day_phase_and_tint();
    test_threat_vignette_and_embers();
    test_digest_rejects_stale_or_bad();
    test_digest_signed_net();
    puts("test_ashbox_m1: PASS");
    return 0;
}
