/* Survivors, detail, achievements and restart confirm. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include <stdio.h>


void paint_survivors(void)
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

void paint_survivor_detail(void)
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
            if (p->state == ASH_PERSON_INJURED && p->injure_remain_s > 0)
                snprintf(sub, sizeof(sub), "  重伤约剩%u时 ·要用药  ·  ",
                         (unsigned)((p->injure_remain_s + 3599u) / 3600u));
            else if (p->injure_remain_s > 0)
                snprintf(sub, sizeof(sub), "  热伤约剩%u时  ·  ",
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

void paint_achieve(void)
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

void paint_restart_confirm(void)
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
