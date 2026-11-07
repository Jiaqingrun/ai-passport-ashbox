#include "ashbox_sprites.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ASH_SPR_W 240
#define ASH_SPR_H 176
#define ASH_SPR_PX (ASH_SPR_W * ASH_SPR_H)
#define ASH_SPR_MAGIC 0x31525341u /* 'ASR1' LE */

typedef struct {
    const uint8_t *start;
    const uint8_t *end;
} ash_spr_blob_t;

#define ASH_SPR_BLOB(sym)                                                      \
    {                                                                          \
        sym##_start, sym##_end                                                 \
    }

/* Embedded by CMake target_add_binary_data(…/sprites_rle/<name>.bin). */
extern const uint8_t idle_f0_bin_start[] asm("_binary_idle_f0_bin_start");
extern const uint8_t idle_f0_bin_end[] asm("_binary_idle_f0_bin_end");
extern const uint8_t idle_f1_bin_start[] asm("_binary_idle_f1_bin_start");
extern const uint8_t idle_f1_bin_end[] asm("_binary_idle_f1_bin_end");
extern const uint8_t idle_f2_bin_start[] asm("_binary_idle_f2_bin_start");
extern const uint8_t idle_f2_bin_end[] asm("_binary_idle_f2_bin_end");

extern const uint8_t away_f0_bin_start[] asm("_binary_away_f0_bin_start");
extern const uint8_t away_f0_bin_end[] asm("_binary_away_f0_bin_end");
extern const uint8_t away_f1_bin_start[] asm("_binary_away_f1_bin_start");
extern const uint8_t away_f1_bin_end[] asm("_binary_away_f1_bin_end");
extern const uint8_t away_f2_bin_start[] asm("_binary_away_f2_bin_start");
extern const uint8_t away_f2_bin_end[] asm("_binary_away_f2_bin_end");

extern const uint8_t weak_f0_bin_start[] asm("_binary_weak_f0_bin_start");
extern const uint8_t weak_f0_bin_end[] asm("_binary_weak_f0_bin_end");
extern const uint8_t weak_f1_bin_start[] asm("_binary_weak_f1_bin_start");
extern const uint8_t weak_f1_bin_end[] asm("_binary_weak_f1_bin_end");
extern const uint8_t weak_f2_bin_start[] asm("_binary_weak_f2_bin_start");
extern const uint8_t weak_f2_bin_end[] asm("_binary_weak_f2_bin_end");

extern const uint8_t threat_f0_bin_start[] asm("_binary_threat_f0_bin_start");
extern const uint8_t threat_f0_bin_end[] asm("_binary_threat_f0_bin_end");
extern const uint8_t threat_f1_bin_start[] asm("_binary_threat_f1_bin_start");
extern const uint8_t threat_f1_bin_end[] asm("_binary_threat_f1_bin_end");
extern const uint8_t threat_f2_bin_start[] asm("_binary_threat_f2_bin_start");
extern const uint8_t threat_f2_bin_end[] asm("_binary_threat_f2_bin_end");

extern const uint8_t return_f0_bin_start[] asm("_binary_return_f0_bin_start");
extern const uint8_t return_f0_bin_end[] asm("_binary_return_f0_bin_end");
extern const uint8_t return_f1_bin_start[] asm("_binary_return_f1_bin_start");
extern const uint8_t return_f1_bin_end[] asm("_binary_return_f1_bin_end");
extern const uint8_t return_f2_bin_start[] asm("_binary_return_f2_bin_start");
extern const uint8_t return_f2_bin_end[] asm("_binary_return_f2_bin_end");

extern const uint8_t night_f0_bin_start[] asm("_binary_night_f0_bin_start");
extern const uint8_t night_f0_bin_end[] asm("_binary_night_f0_bin_end");
extern const uint8_t night_f1_bin_start[] asm("_binary_night_f1_bin_start");
extern const uint8_t night_f1_bin_end[] asm("_binary_night_f1_bin_end");
extern const uint8_t night_f2_bin_start[] asm("_binary_night_f2_bin_start");
extern const uint8_t night_f2_bin_end[] asm("_binary_night_f2_bin_end");

