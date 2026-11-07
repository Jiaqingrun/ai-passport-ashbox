#include "ashbox_ui.h"
#include "ashbox_help_copy.h"

#include "ashbox_content.h"
#include "ashbox_embackup.h"
#include "ashbox_meta.h"
#include "ashbox_runtime.h"
#include "ashbox_save.h"
#include "ashbox_settings.h"
#include "ashbox_sfx.h"
#include "ashbox_sprites.h"
#include "ashbox_ui_layout.h"

/*
 * UI module map (keep lock order: ashbox_runtime_lock → bsp_lvgl_lock):
 *   home/stage/HUD  · pick/hub/build/farm/trade/stash/duty
 *   settings + embackup confirm · guide/help · power/sleep · tick/settle
 * Geometry: ashbox_ui_layout.h (LAYOUT_LOCK). Page TU split remains C-UI1 follow-up.
 */

#include "bsp_battery.h"
#include "bsp_display.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

LV_FONT_DECLARE(ashbox_font_16);

static const char *TAG = "ash_ui";

/* 末世像素皮 v6：破败但可读 —— 字清楚、锈血仍在 */
#define COL_BG 0x0A0808
#define COL_SURF 0x1A1412
#define COL_SURF2 0x2A201C
#define COL_LINE 0x8A4830
#define COL_TEXT 0xE8DCC8
#define COL_MUTED 0x9A8A7A
#define COL_AMBER 0xE08028
#define COL_GOLD 0xD4A017
#define COL_SAFE 0x58A050
#define COL_DANGER 0xC02828
#define COL_INK 0x0A0808
#define COL_COST_OK 0x3A5A3A
#define COL_COST_SHORT 0x8C2C28
#define COL_SEL 0x3A2418

typedef enum {
    UI_HOME = 0,
    UI_PICK,
    UI_HUB, /* 据点枢纽：菜地优先，不再把菜地埋进建设 */
    UI_BUILD,
    UI_SETTINGS,
    UI_HELP,
    UI_REPORT,
    UI_CRISIS,
    UI_QUEST,
    UI_ORDER,
    UI_JOURNAL,
    UI_STASH,
    UI_RES,
    UI_EQUIP,
    UI_FARM,
    UI_TRADE,
    UI_DUTY,
    UI_SURVIVORS,
    UI_SURVIVOR_DETAIL,
    UI_ACHIEVE,
    UI_RESTART_CONFIRM,
} ui_page_t;

#define ASH_LIST_MAX 16

static ash_game_t *s_g;
static ashbox_meta_t s_meta;
static uint8_t s_surv_pid;
static ash_settings_t s_cfg;
static ui_page_t s_page;
static int s_depart_step; /* 0 tier, 1 person, 2 loadout */
static ash_tier_t s_pick_tier;
static uint8_t s_pick_person;
static ash_loadout_t s_pick_load;
static bool s_save_dirty;
/* Set from button callback without LVGL; consumed under runtime+LVGL locks. */
static volatile bool s_wake_apply_pending;
static lv_obj_t *s_scr;
static lv_obj_t *s_title;
static lv_obj_t *s_day;
/* 捡来的「烬芯」：分段电芯 + 锈夹 + 裂口，不像普通手机电池 */
#define BAT_CELLS 4
static lv_obj_t *s_bat_tag;              /* 「烬」 */
static lv_obj_t *s_bat_shell;
static lv_obj_t *s_bat_cell[BAT_CELLS];
static lv_obj_t *s_bat_clamp;            /* 左侧锈夹 */
static lv_obj_t *s_bat_spark;            /* 右侧触点火花 */
static lv_obj_t *s_bat_scar;             /* 壳上裂痕 */
static lv_obj_t *s_water;
static lv_obj_t *s_food;
static lv_obj_t *s_parts;
static lv_obj_t *s_ember;
static lv_obj_t *s_people;
static lv_obj_t *s_status;
static lv_obj_t *s_threat_lbl;
static lv_obj_t *s_threat_bar;
static lv_obj_t *s_radio;
static lv_obj_t *s_sprite;
static lv_obj_t *s_card_title;
static lv_obj_t *s_card_value;
static lv_obj_t *s_card_sub;
static lv_obj_t *s_btn_pri;
static lv_obj_t *s_btn_sec;
static lv_obj_t *s_btn_pri_lab;
static lv_obj_t *s_btn_sec_lab;
static lv_obj_t *s_hint;
static lv_obj_t *s_list_labels[ASH_LIST_MAX];
static lv_obj_t *s_list_subs[ASH_LIST_MAX];
static lv_obj_t *s_list_rows[ASH_LIST_MAX];
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_lines[5];
static lv_obj_t *s_modal_btn;
static bool s_block_modal;
static bool s_wanderer_bell_armed;
static bool s_exile_confirm;
static lv_obj_t *s_home_layer;
static lv_obj_t *s_list_layer;
static lv_obj_t *s_scroll_track;
static lv_obj_t *s_scroll_thumb;

static int s_focus; /* 0 primary, 1 secondary / list index */
static int s_list_sel;
static bool s_want_sleep;
static volatile bool s_wake_guard;
static bool s_night_aftermath;
static int64_t s_mono_last_us;
static int64_t s_settle_accum_us;
static int64_t s_idle_us;
static int64_t s_ignore_ok_until_us;
static int64_t s_last_nvs_flush_us;
static bool s_cfg_dirty;
static bool s_nvs_force; /* flush before next LVGL re-entry (depart / sleep) */
static bool s_arm_sleep; /* set under LVGL; arm deep-sleep only after NVS */
static int s_bat_soc_cache = -1;
static int s_bat_mv_cache = -1;
static int64_t s_bat_next_sample_us;
/* 远征/菜地等时钟推进若只留在 RAM，刷机/复位会回到出发瞬间的剩余时间 */
#define SAVE_FLUSH_PERIOD_US (15LL * 1000000LL)
#define BAT_SAMPLE_PERIOD_US (1000000LL)
static int s_last_scene = -1;
static int s_anim_frame;
static int64_t s_anim_next_us;
static int s_last_sprite_scene = -1;
static int s_last_sprite_frame = -1;
/* 主页手动换景：-1=跟自动聚焦；仅能落在 ash_game_scenes_available 内 */
static int s_scene_pick = -1;
/* 上下短按改焦点；长按换景时还原短按误切的焦点 */
static int s_nav_focus_snap = -1;
static char s_radio_cache[64];
static char s_card_sub_cache[48];
static char s_scroll_shown[160];
static uint32_t s_home_fp;
static volatile bool s_drain_input;
/* Skip redundant LVGL invalidates when timers tick every second. */
static int64_t s_clock_sec_shown = -1;
static uint32_t s_day_key_shown = UINT32_MAX;
static uint32_t s_exp_rem_shown = UINT32_MAX;
static int s_threat_shown = -1;
static int s_bat_level_shown = -999;
#define SPRITE_ANIM_PERIOD_US 320000LL

/* 空闲省电：调暗 / 熄屏 / 深睡。时钟用 esp_timer_get_time() 累加，不走 light sleep。 */
typedef enum {
    PWR_ACTIVE = 0,
    PWR_DIM,
    PWR_BLANK,
} pwr_stage_t;

#define PWR_DIM_US (60LL * 1000000LL)           /* 60s → 背光压低 + 停电台/动画 */
#define PWR_BLANK_US (180LL * 1000000LL)        /* 180s → 背光关（远征中跳过，倒计时须可见） */
#define PWR_SLEEP_EXP_US (300LL * 1000000LL)    /* 远征等待 5 分钟 → 深睡 */
#define PWR_SLEEP_STATION_US (600LL * 1000000LL) /* 站内主页 10 分钟 → 深睡兜底 */
#define GEAR_PICK_COUNT 3 /* 轻装 / 撬棍 / 手枪 — 勿绑 ASH_LOAD_COUNT */

static pwr_stage_t s_pwr;
/* 菜地列表：每项对应田地 + 动作（种谷/种薯/收/等） */
static uint8_t s_farm_plot[ASH_LIST_MAX];
static uint8_t s_farm_act[ASH_LIST_MAX]; /* 0 grain, 1 root, 2 harvest, 3 wait */
static int s_farm_n;
#define SETTINGS_COUNT 7

static void flush_save(bool mirror);
static void mark_save(void);
static void flush_nvs_runtime_only(bool force);
static void sample_battery(void);
static void refresh_wall_clock(void);
static void refresh_home_day_title(void);
static void set_cost_color(lv_obj_t *lab, bool enough);
static void paint_hub_scrollbar(int sel, int count, int view);
static void paint_survivors(void);
static void paint_survivor_detail(void);
static void paint_achieve(void);
static void paint_restart_confirm(void);
static void paint_settings(void);
static void set_card_sub_tip(const char *tip);

void ashbox_ui_arm_ok_guard_ms(int ms)
{
    if (ms < 0) ms = 0;
    s_ignore_ok_until_us = esp_timer_get_time() + (int64_t)ms * 1000LL;
    s_drain_input = true;
}

bool ashbox_ui_take_input_drain_request(void)
{
    if (!s_drain_input) return false;
    s_drain_input = false;
    return true;
}

static bool ok_guard_blocks(void)
{
    return esp_timer_get_time() < s_ignore_ok_until_us;
}

static int wrap(int v, int n, int dir)
{
    if (n <= 0) return 0;
    v += dir;
    if (v < 0) return n - 1;
    if (v >= n) return 0;
    return v;
}

static const lv_font_t *F(void) { return &ashbox_font_16; }

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, F(), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

/** Instruction / modal body: fixed width + wrap so copy is not clipped off-screen. */
static void label_set_wrap(lv_obj_t *lab, int32_t w, int32_t h)
{
    if (!lab) return;
    lv_obj_set_width(lab, w);
    if (h > 0) lv_obj_set_height(lab, h);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_LEFT, 0);
}

static void style_panel(lv_obj_t *o, uint32_t bg)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 0, 0); /* 像素风：直角 */
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_pad_all(o, 0, 0);
}

/* Apply in RAM only; NVS deferred to flush_nvs_runtime_only (never under LVGL). */
static void persist_cfg(void)
{
    ashbox_settings_apply(&s_cfg);
    s_cfg_dirty = true;
}

static void label_set_if(lv_obj_t *lab, const char *txt)
{
    if (!lab || !txt) return;
    const char *cur = lv_label_get_text(lab);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(lab, txt);
}

/* I2C sample — caller must NOT hold bsp_lvgl_lock. */
static void sample_battery(void)
{
    int64_t now = esp_timer_get_time();
    if (s_bat_next_sample_us != 0 && now < s_bat_next_sample_us) return;
    s_bat_next_sample_us = now + BAT_SAMPLE_PERIOD_US;
    s_bat_soc_cache = bsp_battery_soc();
    s_bat_mv_cache = bsp_battery_mv();
}

/* Paint-only: uses cache from sample_battery(); no I2C. */
static void refresh_battery(void)
{
    if (!s_bat_shell || !s_bat_cell[0]) return;
    int soc = s_bat_soc_cache;
    int mv = s_bat_mv_cache;
    int level = soc;
    if (level < 0) {
        if (mv < 0) level = -1;
        else if (mv >= 4100) level = 90;
        else if (mv >= 3900) level = 70;
        else if (mv >= 3700) level = 45;
        else if (mv >= 3500) level = 25;
        else level = 10;
    } else if (level > 100) {
        level = 100;
    }

    if (level == s_bat_level_shown) return;
    s_bat_level_shown = level;

    int lit = 0;
    uint32_t live = COL_AMBER;
    uint32_t rim = COL_LINE;
    uint32_t spark = COL_MUTED;
    bool dead = (level < 0);
    if (!dead) {
        /* 4 格：残芯一格一格灭，不像连续进度条 */
        lit = (level + 12) / 25;
        if (lit > BAT_CELLS) lit = BAT_CELLS;
        if (level > 0 && lit < 1) lit = 1;
        if (level >= 70) live = 0xC8A030; /* 锈金余辉 */
        else if (level >= 40) live = COL_AMBER;
        else if (level >= 18) live = 0xB04820;
        else live = COL_DANGER;
        rim = (level < 18) ? COL_DANGER : COL_LINE;
        spark = (level >= 40) ? live : ((level >= 18) ? COL_LINE : COL_DANGER);
    }

    lv_obj_set_style_border_color(s_bat_shell, lv_color_hex(rim), 0);
    if (s_bat_clamp) {
        lv_obj_set_style_bg_color(s_bat_clamp, lv_color_hex(rim), 0);
    }
    if (s_bat_spark) {
        lv_obj_set_style_bg_color(s_bat_spark, lv_color_hex(spark), 0);
        if (dead || lit <= 0) lv_obj_add_flag(s_bat_spark, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_bat_spark, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_bat_scar) {
        /* 低电量裂痕外露 */
        if (!dead && level < 40) lv_obj_clear_flag(s_bat_scar, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_bat_scar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_bat_scar, lv_color_hex(level < 18 ? COL_DANGER : COL_LINE), 0);
    }

    for (int i = 0; i < BAT_CELLS; i++) {
        bool on = (!dead && i < lit);
        uint32_t c = on ? live : COL_SURF2;
        /* 空格带锈点边，像抠掉的旧芯 */
        lv_obj_set_style_bg_color(s_bat_cell[i], lv_color_hex(c), 0);
        lv_obj_set_style_border_color(s_bat_cell[i],
                                      lv_color_hex(on ? live : COL_SURF), 0);
        /* 末格将灭：只亮一半高，像漏电 */
        if (on && i == lit - 1 && level < 40 && level >= 0) {
            lv_obj_set_height(s_bat_cell[i], 4);
            lv_obj_set_y(s_bat_cell[i], 4);
        } else {
            lv_obj_set_height(s_bat_cell[i], 6);
            lv_obj_set_y(s_bat_cell[i], 3);
        }
    }
}

/* Dual↔solo 改宽后，若标签仍按旧宽居中，可滚动父级会把字滚成一道横杠。 */
static void layout_btn_label(lv_obj_t *btn, lv_obj_t *lab)
{
    if (!btn || !lab) return;
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (lv_obj_get_scroll_x(btn) != 0 || lv_obj_get_scroll_y(btn) != 0) {
        lv_obj_scroll_to(btn, 0, 0, LV_ANIM_OFF);
    }
    /* Prefer style size from set_bar_dual; coords can lag a frame. */
    int32_t bw = lv_obj_get_style_width(btn, LV_PART_MAIN);
    int32_t bh = lv_obj_get_style_height(btn, LV_PART_MAIN);
    if (bw <= 12) bw = lv_obj_get_width(btn);
    if (bh <= 8) bh = lv_obj_get_height(btn);
    if (bw <= 12 || bh <= 8) return;
    lv_label_set_long_mode(lab, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(lab, bw - 8, (int32_t)F()->line_height);
    lv_obj_align(lab, LV_ALIGN_CENTER, 0, 0);
}

static void set_btn(lv_obj_t *btn, lv_obj_t *lab, const char *txt, bool on, bool focus)
{
    if (!lab) {
        if (btn == s_btn_pri) lab = s_btn_pri_lab;
        else if (btn == s_btn_sec) lab = s_btn_sec_lab;
        else lab = lv_obj_get_child(btn, 0);
    }
    if (!btn || !lab) return;
    label_set_if(lab, txt);
    uint32_t bg = (on && focus) ? COL_AMBER : COL_SURF2;
    uint32_t fg = (on && focus) ? COL_INK : COL_MUTED;
    if (!on) {
        bg = COL_SURF2;
        fg = COL_MUTED;
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(fg), 0);
    layout_btn_label(btn, lab);
}

/* 此刻带：标题 / 主状态（提示已并入广播滚动行） */
#define CARD_TITLE_X 42
#define CARD_TITLE_W 72
#define CARD_VALUE_X 116
#define CARD_VALUE_W 112
/* LOCKED home chrome — docs/ashbox/LAYOUT_LOCK.md — do not tweak without a new decision. */
#define SAFE_TOP ASHBOX_UI_SAFE_TOP
#define HDR_H ASHBOX_UI_HDR_H
#define STAGE_H ASHBOX_UI_STAGE_H
#define NOW_BAND_H ASHBOX_UI_NOW_BAND_H
#define LIST_LAYER_H ASHBOX_UI_LIST_LAYER_H
#define CONTENT_Y ASHBOX_UI_CONTENT_Y
#define HINT_Y 250
#define BAR_Y 268
#define BAR_H 44
#define BTN_Y 8
#define BTN_H 28
#define BTN_GAP 8
#define BTN_MARGIN 12
#define BTN_SEC_W 68
#define BTN_PRI_DUAL_W (240 - BTN_MARGIN * 2 - BTN_GAP - BTN_SEC_W) /* 140 */
#define BTN_PRI_SOLO_W (240 - BTN_MARGIN * 2)                         /* 216 */
#define NOW_ROW_RES 1
#define NOW_ROW_RADIO 17
#define NOW_ROW_CARD 34

static void refresh_home_scroll_line(void)
{
    if (!s_radio) return;
    char buf[160];
    if (s_radio_cache[0] && s_card_sub_cache[0]) {
        snprintf(buf, sizeof(buf), "  %s · %s   ·  ", s_radio_cache, s_card_sub_cache);
    } else if (s_card_sub_cache[0]) {
        snprintf(buf, sizeof(buf), "  %s   ·  ", s_card_sub_cache);
    } else if (s_radio_cache[0]) {
        snprintf(buf, sizeof(buf), "  %s   ·  ", s_radio_cache);
    } else {
        buf[0] = '\0';
    }
    if (strcmp(s_scroll_shown, buf) == 0) return;
    snprintf(s_scroll_shown, sizeof(s_scroll_shown), "%s", buf);
    lv_label_set_text(s_radio, buf);
}

static void set_radio_text(const char *body)
{
    const char *src = body ? body : "";
    if (strcmp(s_radio_cache, src) == 0) return;
    snprintf(s_radio_cache, sizeof(s_radio_cache), "%s", src);
    refresh_home_scroll_line();
}

static void hide_list_scrollbar(void)
{
    if (s_scroll_track) lv_obj_add_flag(s_scroll_track, LV_OBJ_FLAG_HIDDEN);
}

static void set_cost_color(lv_obj_t *lab, bool enough)
{
    if (!lab) return;
    lv_obj_set_style_text_color(lab, lv_color_hex(enough ? COL_COST_OK : COL_COST_SHORT), 0);
}

static void set_card_sub_tip(const char *tip)
{
    const char *src = tip ? tip : "";
    if (strcmp(s_card_sub_cache, src) == 0) return;
    snprintf(s_card_sub_cache, sizeof(s_card_sub_cache), "%s", src);
    if (s_card_sub) {
        lv_label_set_text(s_card_sub, "");
        lv_obj_add_flag(s_card_sub, LV_OBJ_FLAG_HIDDEN);
    }
    refresh_home_scroll_line();
}

static void set_modal_visible(bool on)
{
    if (!s_modal) return;
    if (on) {
        lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        if (s_hint) lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        if (s_hint) lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        s_block_modal = false;
    }
}

static void show_block_modal(ash_act_t act)
{
    char reason[48];
    if (!ash_game_block_reason(s_g, act, reason, sizeof(reason)) || reason[0] == '\0') {
        if (act == ASH_ACT_DEPART) {
            uint16_t need = ash_game_expedition_water_cost(s_pick_tier);
            if (s_g->water < need)
                snprintf(reason, sizeof(reason), "水不够 · 需带%u", (unsigned)need);
            else
                snprintf(reason, sizeof(reason), "现在不能做");
        } else {
            snprintf(reason, sizeof(reason), "现在不能做");
        }
    }
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], "无法进行");
    lv_label_set_text(s_modal_lines[1], reason);
    lv_label_set_text(s_modal_lines[2], "");
    lv_label_set_text(s_modal_lines[3], "");
    lv_label_set_text(s_modal_lines[4], "按任意键关闭");
    if (s_modal_btn) {
        lv_obj_clear_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lv_obj_get_child(s_modal_btn, 0), "知道了");
    }
    s_block_modal = true;
    ashbox_sfx_play(ASH_SFX_WARN);
}

static int list_scroll_top(int sel, int count, int view)
{
    if (view <= 0) view = 1;
    if (count < view) view = count;
    int top = 0;
    if (sel >= view) top = sel - view + 1;
    if (top > count - view) top = count - view;
    if (top < 0) top = 0;
    return top;
}

static uint32_t home_ui_fingerprint(void)
{
    if (!s_g) return 0;
    uint32_t mode = 0;
    if (ash_game_crisis_pending(s_g)) mode = 1;
    else if (s_g->report.pending) mode = 2u + (s_g->report.branch_resolved ? 1u : 0u);
    else if (s_g->exp.active) mode = 4u + (ash_game_chore_pending(s_g) ? 1u : 0u);
    else if (ash_game_wanderer_pending(s_g)) mode = 9u + (ash_game_wanderer_can_join(s_g) ? 1u : 0u);
    else if (!ash_game_can_depart(s_g)) mode = 6u + (ash_game_duty_available(s_g) ? 1u : 0u);
    else mode = 8;
    uint32_t f = mode;
    f |= ((uint32_t)ash_game_scene(s_g, s_wake_guard, s_night_aftermath) & 0xFFu) << 8;
    f |= (s_g->threat >= 80 ? 1u : 0u) << 16;
    f |= (s_g->food == 0 ? 1u : 0u) << 17;
    f |= ((uint32_t)s_g->pop & 0x1Fu) << 18;
    f |= (s_wake_guard ? 1u : 0u) << 23;
    f |= ((uint32_t)s_g->day_key & 0xFFu) << 24;
    f ^= ((uint32_t)s_g->food << 1) ^ ((uint32_t)s_g->parts << 7) ^ ((uint32_t)s_g->ember << 13);
    /* Tip/radio text are refreshed in timers_only; hashing them forced full paint. */
    uint32_t tip_bucket = 0;
    if (ash_game_crisis_pending(s_g)) tip_bucket = 1;
    else if (s_g->report.pending) tip_bucket = 2u + (s_g->report.branch_resolved ? 1u : 0u);
    else if (s_g->exp.active) {
        tip_bucket = 4;
        if (ash_game_chore_pending(s_g)) tip_bucket = 5;
        else if (s_g->threat >= 50 && s_g->night_order == ASH_ORDER_NONE) tip_bucket = 6;
        else if (ash_game_farm_ready_count(s_g) > 0) tip_bucket = 7;
        else if (ash_game_stash_full(s_g)) tip_bucket = 8;
    } else if (ash_game_wanderer_pending(s_g)) {
        tip_bucket = 9;
    } else {
        tip_bucket = 10;
        if (ash_game_duty_left(s_g) > 0) tip_bucket = 11;
        else if (ash_game_farm_ready_count(s_g) > 0) tip_bucket = 12;
        else if (ash_game_stash_full(s_g)) tip_bucket = 13;
        else if (ash_game_trade_available(s_g) || ash_game_trade2_available(s_g)) tip_bucket = 14;
    }
    f ^= tip_bucket * 2654435761u;
    return f;
}

