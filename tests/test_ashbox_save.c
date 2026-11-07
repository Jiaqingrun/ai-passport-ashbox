#include "ashbox_save.h"
#include "nvs_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t crc32_bytes(const uint8_t *data, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~c;
}

/* CRC field sits at `crc_off` (may be before trailing struct padding). */
static void seal_legacy(uint8_t *blob, size_t len, size_t crc_off)
{
    assert(len >= 8 && crc_off + 4 <= len);
    memset(blob + crc_off, 0, 4);
    uint32_t crc = crc32_bytes(blob, len);
    memcpy(blob + crc_off, &crc, 4);
}

static void test_decode_v11_roundtrip(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.food = 33; /* under post-migrate clamp_economy cap */
    g.iron = 3;
    g.med_stim = 1;
    g.day_latch_trade = 12;
    g.crc = ash_game_crc(&g);

    ash_game_t out;
    memset(&out, 0xA5, sizeof(out));
    assert(ashbox_save_decode((const uint8_t *)&g, sizeof(g), &out) == ESP_OK);
    assert(out.ver == ASHBOX_SAVE_VER);
    assert(out.food == 33);
    assert(out.iron == 3);
    assert(out.med_stim == 1);
    assert(out.day_latch_trade == 12);
}

static void test_migrate_v10_widens_day_latches(void)
{
    ash_game_t cur;
    ash_game_new(&cur);
    cur.food = 41;
    cur.iron = 7;
    cur.day_key = 257;
    cur.trade_day = 1; /* low8 of 257 — legacy collision day */
    cur.ember_trade_day = 0; /* day-low8 0 must remain day 0 after widen, not never */
    cur.duty_day = 1;
    cur.duty_count = 2;

    const size_t v10n = ashbox_save_v10_bytes();
    uint8_t blob[768];
    assert(v10n <= sizeof(blob));
    memset(blob, 0, v10n);
    memcpy(blob, &cur, offsetof(ash_game_t, day_latch_trade));
    uint16_t ver = 10;
    memcpy(blob + sizeof(uint32_t), &ver, sizeof(ver));
    seal_legacy(blob, v10n, offsetof(ash_game_t, day_latch_trade));

    ash_game_t out;
    assert(ashbox_save_decode(blob, v10n, &out) == ESP_OK);
    assert(out.ver == ASHBOX_SAVE_VER);
    assert(out.food == 41);
    assert(out.iron == 7);
    assert(out.day_latch_trade == 257); /* low8 matched day_key → full day */
    assert(out.day_latch_ember == 0);   /* day 0 stays 0, not remapped to never */
    assert(out.day_latch_duty == 257);
    assert(out.crc == ash_game_crc(&out));
}

static void test_migrate_v9_zeros_v10_fields(void)
{
    ash_game_t cur;
    ash_game_new(&cur);
    cur.food = 41;
    cur.water = 40;
    cur.parts = 12;

    const size_t v9n = ashbox_save_v9_bytes();
    uint8_t blob[768];
    assert(v9n <= sizeof(blob));
    memset(blob, 0, v9n);
    memcpy(blob, &cur, offsetof(ash_game_t, iron));
    uint16_t ver = 9;
    memcpy(blob + sizeof(uint32_t), &ver, sizeof(ver));
    seal_legacy(blob, v9n, offsetof(ash_game_t, iron));

    ash_game_t out;
    assert(ashbox_save_decode(blob, v9n, &out) == ESP_OK);
    assert(out.ver == ASHBOX_SAVE_VER);
    assert(out.food == 41);
    assert(out.iron == 0);
    assert(out.crowbar == 0);
    assert(out.med_stim == 0);
    assert(out.med_milk == 0);
    assert(out.med_abx == 0);
    assert(out.backpack == 0);
    assert(out.wanderer_pending == 0);
    assert(out.day_latch_trade == ASHBOX_DAY_LATCH_NEVER);
    assert(out.crc == ash_game_crc(&out));
}

static void test_migrate_v8_seeds_water(void)
{
    ash_game_t cur;
    ash_game_new(&cur);
    cur.food = 40;

    const size_t v8n = ashbox_save_v8_bytes();
    uint8_t blob[768];
    assert(v8n <= sizeof(blob));
    memset(blob, 0, v8n);
    memcpy(blob, &cur, offsetof(ash_game_t, water));
    uint16_t ver = 8;
    memcpy(blob + sizeof(uint32_t), &ver, sizeof(ver));
    seal_legacy(blob, v8n, offsetof(ash_game_t, water));

    ash_game_t out;
    assert(ashbox_save_decode(blob, v8n, &out) == ESP_OK);
    assert(out.ver == ASHBOX_SAVE_VER);
    assert(out.water == 40); /* migrate_from_v8 default */
    assert(out.iron == 0);
}