extern const uint8_t gate_f0_bin_start[] asm("_binary_gate_f0_bin_start");
extern const uint8_t gate_f0_bin_end[] asm("_binary_gate_f0_bin_end");
extern const uint8_t gate_f1_bin_start[] asm("_binary_gate_f1_bin_start");
extern const uint8_t gate_f1_bin_end[] asm("_binary_gate_f1_bin_end");
extern const uint8_t gate_f2_bin_start[] asm("_binary_gate_f2_bin_start");
extern const uint8_t gate_f2_bin_end[] asm("_binary_gate_f2_bin_end");

extern const uint8_t duo_f0_bin_start[] asm("_binary_duo_f0_bin_start");
extern const uint8_t duo_f0_bin_end[] asm("_binary_duo_f0_bin_end");
extern const uint8_t duo_f1_bin_start[] asm("_binary_duo_f1_bin_start");
extern const uint8_t duo_f1_bin_end[] asm("_binary_duo_f1_bin_end");
extern const uint8_t duo_f2_bin_start[] asm("_binary_duo_f2_bin_start");
extern const uint8_t duo_f2_bin_end[] asm("_binary_duo_f2_bin_end");

extern const uint8_t farm_f0_bin_start[] asm("_binary_farm_f0_bin_start");
extern const uint8_t farm_f0_bin_end[] asm("_binary_farm_f0_bin_end");
extern const uint8_t farm_f1_bin_start[] asm("_binary_farm_f1_bin_start");
extern const uint8_t farm_f1_bin_end[] asm("_binary_farm_f1_bin_end");
extern const uint8_t farm_f2_bin_start[] asm("_binary_farm_f2_bin_start");
extern const uint8_t farm_f2_bin_end[] asm("_binary_farm_f2_bin_end");

extern const uint8_t stash_f0_bin_start[] asm("_binary_stash_f0_bin_start");
extern const uint8_t stash_f0_bin_end[] asm("_binary_stash_f0_bin_end");
extern const uint8_t stash_f1_bin_start[] asm("_binary_stash_f1_bin_start");
extern const uint8_t stash_f1_bin_end[] asm("_binary_stash_f1_bin_end");
extern const uint8_t stash_f2_bin_start[] asm("_binary_stash_f2_bin_start");
extern const uint8_t stash_f2_bin_end[] asm("_binary_stash_f2_bin_end");

extern const uint8_t trade_f0_bin_start[] asm("_binary_trade_f0_bin_start");
extern const uint8_t trade_f0_bin_end[] asm("_binary_trade_f0_bin_end");
extern const uint8_t trade_f1_bin_start[] asm("_binary_trade_f1_bin_start");
extern const uint8_t trade_f1_bin_end[] asm("_binary_trade_f1_bin_end");
extern const uint8_t trade_f2_bin_start[] asm("_binary_trade_f2_bin_start");
extern const uint8_t trade_f2_bin_end[] asm("_binary_trade_f2_bin_end");

extern const uint8_t watch_f0_bin_start[] asm("_binary_watch_f0_bin_start");
extern const uint8_t watch_f0_bin_end[] asm("_binary_watch_f0_bin_end");
extern const uint8_t watch_f1_bin_start[] asm("_binary_watch_f1_bin_start");
extern const uint8_t watch_f1_bin_end[] asm("_binary_watch_f1_bin_end");
extern const uint8_t watch_f2_bin_start[] asm("_binary_watch_f2_bin_start");
extern const uint8_t watch_f2_bin_end[] asm("_binary_watch_f2_bin_end");