static void refresh_wall_clock(void)
{
    if (!s_day) return;
    char buf[16];
    int64_t sec = 0;
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) sec = (int64_t)tv.tv_sec;
    /* After power-cut the chip RTC restarts near epoch while save timeline keeps
     * running; show the game anchor so the header does not jump to 00:00:00. */
    if (s_g && s_g->last_settle_unix > 0 &&
        !ash_game_wall_unix_usable(s_g, sec)) {
        sec = s_g->last_settle_unix;
    }
    if (sec > 0) {
        if (sec == s_clock_sec_shown) return;
        s_clock_sec_shown = sec;
        time_t t = (time_t)sec;
        struct tm tm;
        if (localtime_r(&t, &tm) != NULL) {
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
            label_set_if(s_day, buf);
            return;
        }
    }
    s_clock_sec_shown = -1;
    label_set_if(s_day, "--:--:--");
}

static void refresh_home_day_title(void)
{
    if (!s_title || !s_g) return;
    if (s_g->day_key == s_day_key_shown) return;
    s_day_key_shown = s_g->day_key;
    char buf[24];
    snprintf(buf, sizeof(buf), "第%u天", (unsigned)s_g->day_key);
    label_set_if(s_title, buf);
    lv_obj_set_style_text_color(s_title, lv_color_hex(COL_AMBER), 0);
}

static void show_home_chrome(bool home)
{
    hide_list_scrollbar();
    if (home) {
        lv_obj_clear_flag(s_home_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_list_layer, LV_OBJ_FLAG_HIDDEN);
        refresh_home_day_title();
    } else {
        lv_obj_add_flag(s_home_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_list_layer, LV_OBJ_FLAG_HIDDEN);
    }
    refresh_wall_clock();
}

/* Shared decode buffer always returns the same dsc pointer — key by scene+frame. */
static void sprite_show(ashbox_scene_t sc, int frame)
{
    if (!s_sprite) return;
    if (frame < 0) frame = 0;
    frame %= ASHBOX_SPRITE_FRAMES;
    if ((int)sc == s_last_sprite_scene && frame == s_last_sprite_frame) return;
    const lv_image_dsc_t *dsc = ashbox_sprite_get_frame(sc, frame);
    if (!dsc) return;
    s_last_sprite_scene = (int)sc;
    s_last_sprite_frame = frame;
    lv_image_set_src(s_sprite, dsc);
    lv_obj_invalidate(s_sprite);
}

static void apply_sprite(ashbox_scene_t sc, bool force)
{
    if (!force && (int)sc == s_last_scene) {
        sprite_show(sc, s_anim_frame);
        return;
    }
    s_last_scene = (int)sc;
    s_anim_frame = 0;
    s_anim_next_us = esp_timer_get_time() + SPRITE_ANIM_PERIOD_US;
    sprite_show(sc, 0);
}

static ashbox_scene_t home_scene_now(void)
{
    int auto_sc = ash_game_scene(s_g, s_wake_guard, s_night_aftermath);
    if (s_scene_pick < 0) return (ashbox_scene_t)auto_sc;
    int avail[ASHBOX_SCENE_COUNT];
    int n = ash_game_scenes_available(s_g, s_wake_guard, s_night_aftermath, avail,
                                      ASHBOX_SCENE_COUNT);
    for (int i = 0; i < n; i++) {
        if (avail[i] == s_scene_pick) return (ashbox_scene_t)s_scene_pick;
    }
    s_scene_pick = -1;
    return (ashbox_scene_t)auto_sc;
}

static void cycle_home_scene(int dir)
{
    int avail[ASHBOX_SCENE_COUNT];
    int n = ash_game_scenes_available(s_g, s_wake_guard, s_night_aftermath, avail,
                                      ASHBOX_SCENE_COUNT);
    if (n <= 1) {
        ashbox_sfx_play(ASH_SFX_CLICK);
        return;
    }
    int cur = (s_scene_pick >= 0) ? s_scene_pick
                                  : ash_game_scene(s_g, s_wake_guard, s_night_aftermath);
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (avail[i] == cur) {
            idx = i;
            break;
        }
    }
    idx = wrap(idx, n, dir);
    s_scene_pick = avail[idx];
    apply_sprite((ashbox_scene_t)s_scene_pick, true);
    ashbox_sfx_play(ASH_SFX_MOVE);
}

static void set_home_hint(const char *base)
{
    int avail[ASHBOX_SCENE_COUNT];
    int n = ash_game_scenes_available(s_g, s_wake_guard, s_night_aftermath, avail,
                                      ASHBOX_SCENE_COUNT);
    if (n > 1 && base && base[0]) {
        char tip[96];
        snprintf(tip, sizeof(tip), "%s · 上下长按换景", base);
        label_set_if(s_hint, tip);
    } else {
        label_set_if(s_hint, base);
    }
}

static void tick_sprite_anim(void)
{
    /* 调暗后停帧切换，避免无谓 SPI 刷屏 */
    if (s_page != UI_HOME || s_wake_guard || s_pwr != PWR_ACTIVE) return;
    int64_t now = esp_timer_get_time();
    if (now < s_anim_next_us) return;
    s_anim_next_us = now + SPRITE_ANIM_PERIOD_US;
    ashbox_scene_t sc = home_scene_now();
    if ((int)sc != s_last_scene) {
        apply_sprite(sc, true);
        return;
    }
    s_anim_frame = (s_anim_frame + 1) % ASHBOX_SPRITE_FRAMES;
    sprite_show(sc, s_anim_frame);
}

static uint8_t dim_brightness(uint8_t full)
{
    uint8_t d = (uint8_t)(full / 4u);
    if (d < 10) d = 10;
    if (d > 20) d = 20;
    if (d > full) d = full;
    return d;
}

static void power_restore_active(void)
{
    if (s_pwr == PWR_ACTIVE) return;
    s_pwr = PWR_ACTIVE;
    ashbox_settings_apply(&s_cfg);
    s_anim_next_us = esp_timer_get_time() + SPRITE_ANIM_PERIOD_US;
}

/* 任务/危机到达：从调暗或熄屏拉回全亮，并取消待发深睡（玩家吐槽「完成后不亮屏」） */
static void power_attention_wake(const char *reason)
{
    s_idle_us = 0;
    s_want_sleep = false;
    if (s_pwr == PWR_ACTIVE) return;
    pwr_stage_t was = s_pwr;
    s_pwr = PWR_ACTIVE;
    ashbox_settings_apply(&s_cfg);
    s_anim_next_us = esp_timer_get_time() + SPRITE_ANIM_PERIOD_US;
    ESP_LOGI(TAG, "power attention wake from %d (%s)", (int)was, reason ? reason : "?");
}

static void power_on_input(void)
{
    s_idle_us = 0;
    power_restore_active();
}

static bool guide_active(void)
{
    if (!s_g) return false;
    if (s_cfg.guide_step >= ASH_GUIDE_DONE) return false;
    if (s_g->report.pending || ash_game_crisis_pending(s_g)) return false;
    return true;
}

static bool power_blocks_autosleep(void)
{
    if (!s_g) return true;
    if (s_wake_guard) return true;
    if (guide_active()) return true;
    if (s_g->report.pending || ash_game_crisis_pending(s_g)) return true;
    if (s_page == UI_REPORT || s_page == UI_CRISIS) return true;
    return false;
}

static void power_tick(int64_t dt_us)
{
    if (s_wake_guard) {
        s_idle_us = 0;
        return;
    }
    s_idle_us += dt_us;

    if (s_pwr == PWR_ACTIVE && s_idle_us >= PWR_DIM_US) {
        s_pwr = PWR_DIM;
        bsp_display_backlight(dim_brightness(s_cfg.brightness));
        /* 停背景电台：I2S/编解码持续写片是醒着态最大耗电之一 */
        ashbox_sfx_set_radio(false);
        ESP_LOGI(TAG, "power dim idle=%ds", (int)(s_idle_us / 1000000LL));
    }
    ashbox_runtime_lock();
    const bool on_expedition = s_g && s_g->exp.active && !s_g->report.pending;
    /* 远征中禁止自动息屏：倒计时必须继续可见，避免「时间停了」的错觉 */
    if (s_pwr == PWR_DIM && s_idle_us >= PWR_BLANK_US && !on_expedition) {
        s_pwr = PWR_BLANK;
        bsp_display_backlight(0);
        ESP_LOGI(TAG, "power blank idle=%ds", (int)(s_idle_us / 1000000LL));
    }

    if (power_blocks_autosleep()) {
        ashbox_runtime_unlock();
        return;
    }
    const int64_t limit = on_expedition ? PWR_SLEEP_EXP_US : PWR_SLEEP_STATION_US;
    if (s_idle_us >= limit && !s_want_sleep) {
        flush_nvs_runtime_only(true);
        ashbox_runtime_unlock();
        ashbox_sfx_set_enabled(s_cfg.volume > 0);
        ashbox_sfx_play(ASH_SFX_SLEEP);
        s_want_sleep = true;
        ESP_LOGI(TAG, "power sleep request idle=%ds exp=%d", (int)(s_idle_us / 1000000LL),
                 on_expedition ? 1 : 0);
        return;
    }
    ashbox_runtime_unlock();
}

static void log_mem_budget(const char *where)
{
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    const size_t heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG,
             "mem[%s] lv_free=%u lv_big=%u lv_used%%=%u sys_free=%u sys_big=%u",
             where, (unsigned)mon.free_size, (unsigned)mon.free_biggest_size,
             (unsigned)mon.used_pct, (unsigned)heap, (unsigned)largest);
}

static void paint_list_sel(int count)
{
    for (int i = 0; i < ASH_LIST_MAX; i++) {
        if (i >= count) {
            lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[i],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[i],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
}

/* Home keeps dual actions; sub-pages hide「返回」and use OK long-press. */
static void set_bar_dual(bool dual)
{
    if (dual) {
        lv_obj_set_size(s_btn_pri, BTN_PRI_DUAL_W, BTN_H);
        lv_obj_set_pos(s_btn_pri, BTN_MARGIN, BTN_Y);
        lv_obj_set_size(s_btn_sec, BTN_SEC_W, BTN_H);
        lv_obj_set_pos(s_btn_sec, BTN_MARGIN + BTN_PRI_DUAL_W + BTN_GAP, BTN_Y);
        lv_obj_clear_flag(s_btn_sec, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_size(s_btn_pri, BTN_PRI_SOLO_W, BTN_H);
        lv_obj_set_pos(s_btn_pri, BTN_MARGIN, BTN_Y);
        lv_obj_add_flag(s_btn_sec, LV_OBJ_FLAG_HIDDEN);
    }
    /* Resize first, then re-fit labels so CLIP width matches the new shell. */
    layout_btn_label(s_btn_pri, s_btn_pri_lab);
    if (dual) layout_btn_label(s_btn_sec, s_btn_sec_lab);
}

static void paint_home(void);
static void paint_guide_overlay(void);
static void paint_hub(void);
static void paint_build(void);
static void paint_duty(void);
static void advance_guide(void);
static void skip_guide(void);
static void enter_page(ui_page_t page);

static void paint_pick(void);
static void paint_quest(void);

static void go_back(void)
{
    ashbox_sfx_play(ASH_SFX_BACK);
    if (s_page == UI_PICK && s_depart_step > 0) {
        s_depart_step--;
        s_list_sel = (s_depart_step == 0) ? (int)s_pick_tier
                     : (s_depart_step == 1) ? (int)s_pick_person
                                           : (int)s_pick_load;
        enter_page(UI_PICK);
        paint_pick();
        return;
    }
    if (s_page == UI_BUILD) {
        s_list_sel = 6; /* 建设 */
        enter_page(UI_HUB);
        paint_hub();
        return;
    }
    if (s_page == UI_SURVIVOR_DETAIL) {
        s_list_sel = (int)s_surv_pid;
        enter_page(UI_SURVIVORS);
        paint_survivors();
        return;
    }
    if (s_page == UI_RESTART_CONFIRM) {
        s_list_sel = 6;
        enter_page(UI_SETTINGS);
        paint_settings();
        return;
    }
    if (s_page == UI_DUTY || s_page == UI_SETTINGS || s_page == UI_HELP || s_page == UI_QUEST ||
        s_page == UI_ORDER || s_page == UI_JOURNAL || s_page == UI_STASH || s_page == UI_RES ||
        s_page == UI_EQUIP || s_page == UI_FARM || s_page == UI_TRADE ||
        s_page == UI_SURVIVORS || s_page == UI_ACHIEVE) {
        if (s_page == UI_SETTINGS) {
            ash_em_ui_t em = ashbox_embackup_ui_state();
            if (em == ASH_EM_UI_BACKUP_BUSY || em == ASH_EM_UI_IMPORT_BUSY) {
                ashbox_sfx_play(ASH_SFX_WARN);
                return; /* stay; do not abort an in-flight transfer via Back */
            }
            ashbox_embackup_leave_import();
            ashbox_embackup_clear_ui_pulse();
        }
        if (s_page == UI_DUTY) s_list_sel = 0;
        else if (s_page == UI_FARM) s_list_sel = 1;
        else if (s_page == UI_RES) s_list_sel = 2;
        else if (s_page == UI_STASH) s_list_sel = 3;
        else if (s_page == UI_SURVIVORS) s_list_sel = 4;
        else if (s_page == UI_EQUIP) s_list_sel = 5;
        else if (s_page == UI_TRADE) s_list_sel = 7;
        else if (s_page == UI_ORDER) s_list_sel = 8;
        else if (s_page == UI_QUEST) s_list_sel = 9;
        else if (s_page == UI_ACHIEVE) s_list_sel = 10;
        else if (s_page == UI_JOURNAL) s_list_sel = 11;
        else if (s_page == UI_SETTINGS) s_list_sel = 12;
        else s_list_sel = 13; /* help */
        enter_page(UI_HUB);
        paint_hub();
        return;
    }
    enter_page(UI_HOME);
    paint_home();
}

static void paint_home_threat(void)
{
    char buf[48];
    int threat = (int)s_g->threat;
    if (threat == s_threat_shown) return;
    s_threat_shown = threat;
    lv_bar_set_value(s_threat_bar, threat, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_threat_bar,
                              lv_color_hex(threat >= 80 ? COL_DANGER
                                                        : (threat >= 50 ? COL_AMBER : COL_LINE)),
                              LV_PART_INDICATOR);
    {
        const char *pace = ash_game_threat_pace_word(s_g);
        if (pace[0])
            snprintf(buf, sizeof(buf), "%u%%·%s", (unsigned)threat, pace);
        else
            snprintf(buf, sizeof(buf), "%u%%", (unsigned)threat);
    }
    label_set_if(s_threat_lbl, buf);
    lv_obj_set_style_text_color(s_threat_lbl, lv_color_hex(COL_DANGER), 0);
    /* 平时只留底边威胁条；高威胁才露百分比 */
    if (threat >= 80) lv_obj_clear_flag(s_threat_lbl, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_threat_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void paint_home(void)
{
    if (s_g) (void)ash_game_try_pocket_refill(s_g);
    s_page = UI_HOME;
    show_home_chrome(true);
    char buf[48];
    refresh_battery();

    /* 资源改到据点「资源」页；主页资源行留给状态/滚动消息 */
    lv_obj_add_flag(s_water, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_food, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_parts, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ember, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(s_status, 4, NOW_ROW_RES);
    lv_obj_set_width(s_status, 140);

    /* 远征/危机/战报已在「此刻」行表达，资源行不再重复；仅留虚弱等补充态 */
    if (ash_game_crisis_pending(s_g) || s_g->report.pending || s_g->exp.active) {
        label_set_if(s_status, "");
        lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (s_g->pop == 0) {
        label_set_if(s_status, "在等过路客上门");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_AMBER), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (ash_game_wanderer_pending(s_g)) {
        label_set_if(s_status, "门外有人");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_AMBER), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (ash_game_total_food(s_g) == 0 || ash_game_total_water(s_g) == 0) {
        label_set_if(s_status, ash_game_total_water(s_g) == 0 ? "口渴虚弱" : "全员虚弱");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_DANGER), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (s_g->hide_penalty_h > 0) {
        char hb[24];
        snprintf(hb, sizeof(hb), "躲藏约%u时", (unsigned)s_g->hide_penalty_h);
        label_set_if(s_status, hb);
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_AMBER), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else if (s_g->well_upgrade_remain_s > 0) {
        label_set_if(s_status, "升井中");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_AMBER), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        label_set_if(s_status, "站内安稳");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_SAFE), 0);
        lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    }

    paint_home_threat();

    set_radio_text(ash_game_radio(s_g));

    apply_sprite(home_scene_now(), false);

    set_modal_visible(false);

    set_bar_dual(true);
    /* 过路客申请挡在最前：不点留下/拒绝就不让开抉择、站事、出发 */
    if (ash_game_wanderer_pending(s_g)) {
        if (!s_wanderer_bell_armed) {
            s_wanderer_bell_armed = true;
            ashbox_sfx_play(ASH_SFX_BELL);
            power_attention_wake("wanderer");
        }
        label_set_if(s_card_title, "过路客上门");
        label_set_if(s_card_value, ash_game_wanderer_name(s_g));
        s_exp_rem_shown = UINT32_MAX;
        {
            char tip[72];
            const char *block = ash_game_wanderer_join_block_reason(s_g);
            if (ash_game_wanderer_can_join(s_g)) {
                snprintf(tip, sizeof(tip), "%s · 耗20口粮+10水",
                         ash_game_work_trait_name(ash_game_wanderer_trait(s_g)));
            } else {
                snprintf(tip, sizeof(tip), "%s · %s",
                         ash_game_work_trait_name(ash_game_wanderer_trait(s_g)),
                         block && block[0] ? block : "现在留不下");
            }
            set_card_sub_tip(tip);
        }
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "留下", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "打发", true, s_focus == 1);
        set_home_hint("必须先选留下或打发");
    } else if (ash_game_crisis_pending(s_g)) {
        s_wanderer_bell_armed = false;
        label_set_if(s_card_title, "站内危机");
        label_set_if(s_card_value, "威胁临门");
        s_exp_rem_shown = UINT32_MAX;
        set_card_sub_tip("必须先守夜");
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "守夜", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
        set_home_hint("短按确定 · 长按深睡");
    } else if (s_g->report.pending) {
        s_wanderer_bell_armed = false;
        label_set_if(s_card_title, "战报");
        label_set_if(s_card_value, s_g->report.branch_resolved ? "待入账" : "待抉择");
        s_exp_rem_shown = UINT32_MAX;
        if (!s_g->report.branch_resolved && s_g->report.tag_id) {
            snprintf(buf, sizeof(buf), "遭遇·%s · 左右抉择",
                     ash_game_tag_label(s_g->report.tag_id));
            set_card_sub_tip(buf);
        } else {
            set_card_sub_tip(s_g->report.branch_resolved ? "点按收下" : "醒着抉择");
        }
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0),
                s_g->report.branch_resolved ? "入账" : "抉择", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
        set_home_hint("短按确定 · 长按深睡");
    } else if (s_g->exp.active) {
        label_set_if(s_card_title, "远征中");
        uint32_t rem = s_g->exp.remaining_s;
        s_exp_rem_shown = rem;
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 (unsigned)(rem / 3600), (unsigned)((rem / 60) % 60), (unsigned)(rem % 60));
        label_set_if(s_card_value, buf);
        set_card_sub_tip(ash_game_wait_nudge(s_g));
        if (ash_game_chore_pending(s_g)) {
            set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "站事", true, s_focus == 0);
            set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
            set_home_hint(ash_game_chore_hint(s_g));
        } else {
            set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "远征中", false, false);
            set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
            if (s_focus != 1) s_focus = 1;
            set_home_hint("据点还有事 · 长按可深睡");
        }
    } else if (!ash_game_can_depart(s_g)) {
        s_wanderer_bell_armed = false;
        label_set_if(s_card_title, "远征");
        label_set_if(s_card_value, "禁止出发");
        s_exp_rem_shown = UINT32_MAX;
        set_card_sub_tip(ash_game_home_nudge(s_g));
        if (ash_game_duty_available(s_g)) {
            set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "日勤", true, s_focus == 0);
            set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
            set_home_hint("可做站内日勤 · 长按深睡");
        } else {
            set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "无法出发", false, false);
            set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
            if (s_focus != 1) s_focus = 1;
            set_home_hint("先把据点事处理完再出发 · 长按深睡");
        }
    } else {
        label_set_if(s_card_title, "远征");
        label_set_if(s_card_value, "可出发");
        s_exp_rem_shown = UINT32_MAX;
        set_card_sub_tip(ash_game_home_nudge(s_g));
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "出发", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
        if (ash_game_duty_left(s_g) > 0)
            set_home_hint("据点第一项是日勤 · 长按深睡");
        else if (ash_game_farm_ready_count(s_g) > 0 || ash_game_stash_full(s_g) ||
                 ash_game_trade_available(s_g) || ash_game_trade2_available(s_g))
            set_home_hint("顶上提示就是下一步 · 长按深睡");
        else if (ash_game_farm_unlocked(s_g))
            set_home_hint("据点可以去种菜 · 长按深睡");
        else
            set_home_hint("先走近路带回物资 · 长按深睡");
    }

    s_home_fp = home_ui_fingerprint();

    if (guide_active()) {
        paint_guide_overlay();
        return;
    }

    if (s_wake_guard) {
        set_modal_visible(true);
        label_set_if(s_modal_lines[0], "按任意键");
        label_set_if(s_modal_lines[1], "结束静默");
        label_set_if(s_modal_lines[2], "");
        label_set_if(s_modal_lines[3], "");
        label_set_if(s_modal_lines[4], "");
        label_set_if(lv_obj_get_child(s_modal_btn, 0), "继续");
    }
}

static void paint_guide_overlay(void)
{
    static const char *titles[ASH_GUIDE_DONE] = {
        "三条线做完才过", "今天先做", "人回来要醒着", "盖盖子时间照走", "线索一天一步", "菜地会累人",
    };
    static const char *body[ASH_GUIDE_DONE] = {
        "三条线做完才算过了这段日子\n铁皮来客 · 余烬低语 · 门后灰潮\n故事翻层后还能继续玩",
        "今天先派阿禾走近路\n上下选 · 确定键确认\n零件盖建设 · 余烬开闸去猎场",
        "人回来后醒着点战报\n抉择选完物资才入账\n门外过路客要先留下或打发",
        "盖上盖子时间照走\n深睡会补上这段时间\n断电倒计时不走 · 深睡才补",
        "每条线索一天只往前一步\n明天才能再推同一条\n帮助页还能再查怎么玩",
        "稻谷约1天基础30口粮\n红薯约3天基础240口粮\n田作每时疲劳+5 · 满80歇",
    };
    uint8_t step = s_cfg.guide_step;
    if (step >= ASH_GUIDE_DONE) step = ASH_GUIDE_DONE - 1u;
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], titles[step]);
    lv_label_set_text(s_modal_lines[1], body[step]);
    lv_label_set_text(s_modal_lines[2], "");
    lv_label_set_text(s_modal_lines[3], "");
    {
        char prog[16];
        snprintf(prog, sizeof(prog), "%u/%u", (unsigned)(step + 1u), (unsigned)ASH_GUIDE_DONE);
        lv_label_set_text(s_modal_lines[4], prog);
    }
    bool last = (step + 1u >= ASH_GUIDE_DONE);
    lv_label_set_text(lv_obj_get_child(s_modal_btn, 0), last ? "开始" : "下一步");
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), last ? "开始" : "下一步", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "确定继续 · 长按跳过");
}

