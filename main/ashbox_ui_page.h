#pragma once

/* Internal contract between the core UI unit and the page units.
 *
 * The public surface stays in ashbox_ui.h; everything here is shared only so a
 * page painter can live in its own translation unit without changing behaviour.
 * Lock order is unchanged: ashbox_runtime_lock -> bsp_lvgl_lock.
 */

#include "ashbox_game.h"
#include "ashbox_meta.h"
#include "ashbox_settings.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#define ASH_LIST_MAX 16

/* Page list sizes the core dispatcher also queries. */
#define STASH_COUNT 4
#define STASH_VIEW 4
/* U-4: rows follow the living roster, not the six save slots. */
#define EQUIP_CRAFT_N 4
int equip_person_rows(void);
int equip_craft_base(void);
int equip_count(void);

/* Page list sizes (single source of truth for unit and dispatcher). */
#define BUILD_COUNT 15 /* +工作台；超级核心仍在末项 */
#define BUILD_VIEW 5
#define FARM_VIEW 4
#define SETTINGS_COUNT 7
#define SETTINGS_VIEW 5
#define PICK_TIER_VIEW 4
#define GEAR_PICK_COUNT 3 /* 轻装 / 撬棍 / 手枪 — 勿绑 ASH_LOAD_COUNT */

/* Page palette (shared by every painter). */
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

/* Page-build state owned by ashbox_ui.c. */
extern ash_game_t *s_g;
extern ui_page_t s_page;
extern lv_obj_t *s_title;
extern lv_obj_t *s_hint;
extern lv_obj_t *s_btn_pri;
extern lv_obj_t *s_btn_pri_lab;
extern lv_obj_t *s_btn_sec;
extern lv_obj_t *s_list_rows[ASH_LIST_MAX];
extern lv_obj_t *s_list_labels[ASH_LIST_MAX];
extern lv_obj_t *s_list_subs[ASH_LIST_MAX];
extern int s_focus;
extern int s_list_sel;
extern lv_obj_t *s_modal_btn;
extern lv_obj_t *s_modal_lines[5];
extern ashbox_meta_t s_meta;
extern ash_settings_t s_cfg;
extern uint8_t s_surv_pid;
extern int s_depart_step;
extern ash_tier_t s_pick_tier;
extern uint8_t s_pick_person;
extern ash_loadout_t s_pick_load;
extern uint8_t s_farm_plot[ASH_LIST_MAX];
extern uint8_t s_farm_act[ASH_LIST_MAX];
extern int s_farm_n;

/* Shared widget helpers (defined in ashbox_ui.c). */
void show_home_chrome(bool home);
void set_btn(lv_obj_t *btn, lv_obj_t *lab, const char *txt, bool on, bool focus);
void set_bar_dual(bool dual);
void set_modal_visible(bool on);
void set_cost_color(lv_obj_t *lab, bool enough);
int list_scroll_top(int sel, int count, int view);
void paint_hub_scrollbar(int sel, int count, int view);
void paint_list_sel(int count);

/* Page painters implemented in their own units (called from the core dispatcher). */
void paint_help(void);
void paint_trade(void);
void paint_stash(void);
void paint_equip(void);
void paint_pick(void);
void paint_build(void);
void paint_farm(void);
void paint_settings(void);
void paint_settings_confirm(void);
void paint_survivors(void);
void paint_survivor_detail(void);
void paint_achieve(void);
void paint_restart_confirm(void);

/* Page helpers the core dispatcher also queries. */
ash_craft_t equip_craft_kind(int row);
bool trade_tide_offer_ok(void);
int trade_list_count(void);
bool trade_sel_is_tide(int sel);
ash_trade_t trade_at_sel(int sel);
