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

/*
 * F-TEST-V12: frozen 744-byte ASH2 version-12 blob, written as literal bytes
 * so it never depends on today's ash_game_t layout. It is a plausible
 * mid-game state: day 37, six survivors, a hunt expedition in flight,
 * water/iron/well progress, late-game super-core unlocks, and two non-zero
 * day latches (trade=36, duty=37).
 *
 * CRC 0xE28BACD0 is CRC-32/ISO-HDLC (reflected poly 0xEDB88320, init/final
 * 0xFFFFFFFF) over these 744 bytes with the trailing crc field zeroed and
 * pad_v12 zeroed -- exactly the pre-seal normalization ashbox_save_decode()
 * performs for v12. It was computed once offline from the frozen bytes below
 * and inlined; decode re-verifies it.
 *
 * pad_v12 is deliberately dirty (A5 5A 3C) while the CRC was hashed with it
 * zeroed: that models an old seal which normalized the pad for hashing but did
 * not clear the stored copy, and the load path must clear it.
 */
static const uint8_t k_v12_blob[744] = {
    /* 0x000 */ 0x32,0x41,0x53,0x48,0x0C,0x00,0x2E,0x00,0x49,0x00,0x06,0x00,
    /* 0x00C */ 0x02,0x02,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x018 */ 0x00,0x00,0x00,0x00,0x02,0x0F,0x00,0x00,0x01,0x00,0x00,0x00,
    /* 0x024 */ 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x37,0x00,0x00,
    /* 0x030 */ 0x02,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x03C */ 0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    /* 0x048 */ 0x8C,0x0A,0x00,0x00,0x20,0x1C,0x00,0x00,0x02,0x00,0x00,0x00,
    /* 0x054 */ 0x03,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x2A,0xFF,0x00,0x00,
    /* 0x060 */ 0x25,0x00,0x00,0x00,0x0E,0x00,0x00,0x00,0x00,0x78,0xE7,0x68,
    /* 0x06C */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x078 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x084 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x090 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x09C */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x0A8 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x0B4 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x0C0 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x0CC */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x0D8 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x01,
    /* 0x0E4 */ 0x01,0x00,0x0A,0x00,0x00,0xE9,0x93,0x81,0xE7,0x9A,0xAE,0xE5,
    /* 0x0F0 */ 0xA2,0x99,0xE6,0x95,0xB2,0xE5,0xA3,0xB0,0xE8,0xA2,0xAB,0xE8,
    /* 0x0FC */ 0xAE,0xB0,0xE8,0xBF,0x9B,0xE5,0x8C,0xA3,0x00,0x00,0x00,0x00,
    /* 0x108 */ 0x00,0x00,0x00,0x00,0x00,0xE7,0x81,0xB6,0xE9,0x87,0x8C,0xE8,
    /* 0x114 */ 0xBF,0x98,0xE6,0x9C,0x89,0xE4,0xBD,0x99,0xE6,0xB8,0xA9,0x00,
    /* 0x120 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x12C */ 0x00,0x00,0x00,0x00,0x00,0xE4,0xBB,0x93,0xE8,0xB4,0xA6,0xE5,
    /* 0x138 */ 0xAF,0xB9,0xE8,0xBF,0x87,0x20,0xC2,0xB7,0x20,0xE5,0x85,0x88,
    /* 0x144 */ 0xE6,0x94,0xBE,0xE4,0xB8,0x8B,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x150 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x15C */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x168 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x174 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x180 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x18C */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x198 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1A4 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1B0 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1BC */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1C8 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1D4 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1E0 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1EC */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x1F8 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x204 */ 0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x02,0x02,0x07,0x03,0x01,
    /* 0x210 */ 0x01,0x02,0x1E,0x00,0x0C,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    /* 0x21C */ 0x10,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x228 */ 0x25,0x02,0x03,0x00,0x02,0x01,0x25,0x01,0x01,0x04,0xFF,0x02,
    /* 0x234 */ 0x03,0x01,0x02,0x01,0x01,0x00,0xFF,0xFF,0x01,0x01,0x02,0x00,
    /* 0x240 */ 0x23,0x00,0x08,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x24C */ 0x00,0x00,0x00,0x00,0x30,0x00,0x01,0x01,0x01,0x01,0x02,0x01,
    /* 0x258 */ 0x00,0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
    /* 0x264 */ 0x00,0x00,0x00,0x00,0x01,0x01,0x00,0x01,0x10,0x0E,0x00,0x00,
    /* 0x270 */ 0xFF,0x00,0x01,0x03,0x24,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 0x27C */ 0x25,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x03,0x00,0x00,0x00,
    /* 0x288 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x14,0x00,0x00,
    /* 0x294 */ 0x04,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x2A0 */ 0x00,0x3C,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x2AC */ 0x00,0x00,0x00,0x00,0xFF,0x05,0x00,0x00,0x00,0x01,0x00,0x01,
    /* 0x2B8 */ 0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x20,0x1C,0x00,0x00,
    /* 0x2C4 */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x01,
    /* 0x2D0 */ 0x02,0x00,0x00,0x01,0x01,0x00,0x02,0x32,0x00,0x04,0x00,0x00,
    /* 0x2DC */ 0x05,0x00,0x00,0x00,0x00,0xA5,0x5A,0x3C,0xD0,0xAC,0x8B,0xE2,
};

