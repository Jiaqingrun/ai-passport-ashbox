#include "ashbox_radio.h"

#include <stddef.h>

static const ash_radio_band_t k_bands[ASH_RADIO_BANDS] = {
    {"近频", "87.6"},
    {"旧城", "102.3"},
    {"远波", "144.8"},
    {"灰带", "156.0"},
};

static const char *const k_titles[ASH_RADIO_KIND_COUNT] = {
    "旧闻", "地标", "人声", "空白",
};
static const char *const k_bodies[ASH_RADIO_KIND_COUNT] = {
    "有人在念一份名单。\n念到一半，信号断了。\n名单上没有你的名字。",
    "北边的水塔还亮着。\n塔下有人堆了新的柴。\n明天可以往那边走。",
    "一个女声在问：还有人吗？\n她重复了三遍。\n你按住发话键，没说话。",
    "只有电流声。\n像雨，又像很远的地方在烧东西。\n你听了很久。",
};
static const char *const k_effects[ASH_RADIO_KIND_COUNT] = {
    "记下一段旧闻", "零件 +1", "交情 +1", "共振 +1",
};

#define CLAR(g, b) ((uint8_t)(((g)->sig_bits >> (2u * (b))) & 3u))
#define SET_CLAR(g, b, v)                                                      \
    do {                                                                       \
        (g)->sig_bits = (uint16_t)(((g)->sig_bits) &                         \
                                   ~(3u << (2u * (b))));                      \
        (g)->sig_bits = (uint16_t)(((g)->sig_bits) | ((uint16_t)(v) <<        \
                                                     (2u * (b))));           \
    } while (0)
#define RES(g) ((uint8_t)(((g)->sig_bits >> 8) & 3u))
#define LISTENS(g) ((uint8_t)(((g)->sig_bits >> 10) & 3u))
#define PROG(g) ((uint8_t)(((g)->sig_bits >> 12) & 3u))

const ash_radio_band_t *ash_radio_band(uint8_t band)
{
    return band < ASH_RADIO_BANDS ? &k_bands[band] : &k_bands[0];
}

void ash_radio_sync_day(ash_game_t *g)
{
    if (!g) return;
    uint16_t d = (uint16_t)g->day_key;
    if (g->sig_day == d) return;
    g->sig_day = d;
    /* Only the daily listen count resets; clarity progress carries over. */
    g->sig_bits = (uint16_t)(g->sig_bits & ~(3u << 10));
}

bool ash_radio_window_open(int hour)
{
    if (hour < 0) return false;
    return (hour >= 19 || hour < 1) || (hour >= 9 && hour < 15);
}

uint8_t ash_radio_listens(const ash_game_t *g)
{
    return g ? LISTENS(g) : ASH_RADIO_DAILY;
}

uint8_t ash_radio_clarity(const ash_game_t *g, uint8_t band)
{
    if (!g || band >= ASH_RADIO_BANDS) return 0;
    return CLAR(g, band);
}

uint8_t ash_radio_resonance(const ash_game_t *g)
{
    return g ? RES(g) : 0;
}

uint8_t ash_radio_progress(const ash_game_t *g)
{
    return g ? PROG(g) : 0;
}

uint8_t ash_radio_target(uint16_t day, uint8_t band)
{
    return (uint8_t)((day * 13u + band * 5u + 3u) % ASH_RADIO_NOTCHES);
}

uint8_t ash_radio_signal(uint16_t day, uint8_t band, uint8_t pos)
{
    if (pos >= ASH_RADIO_NOTCHES) return 0;
    int target = (int)ash_radio_target(day, band);
    int dist = (int)pos - target;
    if (dist < 0) dist = -dist;
    int s = 4 - dist;
    if (s < 1) s = 1;
    return (uint8_t)s;
}

bool ash_radio_can_listen(const ash_game_t *g)
{
    return g && LISTENS(g) < ASH_RADIO_DAILY;
}

void ash_radio_mark_listen(ash_game_t *g, uint8_t band, bool success, bool in_window)
{
    if (!g || band >= ASH_RADIO_BANDS) return;
    ash_radio_sync_day(g);
    if (LISTENS(g) >= ASH_RADIO_DAILY) return;
    uint8_t n = (uint8_t)(LISTENS(g) + 1u);
    if (n > ASH_RADIO_DAILY) n = ASH_RADIO_DAILY;
    g->sig_bits = (uint16_t)((g->sig_bits & ~(3u << 10)) | ((uint16_t)n << 10));
    if (!success || !in_window) return;
    uint8_t p = (uint8_t)(PROG(g) + 1u);
    if (p >= 3u) {
        uint8_t c = CLAR(g, band);
        if (c < ASH_RADIO_CLARITY_MAX) SET_CLAR(g, band, (uint8_t)(c + 1u));
        p = 0;
    }
    g->sig_bits = (uint16_t)((g->sig_bits & ~(3u << 12)) | ((uint16_t)p << 12));
}

ash_radio_kind_t ash_radio_result_kind(const ash_game_t *g, uint8_t band)
{
    if (!g || band >= ASH_RADIO_BANDS) return ASH_RADIO_BLANK;
    uint8_t c = CLAR(g, band);
    return (ash_radio_kind_t)(((uint16_t)g->day_key * 3u + band * 5u + c * 2u) % 4u);
}

const char *ash_radio_result_title(ash_radio_kind_t kind)
{
    return kind < ASH_RADIO_KIND_COUNT ? k_titles[kind] : k_titles[ASH_RADIO_BLANK];
}

const char *ash_radio_result_body(ash_radio_kind_t kind)
{
    return kind < ASH_RADIO_KIND_COUNT ? k_bodies[kind] : k_bodies[ASH_RADIO_BLANK];
}

const char *ash_radio_result_effect(ash_radio_kind_t kind)
{
    return kind < ASH_RADIO_KIND_COUNT ? k_effects[kind] : k_effects[ASH_RADIO_BLANK];
}

void ash_radio_apply(ash_game_t *g, ash_radio_kind_t kind)
{
    if (!g) return;
    switch (kind) {
    case ASH_RADIO_LANDMARK:
        if (g->parts < ASHBOX_PARTS_CAP) g->parts++;
        break;
    case ASH_RADIO_VOICE:
        for (uint8_t i = 0; i < g->pop && i < ASHBOX_PERSON_MAX; i++) {
            if (g->person[i].state == ASH_PERSON_AWAY) continue;
            if (g->person[i].trust < 20) g->person[i].trust++;
        }
        break;
    case ASH_RADIO_BLANK: {
        uint8_t r = RES(g);
        if (r < 3) g->sig_bits = (uint16_t)((g->sig_bits & ~(3u << 8)) |
                                            ((uint16_t)(r + 1u) << 8));
        if (g->threat > 0) g->threat--;
        break;
    }
    default:
        break;
    }
}
