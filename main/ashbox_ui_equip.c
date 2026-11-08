/* Equip page: per-person gear plus the craft-bench items. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>


/* 装备页末尾：水罐/手枪/子弹/撬棍（工作台在建设页） */

int equip_person_rows(void)
{
    int pop = (s_g && s_g->pop > 0) ? (int)s_g->pop : 1;
    if (pop > ASHBOX_PERSON_MAX) pop = ASHBOX_PERSON_MAX;
    return pop * 2;
}

int equip_craft_base(void)
{
    return equip_person_rows();
}

int equip_count(void)
{
    return equip_person_rows() + EQUIP_CRAFT_N;
}



ash_craft_t equip_craft_kind(int row)
{
    static const ash_craft_t k_map[EQUIP_CRAFT_N] = {
        ASH_CRAFT_BUCKET, ASH_CRAFT_PISTOL, ASH_CRAFT_AMMO, ASH_CRAFT_CROWBAR,
    };
    int off = row - equip_craft_base();
    if (off < 0 || off >= EQUIP_CRAFT_N) return ASH_CRAFT_BUCKET;
    return k_map[off];
}
void paint_equip(void)
{
    s_page = UI_EQUIP;
    show_home_chrome(false);
    lv_label_set_text(s_title, "装备");
    char line[48];
    char sub[64];
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= equip_count()) s_list_sel = equip_count() - 1;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    int view = 5;
    if (equip_count() < view) view = equip_count();
    int top = list_scroll_top(s_list_sel, equip_count(), view);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= equip_count()) break;
        bool cost_row = false;
        bool enough = true;
        if (i >= equip_craft_base()) {
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
    paint_hub_scrollbar(s_list_sel, equip_count(), view);
    set_bar_dual(false);
    if (s_list_sel >= equip_craft_base()) {
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