/* F-TEST-V12: the frozen image decodes through migrate_from_v12() (v12->v13). */
static void test_migrate_v12_frozen_blob(void)
{
    assert(sizeof(k_v12_blob) == 744u);
    assert(offsetof(ash_game_t, pace_era) == sizeof(k_v12_blob) - sizeof(uint32_t));

    uint32_t frozen_crc = 0;
    memcpy(&frozen_crc, k_v12_blob + sizeof(k_v12_blob) - sizeof(frozen_crc),
           sizeof(frozen_crc));
    assert(frozen_crc == 0xE28BACD0u); /* inlined CRC documented above */

    const size_t ver_off = offsetof(ash_game_t, ver);
    const size_t journal_off = offsetof(ash_game_t, journal);
    const size_t journal_len = sizeof(((ash_game_t *)0)->journal[0]);
    const size_t pad_off = offsetof(ash_game_t, pad_v12);
    const size_t prefix_end = offsetof(ash_game_t, pace_era);

    /* Dirty fixture pad proves the load path, not the fixture, zeroes it. */
    assert(k_v12_blob[pad_off + 0] == 0xA5u);
    assert(k_v12_blob[pad_off + 1] == 0x5Au);
    assert(k_v12_blob[pad_off + 2] == 0x3Cu);

    uint8_t raw[sizeof(k_v12_blob)];
    memcpy(raw, k_v12_blob, sizeof(raw));

    ash_game_t out;
    memset(&out, 0xA5, sizeof(out));
    /* A rejected v12 blob would fall back to rescue/backup/new game. */
    assert(ashbox_save_decode(raw, sizeof(raw), &out) == ESP_OK);
    assert(out.magic == ASHBOX_MAGIC);
    assert(out.ver == ASHBOX_SAVE_VER);

    /* Spot-check that the frozen bytes land in the intended v12 fields. */
    assert(out.food == 46u);
    assert(out.parts == 73u);
    assert(out.ember == 6u);
    assert(out.dorm_lv == 2u && out.kitchen_lv == 2u && out.workshop_lv == 2u);
    assert(out.gate_unlocked == 1u);
    assert(out.day_key == 37u);
    assert(out.pop == 6u);
    assert(out.person[0].id == 0u && out.person[0].state == ASH_PERSON_HEALTHY);
    assert(out.person[0].trust == 2 && out.person[0].fatigue == 15u);
    assert(out.person[2].state == ASH_PERSON_AWAY);
    assert(out.person_ext[0].id == 3u);
    assert(out.person_ext[1].id == 4u && out.person_ext[1].state == ASH_PERSON_WEAK);
    assert(out.person_ext[2].trust == -1);
    assert(out.exp.active && out.exp.tier == ASH_TIER_HUNT);
    assert(out.exp.loadout == ASH_LOAD_PARTS);
    assert(out.exp.remaining_s == 2700u && out.exp.duration_s == 7200u);
    assert(out.threat == 42u);
    assert(out.expedition_total == 14u);
    assert(out.last_settle_unix == 1760000000);
    assert(out.day_latch_trade == 36u && out.day_latch_duty == 37u);
    assert(out.day_latch_abx == ASHBOX_DAY_LATCH_NEVER);
    assert(out.water == 35u && out.stash_water == 8u);
    assert(out.iron == 48u && out.well_lv == ASHBOX_WELL_LV_WELL);
    assert(out.build_cap_bonus == 2u && out.loot_bonus_perm_pct == 50u);
    assert(out.farm_plots_unlocked == ASHBOX_FARM_PLOTS_MAX);
    assert(out.person_job[0] == ASHBOX_JOB_FARM);
    assert(out.person_job[1] == ASHBOX_JOB_WORK);

    /* Every v12 byte through pace_era survives byte-for-byte except the three
     * intentional migration writes: ver is stamped to 13, pad_v12 is cleared,
     * and migrate_from_v12() appends one journal notice (journal,
     * journal_head, journal_count). */
    assert(memcmp(&out, k_v12_blob, ver_off) == 0);
    assert(memcmp((const uint8_t *)&out + ver_off + sizeof(out.ver),
                  k_v12_blob + ver_off + sizeof(out.ver),
                  journal_off - (ver_off + sizeof(out.ver))) == 0);

    /* Old journal entries survive; exactly the old head slot gains the notice. */
    const uint8_t *frozen_journal = k_v12_blob + journal_off;
    assert(memcmp(out.journal[0], frozen_journal, 3u * journal_len) == 0);
    assert(memcmp(out.journal[4], frozen_journal + 4u * journal_len,
                  4u * journal_len) == 0);
    assert(memcmp(out.journal[3], frozen_journal + 3u * journal_len,
                  journal_len) != 0);
    assert(out.journal[3][0] != '\0');
    assert(out.journal_head == 4u);
    assert(out.journal_count == 4u);

    /* Fields after the journal keep their v12 values (pad handled separately). */
    const size_t tail_off = offsetof(ash_game_t, night_order);
    assert(tail_off == journal_off + sizeof(out.journal) + 2u);
    assert(pad_off + sizeof(out.pad_v12) == prefix_end);
    assert(memcmp((const uint8_t *)&out + tail_off, k_v12_blob + tail_off,
                  pad_off - tail_off) == 0);
    assert(out.pad_v12[0] == 0u && out.pad_v12[1] == 0u && out.pad_v12[2] == 0u);

    /* v13-only fields get exactly migrate_from_v12()'s defaults. */
    assert(out.pace_era == ASHBOX_PACE_LEGACY);
    assert(out.wanderer_trait == ASHBOX_TRAIT_NONE);
    assert(out.wanderer_fired == 0u);
    assert(out.pad_v13a == 0u);
    for (int i = 0; i < ASHBOX_QUEST_N; i++) {
        assert(out.quest_beat_day[i] == ASHBOX_DAY_LATCH_NEVER);
    }
    assert(out.wanderer_sched_day == 0u);
    assert(out.wanderer_t0_min == 0u);
    assert(out.wanderer_t1_min == 0u);
    for (int i = 0; i < ASHBOX_PERSON_MAX; i++) assert(out.person_trait[i] == 0u);
    for (int i = 0; i < ASHBOX_FARM_PLOTS_MAX; i++) assert(out.farm_labor_h[i] == 0u);
    for (size_t i = 0; i < sizeof(out.pad_v13); i++) assert(out.pad_v13[i] == 0u);
    assert(out.crc == ash_game_crc(&out));
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
    test_migrate_v12_frozen_blob();
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
