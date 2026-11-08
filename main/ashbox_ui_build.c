/* Build page: base construction list. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>


void paint_build(void)
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
