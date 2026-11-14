/* C2 leg choice + E1 plan page (M4). Split out of the core dispatcher. */
#include "ashbox_ui_page.h"

#include "ashbox_sfx.h"

#include <stdio.h>

void paint_projects(void)
{
    s_page = UI_PROJECTS;
    show_home_chrome(false);
    set_modal_visible(false);
    lv_label_set_text(s_title, "计划");
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= ASH_PLAN_COUNT) s_list_sel = ASH_PLAN_COUNT - 1;
    char line[48];
    char sub[96];
    for (int i = 0; i < ASH_PLAN_COUNT; i++) {
        uint16_t cost = ash_game_plan_cost_parts((uint8_t)i);
        uint8_t have = ash_game_plan_parts(s_g, (uint8_t)i);
        bool done = ash_game_plan_done(s_g, (uint8_t)i);
        snprintf(line, sizeof(line), "%s  %s", ash_game_plan_name((uint8_t)i),
                 done ? "已完成" : "");
        uint16_t iron = ash_game_plan_cost_iron((uint8_t)i);
        if (done) {
            snprintf(sub, sizeof(sub), "  %s  ·  ", ash_game_plan_effect_text((uint8_t)i));
        } else if (have >= cost) {
            snprintf(sub, sizeof(sub), "  件%u/%u 铁%u/%u · 铁够才落成  ", (unsigned)have,
                     (unsigned)cost, (unsigned)s_g->iron, (unsigned)iron);
        } else {
            snprintf(sub, sizeof(sub), "  件%u/%u 铁%u/%u · %s  ", (unsigned)have,
                     (unsigned)cost, (unsigned)s_g->iron, (unsigned)iron,
                     ash_game_plan_effect_text((uint8_t)i));
        }
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 36);
        lv_obj_set_pos(s_list_rows[i], 12, 4 + i * 36);
        lv_obj_set_pos(s_list_labels[i], 10, 1);
        lv_obj_set_pos(s_list_subs[i], 10, 17);
    }
    for (int i = ASH_PLAN_COUNT; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    paint_list_sel(ASH_PLAN_COUNT);
    set_bar_dual(false);
    set_btn(s_btn_pri, s_btn_pri_lab, "献5件", true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, "献5件 · 满件+铁够才落成");
}

void activate_projects(void)
{
    if (ash_game_plan_done(s_g, (uint8_t)s_list_sel)) {
        ashbox_sfx_play(ASH_SFX_WARN);
        return;
    }
    if (ash_game_plan_contribute(s_g, (uint8_t)s_list_sel)) {
        ashbox_sfx_play(ASH_SFX_REWARD);
        ashbox_ui_mark_save_urgent();
        paint_projects();
    } else {
        ashbox_sfx_play(ASH_SFX_WARN);
        if (ash_game_plan_parts(s_g, (uint8_t)s_list_sel) >=
            ash_game_plan_cost_parts((uint8_t)s_list_sel)) {
            lv_label_set_text(s_hint, "件满了 · 铁还不够 · 再攒铁");
        }
        ashbox_ui_mark_save_urgent();
    }
}

void paint_leg(void)
{
    s_page = UI_LEG;
    show_home_chrome(false);
    set_modal_visible(false);
    lv_label_set_text(s_title, "途中");
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= ASH_LEG_CHOICE_COUNT) s_list_sel = ASH_LEG_CHOICE_COUNT - 1;
    char line[48];
    char sub[96];
    for (int i = 0; i < ASH_LEG_CHOICE_COUNT; i++) {
        if (i == ASH_LEG_CONTINUE) {
            snprintf(line, sizeof(line), "继续走");
            snprintf(sub, sizeof(sub), "  下一段更险 · 空手+8%%  ·  ");
        } else {
            snprintf(line, sizeof(line), "收手回站");
            snprintf(sub, sizeof(sub), "  带回已走段的六成 · 不再冒险  ·  ");
        }
        lv_label_set_text(s_list_labels[i], line);
        lv_label_set_text(s_list_subs[i], sub);
        lv_label_set_long_mode(s_list_labels[i], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(s_list_labels[i], 196);
        lv_obj_set_width(s_list_subs[i], 196);
        lv_obj_clear_flag(s_list_subs[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_list_rows[i], 216, 56);
        lv_obj_set_pos(s_list_rows[i], 12, 24 + i * 64);
        lv_obj_set_pos(s_list_labels[i], 10, 6);
        lv_obj_set_pos(s_list_subs[i], 10, 30);
    }
    for (int i = ASH_LEG_CHOICE_COUNT; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    paint_list_sel(ASH_LEG_CHOICE_COUNT);
    set_bar_dual(false);
    set_btn(s_btn_pri, s_btn_pri_lab, "决定", true, true);
    s_focus = 0;
    char hint[64];
    snprintf(hint, sizeof(hint), "已走%u/%u段 · 人还在外面",
             (unsigned)s_g->exp_leg, (unsigned)s_g->exp_leg_total);
    lv_label_set_text(s_hint, hint);
}

void activate_leg(void)
{
    bool withdrew = (s_list_sel != ASH_LEG_CONTINUE);
    bool ok = withdrew ? ash_game_leg_withdraw(s_g) : ash_game_leg_continue(s_g);
    if (!ok) {
        ashbox_sfx_play(ASH_SFX_WARN);
        return;
    }
    ashbox_sfx_play(withdrew ? ASH_SFX_REWARD : ASH_SFX_CONFIRM);
    ashbox_ui_mark_save_urgent();
    ashbox_ui_after_leg(withdrew);
}