static void test_bad_crc_rejected(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.crc = ash_game_crc(&g);
    g.crc ^= 1u;
    ash_game_t out;
    assert(ashbox_save_decode((const uint8_t *)&g, sizeof(g), &out) == ESP_ERR_INVALID_CRC);
}

/* B18: corrupt v2 must not silently head-only migrate */
static void test_v2_bad_crc_no_head_fallback(void)
{
    const size_t v3n = ashbox_save_v3_bytes();
    uint8_t blob[768];
    assert(v3n <= sizeof(blob));
    memset(blob, 0x5A, v3n);
    uint32_t magic = ASHBOX_MAGIC;
    uint16_t ver = 2;
    memcpy(blob, &magic, sizeof(magic));
    memcpy(blob + sizeof(magic), &ver, sizeof(ver));
    /* Deliberately wrong CRC at end of v3 layout */
    uint32_t bad = 0xDEADBEEFu;
    memcpy(blob + v3n - 4, &bad, 4);

    ash_game_t out;
    memset(&out, 0, sizeof(out));
    assert(ashbox_save_decode(blob, v3n, &out) == ESP_ERR_INVALID_CRC);

    /* Short / head-sized v2 also rejected (no head-only path). */
    assert(ashbox_save_decode(blob, 16, &out) == ESP_ERR_INVALID_CRC);
}

static void test_pad_v13_seals_crc_gap(void)
{
    assert(sizeof(((ash_game_t *)0)->pad_v12) == 3u);
    assert(offsetof(ash_game_t, pace_era) == offsetof(ash_game_t, pad_v12) + 3u);
    assert(sizeof(((ash_game_t *)0)->pad_v13) == 6u);
    assert(offsetof(ash_game_t, crc) == offsetof(ash_game_t, pad_v13) + 6u);
    assert(offsetof(ash_game_t, crc) + sizeof(uint32_t) == sizeof(ash_game_t));
}

/* Dirty alignment bytes must not reject a logically valid v12 blob. */
static void test_legacy_dirty_pad_still_loads(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.food = 41; /* under dorm_lv=1 clamp_economy cap */
    memset(g.pad_v12, 0, sizeof(g.pad_v12));
    g.crc = 0;
    /* Legacy seal: hash full copy including whatever pad bytes were present. */
    ash_game_t raw = g;
    raw.pad_v12[0] = 0xA5;
    raw.pad_v12[1] = 0x5A;
    raw.pad_v12[2] = 0x3C;
    raw.crc = 0;
    uint32_t legacy = crc32_bytes((const uint8_t *)&raw, sizeof(raw));
    raw.crc = legacy;

    ash_game_t out;
    assert(ashbox_save_decode((const uint8_t *)&raw, sizeof(raw), &out) == ESP_OK);
    assert(out.food == 41);
    assert(out.pad_v12[0] == 0 && out.pad_v12[1] == 0 && out.pad_v12[2] == 0);
    assert(out.pad_v13[0] == 0 && out.pad_v13[1] == 0);
    assert(out.crc == ash_game_crc(&out));
}

static void test_sealed_roundtrip(void)
{
    ash_game_t g;
    ash_game_new(&g);
    g.food = 33; /* under dorm_lv=1 clamp_economy cap */
    g.pop = 2;
    g.pad_v12[0] = 0xFF;
    g.pad_v13[0] = 0xEE;
    g.crc = ash_game_crc(&g); /* crc helper zeros pad */

    ash_game_t out;
    assert(ashbox_save_decode((const uint8_t *)&g, sizeof(g), &out) == ESP_OK);
    assert(out.food == 33);
    assert(out.pad_v12[0] == 0 && out.pad_v12[1] == 0 && out.pad_v12[2] == 0);
    assert(out.pad_v13[0] == 0);
    assert(out.crc == ash_game_crc(&out));
}

static void make_sealed(ash_game_t *g, uint16_t food, uint8_t pop, uint32_t day)
{
    ash_game_new(g);
    g->food = food;
    g->pop = pop;
    g->day_key = day;
    g->last_settle_unix = (int64_t)day * 86400;
    g->magic = ASHBOX_MAGIC;
    g->ver = ASHBOX_SAVE_VER;
    memset(g->pad_v12, 0, sizeof(g->pad_v12));
    memset(g->pad_v13, 0, sizeof(g->pad_v13));
    g->pad_v13a = 0;
    g->crc = ash_game_crc(g);
}