static void advance_guide(void)
{
    if (s_cfg.guide_step < ASH_GUIDE_DONE) s_cfg.guide_step++;
    s_cfg_dirty = true; /* NVS after LVGL unlock */
    if (s_cfg.guide_step >= ASH_GUIDE_DONE) {
        ashbox_sfx_play(ASH_SFX_REWARD);
        set_modal_visible(false);
    } else {
        ashbox_sfx_play(ASH_SFX_CONFIRM);
    }
    paint_home();
}

static void skip_guide(void)
{
    s_cfg.guide_step = ASH_GUIDE_DONE;
    s_cfg_dirty = true;
    ashbox_sfx_play(ASH_SFX_BACK);
    set_modal_visible(false);
    paint_home();
}

#define PICK_TIER_VIEW 4

static void paint_pick(void)
{
    s_page = UI_PICK;
    show_home_chrome(false);
    set_modal_visible(false);
    char line[48];
    char sub[120];
    int count = 3;
    if (s_depart_step == 0) {
        lv_label_set_text(s_title, "选路线");
        count = ASH_TIER_COUNT;
        if (s_list_sel < 0) s_list_sel = 0;
        if (s_list_sel >= count) s_list_sel = count - 1;
        int view = PICK_TIER_VIEW;
        if (count < view) view = count;
        int top = 0;
        if (s_list_sel >= view) top = s_list_sel - view + 1;
        if (top > count - view) top = count - view;
        if (top < 0) top = 0;
        for (int slot = 0; slot < ASH_LIST_MAX; slot++) {
            lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        }
        for (int slot = 0; slot < view; slot++) {
            int i = top + slot;
            if (i >= count) break;
            bool open = ash_game_tier_unlocked(s_g, (ash_tier_t)i);
            if (open) {
                snprintf(line, sizeof(line), "%s · %s", ash_game_tier_name((ash_tier_t)i),
                         ash_game_tier_place((ash_tier_t)i));
                snprintf(sub, sizeof(sub), "  %s · %s   ·  ", ash_game_tier_time_hint((ash_tier_t)i),
                         ash_game_tier_loot_hint((ash_tier_t)i));
            } else {
                snprintf(line, sizeof(line), "%s · 闸门未开", ash_game_tier_name((ash_tier_t)i));
                snprintf(sub, sizeof(sub), "  余烬满3开闸 · 开后可走猎场 · 其余路掉落约+两成   ·  ");
            }
            lv_label_set_text(s_list_labels[slot], line);
            lv_label_set_text(s_list_subs[slot], sub);
            lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_long_mode(s_list_subs[slot], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
            lv_obj_set_width(s_list_labels[slot], 196);
            lv_obj_set_width(s_list_subs[slot], 196);
            lv_obj_set_style_anim_duration(s_list_subs[slot], 10000, LV_PART_MAIN);
            lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[slot], 216, 40);
            lv_obj_set_pos(s_list_rows[slot], 12, 12 + slot * 44);
            lv_obj_set_style_clip_corner(s_list_rows[slot], true, 0);
            lv_obj_set_pos(s_list_labels[slot], 10, 2);
            lv_obj_set_pos(s_list_subs[slot], 10, 20);
            lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
            bool sel = (s_list_sel == i);
            lv_obj_set_style_border_color(s_list_rows[slot],
                                          lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
            lv_obj_set_style_bg_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
        }
        paint_hub_scrollbar(s_list_sel, count, view);
        set_bar_dual(false);
        set_btn(s_btn_pri, s_btn_pri_lab, "下一步", true, true);
        s_focus = 0;
        lv_label_set_text(s_hint, "下行滚动看掉落与空手而归概率 · 帮助有说明");
        return;
    } else if (s_depart_step == 1) {
        lv_label_set_text(s_title, "选人");
        count = s_g->pop;
        if (count > ASHBOX_PERSON_MAX) count = ASHBOX_PERSON_MAX;
        for (int i = 0; i < count; i++) {
            snprintf(line, sizeof(line), "%s · %s", ash_game_person_name((uint8_t)i),
                     ash_game_person_trait((uint8_t)i));
            if (!ash_game_person_can_depart(s_g, (uint8_t)i)) {
                snprintf(sub, sizeof(sub), "  不可派出 · 疲%u   ·  ",
                         (unsigned)(*ash_game_person_mut(s_g, (unsigned)(i))).fatigue);
            } else {
                snprintf(sub, sizeof(sub), "  信任%+d降低空手而归概率 · 疲劳%u(60起概率升高)   ·  ",
                         (int)(*ash_game_person_mut(s_g, (unsigned)(i))).trust, (unsigned)(*ash_game_person_mut(s_g, (unsigned)(i))).fatigue);
            }
            lv_label_set_text(s_list_labels[i], line);
            lv_label_set_text(s_list_subs[i], sub);
            lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
            lv_obj_set_width(s_list_labels[i], 196);
            lv_obj_set_width(s_list_subs[i], 196);
            lv_obj_set_style_anim_duration(s_list_subs[i], 9000, LV_PART_MAIN);
            lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[i], 216, 48);
            lv_obj_set_pos(s_list_rows[i], 12, 12 + i * 52);
            lv_obj_set_pos(s_list_labels[i], 10, 4);
            lv_obj_set_pos(s_list_subs[i], 10, 26);
        }
    } else {
        lv_label_set_text(s_title, "携带");
        count = GEAR_PICK_COUNT;
        uint16_t fc = ash_game_expedition_food_cost(s_pick_tier);
        uint16_t wc = ash_game_expedition_water_cost(s_pick_tier);
        int crow_cap = ash_game_crowbar_carry_cap(s_g, s_pick_person);
        for (int i = 0; i < count; i++) {
            if (i == 0) {
                snprintf(line, sizeof(line), "轻装出门");
                snprintf(sub, sizeof(sub), "  口粮%u 水%u  ·  ", (unsigned)fc, (unsigned)wc);
            } else if (i == 1) {
                snprintf(line, sizeof(line), "带撬棍");
                snprintf(sub, sizeof(sub), "  仓有%u ·上限%u  ·  ", (unsigned)s_g->crowbar,
                         (unsigned)crow_cap);
            } else {
                snprintf(line, sizeof(line), "带手枪");
                snprintf(sub, sizeof(sub), "  手枪%u 子弹%u ·挡突然袭击·略稳  ·  ", (unsigned)s_g->pistol,
                         (unsigned)s_g->ammo);
            }
            lv_label_set_text(s_list_labels[i], line);
            lv_label_set_text(s_list_subs[i], sub);
            lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
            lv_obj_set_width(s_list_labels[i], 196);
            lv_obj_set_width(s_list_subs[i], 196);
            lv_obj_set_style_anim_duration(s_list_subs[i], 9000, LV_PART_MAIN);
            lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[i], 216, 48);
            lv_obj_set_pos(s_list_rows[i], 12, 12 + i * 52);
            lv_obj_set_pos(s_list_labels[i], 10, 4);
            lv_obj_set_pos(s_list_subs[i], 10, 26);
        }
    }
    paint_list_sel(count);
    set_bar_dual(false);
    set_btn(s_btn_pri, s_btn_pri_lab,
            (s_depart_step == 1) ? "下一步" : "确认出发", true, true);
    s_focus = 0;
    if (s_depart_step == 1) {
        lv_label_set_text(s_hint, "信任高更少空手而归 · 疲劳高更容易空手而归 · 个性偏口粮或零件");
    } else {
        uint16_t fc = ash_game_expedition_food_cost(s_pick_tier);
        uint16_t wc = ash_game_expedition_water_cost(s_pick_tier);
        char hb[56];
        if (s_pick_tier == ASH_TIER_DEEP || s_pick_tier == ASH_TIER_NIGHT) {
            snprintf(hb, sizeof(hb), "耗口粮%u水%u · 适合盖上再走", (unsigned)fc, (unsigned)wc);
        } else {
            snprintf(hb, sizeof(hb), "耗口粮%u水%u · 不够就不能走", (unsigned)fc, (unsigned)wc);
        }
        lv_label_set_text(s_hint, hb);
    }
}

#define HUB_COUNT 14
#define DUTY_COUNT ASHBOX_DUTY_KIND_N
#define BUILD_COUNT 15 /* +工作台；超级核心仍在末项 */
#define BUILD_VIEW 5
#define STASH_COUNT 4
#define STASH_VIEW 4
/* 资源页：仓位说明 + 物资概览 + 按人用药（制作已迁出） */
#define RES_MED_BASE 2
#define EQUIP_PERSON_ROWS (ASHBOX_PERSON_MAX * 2)
/* 装备页末尾：水罐/手枪/子弹/撬棍（工作台在建设页） */
#define EQUIP_CRAFT_N 4
#define EQUIP_CRAFT_BASE EQUIP_PERSON_ROWS
#define EQUIP_COUNT (EQUIP_PERSON_ROWS + EQUIP_CRAFT_N)

static ash_craft_t equip_craft_kind(int row)
{
    static const ash_craft_t k_map[EQUIP_CRAFT_N] = {
        ASH_CRAFT_BUCKET, ASH_CRAFT_PISTOL, ASH_CRAFT_AMMO, ASH_CRAFT_CROWBAR,
    };
    int off = row - EQUIP_CRAFT_BASE;
    if (off < 0 || off >= EQUIP_CRAFT_N) return ASH_CRAFT_BUCKET;
    return k_map[off];
}

static int res_count(void)
{
    int pop = s_g ? (int)s_g->pop : 0;
    if (pop < 0) pop = 0;
    if (pop > ASHBOX_PERSON_MAX) pop = ASHBOX_PERSON_MAX;
    return RES_MED_BASE + pop * 3;
}
#define HUB_VIEW 5
#define HUB_ROW_H 34
#define FARM_VIEW 4
#define TRADE_VIEW 4
#define SETTINGS_VIEW 5
#define JOURNAL_VIEW 6

static int hub_scroll_top(int sel, int count)
{
    return list_scroll_top(sel, count, HUB_VIEW);
}

static void paint_hub_scrollbar(int sel, int count, int view)
{
    if (!s_scroll_track || !s_scroll_thumb) return;
    if (view <= 0) view = HUB_VIEW;
    if (count <= view) {
        lv_obj_add_flag(s_scroll_track, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    int track_h = view * HUB_ROW_H;
    if (track_h > LIST_LAYER_H - 16) track_h = LIST_LAYER_H - 16;
    if (track_h < 24) track_h = 24;
    lv_obj_set_size(s_scroll_track, 4, track_h);
    lv_obj_set_pos(s_scroll_track, 232, 8);
    lv_obj_clear_flag(s_scroll_track, LV_OBJ_FLAG_HIDDEN);
    int thumb_h = (track_h * view) / count;
    if (thumb_h < 18) thumb_h = 18;
    if (thumb_h > track_h) thumb_h = track_h;
    int travel = track_h - thumb_h;
    int thumb_y = 0;
    if (count > 1 && travel > 0) thumb_y = (travel * sel) / (count - 1);
    lv_obj_set_size(s_scroll_thumb, 4, thumb_h);
    lv_obj_set_pos(s_scroll_thumb, 0, thumb_y);
    lv_obj_clear_flag(s_scroll_thumb, LV_OBJ_FLAG_HIDDEN);
}


static void paint_survivors(void)
{
    s_page = UI_SURVIVORS;
    show_home_chrome(false);
    lv_label_set_text(s_title, "幸存者");
    int n = s_g ? (int)s_g->pop : 0;
    if (n < 1) n = 1;
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= n) s_list_sel = n - 1;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    int view = 5;
    if (n < view) view = n;
    int top = list_scroll_top(s_list_sel, n, view);
    char line[48];
    char sub[40];
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= n) break;
        if (s_g->pop == 0) {
            snprintf(line, sizeof(line), "站里没人");
            snprintf(sub, sizeof(sub), "  等过路客  ·  ");
        } else {
            uint8_t pid = (uint8_t)i;
            snprintf(line, sizeof(line), "%s · %s", ash_game_person_name(pid),
                     ash_game_person_state_label((*ash_game_person_at(s_g, pid)).state));
            snprintf(sub, sizeof(sub), "  疲劳%u · %s  ·  ",
                     (unsigned)(*ash_game_person_at(s_g, pid)).fatigue,
                     ash_game_job_name(ash_game_person_job(s_g, pid)));
        }
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, n, view);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), s_g->pop ? "看看" : "返回", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "点开看派工和装备 · 长按返回");
}

static void paint_survivor_detail(void)
{
    s_page = UI_SURVIVOR_DETAIL;
    show_home_chrome(false);
    uint8_t pid = s_surv_pid;
    if (pid >= s_g->pop) pid = 0;
    lv_label_set_text(s_title, ash_game_person_name(pid));
    const ash_person_t *p = ash_game_person_at(s_g, pid);
    char line[56];
    char sub[48];
    int n = 7;
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= n) s_list_sel = n - 1;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    int view = 5;
    if (n < view) view = n;
    int top = list_scroll_top(s_list_sel, n, view);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= n) break;
        if (i == 0) {
            snprintf(line, sizeof(line), "状态  %s", ash_game_person_state_label(p->state));
            if (p->injure_remain_s > 0)
                snprintf(sub, sizeof(sub), "  受伤约剩%u时  ·  ",
                         (unsigned)((p->injure_remain_s + 3599u) / 3600u));
            else
                snprintf(sub, sizeof(sub), "  疲劳%u  ·  ", (unsigned)p->fatigue);
        } else if (i == 1) {
            snprintf(line, sizeof(line), "信任  %d", (int)p->trust);
            snprintf(sub, sizeof(sub), "  高了更会拼命  ·  ");
        } else if (i == 2) {
            snprintf(line, sizeof(line), "本事");
            snprintf(sub, sizeof(sub), "  %s  ·  ",
                     ash_game_work_trait_name(ash_game_person_work_trait(s_g, pid)));
        } else if (i == 3) {
            snprintf(line, sizeof(line), "派工  %s", ash_game_job_name(ash_game_person_job(s_g, pid)));
            snprintf(sub, sizeof(sub), "  确定切换休整/田作/工坊  ·  ");
        } else if (i == 4) {
            snprintf(line, sizeof(line), "背包  %s",
                     (*ash_game_equip_bp_at(s_g, pid)) ? "已装" : "未装");
            snprintf(sub, sizeof(sub), "  去装备页拿下  ·  ");
        } else if (i == 5) {
            snprintf(line, sizeof(line), "手枪  %s",
                     (*ash_game_equip_pis_at(s_g, pid)) ? "已挂" : "未挂");
            snprintf(sub, sizeof(sub), "  去装备页拿下  ·  ");
        } else {
            snprintf(line, sizeof(line), "请出营地");
            char reason[40];
            if (ash_game_exile_blocked_reason(s_g, pid, reason, sizeof(reason)))
                snprintf(sub, sizeof(sub), "  %s  ·  ", reason);
            else
                snprintf(sub, sizeof(sub), "  装备留在营地 · 需再确认  ·  ");
        }
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 6 + slot * 34);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, n, view);
    set_bar_dual(false);
    bool can_job = (s_list_sel == 3 && p->state != ASH_PERSON_AWAY);
    bool can_exile = (s_list_sel == 6);
    const char *cta = can_job ? "换派工" : (can_exile ? "离开" : "返回");
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, true, true);
    s_focus = 0;
    if (can_exile) lv_label_set_text(s_hint, "再确认一次才会离开 · 长按返回");
    else if (can_job) lv_label_set_text(s_hint, "田作工坊每时疲劳+5 · 满80歇 · 长按返回");
    else lv_label_set_text(s_hint, "本事来自过路客 · 长按返回");
}

static void paint_achieve(void)
{
    s_page = UI_ACHIEVE;
    show_home_chrome(false);
    lv_label_set_text(s_title, "成就");
    static const uint16_t bits[5] = {
        ASH_ACH_LONE, ASH_ACH_TOGETHER, ASH_ACH_CROWD, ASH_ACH_PACIFIST, ASH_ACH_NOBODY};
    int n = 5;
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= n) s_list_sel = n - 1;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    int view = 5;
    int top = list_scroll_top(s_list_sel, n, view);
    char line[48];
    char sub[56];
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= n) break;
        uint16_t bit = bits[i];
        bool on = ashbox_meta_has(&s_meta, bit);
        snprintf(line, sizeof(line), "%s%s", on ? "" : "·", ash_ach_name(bit));
        snprintf(sub, sizeof(sub), "  %s  ·  ", on ? ash_ach_grant(bit) : ash_ach_cond(bit));
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        lv_obj_set_style_text_color(s_list_labels[slot],
                                    lv_color_hex(on ? COL_GOLD : COL_MUTED), 0);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, n, view);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "返回", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "本局解锁要重新开始才生效");
}

static void paint_restart_confirm(void)
{
    s_page = UI_RESTART_CONFIRM;
    show_home_chrome(false);
    lv_label_set_text(s_title, "重新开始");
    for (int i = 0; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], "只留成就进度");
    lv_label_set_text(s_modal_lines[1], "本局物资建档全清");
    lv_label_set_text(s_modal_lines[2], "超级核心用过就没了");
    lv_label_set_text(s_modal_lines[3], "");
    lv_label_set_text(s_modal_lines[4], "");
    set_bar_dual(true);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "取消", true, s_focus == 0);
    set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "确认重开", true, s_focus == 1);
    if (s_focus != 0 && s_focus != 1) s_focus = 0;
    lv_label_set_text(s_hint, "默认先停在取消");
}

