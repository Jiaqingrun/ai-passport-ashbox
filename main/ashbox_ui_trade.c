/* Trade page: camp/ferry offers plus the shard trade. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>

#define TRADE_VIEW 4

bool trade_tide_offer_ok(void)
{
    return s_g && !s_g->quest2_done && s_g->quest2_step == 2 &&
           s_g->item[ASH_ITEM_SHARD] >= 2;
}

static const ash_trade_t k_camp_trades[] = {
    ASH_TRADE_JERKY_FOR_SEED, ASH_TRADE_FOOD_FOR_WATER, ASH_TRADE_WATER_FOR_FOOD,
    ASH_TRADE_WATER_FOR_IRON, ASH_TRADE_IRON_FOR_WATER, ASH_TRADE_IRON_FOR_PARTS,
    ASH_TRADE_PARTS_FOR_IRON, ASH_TRADE_PARTS_FOR_EMBER, ASH_TRADE_PARTS_FOR_STIM,
    ASH_TRADE_PARTS_FOR_ABX, ASH_TRADE_EMBER_FOR_PARTS, ASH_TRADE_EMBER_FOR_WATER,
    ASH_TRADE_EMBER_FOR_STIM,
};
static const ash_trade_t k_ferry_trades[] = {
    ASH_TRADE_JERKY_FOR_ROOT, ASH_TRADE_PARTS_FOR_ROOT, ASH_TRADE_FOOD_FOR_WATER,
    ASH_TRADE_WATER_FOR_IRON, ASH_TRADE_IRON_FOR_PARTS, ASH_TRADE_PARTS_FOR_EMBER,
    ASH_TRADE_EMBER_FOR_PARTS, ASH_TRADE_EMBER_FOR_WATER,
};

int trade_list_count(void)
{
    int n = 0;
    if (ash_game_camp_open(s_g)) n += (int)(sizeof(k_camp_trades) / sizeof(k_camp_trades[0]));
    if (ash_game_camp2_open(s_g)) n += (int)(sizeof(k_ferry_trades) / sizeof(k_ferry_trades[0]));
    if (trade_tide_offer_ok()) n += 1;
    return n;
}

bool trade_sel_is_tide(int sel)
{
    int base = 0;
    if (ash_game_camp_open(s_g)) base += (int)(sizeof(k_camp_trades) / sizeof(k_camp_trades[0]));
    if (ash_game_camp2_open(s_g)) base += (int)(sizeof(k_ferry_trades) / sizeof(k_ferry_trades[0]));
    return trade_tide_offer_ok() && sel == base;
}

ash_trade_t trade_at_sel(int sel)
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

void paint_trade(void)
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