/* O-TEST-P: primary-only store does not touch sv2; mirrored writes both. */
static void test_store_primary_vs_mirrored(void)
{
    nvs_host_reset();
    ash_game_t g;
    make_sealed(&g, 33, 2, 7);

    assert(ashbox_save_store(&g) == ESP_OK);
    assert(nvs_host_has_key(ASHBOX_NVS_SAVE_KEY));
    assert(!nvs_host_has_key(ASHBOX_NVS_SAVE_BAK_KEY));

    ash_game_t loaded;
    assert(ashbox_save_load(&loaded) == ESP_OK);
    assert(loaded.food == 33);
    assert(loaded.day_key == 7);

    g.food = 41;
    g.day_key = 9;
    g.crc = ash_game_crc(&g);
    assert(ashbox_save_store_mirrored(&g) == ESP_OK);
    assert(nvs_host_has_key(ASHBOX_NVS_SAVE_KEY));
    assert(nvs_host_has_key(ASHBOX_NVS_SAVE_BAK_KEY));
}

/* O-TEST-P: torn primary falls back to sv2 and heals primary. */
static void test_load_recovers_from_backup(void)
{
    nvs_host_reset();
    ash_game_t g;
    make_sealed(&g, 25, 3, 12);
    assert(ashbox_save_store_mirrored(&g) == ESP_OK);

    nvs_host_corrupt_key(ASHBOX_NVS_SAVE_KEY);

    ash_game_t out;
    assert(ashbox_save_load(&out) == ESP_OK);
    assert(out.food == 25);
    assert(out.pop == 3);
    assert(out.day_key == 12);

    /* Heal rewrite both slots — primary must decode again. */
    nvs_host_delete_key(ASHBOX_NVS_SAVE_BAK_KEY);
    assert(ashbox_save_load(&out) == ESP_OK);
    assert(out.food == 25);
}

/* O-RESCUE: dump scan prefers higher day_key among valid blobs. */
static void test_scan_dump_picks_newest_day(void)
{
    ash_game_t older, newer;
    make_sealed(&older, 10, 1, 3);
    make_sealed(&newer, 40, 2, 11);

    uint8_t dump[2048];
    memset(dump, 0x3C, sizeof(dump));
    memcpy(dump + 17, &older, sizeof(older));
    memcpy(dump + 900, &newer, sizeof(newer));

    ash_game_t found;
    assert(ashbox_save_scan_dump(dump, sizeof(dump), &found));
    assert(found.day_key == 11);
    assert(found.food == 40);
}

/* O-RESCUE: garbage + bad CRC ignored; only sealed blob wins. */
static void test_scan_dump_ignores_corrupt(void)
{
    ash_game_t good, bad;
    make_sealed(&good, 18, 2, 5);
    make_sealed(&bad, 99, 3, 99);
    bad.crc ^= 1u;

    uint8_t dump[1600];
    memset(dump, 0xA5, sizeof(dump));
    memcpy(dump + 40, &bad, sizeof(bad));
    memcpy(dump + 800, &good, sizeof(good));

    ash_game_t found;
    assert(ashbox_save_scan_dump(dump, sizeof(dump), &found));
    assert(found.day_key == 5);
    assert(found.food == 18);
}

/* O-RESCUE: init with NO_FREE_PAGES dumps partition, restores mirrored save. */
static void test_init_rescues_from_partition_dump(void)
{
    nvs_host_reset();
    ash_game_t g;
    make_sealed(&g, 37, 3, 15);

    uint8_t dump[2048];
    memset(dump, 0, sizeof(dump));
    memcpy(dump + 64, &g, sizeof(g));
    nvs_host_set_partition_image(dump, sizeof(dump));
    nvs_host_force_no_free_pages_once();

    assert(ashbox_save_init() == ESP_OK);
    assert(nvs_host_has_key(ASHBOX_NVS_SAVE_KEY));
    assert(nvs_host_has_key(ASHBOX_NVS_SAVE_BAK_KEY));

    ash_game_t out;
    assert(ashbox_save_load(&out) == ESP_OK);
    assert(out.food == 37);
    assert(out.day_key == 15);
}

int main(void)
{
    assert(sizeof(ash_game_t) == ASHBOX_SAVE_BLOB_SIZE);
    assert(ashbox_save_v9_bytes() == offsetof(ash_game_t, iron) + sizeof(uint32_t) ||
           ashbox_save_v9_bytes() >= offsetof(ash_game_t, iron) + sizeof(uint32_t));
    test_pad_v13_seals_crc_gap();
    test_decode_v11_roundtrip();
    test_migrate_v10_widens_day_latches();
    test_migrate_v9_zeros_v10_fields();
    test_migrate_v8_seeds_water();
    test_bad_crc_rejected();
    test_v2_bad_crc_no_head_fallback();
    test_legacy_dirty_pad_still_loads();
    test_sealed_roundtrip();
    test_store_primary_vs_mirrored();
    test_load_recovers_from_backup();
    test_scan_dump_picks_newest_day();
    test_scan_dump_ignores_corrupt();
    test_init_rescues_from_partition_dump();
    printf("test_ashbox_save: PASS\n");
    return 0;
}