static void paint_hub(void)
{
    s_page = UI_HUB;
    show_home_chrome(false);
    lv_label_set_text(s_title, "据点");
    char buf[64];
    int ready = ash_game_farm_ready_count(s_g);
    const char *names[] = {"日勤", "菜地", "资源", "仓库", "幸存者", "装备", "建设", "交易", "站令",
                           "线索", "成就", "纪事", "设置", "帮助"};
    int top = hub_scroll_top(s_list_sel, HUB_COUNT);
    for (int i = 0; i < HUB_COUNT; i++) {
        lv_obj_add_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(s_list_labels[i], 188);
        if (i == 0) {
            unsigned left = ash_game_duty_left(s_g);
            if (s_g->exp.active) snprintf(buf, sizeof(buf), "日勤  远征中不可");
            else if (left > 0) snprintf(buf, sizeof(buf), "日勤  今日余%u", left);
            else snprintf(buf, sizeof(buf), "日勤  今日已满");
        } else if (i == 1) {
            if (!ash_game_farm_unlocked(s_g)) snprintf(buf, sizeof(buf), "菜地  食堂升到2级");
            else if (ready > 0) snprintf(buf, sizeof(buf), "菜地  可收%d块田地", ready);
            else if (ash_game_root_unlocked(s_g))
                snprintf(buf, sizeof(buf), "菜地  稻谷快/红薯饱");
            else
                snprintf(buf, sizeof(buf), "菜地  可种稻谷");
        } else if (i == 2) {
            snprintf(buf, sizeof(buf), "资源  仓位%u/%u",
                     (unsigned)ash_game_stash_slots_used(s_g),
                     (unsigned)ash_game_stash_slots_cap(s_g));
        } else if (i == 3) {
            snprintf(buf, sizeof(buf), "仓库  %u级 ·可扩容家具", (unsigned)s_g->stash_lv);
        } else if (i == 4) {
            snprintf(buf, sizeof(buf), "幸存者  %u人", (unsigned)s_g->pop);
        } else if (i == 5) {
            snprintf(buf, sizeof(buf), "装备  背包手枪");
        } else if (i == 7) {
            if (ash_game_trade2_available(s_g))
                snprintf(buf, sizeof(buf), "交易  渡口今日可换");
            else if (ash_game_trade_available(s_g))
                snprintf(buf, sizeof(buf), "交易  铁棚今日可换");
            else if (ash_game_camp2_open(s_g) || ash_game_camp_open(s_g))
                snprintf(buf, sizeof(buf), "交易  今日已换");
            else
                snprintf(buf, sizeof(buf), "交易  未通联");
        } else if (i == 8) {
            snprintf(buf, sizeof(buf), "站令  %s",
                     ash_game_order_name((ash_night_order_t)s_g->night_order));
        } else if (i == 9) {
            const char *qn = NULL;
            for (uint8_t q = 0; q < ASHBOX_QUEST_N; q++) {
                bool done = (q < ASHBOX_QUEST_CORE) ? (s_g->quest_done[q] != 0)
                                                    : (s_g->quest2_done != 0);
                if (done) continue;
                qn = ash_game_quest_name(q);
                break;
            }
            if (qn) snprintf(buf, sizeof(buf), "线索  %s", qn);
            else snprintf(buf, sizeof(buf), "线索");
        } else if (i == 10) {
            unsigned got = 0;
            for (uint16_t b = 1; b <= ASH_ACH_ALL_MASK; b <<= 1) {
                if (ashbox_meta_has(&s_meta, b)) got++;
            }
            snprintf(buf, sizeof(buf), "成就  已获%u/5", got);
        } else {
            snprintf(buf, sizeof(buf), "%s", names[i]);
        }
        lv_label_set_text(s_list_labels[i], buf);
        if (i < top || i >= top + HUB_VIEW) {
            lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int vis = i - top;
        lv_obj_set_size(s_list_rows[i], 208, HUB_ROW_H - 4);
        lv_obj_set_pos(s_list_rows[i], 12, 20 + vis * HUB_ROW_H);
        lv_obj_set_pos(s_list_labels[i], 10, 6);
        lv_obj_clear_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[i],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[i],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    for (int i = HUB_COUNT; i < ASH_LIST_MAX; i++) {
        lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    paint_hub_scrollbar(s_list_sel, HUB_COUNT, HUB_VIEW);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "进入", true, true);
    s_focus = 0;
    if (s_list_sel == 0) lv_label_set_text(s_hint, "醒着可做日勤 · 做完疲劳+5");
    else if (s_list_sel == 1) lv_label_set_text(s_hint, "稻谷约1天30 · 红薯约3天240");
    else if (s_list_sel == 4) lv_label_set_text(s_hint, "可派工 · 也可请出营地");
    else if (s_list_sel == 9) lv_label_set_text(s_hint, "每条线一天只往前一步");
    else lv_label_set_text(s_hint, "每项点进完整一页 · 长按返回");
}

static void paint_build(void)
{
    s_page = UI_BUILD;
    show_home_chrome(false);
    lv_label_set_text(s_title, "建设");
    char buf[64];
    const char *names[] = {"宿舍", "食堂", "工坊", "闸门", "仓库扩容", "仓家具", "猎装", "静音轮",
                           "门扣", "灶层", "匣灯", "深坑", "水井", "工作台", "超级核心"};
    uint8_t lvs[] = {s_g->dorm_lv, s_g->kitchen_lv, s_g->workshop_lv, s_g->gate_unlocked};
    int view = BUILD_VIEW;
    if (BUILD_COUNT < view) view = BUILD_COUNT;
    int top = 0;
    if (s_list_sel >= view) top = s_list_sel - view + 1;
    if (top > BUILD_COUNT - view) top = BUILD_COUNT - view;
    if (top < 0) top = 0;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= BUILD_COUNT) break;
        lv_obj_add_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(s_list_labels[slot], 196);
        bool cost_row = false;
        bool enough = true;
        if (i < 3) {
            uint16_t cost = ash_game_upgrade_cost(s_g, i);
            int bmax = ash_game_build_lv_max(s_g);
            if ((int)lvs[i] >= bmax) {
                snprintf(buf, sizeof(buf), "%s  %u级", names[i], (unsigned)lvs[i]);
            } else {
                snprintf(buf, sizeof(buf), "%s  %u级 · %u零件", names[i], (unsigned)lvs[i],
                         (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 3) {
            if (s_g->gate_unlocked) {
                snprintf(buf, sizeof(buf), "%s  已开", names[i]);
            } else {
                snprintf(buf, sizeof(buf), "%s  未开 · 3余烬开猎场", names[i]);
                cost_row = true;
                enough = s_g->ember >= 3;
            }
        } else if (i == 4) {
            uint16_t cost = ash_game_stash_upgrade_cost(s_g);
            if (s_g->stash_lv >= ASHBOX_STASH_LV_MAX) {
                snprintf(buf, sizeof(buf), "%s  %u级", names[i], (unsigned)s_g->stash_lv);
            } else {
                snprintf(buf, sizeof(buf), "%s  %u级 · %u零件", names[i],
                         (unsigned)s_g->stash_lv, (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 5) {
            uint16_t cost = ash_game_furn_upgrade_cost(s_g);
            if (s_g->furn_lv >= ASHBOX_FURN_LV_MAX) {
                snprintf(buf, sizeof(buf), "货架+干燥  已满");
            } else if (s_g->furn_lv == 0) {
                snprintf(buf, sizeof(buf), "货架 仓+20 ·%u零件", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            } else {
                snprintf(buf, sizeof(buf), "信物装干燥架 ·%u零件", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost && s_g->item[ASH_ITEM_TOKEN] > 0;
            }
        } else if (i == 6) {
            uint16_t ce = ash_game_gear_upgrade_cost_ember(s_g);
            uint16_t cp = ash_game_gear_upgrade_cost_parts(s_g);
            if (s_g->gear_lv >= ASHBOX_GEAR_LV_MAX) {
                snprintf(buf, sizeof(buf), "猎装  %u级", (unsigned)s_g->gear_lv);
            } else {
                snprintf(buf, sizeof(buf), "猎装  %u级 ·%u余烬+%u零件", (unsigned)s_g->gear_lv,
                         (unsigned)ce, (unsigned)cp);
                cost_row = true;
                enough = s_g->ember >= ce && s_g->parts >= cp;
            }
        } else if (i == 7) {
            if (s_g->quiet_gear) {
                snprintf(buf, sizeof(buf), "静音轮  已装");
            } else {
                uint16_t cost = ash_game_quiet_gear_cost(s_g);
                snprintf(buf, sizeof(buf), "静音轮 ·%u零件", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 8) {
            if (s_g->door_bolt) {
                snprintf(buf, sizeof(buf), "门扣  已装");
            } else if (!s_g->quiet_gear) {
                snprintf(buf, sizeof(buf), "门扣  需先装静音轮");
            } else {
                uint16_t cost = ash_game_door_bolt_cost(s_g);
                snprintf(buf, sizeof(buf), "门扣 ·%u零件", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 9) {
            if (ash_game_has_stove_lining(s_g)) {
                snprintf(buf, sizeof(buf), "灶层  已装");
            } else if (!s_g->door_bolt) {
                snprintf(buf, sizeof(buf), "灶层  需先装门扣");
            } else {
                uint16_t cost = ash_game_stove_lining_cost(s_g);
                snprintf(buf, sizeof(buf), "灶层 ·%u零件 ·口粮更省", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 10) {
            if (ash_game_has_box_lamp(s_g)) {
                snprintf(buf, sizeof(buf), "匣灯  已装");
            } else if (s_g->gear_lv < ASHBOX_GEAR_LV_MAX) {
                snprintf(buf, sizeof(buf), "匣灯  需猎装满级");
            } else {
                uint16_t cost = ash_game_box_lamp_cost(s_g);
                snprintf(buf, sizeof(buf), "匣灯 ·%u余烬 ·空手威胁轻", (unsigned)cost);
                cost_row = true;
                enough = s_g->ember >= cost;
            }
        } else if (i == 11) {
            if (s_g->well_lv >= ASHBOX_WELL_LV_PIT) {
                snprintf(buf, sizeof(buf), "深坑  已挖");
            } else if (!s_g->shovel) {
                snprintf(buf, sizeof(buf), "深坑  需先带回铁铲");
            } else {
                uint16_t cost = ash_game_pit_cost(s_g);
                snprintf(buf, sizeof(buf), "深坑 ·%u零件 ·渗水", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 12) {
            if (s_g->well_lv >= ASHBOX_WELL_LV_WELL) {
                snprintf(buf, sizeof(buf), "水井  已成");
            } else if (s_g->well_upgrade_remain_s > 0) {
                snprintf(buf, sizeof(buf), "水井  施工中");
            } else if (s_g->well_lv != ASHBOX_WELL_LV_PIT) {
                snprintf(buf, sizeof(buf), "水井  需先有深坑");
            } else {
                uint16_t cost = ash_game_well_upgrade_cost(s_g);
                snprintf(buf, sizeof(buf), "升水井 ·%u零件 ·井停一天", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost && !s_g->exp.active;
            }
        } else if (i == 13) {
            if (s_g->craft_bench) {
                snprintf(buf, sizeof(buf), "工作台  已支好");
            } else {
                snprintf(buf, sizeof(buf), "工作台 ·%u零件+%u铁 ·解锁制作",
                         (unsigned)ASHBOX_CRAFT_BENCH_PARTS, (unsigned)ASHBOX_CRAFT_BENCH_IRON);
                cost_row = true;
                enough = ash_game_craft_can(s_g, ASH_CRAFT_BENCH);
            }
        } else {
            if (s_g->build_cap_bonus > 0) {
                snprintf(buf, sizeof(buf), "超级核心  已用");
            } else if (s_g->super_core == 0) {
                snprintf(buf, sizeof(buf), "超级核心  没有");
            } else {
                snprintf(buf, sizeof(buf), "用超级核心 ·田地建档远征");
                cost_row = true;
                enough = true;
            }
        }
        lv_label_set_text(s_list_labels[slot], buf);
        if (cost_row) set_cost_color(s_list_labels[slot], enough);
        else lv_obj_set_style_text_color(s_list_labels[slot], lv_color_hex(COL_TEXT), 0);
        lv_obj_set_size(s_list_rows[slot], 216, 30);
        lv_obj_set_pos(s_list_rows[slot], 12, 10 + slot * 34);
        lv_obj_set_pos(s_list_labels[slot], 10, 6);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, BUILD_COUNT, BUILD_VIEW);
    set_modal_visible(false);
    const char *cta = "升级";
    if (s_list_sel == 3) cta = "解锁";
    else if (s_list_sel == 7 || s_list_sel == 8 || s_list_sel == 9 || s_list_sel == 10) cta = "安装";
    else if (s_list_sel == 13) cta = s_g->craft_bench ? "已有" : "去做";
    else if (s_list_sel == 14) cta = "用掉";
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, true, true);
    s_focus = 0;
    if (s_list_sel == 14)
        lv_label_set_text(s_hint, "用一次：田地变4块 · 建顶升高 · 远征多一半");
    else if (s_list_sel == 13)
        lv_label_set_text(s_hint, "工作台支好后 · 去装备页做水罐手枪弹药撬棍");
    else if (s_list_sel == 10)
        lv_label_set_text(s_hint, "匣灯=空手而归时威胁少涨一点");
    else if (s_list_sel == 9)
        lv_label_set_text(s_hint, "灶层=口粮更省 · 要先装门扣");
    else if (s_list_sel == 8)
        lv_label_set_text(s_hint, "门扣=把门加固 · 威胁涨得更慢 · 修门更省");
    else if (s_list_sel == 7)
        lv_label_set_text(s_hint, "静音轮=降低噪音 · 威胁涨得慢");
    else if (s_list_sel == 6)
        lv_label_set_text(s_hint, ash_game_gear_hint(s_g));
    else if (s_list_sel == 2)
        lv_label_set_text(s_hint, "零件更快 · 门外更噪");
    else if (s_list_sel == 0)
        lv_label_set_text(s_hint, "L3升满歇得快 · 口粮不加");
    else if (s_list_sel == 3)
        lv_label_set_text(s_hint, "开闸解锁猎场 · 近路废街深处过夜掉落约多两成 · 修门更省");
    else if (s_list_sel == 5)
        lv_label_set_text(s_hint, "加仓防满停产 · 催熟加快田地");
    else if (s_list_sel == 4)
        lv_label_set_text(s_hint, "仓满会停产 · 先扩容");
    else if (s_list_sel == 1)
        lv_label_set_text(s_hint, "食堂降日耗 · 高档解锁菜地");
    else if (s_list_sel == 12)
        lv_label_set_text(s_hint, "升井只停产水 · 别处照常");
    else
        lv_label_set_text(s_hint, "往下翻还有工作台、灶层、匣灯和水井");
}

static void paint_stash(void)
{
    s_page = UI_STASH;
    show_home_chrome(false);
    lv_label_set_text(s_title, "仓库");
    char line[48];
    char sub[72];
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= STASH_COUNT) s_list_sel = STASH_COUNT - 1;
    int view = STASH_VIEW;
    if (STASH_COUNT < view) view = STASH_COUNT;
    int top = 0;
    if (s_list_sel >= view) top = s_list_sel - view + 1;
    if (top > STASH_COUNT - view) top = STASH_COUNT - view;
    if (top < 0) top = 0;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= STASH_COUNT) break;
        /* 升级置顶：进仓库立刻看到扩容，无需滚到底 */
        bool cost_row = false;
        bool enough = true;
        if (i == 0) {
            uint16_t cost = ash_game_stash_upgrade_cost(s_g);
            snprintf(line, sizeof(line), "扩容仓库");
            if (s_g->stash_lv >= ASHBOX_STASH_LV_MAX)
                snprintf(sub, sizeof(sub), "  已满 %u级   ·  ", (unsigned)s_g->stash_lv);
            else {
                snprintf(sub, sizeof(sub), "  %u级升到%u ·%u零件  ·  ", (unsigned)s_g->stash_lv,
                         (unsigned)(s_g->stash_lv + 1u), (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            }
        } else if (i == 1) {
            uint16_t cost = ash_game_furn_upgrade_cost(s_g);
            snprintf(line, sizeof(line), "仓家具");
            if (s_g->furn_lv >= ASHBOX_FURN_LV_MAX)
                snprintf(sub, sizeof(sub), "  货架+干燥 已满  ·  ");
            else if (s_g->furn_lv == 0) {
                snprintf(sub, sizeof(sub), "  货架仓+20 ·%u零件  ·  ", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost;
            } else {
                snprintf(sub, sizeof(sub), "  信物装干燥架 ·%u零件  ·  ", (unsigned)cost);
                cost_row = true;
                enough = s_g->parts >= cost && s_g->item[ASH_ITEM_TOKEN] > 0;
            }
        } else if (i == 2) {
            snprintf(line, sizeof(line), "干肉换口粮");
            snprintf(sub, sizeof(sub), "  干肉%u ·换+4口粮  ·  ",
                     (unsigned)s_g->item[ASH_ITEM_JERKY]);
            cost_row = true;
            enough = s_g->item[ASH_ITEM_JERKY] > 0;
        } else {
            snprintf(line, sizeof(line), "物品一览");
            snprintf(sub, sizeof(sub), "  铲%u 罐%u 手枪%u 子弹%u 撬棍%u 背包%u  ·  ",
                     (unsigned)s_g->shovel, (unsigned)s_g->bucket, (unsigned)s_g->pistol,
                     (unsigned)s_g->ammo, (unsigned)s_g->crowbar, (unsigned)s_g->backpack);
        }
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        lv_obj_set_style_text_color(s_list_labels[slot], lv_color_hex(COL_TEXT), 0);
        if (cost_row) set_cost_color(s_list_subs[slot], enough);
        else lv_obj_set_style_text_color(s_list_subs[slot], lv_color_hex(COL_MUTED), 0);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[slot], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_width(s_list_labels[slot], 196);
        lv_obj_set_width(s_list_subs[slot], 196);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, STASH_COUNT, view);
    set_bar_dual(false);
    const char *cta = (s_list_sel <= 1) ? "升级" : (s_list_sel == 3 ? "查看" : "执行");
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, true, true);
    s_focus = 0;
    if (s_list_sel == 0)
        lv_label_set_text(s_hint, "零件扩仓 · 仓满了东西会白丢");
    else if (s_list_sel == 1)
        lv_label_set_text(s_hint, "货架能加仓 · 干燥架能催熟菜地");
    else
        lv_label_set_text(s_hint, "物资会自动入仓 · 长按返回");
}

static void paint_res(void)
{
    s_page = UI_RES;
    show_home_chrome(false);
    lv_label_set_text(s_title, "资源");
    char line[48];
    char sub[96];
    int n = res_count();
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= n) s_list_sel = n - 1;
    int view = 5;
    if (n < view) view = n;
    int top = list_scroll_top(s_list_sel, n, view);
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= n) break;
        bool cost = false;
        bool enough = true;
        if (i == 0) {
            snprintf(line, sizeof(line), "仓位 %u/%u", (unsigned)ash_game_stash_slots_used(s_g),
                     (unsigned)ash_game_stash_slots_cap(s_g));
            snprintf(sub, sizeof(sub),
                     "  口粮300占1仓位 ·铁50占1仓位 ·水罐/背包各占1仓位  ·  ");
        } else if (i == 1) {
            snprintf(line, sizeof(line), "口粮%u 水%u/%u", (unsigned)ash_game_total_food(s_g),
                     (unsigned)ash_game_total_water(s_g), (unsigned)ash_game_water_cap(s_g));
            snprintf(sub, sizeof(sub), "  铁%u 零件%u 余烬%u  ·  ", (unsigned)s_g->iron,
                     (unsigned)ash_game_total_parts(s_g), (unsigned)s_g->ember);
        } else {
            int off = i - RES_MED_BASE;
            uint8_t pid = (uint8_t)(off / 3);
            int mid = off % 3;
            const char *who = ash_game_person_name(pid);
            const char *st = ash_game_person_state_label((*ash_game_person_mut(s_g, (unsigned)(pid))).state);
            if (mid == 0) {
                snprintf(line, sizeof(line), "%s·%s", who, ash_content_med_name_milk());
                snprintf(sub, sizeof(sub), "  剩%u ·清累 ·%s  ·  ", (unsigned)s_g->med_milk, st);
                enough = s_g->med_milk > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state != ASH_PERSON_AWAY;
            } else if (mid == 1) {
                snprintf(line, sizeof(line), "%s·%s", who, ash_content_med_name_stim());
                snprintf(sub, sizeof(sub), "  剩%u ·压重伤 ·%s  ·  ", (unsigned)s_g->med_stim, st);
                enough = s_g->med_stim > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state == ASH_PERSON_INJURED;
            } else {
                snprintf(line, sizeof(line), "%s·%s", who, ash_content_med_name_abx());
                snprintf(sub, sizeof(sub), "  剩%u ·退热伤 ·%s  ·  ", (unsigned)s_g->med_abx, st);
                enough = s_g->med_abx > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state == ASH_PERSON_INFECTED;
            }
            cost = true;
        }
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        if (cost) set_cost_color(s_list_subs[slot], enough);
        else lv_obj_set_style_text_color(s_list_subs[slot], lv_color_hex(COL_MUTED), 0);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, n, view);
    set_bar_dual(false);
    bool on_med = (s_list_sel >= RES_MED_BASE);
    if (on_med) {
        int off = s_list_sel - RES_MED_BASE;
        uint8_t pid = (uint8_t)(off / 3);
        int mid = off % 3;
        bool can = false;
        if (mid == 0) can = s_g->med_milk > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state != ASH_PERSON_AWAY;
        else if (mid == 1) can = s_g->med_stim > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state == ASH_PERSON_INJURED;
        else can = s_g->med_abx > 0 && (*ash_game_person_mut(s_g, (unsigned)(pid))).state == ASH_PERSON_INFECTED;
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), can ? "用" : "不能", can, true);
        lv_label_set_text(s_hint, "药可从撬棍掉落或交易换到 · 按人用药");
    } else if (s_list_sel == 0) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "查看", true, true);
        lv_label_set_text(s_hint, "仓满会白丢东西 · 去仓库扩容");
    } else {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "查看", true, true);
        lv_label_set_text(s_hint, "做水罐手枪等请去装备页 · 工作台在建设");
    }
    s_focus = 0;
}

static void paint_equip(void)
{
    s_page = UI_EQUIP;
    show_home_chrome(false);
    lv_label_set_text(s_title, "装备");
    char line[48];
    char sub[64];
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= EQUIP_COUNT) s_list_sel = EQUIP_COUNT - 1;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    int view = 5;
    if (EQUIP_COUNT < view) view = EQUIP_COUNT;
    int top = list_scroll_top(s_list_sel, EQUIP_COUNT, view);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= EQUIP_COUNT) break;
        bool cost_row = false;
        bool enough = true;
        if (i >= EQUIP_CRAFT_BASE) {
            ash_craft_t ck = equip_craft_kind(i);
            snprintf(line, sizeof(line), "%s", ash_game_craft_label(ck));
            snprintf(sub, sizeof(sub), "  %s  ·  ", ash_game_craft_hint(ck));
            cost_row = true;
            enough = ash_game_craft_can(s_g, ck);
        } else {
            uint8_t pid = (uint8_t)(i / 2);
            bool is_gun = (i % 2) == 1;
            if (pid >= s_g->pop) {
                snprintf(line, sizeof(line), "空位");
                snprintf(sub, sizeof(sub), "  等人入住  ·  ");
            } else if (!is_gun) {
                snprintf(line, sizeof(line), "%s·背包", ash_game_person_name(pid));
                if ((*ash_game_equip_bp_mut(s_g, (unsigned)(pid))))
                    snprintf(sub, sizeof(sub), "  已装 ·点取下  ·  ");
                else
                    snprintf(sub, sizeof(sub), "  未装 ·仓有%u  ·  ", (unsigned)s_g->backpack);
            } else {
                snprintf(line, sizeof(line), "%s·手枪", ash_game_person_name(pid));
                if ((*ash_game_equip_pis_mut(s_g, (unsigned)(pid))))
                    snprintf(sub, sizeof(sub), "  已挂 ·点取下  ·  ");
                else
                    snprintf(sub, sizeof(sub), "  未挂 ·仓有%u  ·  ", (unsigned)s_g->pistol);
            }
        }
        lv_label_set_text(s_list_labels[slot], line);
        lv_label_set_text(s_list_subs[slot], sub);
        if (cost_row) set_cost_color(s_list_subs[slot], enough);
        else lv_obj_set_style_text_color(s_list_subs[slot], lv_color_hex(COL_MUTED), 0);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 1);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, EQUIP_COUNT, view);
    set_bar_dual(false);
    if (s_list_sel >= EQUIP_CRAFT_BASE) {
        bool can = ash_game_craft_can(s_g, equip_craft_kind(s_list_sel));
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), can ? "去做" : "不够", can, true);
        lv_label_set_text(s_hint,
                          s_g->craft_bench ? "工作台已在建设页支好 · 这里做用具"
                                          : "先去建设页做工作台，才能在这里制作");
    } else {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "切换", true, true);
        lv_label_set_text(s_hint, "同一人不重复装同一件 · 往下可做水罐手枪");
    }
    s_focus = 0;
}

