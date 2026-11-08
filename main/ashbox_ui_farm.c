/* Farm page: plots and farm actions. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>


void paint_farm(void)
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
