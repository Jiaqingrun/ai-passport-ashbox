/* Stash page: storage size and shelves. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>

#define STASH_COUNT 4
#define STASH_VIEW 4

void paint_stash(void)
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