static void paint_farm(void)
{
    s_page = UI_FARM;
    show_home_chrome(false);
    lv_label_set_text(s_title, "菜地");
    char line[40];
    char sub[72];
    char st[32];
    s_farm_n = 0;
    if (!ash_game_farm_unlocked(s_g)) {
        lv_label_set_text(s_list_labels[0], "食堂未达标");
        lv_label_set_text(s_list_subs[0], "  升到食堂Lv2   ·  ");
        lv_obj_clear_flag(s_list_subs[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[0], 216, 48);
        lv_obj_set_pos(s_list_rows[0], 12, 40);
        lv_obj_clear_flag(s_list_rows[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        paint_list_sel(1);
        set_bar_dual(false);
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "返回", true, true);
        s_focus = 0;
        lv_label_set_text(s_hint, "确定返回");
        return;
    }
    bool root_ok = ash_game_root_unlocked(s_g);
    for (uint8_t plot = 0; plot < ASHBOX_FARM_PLOTS; plot++) {
        ash_game_farm_status(s_g, plot, st, sizeof(st));
        if ((*ash_game_farm_mut(s_g, (unsigned)(plot))).crop == 0) {
            s_farm_plot[s_farm_n] = plot;
            s_farm_act[s_farm_n] = 0;
            snprintf(line, sizeof(line), "田地%u种稻谷", (unsigned)(plot + 1));
            if (s_g->item[ASH_ITEM_SEED] == 0)
                snprintf(sub, sizeof(sub), "  没有稻谷种 ·去交易或远征  ·  ");
            else if (s_g->pace_era == ASHBOX_PACE_V13)
                snprintf(sub, sizeof(sub), "  种%u ·约1天 ·基础30  ·  ",
                         (unsigned)s_g->item[ASH_ITEM_SEED]);
            else
                snprintf(sub, sizeof(sub), "  种%u ·约4时 ·收+4~6  ·  ",
                         (unsigned)s_g->item[ASH_ITEM_SEED]);
            lv_label_set_text(s_list_labels[s_farm_n], line);
            lv_label_set_text(s_list_subs[s_farm_n], sub);
            set_cost_color(s_list_subs[s_farm_n], s_g->item[ASH_ITEM_SEED] > 0);
            s_farm_n++;
            if (root_ok) {
                s_farm_plot[s_farm_n] = plot;
                s_farm_act[s_farm_n] = 1;
                snprintf(line, sizeof(line), "田地%u种红薯", (unsigned)(plot + 1));
                if (s_g->seed_root == 0)
                    snprintf(sub, sizeof(sub), "  没有红薯种 ·渡口可换  ·  ");
                else if (s_g->pace_era == ASHBOX_PACE_V13)
                    snprintf(sub, sizeof(sub), "  种%u ·约3天 ·基础240  ·  ",
                             (unsigned)s_g->seed_root);
                else
                    snprintf(sub, sizeof(sub), "  种%u ·约6时 ·收+7~10  ·  ",
                             (unsigned)s_g->seed_root);
                lv_label_set_text(s_list_labels[s_farm_n], line);
                lv_label_set_text(s_list_subs[s_farm_n], sub);
                set_cost_color(s_list_subs[s_farm_n], s_g->seed_root > 0);
                s_farm_n++;
            }
        } else if ((*ash_game_farm_mut(s_g, (unsigned)(plot))).remain_s == 0) {
            s_farm_plot[s_farm_n] = plot;
            s_farm_act[s_farm_n] = 2;
            snprintf(line, sizeof(line), "田地%u收获", (unsigned)(plot + 1));
            snprintf(sub, sizeof(sub), "  %s  ·  ", st);
            lv_label_set_text(s_list_labels[s_farm_n], line);
            lv_label_set_text(s_list_subs[s_farm_n], sub);
            s_farm_n++;
        } else {
            s_farm_plot[s_farm_n] = plot;
            s_farm_act[s_farm_n] = 3;
            snprintf(line, sizeof(line), "田地%u生长", (unsigned)(plot + 1));
            snprintf(sub, sizeof(sub), "  %s  ·  ", st);
            lv_label_set_text(s_list_labels[s_farm_n], line);
            lv_label_set_text(s_list_subs[s_farm_n], sub);
            s_farm_n++;
        }
    }
    if (s_farm_n < 1) s_farm_n = 1;
    if (s_list_sel >= s_farm_n) s_list_sel = s_farm_n - 1;
    if (s_list_sel < 0) s_list_sel = 0;
    {
        /* 先把行文案留在 labels[0..n)；再映射到视窗槽，避免满项溢出 */
        char farm_title[ASH_LIST_MAX][48];
        char farm_sub[ASH_LIST_MAX][56];
        uint8_t farm_plot_copy[ASH_LIST_MAX];
        uint8_t farm_act_copy[ASH_LIST_MAX];
        for (int i = 0; i < s_farm_n; i++) {
            snprintf(farm_title[i], sizeof(farm_title[i]), "%s", lv_label_get_text(s_list_labels[i]));
            snprintf(farm_sub[i], sizeof(farm_sub[i]), "%s", lv_label_get_text(s_list_subs[i]));
            farm_plot_copy[i] = s_farm_plot[i];
            farm_act_copy[i] = s_farm_act[i];
        }
        int view = FARM_VIEW;
        if (s_farm_n < view) view = s_farm_n;
        int top = list_scroll_top(s_list_sel, s_farm_n, view);
        for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        for (int slot = 0; slot < view; slot++) {
            int i = top + slot;
            if (i >= s_farm_n) break;
            s_farm_plot[slot] = farm_plot_copy[i];
            s_farm_act[slot] = farm_act_copy[i];
            lv_label_set_text(s_list_labels[slot], farm_title[i]);
            lv_label_set_text(s_list_subs[slot], farm_sub[i]);
            set_cost_color(s_list_subs[slot],
                           (farm_act_copy[i] == 2) ||
                               (farm_act_copy[i] == 0 && s_g->item[ASH_ITEM_SEED] > 0) ||
                               (farm_act_copy[i] == 1 && s_g->seed_root > 0));
            lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[slot], 216, 40);
            lv_obj_set_pos(s_list_rows[slot], 12, 10 + slot * 44);
            lv_obj_set_pos(s_list_labels[slot], 10, 2);
            lv_obj_set_pos(s_list_subs[slot], 10, 20);
            lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
            bool sel = (s_list_sel == i);
            lv_obj_set_style_border_color(s_list_rows[slot],
                                          lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
            lv_obj_set_style_bg_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
        }
        /* 激活时按全局选中索引取动作，不依赖视窗槽 */
        for (int i = 0; i < s_farm_n; i++) {
            s_farm_plot[i] = farm_plot_copy[i];
            s_farm_act[i] = farm_act_copy[i];
        }
        paint_hub_scrollbar(s_list_sel, s_farm_n, view);
    }
    set_bar_dual(false);
    {
        uint8_t act = (s_list_sel >= 0 && s_list_sel < s_farm_n) ? s_farm_act[s_list_sel] : 3;
        bool can = (act == 2) || (act == 0 && s_g->item[ASH_ITEM_SEED] > 0) ||
                   (act == 1 && s_g->seed_root > 0);
        const char *cta = (act == 3) ? "等待" : (can ? "执行" : "无种");
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, can && act != 3, true);
    }
    s_focus = 0;
    if (s_g->pace_era == ASHBOX_PACE_V13)
        lv_label_set_text(s_hint, "田作加收成 · 疲劳+5/时 · 装不下会坏");
    else
        lv_label_set_text(s_hint, root_ok ? "稻谷快收粮少 · 红薯慢收更饱" : "先种稻谷稳粮 · 长按返回");
}

static bool trade_tide_offer_ok(void)
{
    return s_g && !s_g->quest2_done && s_g->quest2_step == 2 &&
           s_g->item[ASH_ITEM_SHARD] >= 2;
}

static const ash_trade_t k_camp_trades[] = {
    ASH_TRADE_JERKY_FOR_SEED, ASH_TRADE_FOOD_FOR_WATER, ASH_TRADE_WATER_FOR_FOOD,
    ASH_TRADE_WATER_FOR_IRON, ASH_TRADE_IRON_FOR_WATER, ASH_TRADE_IRON_FOR_PARTS,
    ASH_TRADE_PARTS_FOR_IRON, ASH_TRADE_PARTS_FOR_EMBER, ASH_TRADE_PARTS_FOR_STIM,
    ASH_TRADE_PARTS_FOR_ABX,
};
static const ash_trade_t k_ferry_trades[] = {
    ASH_TRADE_JERKY_FOR_ROOT, ASH_TRADE_PARTS_FOR_ROOT, ASH_TRADE_FOOD_FOR_WATER,
    ASH_TRADE_WATER_FOR_IRON, ASH_TRADE_IRON_FOR_PARTS, ASH_TRADE_PARTS_FOR_EMBER,
};

static int trade_list_count(void)
{
    int n = 0;
    if (ash_game_camp_open(s_g)) n += (int)(sizeof(k_camp_trades) / sizeof(k_camp_trades[0]));
    if (ash_game_camp2_open(s_g)) n += (int)(sizeof(k_ferry_trades) / sizeof(k_ferry_trades[0]));
    if (trade_tide_offer_ok()) n += 1;
    return n;
}

static bool trade_sel_is_tide(int sel)
{
    int base = 0;
    if (ash_game_camp_open(s_g)) base += (int)(sizeof(k_camp_trades) / sizeof(k_camp_trades[0]));
    if (ash_game_camp2_open(s_g)) base += (int)(sizeof(k_ferry_trades) / sizeof(k_ferry_trades[0]));
    return trade_tide_offer_ok() && sel == base;
}

static ash_trade_t trade_at_sel(int sel)
{
    int camp_n = (int)(sizeof(k_camp_trades) / sizeof(k_camp_trades[0]));
    int ferry_n = (int)(sizeof(k_ferry_trades) / sizeof(k_ferry_trades[0]));
    if (ash_game_camp_open(s_g)) {
        if (sel >= 0 && sel < camp_n) return k_camp_trades[sel];
        sel -= camp_n;
    }
    if (ash_game_camp2_open(s_g) && sel >= 0 && sel < ferry_n) return k_ferry_trades[sel];
    return ASH_TRADE_FOOD_FOR_WATER;
}

static void paint_trade(void)
{
    s_page = UI_TRADE;
    show_home_chrome(false);
    bool camp = ash_game_camp_open(s_g);
    bool camp2 = ash_game_camp2_open(s_g);
    lv_label_set_text(s_title, camp2 ? "营地互通" : "铁棚交易");
    if (!camp && !camp2 && !trade_tide_offer_ok()) {
        lv_label_set_text(s_list_labels[0], "尚未通联");
        lv_label_set_text(s_list_subs[0], "  深处通联铁棚 · 灰潮后渡口  ·  ");
        lv_obj_clear_flag(s_list_subs[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[0], 216, 48);
        lv_obj_set_pos(s_list_rows[0], 12, 40);
        lv_obj_clear_flag(s_list_rows[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        paint_list_sel(1);
        set_bar_dual(false);
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "返回", true, true);
        lv_label_set_text(s_hint, "确定返回");
        return;
    }
    if (!camp && !camp2 && trade_tide_offer_ok()) {
        lv_label_set_text(s_title, "灰片听潮");
    }
    int n = trade_list_count();
    if (n <= 0) n = 1;
    if (s_list_sel >= n) s_list_sel = n - 1;
    if (s_list_sel < 0) s_list_sel = 0;
    {
        char titles[ASH_LIST_MAX][40];
        char subs[ASH_LIST_MAX][56];
        bool enough_a[ASH_LIST_MAX];
        for (int i = 0; i < n; i++) {
            if (trade_sel_is_tide(i)) {
                snprintf(titles[i], sizeof(titles[i]), "灰片听潮");
                snprintf(subs[i], sizeof(subs[i]), "  交2灰片 · 不用出门也能推进  ·  ");
                enough_a[i] = s_g->item[ASH_ITEM_SHARD] >= 2;
            } else {
                ash_trade_t kind = trade_at_sel(i);
                snprintf(titles[i], sizeof(titles[i]), "%s", ash_game_trade_label(kind));
                snprintf(subs[i], sizeof(subs[i]), "  %s   ·  ", ash_game_trade_hint(kind));
                enough_a[i] = ash_game_trade_can_pay(s_g, kind);
            }
        }
        int view = TRADE_VIEW;
        if (n < view) view = n;
        int top = list_scroll_top(s_list_sel, n, view);
        for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        for (int slot = 0; slot < view; slot++) {
            int i = top + slot;
            if (i >= n) break;
            lv_label_set_text(s_list_labels[slot], titles[i]);
            lv_label_set_text(s_list_subs[slot], subs[i]);
            set_cost_color(s_list_subs[slot], enough_a[i]);
            lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[slot], 216, 36);
            lv_obj_set_pos(s_list_rows[slot], 12, 10 + slot * 40);
            lv_obj_set_pos(s_list_labels[slot], 10, 2);
            lv_obj_set_pos(s_list_subs[slot], 10, 18);
            lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
            bool sel = (s_list_sel == i);
            lv_obj_set_style_border_color(s_list_rows[slot],
                                          lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
            lv_obj_set_style_bg_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
        }
        paint_hub_scrollbar(s_list_sel, n, view);
    }
    set_bar_dual(false);
    if (trade_sel_is_tide(s_list_sel)) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "交灰片", true, true);
        s_focus = 0;
        lv_label_set_text(s_hint, "没有干肉也能用灰片推进灰潮线");
        return;
    }
    ash_trade_t cur = trade_at_sel(s_list_sel);
    bool camp2_kind =
        (cur == ASH_TRADE_JERKY_FOR_ROOT || cur == ASH_TRADE_PARTS_FOR_ROOT);
    bool chain = (cur >= ASH_TRADE_FOOD_FOR_WATER);
    bool day_ok = camp2_kind ? ash_game_trade2_available(s_g)
                             : (chain ? (ash_game_trade_available(s_g) || ash_game_trade2_available(s_g))
                                      : ash_game_trade_available(s_g));
    bool can_pay = ash_game_trade_can_pay(s_g, cur);
    bool avail = day_ok && can_pay;
    const char *cta = !day_ok ? "今日已换" : (can_pay ? "成交" : "不够");
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, avail, true);
    s_focus = 0;
    {
        char foot[48];
        ash_game_trade_footer(s_g, cur, foot, sizeof(foot));
        if (!day_ok) {
            if ((!camp2_kind && s_g->rep_shed >= 2) || (camp2_kind && s_g->rep_ferry >= 2))
                lv_label_set_text(s_hint, "今日二次已用 · 长按返回");
            else
                lv_label_set_text(s_hint, "该营地今日已换 · 长按返回");
        } else if (!can_pay) {
            lv_label_set_text(s_hint, "材料不够 · 见上望值");
        } else {
            lv_label_set_text(s_hint, foot);
        }
    }
}

static void paint_order(void)
{
    s_page = UI_ORDER;
    show_home_chrome(false);
    lv_label_set_text(s_title, "过夜站令");
    for (int i = 0; i < ASH_ORDER_COUNT; i++) {
        char line[40];
        char sub[56];
        snprintf(line, sizeof(line), "%s%s", ash_game_order_name((ash_night_order_t)i),
                 (s_g->night_order == (uint8_t)i) ? " · 当前" : "");
        snprintf(sub, sizeof(sub), "  %s   ·  ", ash_game_order_hint((ash_night_order_t)i));
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 42);
        lv_obj_set_pos(s_list_rows[i], 12, 8 + i * 46);
        lv_obj_set_pos(s_list_labels[i], 10, 2);
        lv_obj_set_pos(s_list_subs[i], 10, 22);
    }
    for (int i = ASH_ORDER_COUNT; i < ASH_LIST_MAX; i++) {
        lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    paint_list_sel(ASH_ORDER_COUNT);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "设定", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "只在深睡这段生效 · 长按返回");
}

static void paint_journal(void)
{
    s_page = UI_JOURNAL;
    show_home_chrome(false);
    lv_label_set_text(s_title, "纪事");
    int n = (int)s_g->journal_count;
    if (n > ASHBOX_JOURNAL_N) n = ASHBOX_JOURNAL_N;
    if (n == 0) {
        lv_label_set_text(s_list_labels[0], "尚无纪事");
        lv_obj_add_flag(s_list_subs[0], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[0], 216, 30);
        lv_obj_set_pos(s_list_rows[0], 12, 40);
        lv_obj_set_pos(s_list_labels[0], 10, 6);
        lv_obj_clear_flag(s_list_rows[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        hide_list_scrollbar();
        paint_list_sel(1);
    } else {
        if (s_list_sel < 0) s_list_sel = 0;
        if (s_list_sel >= n) s_list_sel = n - 1;
        int view = JOURNAL_VIEW;
        if (n < view) view = n;
        int top = list_scroll_top(s_list_sel, n, view);
        for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        for (int slot = 0; slot < view; slot++) {
            int i = top + slot;
            if (i >= n) break;
            char padded[48];
            bool sel = (s_list_sel == i);
            snprintf(padded, sizeof(padded), sel ? "  %s   ·  " : "%s",
                     ash_game_journal_line(s_g, (unsigned)i));
            lv_label_set_text(s_list_labels[slot], padded);
            lv_obj_add_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_long_mode(s_list_labels[slot],
                                   sel ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR : LV_LABEL_LONG_MODE_CLIP);
            lv_obj_set_width(s_list_labels[slot], 196);
            lv_obj_set_style_anim_duration(s_list_labels[slot], 8000, LV_PART_MAIN);
            lv_obj_set_size(s_list_rows[slot], 216, 28);
            lv_obj_set_pos(s_list_rows[slot], 12, 12 + slot * 30);
            lv_obj_set_pos(s_list_labels[slot], 10, 5);
            lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_clip_corner(s_list_rows[slot], true, 0);
            lv_obj_set_style_border_color(s_list_rows[slot],
                                          lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
            lv_obj_set_style_bg_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
        }
        paint_hub_scrollbar(s_list_sel, n, view);
    }
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "已阅", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "最新在上 · 长按返回");
}

static void paint_quest(void)
{
    s_page = UI_QUEST;
    show_home_chrome(false);
    lv_label_set_text(s_title, "线索");
    for (int i = 0; i < ASHBOX_QUEST_N; i++) {
        char line[48];
        char sub[72];
        snprintf(line, sizeof(line), "%s", ash_game_quest_name((uint8_t)i));
        snprintf(sub, sizeof(sub), "  %s   ·  ", ash_game_quest_hint(s_g, (uint8_t)i));
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_set_style_anim_duration(s_list_subs[i], 8000, LV_PART_MAIN);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 50);
        lv_obj_set_pos(s_list_rows[i], 12, 16 + i * 54);
        lv_obj_set_pos(s_list_labels[i], 10, 6);
        lv_obj_set_pos(s_list_subs[i], 10, 28);
    }
    paint_list_sel(ASHBOX_QUEST_N);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "已阅", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "灰潮换种在交易 · 亦可收获推");
}

static void paint_crisis(void)
{
    s_page = UI_CRISIS;
    show_home_chrome(false);
    lv_label_set_text(s_title, "守夜危机");
    for (int i = 0; i < ASH_CRISIS_COUNT; i++) {
        char line[40];
        char sub[48];
        bool ok = ash_game_crisis_option_ok(s_g, (ash_crisis_opt_t)i);
        snprintf(line, sizeof(line), "%s", ash_game_crisis_label((ash_crisis_opt_t)i));
        if (i == ASH_CRISIS_FORTIFY) {
            uint16_t cost = ash_game_crisis_fortify_cost(s_g);
            if (ok) {
                snprintf(sub, sizeof(sub), "  %s ·耗%u件  ·  ", ash_game_crisis_hint((ash_crisis_opt_t)i),
                         (unsigned)cost);
            } else {
                snprintf(sub, sizeof(sub), "  需%u零件 ·现有%u  ·  ", (unsigned)cost,
                         (unsigned)s_g->parts);
            }
        } else if (ok) {
            snprintf(sub, sizeof(sub), "  %s  ·  ", ash_game_crisis_hint((ash_crisis_opt_t)i));
        } else if (i == ASH_CRISIS_TALK) {
            snprintf(sub, sizeof(sub), "  眼下不能开口 · 先躲或加固  ·  ");
        } else {
            snprintf(sub, sizeof(sub), "  眼下不能躲 · 先开口或加固  ·  ");
        }
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        if (i == ASH_CRISIS_FORTIFY) set_cost_color(s_list_subs[i], ok);
        else lv_obj_set_style_text_color(s_list_subs[i], lv_color_hex(COL_MUTED), 0);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 50);
        lv_obj_set_pos(s_list_rows[i], 12, 16 + i * 54);
        lv_obj_set_pos(s_list_labels[i], 10, 6);
        lv_obj_set_pos(s_list_subs[i], 10, 28);
    }
    paint_list_sel(ASH_CRISIS_COUNT);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "执行", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "必须抉择 · 不可返回");
}

static void paint_settings_confirm(void)
{
    show_home_chrome(false);
    lv_label_set_text(s_title, "导入存档");
    for (int i = 0; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], "确认覆盖当前进度");
    lv_label_set_text(s_modal_lines[1], "会先备份到存档管家");
    lv_label_set_text(s_modal_lines[2], "写入后设备会复位");
    lv_label_set_text(s_modal_lines[3], "");
    lv_label_set_text(s_modal_lines[4], "");
    lv_obj_add_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
    set_bar_dual(true);
    /* 默认焦点在取消，避免误覆盖 */
    if (s_focus != 0 && s_focus != 1) s_focus = 0;
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "取消", true, s_focus == 0);
    set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "覆盖", true, s_focus == 1);
    if (s_hint) {
        lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_hint, "上下选 · 确定");
    }
}

static void paint_settings(void)
{
    s_page = UI_SETTINGS;
    show_home_chrome(false);
    lv_label_set_text(s_title, "设置");

    ash_em_ui_t em = ashbox_embackup_ui_state();
    if (em == ASH_EM_UI_IMPORT_CONFIRM) {
        paint_settings_confirm();
        return;
    }

    char titles[SETTINGS_COUNT][48];
    char subs[SETTINGS_COUNT][40];
    snprintf(titles[0], sizeof(titles[0]), "亮度  %u%%", (unsigned)s_cfg.brightness);
    snprintf(subs[0], sizeof(subs[0]), "确定 +10%%  上下选");
    snprintf(titles[1], sizeof(titles[1]), "音量  %u%%", (unsigned)s_cfg.volume);
    snprintf(subs[1], sizeof(subs[1]), "音效音量");
    snprintf(titles[2], sizeof(titles[2]), "%s", s_cfg.radio_on ? "电台声  开" : "电台声  关");
    snprintf(subs[2], sizeof(subs[2]), "背景噪声");
    snprintf(titles[3], sizeof(titles[3]), "重看开局说明");
    snprintf(subs[3], sizeof(subs[3]), "再走一回入门");
    snprintf(titles[4], sizeof(titles[4]), "备份存档");
    snprintf(subs[4], sizeof(subs[4]), "%s",
             ashbox_embackup_host_ok() ? "导出到存档管家" : "需先连接存档管家");
    snprintf(titles[5], sizeof(titles[5]), "导入存档");
    snprintf(subs[5], sizeof(subs[5]), "%s",
             ashbox_embackup_host_ok() ? "等待管家发来后确认" : "需先连接存档管家");
    snprintf(titles[6], sizeof(titles[6]), "重新开始");
    snprintf(subs[6], sizeof(subs[6]), "只留成就 · 本局清空");

    char status_buf[40];
    const char *status = NULL;
    int pct = ashbox_embackup_progress_pct();
    if (em == ASH_EM_UI_NO_HOST) {
        status = "请先打开存档管家并连接";
    } else if (em == ASH_EM_UI_BACKUP_BUSY) {
        if (pct >= 0) {
            snprintf(status_buf, sizeof(status_buf), "正在备份 %d%%", pct);
            status = status_buf;
        } else {
            status = "正在备份";
        }
    } else if (em == ASH_EM_UI_BACKUP_OK) {
        status = "备份完成";
    } else if (em == ASH_EM_UI_BACKUP_FAIL) {
        status = "备份未完成";
    } else if (em == ASH_EM_UI_IMPORT_WAIT) {
        status = "等待管家发来文件";
    } else if (em == ASH_EM_UI_IMPORT_BUSY) {
        if (ashbox_embackup_import_preamble()) {
            if (pct >= 0) {
                snprintf(status_buf, sizeof(status_buf), "备份当前 %d%%", pct);
                status = status_buf;
            } else {
                status = "备份当前档";
            }
        } else if (pct >= 0) {
            snprintf(status_buf, sizeof(status_buf), "正在导入 %d%%", pct);
            status = status_buf;
        } else {
            status = "正在导入";
        }
    } else if (em == ASH_EM_UI_IMPORT_DONE_HINT) {
        status = "写入完成 · 即将复位";
    } else if (em == ASH_EM_UI_IMPORT_CONFIRM) {
        status = "确认覆盖 · 默认取消";
    }

    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= SETTINGS_COUNT) s_list_sel = SETTINGS_COUNT - 1;
    int set_view = SETTINGS_VIEW;
    if (SETTINGS_COUNT < set_view) set_view = SETTINGS_COUNT;
    int set_top = list_scroll_top(s_list_sel, SETTINGS_COUNT, set_view);
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < set_view; slot++) {
        int i = set_top + slot;
        if (i >= SETTINGS_COUNT) break;
        lv_obj_set_size(s_list_rows[slot], 216, 32);
        lv_obj_set_pos(s_list_rows[slot], 12, 10 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 2);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_label_set_text(s_list_labels[slot], titles[i]);
        lv_label_set_text(s_list_subs[slot], subs[i]);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(s_list_labels[slot], 196);
        lv_obj_set_width(s_list_subs[slot], 196);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, SETTINGS_COUNT, set_view);
    set_modal_visible(false);
    lv_obj_clear_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
    set_bar_dual(false);
    const char *cta = "调节";
    if (em == ASH_EM_UI_BACKUP_BUSY || em == ASH_EM_UI_IMPORT_BUSY) {
        cta = "进行中";
    } else if (s_list_sel == 3) {
        cta = "开始";
    } else if (s_list_sel == 4) {
        cta = "备份";
    } else if (s_list_sel == 5) {
        cta = "导入";
    }
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, status ? status : "确定长按 · 返回");
}

