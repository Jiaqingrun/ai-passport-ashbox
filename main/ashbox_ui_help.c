/* Help page: the "how to play" list. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include "ashbox_help_copy.h"

/* U-2: eight reading sections; their first line is tinted so the list reads as
 * chapters instead of one 94-line wall. The indices follow the help copy order. */
static const uint8_t k_help_section[] = {0, 4, 38, 50, 59, 63, 71, 77};
#define HELP_SECTION_N (int)(sizeof(k_help_section) / sizeof(k_help_section[0]))

static int help_section_of(int line)
{
    int sec = 0;
    for (int i = 0; i < HELP_SECTION_N; i++) {
        if (line >= k_help_section[i]) sec = i;
    }
    return sec;
}

void paint_help(void)
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
        lv_obj_set_style_text_color(
            s_list_labels[slot],
            lv_color_hex(help_section_of(i) >= 0 && k_help_section[help_section_of(i)] == i
                             ? COL_AMBER
                             : COL_TEXT),
            0);
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
    {
        char hint[48];
        snprintf(hint, sizeof(hint), "第%u段 · %d/%u · 确定返回",
                 (unsigned)(help_section_of(s_list_sel) + 1), s_list_sel + 1,
                 (unsigned)ASHBOX_HELP_COUNT);
        lv_label_set_text(s_hint, hint);
    }
}
