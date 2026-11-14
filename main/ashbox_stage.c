#include "ashbox_stage.h"

#include "ashbox_ambience.h"
#include "ashbox_ui_page.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(ashbox_font_16);

#define STAGE_W 240
#define STAGE_H 172
#define EMBER_POOL 14

#define CHIP_H 20
#define CHIP_TOP_Y 3
#define CHIP_BOTTOM_Y (STAGE_H - 3 - CHIP_H)

/* The stove sits in the same right-hand spot in all twelve scenes. */
#define FIRE_X 188
#define FIRE_Y 120

enum { CHIP_TL = 0, CHIP_TR, CHIP_BL, CHIP_BR };

typedef struct {
    lv_obj_t *obj;
    int x;
    int y;
    int vy;   /* px per second */
    int frac; /* sub-pixel carry, 1/1000 px */
    int life_ms;
    int age_ms;
} ember_t;

static lv_obj_t *s_root;
static lv_obj_t *s_tint;
static lv_obj_t *s_glow[2];
static lv_obj_t *s_vignette;
static lv_obj_t *s_embers[EMBER_POOL];
static ember_t s_ember[EMBER_POOL];
static lv_obj_t *s_chip[4];
static lv_obj_t *s_chip_lab[4];
static char s_chip_txt[4][24];

static uint32_t s_tint_rgb;
static int s_tint_opa = -1;
static int s_vig_opa = -1;
static bool s_glow_off;
static int64_t s_next_particle_ms;
static int64_t s_next_chip_ms;
static uint32_t s_rand = 0x51A7BEEFu;

static uint32_t rnd(void)
{
    s_rand = s_rand * 1103515245u + 12345u;
    return (s_rand >> 16) & 0x7FFFu;
}