static void paint_help(void)
{
    s_page = UI_HELP;
    show_home_chrome(false);
    lv_label_set_text(s_title, "怎么玩");
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= ASHBOX_HELP_COUNT) s_list_sel = ASHBOX_HELP_COUNT - 1;
    int view = ASHBOX_HELP_VIEW;
    if (ASHBOX_HELP_COUNT < view) view = ASHBOX_HELP_COUNT;
    int top = 0;
    if (s_list_sel >= view) top = s_list_sel - view + 1;
    if (top > ASHBOX_HELP_COUNT - view) top = ASHBOX_HELP_COUNT - view;
    if (top < 0) top = 0;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= ASHBOX_HELP_COUNT) break;
        bool sel = (s_list_sel == i);
        lv_obj_set_size(s_list_rows[slot], 216, ASHBOX_HELP_ROW_H);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * ASHBOX_HELP_ROW_STEP);
        lv_obj_set_pos(s_list_labels[slot], 10, 4);
        lv_obj_add_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        /* 操作说明必须自动换行；禁止 CLIP/跑马灯裁掉字 */
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(s_list_labels[slot], ASHBOX_HELP_LABEL_W);
        lv_obj_set_height(s_list_labels[slot], ASHBOX_HELP_ROW_H - 8);
        lv_obj_set_style_text_align(s_list_labels[slot], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_clip_corner(s_list_rows[slot], true, 0);
        lv_label_set_text(s_list_labels[slot], ashbox_help_line((unsigned)i));
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, ASHBOX_HELP_COUNT, view);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "已阅", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "上下翻看说明 · 确定返回");
}

static void paint_report(void)
{
    s_page = UI_REPORT;
    char buf[72];
    if (!s_g->report.branch_resolved && s_g->report.branch_count > 0) {
        show_home_chrome(false);
        set_modal_visible(false);
        lv_label_set_text(s_title, "归来抉择");
        int n = s_g->report.branch_count;
        if (n > ASHBOX_BRANCH_MAX) n = ASHBOX_BRANCH_MAX;
        for (int i = 0; i < n; i++) {
            uint8_t bid = s_g->report.branch_ids[i];
            char effect[40];
            snprintf(buf, sizeof(buf), "%s", ash_game_branch_label(bid));
            lv_label_set_text(s_list_labels[i], buf);
            ash_game_branch_effect(bid, effect, sizeof(effect));
            snprintf(buf, sizeof(buf), "  %s   ·  ", effect);
            lv_label_set_text(s_list_subs[i], buf);
            lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
            lv_obj_set_width(s_list_labels[i], 196);
            lv_obj_set_width(s_list_subs[i], 196);
            lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(s_list_rows[i], 216, 48);
            lv_obj_set_pos(s_list_rows[i], 12, 12 + i * 52);
            lv_obj_set_pos(s_list_labels[i], 10, 4);
            lv_obj_set_pos(s_list_subs[i], 10, 26);
        }
        paint_list_sel(n);
        set_bar_dual(false);
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "选定", true, true);
        s_focus = 0;
        lv_label_set_text(s_hint, "要醒着做抉择 · 深睡进度仍保留");
        return;
    }
    show_home_chrome(true);
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], "确认收获");
    snprintf(buf, sizeof(buf), "口粮 %+d", (int)s_g->report.food_delta);
    lv_label_set_text(s_modal_lines[1], buf);
    snprintf(buf, sizeof(buf), "零件 %+d · 水 %+d", (int)s_g->report.parts_delta,
             (int)s_g->report_water_delta);
    lv_label_set_text(s_modal_lines[2], buf);
    if (s_g->report.ember_delta != 0) {
        snprintf(buf, sizeof(buf), "余烬 %+d", (int)s_g->report.ember_delta);
        lv_label_set_text(s_modal_lines[3], buf);
        lv_label_set_text(s_modal_lines[4], s_g->report.lines[2]);
    } else {
        lv_label_set_text(s_modal_lines[3], s_g->report.lines[0]);
        lv_label_set_text(s_modal_lines[4], s_g->report.lines[2][0] ? s_g->report.lines[2]
                                                                     : s_g->report.lines[1]);
    }
    lv_label_set_text(lv_obj_get_child(s_modal_btn, 0), "入账");
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "入账", true, true);
    lv_label_set_text(s_hint, "点按后资源到账");
}