static const ash_spr_blob_t s_blobs[ASHBOX_SCENE_COUNT][ASHBOX_SPRITE_FRAMES] = {
    {ASH_SPR_BLOB(idle_f0_bin), ASH_SPR_BLOB(idle_f1_bin), ASH_SPR_BLOB(idle_f2_bin)},
    {ASH_SPR_BLOB(away_f0_bin), ASH_SPR_BLOB(away_f1_bin), ASH_SPR_BLOB(away_f2_bin)},
    {ASH_SPR_BLOB(weak_f0_bin), ASH_SPR_BLOB(weak_f1_bin), ASH_SPR_BLOB(weak_f2_bin)},
    {ASH_SPR_BLOB(threat_f0_bin), ASH_SPR_BLOB(threat_f1_bin), ASH_SPR_BLOB(threat_f2_bin)},
    {ASH_SPR_BLOB(return_f0_bin), ASH_SPR_BLOB(return_f1_bin), ASH_SPR_BLOB(return_f2_bin)},
    {ASH_SPR_BLOB(night_f0_bin), ASH_SPR_BLOB(night_f1_bin), ASH_SPR_BLOB(night_f2_bin)},
    {ASH_SPR_BLOB(gate_f0_bin), ASH_SPR_BLOB(gate_f1_bin), ASH_SPR_BLOB(gate_f2_bin)},
    {ASH_SPR_BLOB(duo_f0_bin), ASH_SPR_BLOB(duo_f1_bin), ASH_SPR_BLOB(duo_f2_bin)},
    {ASH_SPR_BLOB(farm_f0_bin), ASH_SPR_BLOB(farm_f1_bin), ASH_SPR_BLOB(farm_f2_bin)},
    {ASH_SPR_BLOB(stash_f0_bin), ASH_SPR_BLOB(stash_f1_bin), ASH_SPR_BLOB(stash_f2_bin)},
    {ASH_SPR_BLOB(trade_f0_bin), ASH_SPR_BLOB(trade_f1_bin), ASH_SPR_BLOB(trade_f2_bin)},
    {ASH_SPR_BLOB(watch_f0_bin), ASH_SPR_BLOB(watch_f1_bin), ASH_SPR_BLOB(watch_f2_bin)},
};

/* One decoded stage at a time (no PSRAM). UI keys refresh by scene+frame, not pointer. */
static uint16_t s_fb[ASH_SPR_PX];
static lv_image_dsc_t s_dsc;
static int s_cached_scene = -1;
static int s_cached_frame = -1;

static uint16_t rd_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool decode_asr1(const uint8_t *data, size_t len, uint16_t *out_fb)
{
    if (!data || !out_fb || len < 10) return false;
    if (rd_u32_le(data) != ASH_SPR_MAGIC) return false;
    uint16_t w = rd_u16_le(data + 4);
    uint16_t h = rd_u16_le(data + 6);
    uint16_t pn = rd_u16_le(data + 8);
    if (w != ASH_SPR_W || h != ASH_SPR_H || pn == 0 || pn > 256) return false;
    size_t off = 10;
    if (off + (size_t)pn * 2u > len) return false;

    uint16_t palette[256];
    for (uint16_t i = 0; i < pn; i++) {
        palette[i] = rd_u16_le(data + off + (size_t)i * 2u);
    }
    off += (size_t)pn * 2u;

    size_t written = 0;
    while (off + 1 < len && written < ASH_SPR_PX) {
        uint8_t count = data[off++];
        uint8_t idx = data[off++];
        if (count == 0 || idx >= pn) return false;
        if (written + (size_t)count > ASH_SPR_PX) return false;
        uint16_t c = palette[idx];
        for (uint8_t n = 0; n < count; n++) {
            out_fb[written++] = c;
        }
    }
    return written == ASH_SPR_PX && off == len;
}

static void ensure_decoded(ashbox_scene_t scene, int frame)
{
    if ((int)scene == s_cached_scene && frame == s_cached_frame) return;
    const ash_spr_blob_t *b = &s_blobs[scene][frame];
    size_t len = (size_t)(b->end - b->start);
    if (!decode_asr1(b->start, len, s_fb)) {
        /* Fail-safe: solid dark so UI never points at garbage. */
        memset(s_fb, 0, sizeof(s_fb));
    }
    s_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_dsc.header.flags = 0;
    s_dsc.header.w = ASH_SPR_W;
    s_dsc.header.h = ASH_SPR_H;
    s_dsc.header.stride = ASH_SPR_W * 2;
    s_dsc.header.reserved_2 = 0;
    s_dsc.data_size = sizeof(s_fb);
    s_dsc.data = (const uint8_t *)s_fb;
    s_cached_scene = (int)scene;
    s_cached_frame = frame;
}

const lv_image_dsc_t *ashbox_sprite_get_frame(ashbox_scene_t scene, int frame)
{
    if (frame < 0) frame = 0;
    frame %= ASHBOX_SPRITE_FRAMES;
    if ((unsigned)scene >= ASHBOX_SCENE_COUNT) scene = ASHBOX_SCENE_IDLE;
    ensure_decoded(scene, frame);
    return &s_dsc;
}

const lv_image_dsc_t *ashbox_sprite_get(ashbox_scene_t scene)
{
    return ashbox_sprite_get_frame(scene, 0);
}