static lv_obj_t *mk_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color,
                        int opa)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, (lv_opa_t)opa, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void chip_create(int i, int x, int w)
{
    int y = (i == CHIP_TL || i == CHIP_TR) ? CHIP_TOP_Y : CHIP_BOTTOM_Y;
    s_chip[i] = lv_obj_create(s_root);
    lv_obj_set_pos(s_chip[i], x, y);
    lv_obj_set_size(s_chip[i], w, CHIP_H);
    lv_obj_set_style_bg_color(s_chip[i], lv_color_hex(COL_SURF), 0);
    lv_obj_set_style_bg_opa(s_chip[i], 200, 0);
    lv_obj_set_style_border_width(s_chip[i], 1, 0);
    lv_obj_set_style_border_color(s_chip[i], lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_border_opa(s_chip[i], 140, 0);
    lv_obj_set_style_radius(s_chip[i], 3, 0);
    lv_obj_set_style_pad_all(s_chip[i], 0, 0);
    lv_obj_set_style_clip_corner(s_chip[i], true, 0);
    lv_obj_clear_flag(s_chip[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_chip[i], LV_OBJ_FLAG_CLICKABLE);

    s_chip_lab[i] = lv_label_create(s_chip[i]);
    lv_obj_set_style_text_font(s_chip_lab[i], &ashbox_font_16, 0);
    lv_obj_set_style_text_color(s_chip_lab[i], lv_color_hex(COL_TEXT), 0);
    lv_label_set_long_mode(s_chip_lab[i], LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_width(s_chip_lab[i], w - 6);
    lv_obj_set_pos(s_chip_lab[i], 3, 1);
    lv_label_set_text(s_chip_lab[i], "");
    lv_obj_add_flag(s_chip[i], LV_OBJ_FLAG_HIDDEN);
    s_chip_txt[i][0] = '\0';
}

static void chip_set(int i, const char *txt, uint32_t color)
{
    if (!s_chip[i]) {
        return;
    }
    const char *src = txt ? txt : "";
    if (strcmp(s_chip_txt[i], src) == 0) {
        return;
    }
    snprintf(s_chip_txt[i], sizeof(s_chip_txt[i]), "%s", src);
    if (src[0] == '\0') {
        lv_obj_add_flag(s_chip[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_chip[i], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_chip_lab[i], src);
    lv_obj_set_style_text_color(s_chip_lab[i], lv_color_hex(color), 0);
}

void ashbox_stage_create(lv_obj_t *parent, int32_t w, int32_t h)
{
    if (!parent || w <= 0 || h <= 0) {
        return;
    }
    s_root = lv_obj_create(parent);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, w, h);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_CLICKABLE);

    s_tint = mk_box(s_root, 0, 0, w, h, 0x101840, 0);

    s_glow[0] = mk_box(s_root, FIRE_X - 28, FIRE_Y - 28, 56, 56, 0xE08028, 0);
    lv_obj_set_style_radius(s_glow[0], 28, 0);
    s_glow[1] = mk_box(s_root, FIRE_X - 46, FIRE_Y - 46, 92, 92, 0xE08028, 0);
    lv_obj_set_style_radius(s_glow[1], 46, 0);
    for (int g = 0; g < 2; g++) {
        /* Amber centre fading to dark at the edge; cheap enough for the C3. */
        lv_obj_set_style_bg_grad_color(s_glow[g], lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_grad_dir(s_glow[g], LV_GRAD_DIR_RADIAL, 0);
    }

    for (int i = 0; i < EMBER_POOL; i++) {
        s_embers[i] = mk_box(s_root, FIRE_X, FIRE_Y, 3, 3, 0xE0A040, 0);
        lv_obj_set_style_radius(s_embers[i], 1, 0);
        lv_obj_add_flag(s_embers[i], LV_OBJ_FLAG_HIDDEN);
        s_ember[i].obj = s_embers[i];
        s_ember[i].age_ms = 0;
        s_ember[i].life_ms = 0;
    }

    /* Red edge frame; border only, so the art centre stays untouched. */
    s_vignette = lv_obj_create(s_root);
    lv_obj_set_pos(s_vignette, 0, 0);
    lv_obj_set_size(s_vignette, w, h);
    lv_obj_set_style_bg_opa(s_vignette, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_vignette, 4, 0);
    lv_obj_set_style_border_color(s_vignette, lv_color_hex(0xC02828), 0);
    lv_obj_set_style_border_opa(s_vignette, 0, 0);
    lv_obj_set_style_radius(s_vignette, 0, 0);
    lv_obj_set_style_pad_all(s_vignette, 0, 0);
    lv_obj_clear_flag(s_vignette, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_vignette, LV_OBJ_FLAG_CLICKABLE);

    chip_create(CHIP_TL, 4, 52);
    chip_create(CHIP_TR, w - 4 - 74, 74);
    chip_create(CHIP_BL, 4, 82);
    chip_create(CHIP_BR, w - 4 - 48, 48);
}

static void ember_spawn(int i)
{
    ember_t *e = &s_ember[i];
    e->x = FIRE_X - 7 + (int)(rnd() % 15u);
    e->y = FIRE_Y - 6 + (int)(rnd() % 8u);
    e->vy = 12 + (int)(rnd() % 7u);
    e->frac = 0;
    e->life_ms = 2500 + (int)(rnd() % 3000u);
    e->age_ms = 0;
    lv_obj_set_pos(e->obj, e->x, e->y);
    lv_obj_clear_flag(e->obj, LV_OBJ_FLAG_HIDDEN);
}

static void particles_update(int64_t now_ms, const ash_ambience_t *amb)
{
    int dt = (int)(now_ms - s_next_particle_ms);
    if (dt < 0) {
        dt = 0;
    }
    if (dt > 500) {
        dt = 500;
    }
    if (dt < 100) {
        return;
    }
    s_next_particle_ms = now_ms;

    for (int i = 0; i < EMBER_POOL; i++) {
        ember_t *e = &s_ember[i];
        if (i >= (int)amb->ember_count) {
            lv_obj_add_flag(e->obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (e->life_ms == 0) {
            ember_spawn(i);
            continue;
        }
        e->age_ms += dt;
        if (e->age_ms >= e->life_ms) {
            ember_spawn(i);
            continue;
        }
        e->frac += e->vy * dt;
        int step = e->frac / 1000;
        e->frac -= step * 1000;
        e->y -= step;
        if (e->y < 40) {
            ember_spawn(i);
            continue;
        }
        int left = e->life_ms - e->age_ms;
        int opa = 190 * left / e->life_ms;
        if (opa > 190) {
            opa = 190;
        }
        lv_obj_set_pos(e->obj, e->x, e->y);
        lv_obj_set_style_bg_opa(e->obj, (lv_opa_t)opa, 0);
    }

    /* Two slow breathing glows; triangle wave avoids libm. */
    int phase = (int)((now_ms / 8) % 2000);
    int tri = phase < 1000 ? phase : 2000 - phase;
    int g0 = 16 + tri * 22 / 1000;
    int g1 = 8 + tri * 16 / 1000;
    lv_obj_set_style_bg_opa(s_glow[0], (lv_opa_t)g0, 0);
    lv_obj_set_style_bg_opa(s_glow[1], (lv_opa_t)g1, 0);

    int vig = 0;
    if (amb->vignette != 0) {
        int pulse = phase < 1000 ? phase : 2000 - phase;
        vig = amb->vignette + pulse * 25 / 1000;
        if (vig > 120) {
            vig = 120;
        }
    }
    if (vig != s_vig_opa) {
        s_vig_opa = vig;
        lv_obj_set_style_border_opa(s_vignette, (lv_opa_t)vig, 0);
    }
}

static void chips_update(const ashbox_stage_view_t *v, const ash_ambience_t *amb)
{
    char b[24];

    if (v->phenom && v->phenom[0]) {
        chip_set(CHIP_TL, v->phenom, v->phenom_danger ? COL_DANGER : COL_MUTED);
    } else {
        chip_set(CHIP_TL, ash_ambience_phase_name(amb->phase), COL_MUTED);
    }

    if (v->threat >= 50) {
        snprintf(b, sizeof(b), "威胁%u%%", (unsigned)v->threat);
        chip_set(CHIP_TR, b, COL_DANGER);
    } else {
        chip_set(CHIP_TR, "", COL_DANGER);
    }

    b[0] = '\0';
    if (v->exp_active) {
        uint32_t rem = v->exp_remaining_s;
        snprintf(b, sizeof(b), "回来%u:%02u", (unsigned)(rem / 3600),
                 (unsigned)((rem / 60) % 60));
    } else if (v->farm_ready > 0) {
        snprintf(b, sizeof(b), "可收%u", (unsigned)v->farm_ready);
    } else if (v->duty_left > 0) {
        snprintf(b, sizeof(b), "日勤%u", (unsigned)v->duty_left);
    } else if (v->stash_full) {
        snprintf(b, sizeof(b), "仓满");
    }
    chip_set(CHIP_BL, b, COL_AMBER);

    if (v->crisis) {
        chip_set(CHIP_BR, "守夜", COL_DANGER);
    } else if (v->report) {
        chip_set(CHIP_BR, "战报", COL_AMBER);
    } else if (v->wanderer) {
        chip_set(CHIP_BR, "门外", COL_TEXT);
    } else {
        chip_set(CHIP_BR, "", COL_TEXT);
    }
}

void ashbox_stage_update(int64_t now_us, const ashbox_stage_view_t *v, bool animate)
{
    if (!s_root || !v) {
        return;
    }
    int64_t now_ms = now_us / 1000;

    ash_ambience_t amb;
    ash_ambience_for(v->hour, v->threat, v->ember, &amb);

    if ((int)amb.tint_opa != s_tint_opa || amb.tint_rgb != s_tint_rgb) {
        s_tint_opa = (int)amb.tint_opa;
        s_tint_rgb = amb.tint_rgb;
        lv_obj_set_style_bg_color(s_tint, lv_color_hex(amb.tint_rgb), 0);
        lv_obj_set_style_bg_opa(s_tint, (lv_opa_t)amb.tint_opa, 0);
    }

    if (now_ms >= s_next_chip_ms) {
        s_next_chip_ms = now_ms + 500;
        chips_update(v, &amb);
    }

    if (animate) {
        s_glow_off = false;
        particles_update(now_ms, &amb);
    } else {
        if (!s_glow_off) {
            s_glow_off = true;
            lv_obj_set_style_bg_opa(s_glow[0], 0, 0);
            lv_obj_set_style_bg_opa(s_glow[1], 0, 0);
        }
        if (s_vig_opa != (int)amb.vignette) {
            s_vig_opa = (int)amb.vignette;
            lv_obj_set_style_border_opa(s_vignette, (lv_opa_t)amb.vignette, 0);
        }
        s_next_particle_ms = now_ms;
    }
}