static void build_screen(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶条下移 SAFE_TOP，躲开外壳遮挡 */
    lv_obj_t *hdr = lv_obj_create(s_scr);
    lv_obj_set_size(hdr, 240, HDR_H);
    lv_obj_set_pos(hdr, 0, SAFE_TOP);
    style_panel(hdr, COL_SURF);
    lv_obj_set_style_border_width(hdr, 0, 0);
    s_title = mk_label(hdr, "第1天", COL_AMBER);
    lv_obj_set_pos(s_title, 6, 1);
    s_day = mk_label(hdr, "--:--", COL_TEXT);
    lv_obj_align(s_day, LV_ALIGN_TOP_MID, 0, 1);
    /* 捡来的电芯匣：锈夹 + 缺角壳 + 四格残芯 + 触点（勿标「烬」，免与资源混淆） */
    s_bat_tag = NULL;
    s_bat_clamp = lv_obj_create(hdr);
    lv_obj_set_size(s_bat_clamp, 3, 12);
    lv_obj_set_pos(s_bat_clamp, 194, 3);
    lv_obj_set_style_bg_color(s_bat_clamp, lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_bg_opa(s_bat_clamp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bat_clamp, 0, 0);
    lv_obj_set_style_border_width(s_bat_clamp, 0, 0);
    lv_obj_set_style_pad_all(s_bat_clamp, 0, 0);
    lv_obj_clear_flag(s_bat_clamp, LV_OBJ_FLAG_SCROLLABLE);
    s_bat_shell = lv_obj_create(hdr);
    lv_obj_set_size(s_bat_shell, 34, 12);
    lv_obj_set_pos(s_bat_shell, 196, 3);
    lv_obj_set_style_bg_color(s_bat_shell, lv_color_hex(0x120E0C), 0);
    lv_obj_set_style_bg_opa(s_bat_shell, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bat_shell, 0, 0);
    lv_obj_set_style_border_width(s_bat_shell, 1, 0);
    lv_obj_set_style_border_color(s_bat_shell, lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_pad_all(s_bat_shell, 0, 0);
    lv_obj_clear_flag(s_bat_shell, LV_OBJ_FLAG_SCROLLABLE);
    /* 左上缺角：像砸裂的壳 */
    lv_obj_t *bat_notch = lv_obj_create(hdr);
    lv_obj_set_size(bat_notch, 5, 3);
    lv_obj_set_pos(bat_notch, 196, 3);
    lv_obj_set_style_bg_color(bat_notch, lv_color_hex(COL_SURF), 0);
    lv_obj_set_style_bg_opa(bat_notch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bat_notch, 0, 0);
    lv_obj_set_style_border_width(bat_notch, 0, 0);
    lv_obj_set_style_pad_all(bat_notch, 0, 0);
    lv_obj_clear_flag(bat_notch, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < BAT_CELLS; i++) {
        s_bat_cell[i] = lv_obj_create(s_bat_shell);
        lv_obj_set_size(s_bat_cell[i], 5, 6);
        lv_obj_set_pos(s_bat_cell[i], 4 + i * 7, 3);
        lv_obj_set_style_bg_color(s_bat_cell[i], lv_color_hex(COL_SURF2), 0);
        lv_obj_set_style_bg_opa(s_bat_cell[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_bat_cell[i], 0, 0);
        lv_obj_set_style_border_width(s_bat_cell[i], 1, 0);
        lv_obj_set_style_border_color(s_bat_cell[i], lv_color_hex(COL_SURF), 0);
        lv_obj_set_style_pad_all(s_bat_cell[i], 0, 0);
        lv_obj_clear_flag(s_bat_cell[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    s_bat_scar = lv_obj_create(s_bat_shell);
    lv_obj_set_size(s_bat_scar, 8, 1);
    lv_obj_set_pos(s_bat_scar, 20, 1);
    lv_obj_set_style_bg_color(s_bat_scar, lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_bg_opa(s_bat_scar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bat_scar, 0, 0);
    lv_obj_set_style_border_width(s_bat_scar, 0, 0);
    lv_obj_set_style_pad_all(s_bat_scar, 0, 0);
    lv_obj_clear_flag(s_bat_scar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_bat_scar, LV_OBJ_FLAG_HIDDEN);
    s_bat_spark = lv_obj_create(hdr);
    lv_obj_set_size(s_bat_spark, 2, 4);
    lv_obj_set_pos(s_bat_spark, 231, 7);
    lv_obj_set_style_bg_color(s_bat_spark, lv_color_hex(COL_AMBER), 0);
    lv_obj_set_style_bg_opa(s_bat_spark, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_bat_spark, 0, 0);
    lv_obj_set_style_border_width(s_bat_spark, 0, 0);
    lv_obj_set_style_pad_all(s_bat_spark, 0, 0);
    lv_obj_clear_flag(s_bat_spark, LV_OBJ_FLAG_SCROLLABLE);

    /* 精灵纯画面 152；其下 56 信息带（资源/广播/此刻），不压站台 */
    s_home_layer = lv_obj_create(s_scr);
    lv_obj_set_size(s_home_layer, 240, LIST_LAYER_H);
    lv_obj_set_pos(s_home_layer, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(s_home_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_home_layer, 0, 0);
    lv_obj_set_style_pad_all(s_home_layer, 0, 0);
    lv_obj_clear_flag(s_home_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_sprite = lv_image_create(s_home_layer);
    lv_obj_set_pos(s_sprite, 0, 0);
    sprite_show(ASHBOX_SCENE_IDLE, 0);

    /* 威胁细条贴精灵底边，几乎不挡画面 */
    s_threat_bar = lv_bar_create(s_home_layer);
    lv_obj_set_size(s_threat_bar, 240, 2);
    lv_obj_set_pos(s_threat_bar, 0, STAGE_H - 2);
    lv_bar_set_range(s_threat_bar, 0, 100);
    lv_obj_set_style_bg_color(s_threat_bar, lv_color_hex(COL_SURF2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_threat_bar, lv_color_hex(COL_LINE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_threat_bar, 0, 0);
    lv_obj_set_style_pad_all(s_threat_bar, 0, 0);

    lv_obj_t *now = lv_obj_create(s_home_layer);
    lv_obj_set_size(now, 240, NOW_BAND_H);
    lv_obj_set_pos(now, 0, STAGE_H);
    style_panel(now, COL_SURF);
    lv_obj_set_style_border_width(now, 0, 0);
    lv_obj_set_style_pad_all(now, 0, 0);

    /* 信息带第 1 行：水/粮/件/烬（左）| 状态（右） */
    s_water = mk_label(now, "水0", COL_TEXT);
    lv_obj_set_pos(s_water, 4, NOW_ROW_RES);
    lv_obj_set_width(s_water, 36);
    lv_label_set_long_mode(s_water, LV_LABEL_LONG_MODE_CLIP);
    s_food = mk_label(now, "粮0", COL_TEXT);
    lv_obj_set_pos(s_food, 40, NOW_ROW_RES);
    lv_obj_set_width(s_food, 36);
    lv_label_set_long_mode(s_food, LV_LABEL_LONG_MODE_CLIP);
    s_parts = mk_label(now, "零件0", COL_TEXT);
    lv_obj_set_pos(s_parts, 76, NOW_ROW_RES);
    lv_obj_set_width(s_parts, 36);
    lv_label_set_long_mode(s_parts, LV_LABEL_LONG_MODE_CLIP);
    s_ember = mk_label(now, "烬0", COL_AMBER);
    lv_obj_set_pos(s_ember, 112, NOW_ROW_RES);
    lv_obj_set_width(s_ember, 36);
    lv_label_set_long_mode(s_ember, LV_LABEL_LONG_MODE_CLIP);
    s_people = mk_label(now, "", COL_MUTED);
    lv_obj_add_flag(s_people, LV_OBJ_FLAG_HIDDEN);
    s_status = mk_label(now, "待命", COL_SAFE);
    lv_obj_set_pos(s_status, 150, NOW_ROW_RES);
    lv_obj_set_width(s_status, 84);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_CLIP);
    s_threat_lbl = mk_label(now, "0%", COL_DANGER);
    lv_obj_align(s_threat_lbl, LV_ALIGN_TOP_RIGHT, -4, NOW_ROW_RES);
    lv_obj_add_flag(s_threat_lbl, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *rb = mk_label(now, "广播", COL_AMBER);
    lv_obj_set_pos(rb, 6, NOW_ROW_RADIO);
    s_radio = mk_label(now, "", COL_MUTED);
    lv_obj_set_pos(s_radio, 42, NOW_ROW_RADIO);
    lv_obj_set_width(s_radio, 190);
    lv_label_set_long_mode(s_radio, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_duration(s_radio, 9000, LV_PART_MAIN);

    lv_obj_t *next_lab = mk_label(now, "此刻", COL_MUTED);
    lv_obj_set_pos(next_lab, 6, NOW_ROW_CARD);
    s_card_title = mk_label(now, "远征", COL_MUTED);
    lv_obj_set_pos(s_card_title, CARD_TITLE_X, NOW_ROW_CARD);
    lv_obj_set_width(s_card_title, CARD_TITLE_W);
    lv_label_set_long_mode(s_card_title, LV_LABEL_LONG_MODE_CLIP);
    s_card_value = mk_label(now, "可出发", COL_AMBER);
    lv_obj_set_pos(s_card_value, CARD_VALUE_X, NOW_ROW_CARD);
    lv_obj_set_width(s_card_value, CARD_VALUE_W);
    lv_label_set_long_mode(s_card_value, LV_LABEL_LONG_MODE_CLIP);
    s_card_sub = mk_label(now, "", COL_MUTED);
    lv_obj_add_flag(s_card_sub, LV_OBJ_FLAG_HIDDEN); /* 提示并入广播行 */
    s_scroll_shown[0] = '\0';

    s_list_layer = lv_obj_create(s_scr);
    lv_obj_set_size(s_list_layer, 240, LIST_LAYER_H);
    lv_obj_set_pos(s_list_layer, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(s_list_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list_layer, 0, 0);
    lv_obj_set_style_pad_all(s_list_layer, 0, 0);
    lv_obj_set_style_clip_corner(s_list_layer, true, 0);
    lv_obj_clear_flag(s_list_layer, LV_OBJ_FLAG_OVERFLOW_VISIBLE); /* 裁切越界行 */
    lv_obj_clear_flag(s_list_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_list_layer, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < ASH_LIST_MAX; i++) {
        s_list_rows[i] = lv_obj_create(s_list_layer);
        lv_obj_set_size(s_list_rows[i], 216, 36);
        lv_obj_set_pos(s_list_rows[i], 12, 8 + i * 40);
        style_panel(s_list_rows[i], COL_SURF);
        lv_obj_set_style_clip_corner(s_list_rows[i], true, 0);
        s_list_labels[i] = mk_label(s_list_rows[i], "", COL_TEXT);
        lv_obj_set_pos(s_list_labels[i], 10, 8);
        lv_obj_set_width(s_list_labels[i], 196);
        s_list_subs[i] = mk_label(s_list_rows[i], "", COL_MUTED);
        lv_obj_set_pos(s_list_subs[i], 10, 28);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_add_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_scroll_track = lv_obj_create(s_list_layer);
    lv_obj_set_size(s_scroll_track, 4, HUB_VIEW * HUB_ROW_H);
    lv_obj_set_pos(s_scroll_track, 232, 20);
    lv_obj_set_style_bg_color(s_scroll_track, lv_color_hex(COL_SURF2), 0);
    lv_obj_set_style_bg_opa(s_scroll_track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_scroll_track, 0, 0);
    lv_obj_set_style_border_width(s_scroll_track, 0, 0);
    lv_obj_set_style_pad_all(s_scroll_track, 0, 0);
    lv_obj_clear_flag(s_scroll_track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scroll_track, LV_OBJ_FLAG_HIDDEN);
    s_scroll_thumb = lv_obj_create(s_scroll_track);
    lv_obj_set_size(s_scroll_thumb, 4, 24);
    lv_obj_set_pos(s_scroll_thumb, 0, 0);
    lv_obj_set_style_bg_color(s_scroll_thumb, lv_color_hex(COL_AMBER), 0);
    lv_obj_set_style_bg_opa(s_scroll_thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_scroll_thumb, 0, 0);
    lv_obj_set_style_border_width(s_scroll_thumb, 0, 0);
    lv_obj_set_style_pad_all(s_scroll_thumb, 0, 0);
    lv_obj_clear_flag(s_scroll_thumb, LV_OBJ_FLAG_SCROLLABLE);

    /* 底栏下压吃掉空闲区；仍留圆角安全边。hint 区仅约一行高：定宽+跑马灯，禁止无宽 CLIP 遮字 */
    s_hint = mk_label(s_scr, "", COL_MUTED);
    lv_obj_set_pos(s_hint, 12, HINT_Y);
    lv_obj_set_width(s_hint, 216);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_duration(s_hint, 10000, LV_PART_MAIN);

    lv_obj_t *bar = lv_obj_create(s_scr);
    lv_obj_set_size(bar, 240, BAR_H);
    lv_obj_set_pos(bar, 0, BAR_Y);
    style_panel(bar, COL_SURF);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_pri = lv_obj_create(bar);
    style_panel(s_btn_pri, COL_AMBER);
    lv_obj_clear_flag(s_btn_pri, LV_OBJ_FLAG_SCROLLABLE);
    s_btn_pri_lab = mk_label(s_btn_pri, "出发", COL_INK);
    lv_obj_center(s_btn_pri_lab);

    s_btn_sec = lv_obj_create(bar);
    style_panel(s_btn_sec, COL_SURF2);
    lv_obj_clear_flag(s_btn_sec, LV_OBJ_FLAG_SCROLLABLE);
    s_btn_sec_lab = mk_label(s_btn_sec, "据点", COL_MUTED);
    lv_obj_center(s_btn_sec_lab);
    set_bar_dual(true); /* 统一主/次键尺寸与 y，避免错位 */

    s_modal = lv_obj_create(s_scr);
    lv_obj_set_size(s_modal, 216, 212);
    lv_obj_center(s_modal);
    style_panel(s_modal, COL_SURF);
    lv_obj_set_style_border_color(s_modal, lv_color_hex(COL_AMBER), 0);
    /* Title + wrapping body + short footers. Fixed heights keep lines from covering each other. */
    s_modal_lines[0] = mk_label(s_modal, "", COL_AMBER);
    lv_obj_set_pos(s_modal_lines[0], 16, 8);
    label_set_wrap(s_modal_lines[0], 184, 20);
    s_modal_lines[1] = mk_label(s_modal, "", COL_TEXT);
    lv_obj_set_pos(s_modal_lines[1], 16, 30);
    label_set_wrap(s_modal_lines[1], 184, 72);
    s_modal_lines[2] = mk_label(s_modal, "", COL_TEXT);
    lv_obj_set_pos(s_modal_lines[2], 16, 104);
    label_set_wrap(s_modal_lines[2], 184, 18);
    s_modal_lines[3] = mk_label(s_modal, "", COL_TEXT);
    lv_obj_set_pos(s_modal_lines[3], 16, 124);
    label_set_wrap(s_modal_lines[3], 184, 18);
    s_modal_lines[4] = mk_label(s_modal, "", COL_MUTED);
    lv_obj_set_pos(s_modal_lines[4], 16, 144);
    label_set_wrap(s_modal_lines[4], 184, 18);
    s_modal_btn = lv_obj_create(s_modal);
    lv_obj_set_size(s_modal_btn, 160, 28);
    lv_obj_align(s_modal_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    style_panel(s_modal_btn, COL_AMBER);
    lv_obj_clear_flag(s_modal_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ml = mk_label(s_modal_btn, "收下", COL_INK);
    layout_btn_label(s_modal_btn, ml);
    set_modal_visible(false);

    /* 全局默认字体，避免个别控件回落到无中文的 Montserrat 方框 */
    lv_obj_set_style_text_font(s_scr, F(), 0);
    lv_screen_load(s_scr);
}

void ashbox_ui_start(ash_game_t *game, bool wake_guard_active)
{
    s_g = game;
    s_wake_guard = wake_guard_active;
    s_want_sleep = false;
    s_focus = 0;
    s_list_sel = 0;
    s_page = UI_HOME;
    s_mono_last_us = esp_timer_get_time();
    s_settle_accum_us = 0;
    s_idle_us = 0;
    s_pwr = PWR_ACTIVE;
    s_anim_frame = 0;
    s_anim_next_us = s_mono_last_us + SPRITE_ANIM_PERIOD_US;
    s_last_sprite_scene = -1;
    s_last_sprite_frame = -1;
    s_radio_cache[0] = '\0';
    s_card_sub_cache[0] = '\0';
    s_scroll_shown[0] = '\0';
    s_home_fp = 0;
    s_clock_sec_shown = -1;
    s_day_key_shown = UINT32_MAX;
    s_exp_rem_shown = UINT32_MAX;
    s_threat_shown = -1;
    s_bat_level_shown = -999;
    if (ashbox_meta_load(&s_meta) != ESP_OK) {
        ashbox_meta_new(&s_meta);
    }
    if (ashbox_settings_load(&s_cfg) != ESP_OK) {
        s_cfg = ashbox_settings_defaults();
    }
    /* 已出过远征的老档：跳过开局引导，避免打扰 */
    if (s_g && s_g->expedition_total >= 1u && s_cfg.guide_step < ASH_GUIDE_DONE) {
        s_cfg.guide_step = ASH_GUIDE_DONE;
        (void)ashbox_settings_save(&s_cfg);
    }
    ashbox_settings_apply(&s_cfg);
    sample_battery(); /* I2C before first paint */
    if (!bsp_lvgl_lock(1000)) return;
    build_screen();
    paint_home();
    log_mem_budget("ui_start");
    bsp_lvgl_unlock();
    ESP_LOGI(TAG, "UI ready");
}

static void mark_save(void)
{
    s_save_dirty = true;
}

/* Player-confirmed state that must survive an immediate power cut (站事/日勤/战报…).
 * Actual NVS write happens after LVGL unlock via flush_nvs_runtime_only. */
static void mark_save_urgent(void)
{
    s_save_dirty = true;
    s_nvs_force = true;
}

/* Caller must hold ashbox_runtime_lock() and must NOT hold bsp_lvgl_lock().
 * mirror=true also refreshes sv2 (urgent / sleep / export); periodic flush stays primary-only. */
static void flush_save(bool mirror)
{
    if (!s_save_dirty || !s_g) return;
    esp_err_t err =
        mirror ? ashbox_save_store_mirrored(s_g) : ashbox_save_store(s_g);
    if (err == ESP_OK) {
        s_save_dirty = false;
        s_last_nvs_flush_us = esp_timer_get_time();
    } else {
        ESP_LOGW(TAG, "save flush failed: %s", esp_err_to_name(err));
    }
}

/* Caller: runtime lock held, LVGL unlocked. */
static void flush_nvs_runtime_only(bool force)
{
    if (s_cfg_dirty) {
        if (ashbox_settings_save(&s_cfg) == ESP_OK) {
            s_cfg_dirty = false;
        } else {
            ESP_LOGW(TAG, "settings flush failed");
        }
    }
    if (!s_g) {
        s_nvs_force = false;
        return;
    }
    if (force || s_nvs_force) {
        s_save_dirty = true;
        flush_save(true);
        s_nvs_force = false;
        return;
    }
    if (!s_save_dirty) return;
    int64_t now = esp_timer_get_time();
    if (s_last_nvs_flush_us == 0 || (now - s_last_nvs_flush_us) >= SAVE_FLUSH_PERIOD_US) {
        flush_save(false);
    }
}

void ashbox_ui_flush_persistent(void)
{
    ashbox_runtime_lock();
    if (s_g) {
        s_save_dirty = true;
        flush_save(true);
        if (s_save_dirty) {
            /* Force store even if dirty flag race; only clear on success (B15). */
            if (ashbox_save_store_mirrored(s_g) == ESP_OK) {
                s_save_dirty = false;
                s_last_nvs_flush_us = esp_timer_get_time();
            }
        }
    }
    if (ashbox_settings_save(&s_cfg) == ESP_OK) {
        s_cfg_dirty = false;
    }
    ashbox_runtime_unlock();
}

/* Caller holds runtime + LVGL locks. */
static void service_wake_apply_locked(void)
{
    if (!s_wake_apply_pending) return;
    s_wake_apply_pending = false;
    ashbox_settings_apply(&s_cfg);
    if (s_page == UI_HOME) paint_home();
}

/* 时间推进后标记脏；重要事件立即双写落盘，平时最多 15s 只写主槽（耐磨） */
static void persist_after_settle(bool urgent)
{
    mark_save();
    if (urgent || s_nvs_force) {
        flush_save(true);
        s_nvs_force = false;
        return;
    }
    int64_t now = esp_timer_get_time();
    if (s_last_nvs_flush_us == 0 || (now - s_last_nvs_flush_us) >= SAVE_FLUSH_PERIOD_US) {
        flush_save(false);
    }
}

static void enter_page(ui_page_t page)
{
    s_page = page;
    set_modal_visible(false);
    /* 仅防「进页瞬间的二次确定」；过长让连按确认像失灵 */
    ashbox_ui_arm_ok_guard_ms(200);
}

static void activate_home(void)
{
    if (s_wake_guard) return;
    if (guide_active()) {
        advance_guide();
        return;
    }
    if (s_focus == 0) {
        if (ash_game_wanderer_pending(s_g)) {
            /* 主按钮=留下；条件不够只警告，弹窗不关 */
            bool ok = ash_game_wanderer_join(s_g);
            ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
            if (ok) {
                s_wanderer_bell_armed = false;
                mark_save_urgent();
            }
            paint_home();
            return;
        }
        if (ash_game_crisis_pending(s_g)) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
            s_list_sel = 0;
            enter_page(UI_CRISIS);
            paint_crisis();
            return;
        }
        if (s_g->report.pending) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
            s_list_sel = 0;
            enter_page(UI_REPORT);
            paint_report();
            return;
        }
        if (ash_game_chore_pending(s_g)) {
            if (ash_game_resolve_chore(s_g)) {
                ashbox_sfx_play(ASH_SFX_REWARD);
                mark_save_urgent();
                paint_home();
            } else {
                ashbox_sfx_play(ASH_SFX_WARN);
            }
            return;
        }
        if (ash_game_can_depart(s_g)) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
            s_depart_step = 0;
            s_list_sel = 0;
            enter_page(UI_PICK);
            paint_pick();
            return;
        }
        if (ash_game_duty_available(s_g)) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
            s_list_sel = 0;
            enter_page(UI_DUTY);
            paint_duty();
            return;
        }
        ashbox_sfx_play(ASH_SFX_WARN);
    } else if (ash_game_wanderer_pending(s_g)) {
        /* 副按钮=拒绝；不进据点，弹窗不自动关 */
        bool ok = ash_game_wanderer_dismiss(s_g);
        ashbox_sfx_play(ok ? ASH_SFX_BACK : ASH_SFX_WARN);
        if (ok) {
            s_wanderer_bell_armed = false;
            mark_save_urgent();
        }
        paint_home();
    } else {
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        s_list_sel = 0;
        enter_page(UI_HUB);
        paint_hub();
    }
}

static void activate_pick(void)
{
    if (ok_guard_blocks()) {
        ESP_LOGI(TAG, "ignore OK (page-enter guard)");
        return;
    }
    if (s_depart_step == 0) {
        if (!ash_game_tier_unlocked(s_g, (ash_tier_t)s_list_sel)) {
            ashbox_sfx_play(ASH_SFX_WARN);
            return;
        }
        s_pick_tier = (ash_tier_t)s_list_sel;
        s_depart_step = 1;
        s_list_sel = 0;
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        paint_pick();
        return;
    }
    if (s_depart_step == 1) {
        if (!ash_game_person_can_depart(s_g, (uint8_t)s_list_sel)) {
            show_block_modal(ASH_ACT_DEPART);
            return;
        }
        s_pick_person = (uint8_t)s_list_sel;
        s_depart_step = 2;
        s_list_sel = 0;
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        paint_pick();
        return;
    }
    {
        uint8_t crow = 0, gun = 0, ammo = 0;
        if (s_list_sel == 1) {
            int cap = ash_game_crowbar_carry_cap(s_g, s_pick_person);
            int stock = (int)s_g->crowbar;
            int n = (cap < stock) ? cap : stock;
            if (n < 0) n = 0;
            if (n > 255) n = 255;
            crow = (uint8_t)n;
        } else if (s_list_sel == 2) {
            bool has = (*ash_game_equip_pis_mut(s_g, (unsigned)(s_pick_person))) || s_g->pistol > 0;
            if (has && s_g->ammo > 0) {
                gun = 1;
                ammo = 1;
            }
        }
        if (ash_game_start_expedition_ex(s_g, s_pick_tier, s_pick_person, crow, gun, ammo)) {
            ashbox_sfx_play(ASH_SFX_DEPART);
            mark_save_urgent();
            enter_page(UI_HOME);
            paint_home();
        } else {
            show_block_modal(ASH_ACT_DEPART);
        }
    }
}

static void activate_stash(void)
{
    if (ok_guard_blocks()) return;
    bool ok = false;
    if (s_list_sel == 0) ok = ash_game_upgrade_stash(s_g);
    else if (s_list_sel == 1) ok = ash_game_upgrade_furn(s_g);
    else if (s_list_sel == 2) ok = ash_game_use_jerky(s_g);
    else if (s_list_sel == 3) {
        ashbox_sfx_play(ASH_SFX_CLICK);
        paint_stash();
        return;
    } else {
        ashbox_sfx_play(ASH_SFX_WARN);
        paint_stash();
        return;
    }
    ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
    if (ok) mark_save_urgent();
    paint_stash();
}

static void activate_res(void)
{
    if (ok_guard_blocks()) return;
    if (s_list_sel < RES_MED_BASE) {
        ashbox_sfx_play(ASH_SFX_CLICK);
        paint_res();
        return;
    }
    bool ok = false;
    int off = s_list_sel - RES_MED_BASE;
    uint8_t pid = (uint8_t)(off / 3);
    int mid = off % 3;
    if (mid == 0) ok = ash_game_use_milk(s_g, pid);
    else if (mid == 1) ok = ash_game_use_stim(s_g, pid);
    else ok = ash_game_use_abx(s_g, pid);
    ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
    if (ok) mark_save_urgent();
    paint_res();
}

static void activate_equip(void)
{
    if (ok_guard_blocks()) return;
    bool ok = false;
    if (s_list_sel >= EQUIP_CRAFT_BASE) {
        ok = ash_game_craft(s_g, equip_craft_kind(s_list_sel));
        ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
        if (ok) mark_save_urgent();
        paint_equip();
        return;
    }
    uint8_t pid = (uint8_t)(s_list_sel / 2);
    bool is_gun = (s_list_sel % 2) == 1;
    if (pid >= s_g->pop) {
        ashbox_sfx_play(ASH_SFX_WARN);
        paint_equip();
        return;
    }
    if (!is_gun) {
        ok = ash_game_equip_backpack(s_g, pid, !(*ash_game_equip_bp_mut(s_g, (unsigned)(pid))));
    } else {
        ok = ash_game_equip_pistol(s_g, pid, !(*ash_game_equip_pis_mut(s_g, (unsigned)(pid))));
    }
    ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
    if (ok) mark_save_urgent();
    paint_equip();
}

static void activate_farm(void)
{
    if (ok_guard_blocks()) return;
    if (!ash_game_farm_unlocked(s_g)) {
        go_back();
        return;
    }
    if (s_list_sel < 0 || s_list_sel >= s_farm_n) {
        ashbox_sfx_play(ASH_SFX_WARN);
        paint_farm();
        return;
    }
    uint8_t plot = s_farm_plot[s_list_sel];
    uint8_t act = s_farm_act[s_list_sel];
    bool ok = false;
    if (act == 0) ok = ash_game_farm_plant(s_g, plot, ASH_CROP_GRAIN);
    else if (act == 1) ok = ash_game_farm_plant(s_g, plot, ASH_CROP_ROOT);
    else if (act == 2) ok = ash_game_farm_harvest(s_g, plot);
    ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
    if (ok) mark_save_urgent();
    paint_farm();
}

static void activate_trade(void)
{
    if (ok_guard_blocks()) return;
    if (!ash_game_camp_open(s_g) && !ash_game_camp2_open(s_g) && !trade_tide_offer_ok()) {
        go_back();
        return;
    }
    bool ok = false;
    if (trade_sel_is_tide(s_list_sel)) ok = ash_game_tide_offer_shards(s_g);
    else ok = ash_game_trade(s_g, trade_at_sel(s_list_sel));
    ashbox_sfx_play(ok ? ASH_SFX_REWARD : ASH_SFX_WARN);
    if (ok) mark_save_urgent();
    paint_trade();
}

static void paint_duty(void)
{
    s_page = UI_DUTY;
    show_home_chrome(false);
    lv_label_set_text(s_title, "日勤");
    char line[40];
    char sub[48];
    unsigned left = ash_game_duty_left(s_g);
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= DUTY_COUNT) s_list_sel = DUTY_COUNT - 1;
    for (int i = 0; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < DUTY_COUNT; i++) {
        snprintf(line, sizeof(line), "%s", ash_game_duty_label((uint8_t)i));
        snprintf(sub, sizeof(sub), "  %s  ·  ", ash_game_duty_hint((uint8_t)i));
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 40);
        lv_obj_set_pos(s_list_rows[i], 12, 16 + i * 44);
        lv_obj_set_pos(s_list_labels[i], 10, 2);
        lv_obj_set_pos(s_list_subs[i], 10, 20);
        lv_obj_clear_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[i],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[i],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    set_bar_dual(false);
    bool can = ash_game_duty_available(s_g);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), can ? "执行" : "已满", can, can);
    s_focus = 0;
    if (!can) lv_label_set_text(s_hint, "今天的日勤次数用完了 · 长按返回");
    else {
        snprintf(line, sizeof(line), "今日余%u次 · 长按返回", left);
        lv_label_set_text(s_hint, line);
    }
}

static void activate_duty(void)
{
    if (ok_guard_blocks()) return;
    if (s_list_sel < 0 || s_list_sel >= DUTY_COUNT) {
        ashbox_sfx_play(ASH_SFX_WARN);
        return;
    }
    bool ok = ash_game_do_duty(s_g, (uint8_t)s_list_sel);
    if (ok) {
        ashbox_sfx_play(ASH_SFX_REWARD);
        mark_save_urgent();
        paint_duty();
    } else {
        paint_duty();
        show_block_modal(ASH_ACT_DUTY);
    }
}

static void maybe_coach(uint8_t bit, const char *line0, const char *line1)
{
    if (s_cfg.coach_seen & bit) return;
    s_cfg.coach_seen = (uint8_t)(s_cfg.coach_seen | bit);
    s_cfg_dirty = true;
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], line0 ? line0 : "");
    lv_label_set_text(s_modal_lines[1], line1 ? line1 : "");
    lv_label_set_text(s_modal_lines[2], "");
    lv_label_set_text(s_modal_lines[3], "");
    lv_label_set_text(s_modal_lines[4], "按任意键关闭");
    if (s_modal_btn) {
        lv_obj_clear_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lv_obj_get_child(s_modal_btn, 0), "知道了");
    }
    s_block_modal = true;
}

static void activate_hub(void)
{
    if (ok_guard_blocks()) {
        ESP_LOGI(TAG, "ignore OK (page-enter guard)");
        return;
    }
    ashbox_sfx_play(ASH_SFX_CONFIRM);
    if (s_list_sel == 0) {
        s_list_sel = 0;
        enter_page(UI_DUTY);
        paint_duty();
        return;
    }
    if (s_list_sel == 1) {
        s_list_sel = 0;
        enter_page(UI_FARM);
        paint_farm();
        maybe_coach(ASH_COACH_FARM, "稻谷约1天基础30", "红薯约3天基础240 · 田作会累");
        return;
    }
    if (s_list_sel == 2) {
        s_list_sel = 0;
        enter_page(UI_RES);
        paint_res();
        return;
    }
    if (s_list_sel == 3) {
        s_list_sel = 0;
        enter_page(UI_STASH);
        paint_stash();
        return;
    }
    if (s_list_sel == 4) {
        s_list_sel = 0;
        enter_page(UI_SURVIVORS);
        paint_survivors();
        return;
    }
    if (s_list_sel == 5) {
        s_list_sel = 0;
        enter_page(UI_EQUIP);
        paint_equip();
        return;
    }
    if (s_list_sel == 6) {
        s_list_sel = 0;
        enter_page(UI_BUILD);
        paint_build();
        maybe_coach(ASH_COACH_BUILD, "零件盖建设", "余烬攒满3可开闸去猎场");
        return;
    }
    if (s_list_sel == 7) {
        s_list_sel = 0;
        enter_page(UI_TRADE);
        paint_trade();
        maybe_coach(ASH_COACH_TRADE, "铁棚渡口一天限换", "声望高了同日还能再换一次");
        return;
    }
    if (s_list_sel == 8) {
        s_list_sel = (int)s_g->night_order;
        enter_page(UI_ORDER);
        paint_order();
        maybe_coach(ASH_COACH_ORDER, "过夜站令只改深睡这段", "节食省粮 · 戒备压威胁 · 赶工多零件");
        return;
    }
    if (s_list_sel == 9) {
        s_list_sel = 0;
        enter_page(UI_QUEST);
        paint_quest();
        return;
    }
    if (s_list_sel == 10) {
        s_list_sel = 0;
        enter_page(UI_ACHIEVE);
        paint_achieve();
        return;
    }
    if (s_list_sel == 11) {
        s_list_sel = 0;
        enter_page(UI_JOURNAL);
        paint_journal();
        return;
    }
    if (s_list_sel == 12) {
        s_list_sel = 0;
        enter_page(UI_SETTINGS);
        paint_settings();
        return;
    }
    s_list_sel = 0;
    enter_page(UI_HELP);
    paint_help();
}

static void activate_build(void)
{
    if (ok_guard_blocks()) return;
    bool ok = false;
    ash_act_t fail_act = ASH_ACT_BUILD;
    if (s_list_sel == 3) ok = ash_game_unlock_gate(s_g);
    else if (s_list_sel == 4) ok = ash_game_upgrade_stash(s_g);
    else if (s_list_sel == 5) ok = ash_game_upgrade_furn(s_g);
    else if (s_list_sel == 6) ok = ash_game_upgrade_gear(s_g);
    else if (s_list_sel == 7) ok = ash_game_buy_quiet_gear(s_g);
    else if (s_list_sel == 8) ok = ash_game_buy_door_bolt(s_g);
    else if (s_list_sel == 9) ok = ash_game_buy_stove_lining(s_g);
    else if (s_list_sel == 10) ok = ash_game_buy_box_lamp(s_g);
    else if (s_list_sel == 11) {
        ok = ash_game_build_pit(s_g);
        fail_act = ASH_ACT_BUILD;
    } else if (s_list_sel == 12) {
        ok = ash_game_start_well_upgrade(s_g);
        fail_act = ASH_ACT_WELL_UPGRADE;
    } else if (s_list_sel == 13) {
        ok = ash_game_craft(s_g, ASH_CRAFT_BENCH);
        fail_act = ASH_ACT_BUILD;
    } else if (s_list_sel == 14) {
        ok = ash_game_use_super_core(s_g);
        if (ok) {
            s_meta.super_core_consumed = 1;
            (void)ashbox_meta_store(&s_meta);
        }
    } else if (s_list_sel >= 0 && s_list_sel < 3) ok = ash_game_upgrade(s_g, s_list_sel);
    if (ok) {
        ashbox_sfx_play(ASH_SFX_REWARD);
        mark_save_urgent();
        paint_build();
    } else {
        paint_build();
        show_block_modal(fail_act);
    }
}

static void activate_order(void)
{
    if (ok_guard_blocks()) return;
    if (ash_game_set_night_order(s_g, (ash_night_order_t)s_list_sel)) {
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        mark_save_urgent();
        go_back();
    } else {
        ashbox_sfx_play(ASH_SFX_WARN);
    }
}

static void activate_settings(void)
{
    if (ok_guard_blocks()) return;

    if (ashbox_embackup_ui_state() == ASH_EM_UI_IMPORT_CONFIRM) {
        if (s_focus == 0) {
            ashbox_embackup_confirm_overwrite(false);
            ashbox_sfx_play(ASH_SFX_CLICK);
        } else {
            ashbox_embackup_confirm_overwrite(true);
            ashbox_sfx_play(ASH_SFX_CONFIRM);
        }
        /* Keep confirm UI until USB task applies cancel/accept; tick will repaint. */
        return;
    }

    if (s_list_sel == 0) {
        s_cfg.brightness = ashbox_settings_step_up(s_cfg.brightness, 10);
        persist_cfg();
        ashbox_sfx_play(ASH_SFX_CLICK);
    } else if (s_list_sel == 1) {
        s_cfg.volume = ashbox_settings_step_up(s_cfg.volume, 0);
        persist_cfg();
        if (s_cfg.volume > 0) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
        }
    } else if (s_list_sel == 2) {
        s_cfg.radio_on = s_cfg.radio_on ? 0 : 1;
        persist_cfg();
        ashbox_sfx_play(ASH_SFX_CLICK);
    } else if (s_list_sel == 3) {
        s_cfg.guide_step = 0;
        persist_cfg();
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        go_back();
        paint_home();
        return;
    } else if (s_list_sel == 4) {
        if (ashbox_embackup_busy()) {
            ashbox_sfx_play(ASH_SFX_WARN);
        } else {
            ashbox_embackup_request_backup();
            ashbox_sfx_play(ashbox_embackup_host_ok() ? ASH_SFX_CONFIRM : ASH_SFX_WARN);
        }
    } else if (s_list_sel == 5) {
        if (ashbox_embackup_busy()) {
            ashbox_sfx_play(ASH_SFX_WARN);
        } else if (!ashbox_embackup_host_ok()) {
            ashbox_sfx_play(ASH_SFX_WARN);
        } else {
            ashbox_embackup_enter_import();
            ashbox_sfx_play(ASH_SFX_CONFIRM);
        }
    } else if (s_list_sel == 6) {
        ashbox_sfx_play(ASH_SFX_CONFIRM);
        s_focus = 0; /* cancel default */
        enter_page(UI_RESTART_CONFIRM);
        paint_restart_confirm();
        return;
    }
    paint_settings();
}

static void activate_restart_confirm(void)
{
    if (ok_guard_blocks()) return;
    if (s_focus == 0) {
        ashbox_sfx_play(ASH_SFX_CLICK);
        s_list_sel = 6;
        enter_page(UI_SETTINGS);
        paint_settings();
        return;
    }
    ashbox_sfx_play(ASH_SFX_WARN);
    ash_game_restart_with_meta(s_g, &s_meta);
    (void)ashbox_meta_store(&s_meta);
    mark_save_urgent();
    s_list_sel = 0;
    enter_page(UI_HOME);
    paint_home();
}

static void activate_survivors(void)
{
    if (ok_guard_blocks()) return;
    if (s_g->pop == 0) {
        ashbox_sfx_play(ASH_SFX_CLICK);
        go_back();
        return;
    }
    s_surv_pid = (uint8_t)s_list_sel;
    s_list_sel = 0;
    ashbox_sfx_play(ASH_SFX_CONFIRM);
    enter_page(UI_SURVIVOR_DETAIL);
    paint_survivor_detail();
}

static void activate_survivor_detail(void)
{
    if (ok_guard_blocks()) return;
    if (s_list_sel == 3) {
        uint8_t cur = ash_game_person_job(s_g, s_surv_pid);
        if (cur == 255) {
            ashbox_sfx_play(ASH_SFX_WARN);
            paint_survivor_detail();
            return;
        }
        uint8_t next = (uint8_t)((cur + 1u) % 3u);
        if (ash_game_set_person_job(s_g, s_surv_pid, next)) {
            ashbox_sfx_play(ASH_SFX_CONFIRM);
            mark_save_urgent();
        } else {
            ashbox_sfx_play(ASH_SFX_WARN);
        }
        paint_survivor_detail();
        return;
    }
    if (s_list_sel == 6) {
        if (!s_exile_confirm) {
            char reason[40];
            if (ash_game_exile_blocked_reason(s_g, s_surv_pid, reason, sizeof(reason))) {
                ashbox_sfx_play(ASH_SFX_WARN);
                paint_survivor_detail();
                return;
            }
            s_exile_confirm = true;
            set_modal_visible(true);
            lv_label_set_text(s_modal_lines[0], "请出营地");
            lv_label_set_text(s_modal_lines[1], ash_game_person_name(s_surv_pid));
            lv_label_set_text(s_modal_lines[2], "装备留在营地");
            lv_label_set_text(s_modal_lines[3], "再按一次确定才走");
            lv_label_set_text(s_modal_lines[4], "");
            ashbox_sfx_play(ASH_SFX_WARN);
            set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "确认离开", true, true);
            lv_label_set_text(s_hint, "长按取消 · 再确定才走");
            return;
        }
        s_exile_confirm = false;
        set_modal_visible(false);
        if (ash_game_exile_person(s_g, s_surv_pid)) {
            ashbox_sfx_play(ASH_SFX_BACK);
            mark_save_urgent();
            s_list_sel = 0;
            enter_page(UI_SURVIVORS);
            paint_survivors();
        } else {
            ashbox_sfx_play(ASH_SFX_WARN);
            paint_survivor_detail();
        }
        return;
    }
    s_exile_confirm = false;
    set_modal_visible(false);
    ashbox_sfx_play(ASH_SFX_CLICK);
    s_list_sel = (int)s_surv_pid;
    enter_page(UI_SURVIVORS);
    paint_survivors();
}

static void activate_achieve(void)
{
    if (ok_guard_blocks()) return;
    ashbox_sfx_play(ASH_SFX_CLICK);
    go_back();
}

static void activate_help(void)
{
    if (ok_guard_blocks()) return;
    go_back();
}

static void activate_report(void)
{
    if (ok_guard_blocks()) return;
    if (!s_g->report.branch_resolved && s_g->report.branch_count > 0) {
        if (ash_game_pick_branch(s_g, (uint8_t)s_list_sel)) {
            ashbox_sfx_play(ASH_SFX_REWARD);
            mark_save_urgent();
            paint_report();
        } else {
            ashbox_sfx_play(ASH_SFX_WARN);
        }
        return;
    }
    ashbox_sfx_play(ASH_SFX_REWARD);
    ash_game_ack_report(s_g);
    s_night_aftermath = false;
    mark_save_urgent();
    enter_page(UI_HOME);
    paint_home();
}

static void activate_crisis(void)
{
    if (ok_guard_blocks()) return;
    if (ash_game_resolve_crisis(s_g, (ash_crisis_opt_t)s_list_sel)) {
        ashbox_sfx_play(ASH_SFX_REWARD);
        mark_save_urgent();
        enter_page(UI_HOME);
        paint_home();
    } else {
        ashbox_sfx_play(ASH_SFX_WARN);
        paint_crisis();
    }
}


static void maybe_show_meta_toasts(void)
{
    if (!s_g) return;
    if (s_g->clear_toast_pending || s_g->achieve_toast_mask) {
        uint16_t neu = ash_game_eval_clear_achievements(s_g, &s_meta);
        if (s_g->run_cleared && s_g->clear_toast_pending) {
            (void)neu;
            (void)ashbox_meta_store(&s_meta);
            set_modal_visible(true);
            lv_label_set_text(s_modal_lines[0], "三线了结 · 可继续玩");
            lv_label_set_text(s_modal_lines[1], "成就要重新开始后生效");
            if (s_g->achieve_toast_mask) {
                uint16_t m = s_g->achieve_toast_mask;
                const char *n = "成就";
                for (uint16_t b = 1; b <= ASH_ACH_ALL_MASK; b <<= 1) {
                    if (m & b) { n = ash_ach_name(b); break; }
                }
                lv_label_set_text(s_modal_lines[2], n);
            } else {
                lv_label_set_text(s_modal_lines[2], "");
            }
            lv_label_set_text(s_modal_lines[3], "");
            lv_label_set_text(s_modal_lines[4], "");
            s_g->clear_toast_pending = 0;
            s_g->achieve_toast_mask = 0;
            mark_save_urgent();
            ashbox_sfx_play(ASH_SFX_REWARD);
        }
    }
    if (s_g->end_toast_pending) {
        set_modal_visible(true);
        lv_label_set_text(s_modal_lines[0], "据点空了 · 撑不下去");
        lv_label_set_text(s_modal_lines[1], "可在设置里重新开始");
        lv_label_set_text(s_modal_lines[2], "");
        lv_label_set_text(s_modal_lines[3], "");
        lv_label_set_text(s_modal_lines[4], "");
        if (s_g->run_cleared && !s_g->joined_after_clear) {
            uint16_t neu = ashbox_meta_unlock(&s_meta, ASH_ACH_NOBODY);
            if (neu) (void)ashbox_meta_store(&s_meta);
        }
        s_g->end_toast_pending = 0;
        mark_save_urgent();
        ashbox_sfx_play(ASH_SFX_WARN);
    }
}

static void paint_home_focus_only(void)
{
    set_bar_dual(true);
    bool can = ash_game_can_depart(s_g) && !s_g->exp.active && !s_g->report.pending &&
               !ash_game_crisis_pending(s_g) && !ash_game_wanderer_pending(s_g);
    if (ash_game_crisis_pending(s_g)) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "守夜", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
    } else if (s_g->report.pending) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0),
                s_g->report.branch_resolved ? "入账" : "抉择", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
    } else if (s_g->exp.active && ash_game_chore_pending(s_g)) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "站事", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
    } else if (s_g->exp.active) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "远征中", false, false);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, true);
        s_focus = 1;
    } else if (ash_game_wanderer_pending(s_g)) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "留下", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "打发", true, s_focus == 1);
    } else if (!can && ash_game_duty_available(s_g)) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "日勤", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
    } else if (!can) {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "无法出发", false, false);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, true);
        s_focus = 1;
    } else {
        set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "出发", true, s_focus == 0);
        set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "据点", true, s_focus == 1);
    }
}

