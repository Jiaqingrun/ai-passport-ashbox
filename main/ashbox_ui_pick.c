/* Departure picker (tier / person / loadout). Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>


void paint_pick(void)
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
                snprintf(sub, sizeof(sub), "  扣粮%u水%u · 空手%u%% · %s   ·  ",
                         (unsigned)ash_game_expedition_food_cost((ash_tier_t)i),
                         (unsigned)ash_game_expedition_water_cost((ash_tier_t)i),
                         ash_game_depart_fail_pct(s_g, (ash_tier_t)i, 0, 0, 0),
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
                snprintf(sub, sizeof(sub), "  空手%u%% · 信任%+d · 疲劳%u   ·  ",
                         ash_game_depart_fail_pct(s_g, s_pick_tier, (uint8_t)i, 0, 0),
                         (int)(*ash_game_person_mut(s_g, (unsigned)(i))).trust,
                         (unsigned)(*ash_game_person_mut(s_g, (unsigned)(i))).fatigue);
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
                snprintf(sub, sizeof(sub), "  口粮%u 水%u · 空手%u%%  ·  ", (unsigned)fc,
                         (unsigned)wc,
                         ash_game_depart_fail_pct(s_g, s_pick_tier, s_pick_person, 0, 0));
            } else if (i == 1) {
                snprintf(line, sizeof(line), "带撬棍");
                snprintf(sub, sizeof(sub), "  仓有%u ·上限%u · 空手%u%%  ·  ", (unsigned)s_g->crowbar,
                         (unsigned)crow_cap,
                         ash_game_depart_fail_pct(s_g, s_pick_tier, s_pick_person, 0, 0));
            } else {
                snprintf(line, sizeof(line), "带手枪");
                snprintf(sub, sizeof(sub), "  枪%u 弹%u · 空手%u%% · 挡袭击  ·  ",
                         (unsigned)s_g->pistol, (unsigned)s_g->ammo,
                         ash_game_depart_fail_pct(s_g, s_pick_tier, s_pick_person, 1, 1));
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