static void paint_home_timers_only(void)
{
    char buf[48];
    refresh_wall_clock();
    refresh_home_day_title();
    if (s_g->exp.active) {
        uint32_t rem = s_g->exp.remaining_s;
        if (rem != s_exp_rem_shown) {
            s_exp_rem_shown = rem;
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                     (unsigned)(rem / 3600), (unsigned)((rem / 60) % 60), (unsigned)(rem % 60));
            label_set_if(s_card_value, buf);
        }
        set_card_sub_tip(ash_game_wait_nudge(s_g));
    } else {
        set_card_sub_tip(ash_game_home_nudge(s_g));
    }
    paint_home_threat();
    refresh_battery();
    set_radio_text(ash_game_radio(s_g));
}

static void on_nav_key(bsp_btn_t btn)
{
    const int dir = (btn == BSP_BTN_DOWN) ? 1 : -1;
    if (s_page == UI_HOME) {
        bool two = ash_game_crisis_pending(s_g) || s_g->report.pending ||
                   ash_game_chore_pending(s_g) || ash_game_wanderer_pending(s_g) ||
                   (ash_game_can_depart(s_g) && !s_g->exp.active) ||
                   (ash_game_duty_available(s_g) && !s_g->exp.active);
        if (two) {
            s_focus = wrap(s_focus, 2, dir);
            ashbox_sfx_play(ASH_SFX_MOVE);
        } else {
            s_focus = 1;
            ashbox_sfx_play(ASH_SFX_CLICK);
        }
        paint_home_focus_only();
        return;
    }
    if (s_page == UI_PICK) {
        int n = (s_depart_step == 0)   ? ASH_TIER_COUNT
                : (s_depart_step == 1) ? (int)s_g->pop
                                       : GEAR_PICK_COUNT;
        if (n > ASHBOX_PERSON_MAX && s_depart_step == 1) n = ASHBOX_PERSON_MAX;
        s_list_sel = wrap(s_list_sel, n, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_pick();
        return;
    }
    if (s_page == UI_HUB) {
        s_list_sel = wrap(s_list_sel, HUB_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_hub();
        return;
    }
    if (s_page == UI_BUILD) {
        s_list_sel = wrap(s_list_sel, BUILD_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_build();
        return;
    }
    if (s_page == UI_ORDER) {
        s_list_sel = wrap(s_list_sel, ASH_ORDER_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_order();
        return;
    }
    if (s_page == UI_JOURNAL) {
        int n = (int)s_g->journal_count;
        if (n < 1) n = 1;
        s_list_sel = wrap(s_list_sel, n, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_journal();
        return;
    }
    if (s_page == UI_STASH) {
        s_list_sel = wrap(s_list_sel, STASH_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_stash();
        return;
    }
    if (s_page == UI_RES) {
        s_list_sel = wrap(s_list_sel, res_count(), dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_res();
        return;
    }
    if (s_page == UI_EQUIP) {
        s_list_sel = wrap(s_list_sel, EQUIP_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_equip();
        return;
    }
    if (s_page == UI_FARM) {
        int n = ash_game_farm_unlocked(s_g) ? (s_farm_n > 0 ? s_farm_n : 1) : 1;
        s_list_sel = wrap(s_list_sel, n, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_farm();
        return;
    }
    if (s_page == UI_TRADE) {
        int n = trade_list_count();
        if (n < 1) n = 1;
        s_list_sel = wrap(s_list_sel, n, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_trade();
        return;
    }
    if (s_page == UI_SURVIVORS) {
        int n = s_g->pop > 0 ? (int)s_g->pop : 1;
        s_list_sel = wrap(s_list_sel, n, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_survivors();
        return;
    }
    if (s_page == UI_SURVIVOR_DETAIL) {
        s_list_sel = wrap(s_list_sel, 7, dir);
        s_exile_confirm = false;
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_survivor_detail();
        return;
    }
    if (s_page == UI_ACHIEVE) {
        s_list_sel = wrap(s_list_sel, 5, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_achieve();
        return;
    }
    if (s_page == UI_RESTART_CONFIRM) {
        s_focus = (s_focus == 0) ? 1 : 0;
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_restart_confirm();
        return;
    }
    if (s_page == UI_DUTY) {
        s_list_sel = wrap(s_list_sel, DUTY_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_duty();
        return;
    }
    if (s_page == UI_SETTINGS) {
        if (ashbox_embackup_ui_state() == ASH_EM_UI_IMPORT_CONFIRM) {
            s_focus = (s_focus == 0) ? 1 : 0;
            ashbox_sfx_play(ASH_SFX_MOVE);
            paint_settings();
            return;
        }
        s_list_sel = wrap(s_list_sel, SETTINGS_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_settings();
        return;
    }
    if (s_page == UI_HELP) {
        s_list_sel = wrap(s_list_sel, ASHBOX_HELP_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_help();
        return;
    }
    if (s_page == UI_QUEST) {
        s_list_sel = wrap(s_list_sel, ASHBOX_QUEST_N, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_quest();
        return;
    }
    if (s_page == UI_REPORT && !s_g->report.branch_resolved && s_g->report.branch_count > 0) {
        s_list_sel = wrap(s_list_sel, (int)s_g->report.branch_count, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_report();
        return;
    }
    if (s_page == UI_CRISIS) {
        s_list_sel = wrap(s_list_sel, ASH_CRISIS_COUNT, dir);
        ashbox_sfx_play(ASH_SFX_MOVE);
        paint_crisis();
    }
}

void ashbox_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    /* Runtime lock before LVGL (C-01 lock order). NVS never under LVGL (C-CB1). */
    ashbox_runtime_lock();
    if (!bsp_lvgl_lock(200)) {
        ashbox_runtime_unlock();
        return;
    }
    service_wake_apply_locked();
    power_on_input();

    /* 深睡唤醒静默 / 待入睡：任意新操作（按下）立即结束，恢复声与界面 */
    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK || ev == BSP_BTN_DOUBLE) {
        if (s_wake_guard) {
            s_wake_guard = false;
            s_wake_apply_pending = false;
            ashbox_settings_apply(&s_cfg);
            if (s_page == UI_HOME) paint_home();
            goto key_out;
        }
        if (s_want_sleep) {
            s_want_sleep = false;
            s_arm_sleep = false;
            ashbox_settings_apply(&s_cfg);
            ESP_LOGI(TAG, "deep sleep cancelled by input");
        }
    }

    /* 上下：用 PRESS 即时响应。主页另支持长按在「当前可看场景」间切换。 */
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        /* B17: 块模态打开时勿改背后列表焦点 */
        if (!s_block_modal && ev == BSP_BTN_PRESS) {
            if (s_page == UI_HOME) s_nav_focus_snap = s_focus;
            on_nav_key(btn);
        } else if (!s_block_modal && ev == BSP_BTN_LONG && s_page == UI_HOME) {
            if (s_nav_focus_snap >= 0) {
                s_focus = s_nav_focus_snap;
                s_nav_focus_snap = -1;
                paint_home_focus_only();
            }
            cycle_home_scene((btn == BSP_BTN_DOWN) ? 1 : -1);
        }
        goto key_out;
    }

    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        if (s_page == UI_HOME) {
            if (guide_active()) {
                skip_guide();
            } else {
                mark_save_urgent();
                s_arm_sleep = true;
                ashbox_sfx_play(ASH_SFX_SLEEP);
            }
        } else if (s_page == UI_PICK || s_page == UI_HUB || s_page == UI_BUILD ||
                   s_page == UI_SETTINGS || s_page == UI_HELP || s_page == UI_QUEST ||
                   s_page == UI_ORDER || s_page == UI_JOURNAL || s_page == UI_STASH ||
                   s_page == UI_RES || s_page == UI_EQUIP || s_page == UI_FARM ||
                   s_page == UI_TRADE || s_page == UI_DUTY || s_page == UI_SURVIVORS ||
                   s_page == UI_SURVIVOR_DETAIL || s_page == UI_ACHIEVE ||
                   s_page == UI_RESTART_CONFIRM) {
            go_back();
        } else if (s_page == UI_REPORT || s_page == UI_CRISIS) {
            /* 战报/危机须醒着明确抉择；深睡不自动选 */
        }
        goto key_out;
    }
    /* 连按被合成 DOUBLE 时补成一次确认，避免「快按确定没反应」 */
    if (ev == BSP_BTN_DOUBLE) ev = BSP_BTN_CLICK;
    if (ev != BSP_BTN_CLICK || btn != BSP_BTN_OK) {
        goto key_out;
    }

    if (s_block_modal) {
        set_modal_visible(false);
        ashbox_sfx_play(ASH_SFX_CLICK);
        goto key_out;
    }
    if (s_page == UI_HOME) {
        if (!ok_guard_blocks()) activate_home();
    } else if (s_page == UI_PICK) {
        activate_pick();
    } else if (s_page == UI_HUB) {
        activate_hub();
    } else if (s_page == UI_BUILD) {
        activate_build();
    } else if (s_page == UI_ORDER) {
        activate_order();
    } else if (s_page == UI_JOURNAL) {
        if (!ok_guard_blocks()) go_back();
    } else if (s_page == UI_STASH) {
        activate_stash();
    } else if (s_page == UI_RES) {
        activate_res();
    } else if (s_page == UI_EQUIP) {
        activate_equip();
    } else if (s_page == UI_FARM) {
        activate_farm();
    } else if (s_page == UI_TRADE) {
        activate_trade();
    } else if (s_page == UI_SURVIVORS) {
        activate_survivors();
    } else if (s_page == UI_SURVIVOR_DETAIL) {
        activate_survivor_detail();
    } else if (s_page == UI_ACHIEVE) {
        activate_achieve();
    } else if (s_page == UI_RESTART_CONFIRM) {
        activate_restart_confirm();
    } else if (s_page == UI_DUTY) {
        activate_duty();
    } else if (s_page == UI_SETTINGS) {
        activate_settings();
    } else if (s_page == UI_HELP) {
        activate_help();
    } else if (s_page == UI_QUEST) {
        if (!ok_guard_blocks()) go_back();
    } else if (s_page == UI_REPORT) {
        activate_report();
    } else if (s_page == UI_CRISIS) {
        activate_crisis();
    }

key_out:
    bsp_lvgl_unlock();
    flush_nvs_runtime_only(false);
    if (s_arm_sleep) {
        s_arm_sleep = false;
        s_want_sleep = true;
    }
    ashbox_runtime_unlock();
}

int ashbox_ui_tick_delay_ms(void)
{
    if (s_want_sleep) return 100;
    if (s_pwr == PWR_BLANK) return 200;
    if (s_pwr == PWR_DIM) return 100;
    /* Awake home/menus: keep keys and sprite anim near LVGL refresh cadence. */
    return 40;
}

void ashbox_ui_tick(void)
{
    int64_t now = esp_timer_get_time();
    int64_t dt_us = now - s_mono_last_us;
    if (dt_us < 0) dt_us = 0;
    s_mono_last_us = now;
    /* Adaptive main loop (~40–200 ms); accumulate µs so settle still hits 1 s. */
    s_settle_accum_us += dt_us;
    int64_t dt_s = s_settle_accum_us / 1000000LL;
    if (dt_s > 0) {
        s_settle_accum_us -= dt_s * 1000000LL;
        ash_clock_info_t clk = {
            .trusted = true,
            .in_deep_sleep_segment = false,
            .dt_s = dt_s,
        };
        ashbox_runtime_lock();
        bool had_report = s_g->report.pending;
        bool had_crisis = ash_game_crisis_pending(s_g);
        bool had_chore = ash_game_chore_pending(s_g);
        int had_farm_ready = ash_game_farm_ready_count(s_g);
        bool was_exp = s_g->exp.active;
        ash_game_settle(s_g, &clk);
        maybe_show_meta_toasts();
        /* settle 用单调 dt 推进逻辑；仅在 RTC 仍贴合存档时间轴时回写时钟。
           断电后 RTC 归零时禁止 note_wall_time，否则会把 day_key/日勤打回第 1 天。 */
        {
            struct timeval tv;
            if (gettimeofday(&tv, NULL) == 0) {
                ash_game_note_wall_time(s_g, (int64_t)tv.tv_sec);
            }
        }
        bool urgent = false;
        bool need_wake = false;
        const char *wake_why = NULL;
        if (s_g->report.pending && !had_report) {
            s_night_aftermath = true;
            urgent = true;
            need_wake = true;
            wake_why = "report";
            ashbox_sfx_play(ASH_SFX_REWARD);
        }
        if (ash_game_crisis_pending(s_g) && !had_crisis) {
            urgent = true;
            need_wake = true;
            wake_why = "crisis";
            ashbox_sfx_play(ASH_SFX_WARN);
        }
        if (ash_game_chore_pending(s_g) && !had_chore) {
            urgent = true;
            need_wake = true;
            if (!wake_why) wake_why = "chore";
            ashbox_sfx_play(ASH_SFX_CLICK);
        }
        if (ash_game_farm_ready_count(s_g) > had_farm_ready) {
            need_wake = true;
            if (!wake_why) wake_why = "farm";
        }
        /* 先亮屏再绘：调暗态也要拉回全亮，不能只处理熄屏 */
        if (need_wake) power_attention_wake(wake_why);
        /* settle 已推进 remaining_s / 菜地；必须周期性落盘，保档刷机才接得上 */
        persist_after_settle(urgent);
        if (s_cfg_dirty) {
            flush_nvs_runtime_only(false);
        }
        sample_battery(); /* I2C outside LVGL (C-BAT) */
        /* 熄屏后仍 settle/存档；注意力事件已亮屏则可重绘 */
        if (s_pwr != PWR_BLANK && bsp_lvgl_lock(50)) {
            service_wake_apply_locked();
            if (s_page == UI_HOME) {
                uint32_t fp = home_ui_fingerprint();
                if (need_wake || (was_exp && !s_g->exp.active) ||
                    (ash_game_chore_pending(s_g) != had_chore) || fp != s_home_fp) {
                    paint_home();
                    s_home_fp = home_ui_fingerprint();
                } else {
                    paint_home_timers_only();
                }
            } else {
                refresh_wall_clock();
            }
            bsp_lvgl_unlock();
        }
        ashbox_runtime_unlock();
    } else {
        /* Sub-second ticks still refresh battery cache without I2C-in-paint. */
        sample_battery();
    }
    if (bsp_lvgl_lock(20)) {
        if (s_pwr == PWR_ACTIVE) {
            tick_sprite_anim();
        }
        {
            static ash_em_ui_t s_em_ui_seen = ASH_EM_UI_IDLE;
            static int s_em_pct_seen = -2;
            ash_em_ui_t em = ashbox_embackup_ui_state();
            int pct = ashbox_embackup_progress_pct();
            bool rose = (em != s_em_ui_seen) &&
                        (em == ASH_EM_UI_IMPORT_CONFIRM ||
                         em == ASH_EM_UI_BACKUP_BUSY ||
                         em == ASH_EM_UI_IMPORT_BUSY ||
                         em == ASH_EM_UI_IMPORT_DONE_HINT);
            /* Jump only on edge into attention — never yank user back every tick. */
            if (rose && s_page != UI_SETTINGS) {
                power_attention_wake("embackup");
                if (em == ASH_EM_UI_IMPORT_CONFIRM) {
                    s_focus = 0; /* 默认取消 */
                }
                enter_page(UI_SETTINGS);
                paint_settings();
                s_em_ui_seen = em;
                s_em_pct_seen = pct;
            } else if (s_page == UI_SETTINGS &&
                       (em != s_em_ui_seen || pct != s_em_pct_seen)) {
                if (em == ASH_EM_UI_IMPORT_CONFIRM && em != s_em_ui_seen) {
                    s_focus = 0;
                }
                s_em_ui_seen = em;
                s_em_pct_seen = pct;
                paint_settings();
            } else if (em != s_em_ui_seen) {
                s_em_ui_seen = em;
                s_em_pct_seen = pct;
            }
        }
        bsp_lvgl_unlock();
    }
    power_tick(dt_us);
}

bool ashbox_ui_wants_deep_sleep(void) { return s_want_sleep; }
void ashbox_ui_clear_deep_sleep_request(void) { s_want_sleep = false; }

void ashbox_ui_on_deep_sleep_aborted(void)
{
    ashbox_runtime_lock();
    s_want_sleep = false;
    s_idle_us = 0;
    s_pwr = PWR_ACTIVE;
    ashbox_settings_apply(&s_cfg);
    if (bsp_lvgl_lock(200)) {
        if (s_page == UI_HOME) paint_home();
        bsp_lvgl_unlock();
    }
    ashbox_runtime_unlock();
}

bool ashbox_ui_wake_guard_active(void) { return s_wake_guard; }

void ashbox_ui_request_clear_wake_guard(void)
{
    /* Button / ISR-adjacent callback: flags only, no LVGL (C-02). */
    if (!s_wake_guard) return;
    s_wake_guard = false;
    s_wake_apply_pending = true;
}

void ashbox_ui_set_wake_guard(bool on)
{
    ashbox_runtime_lock();
    s_wake_guard = on;
    if (!on) s_wake_apply_pending = true;
    if (bsp_lvgl_lock(200)) {
        if (!on) service_wake_apply_locked();
        else if (s_page == UI_HOME) paint_home();
        bsp_lvgl_unlock();
    }
    ashbox_runtime_unlock();
}
