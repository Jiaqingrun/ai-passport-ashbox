/* Help page: chapter chooser and the selected chapter's lines. */
#include "ashbox_ui_page.h"

#include "ashbox_help_copy.h"

#include <stdio.h>

/* Eight reading chapters; their first line is tinted in the topic view. The
 * indices follow the help copy order. */
static const uint8_t k_help_section[] = {0, 4, 38, 50, 59, 63, 71, 77};
#define HELP_CHAPTER_N (int)(sizeof(k_help_section) / sizeof(k_help_section[0]))
static const char *const k_help_chapter_name[] = {
    "基本操作", "日常节奏", "建造与资源", "威胁与守夜",
    "线索与故事", "过客与营地", "成就与重开", "远征与长线",
};

int s_help_chapter;

static int help_chapter_of_line(int line)
{
    int sec = 0;
    for (int i = 0; i < HELP_CHAPTER_N; i++) {
        if (line >= k_help_section[i]) sec = i;
    }
    return sec;
}

static int help_topic_start(int chapter)
{
    if (chapter < 0) chapter = 0;
    if (chapter >= HELP_CHAPTER_N) chapter = HELP_CHAPTER_N - 1;
    return (int)k_help_section[chapter];
}

static int help_topic_count(int chapter)
{
    int start = help_topic_start(chapter);
    int end = (chapter + 1 < HELP_CHAPTER_N) ? (int)k_help_section[chapter + 1] : ASHBOX_HELP_COUNT;
    int n = end - start;
    return n > 0 ? n : 0;
}

int ashbox_help_topic_count(void)
{
    return help_topic_count(s_help_chapter);
}

int ashbox_help_chapter_count(void)
{
    return HELP_CHAPTER_N;
}

void paint_help(void)
{
    s_page = UI_HELP;
    show_home_chrome(false);
    lv_label_set_text(s_title, "怎么玩");
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= HELP_CHAPTER_N) s_list_sel = HELP_CHAPTER_N - 1;
    const int view = 5;
    const int top = list_scroll_top(s_list_sel, HELP_CHAPTER_N, view);
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= HELP_CHAPTER_N) break;
        lv_obj_add_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(s_list_labels[slot], 196);
        lv_obj_set_height(s_list_labels[slot], 18);
        lv_label_set_text(s_list_labels[slot], k_help_chapter_name[i]);
        lv_obj_set_style_text_color(s_list_labels[slot], lv_color_hex(COL_TEXT), 0);
        lv_obj_set_size(s_list_rows[slot], 216, 34);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 14, 6);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot], lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot], lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, HELP_CHAPTER_N, view);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "翻看", true, true);
    s_focus = 0;
    char hint[48];
    snprintf(hint, sizeof(hint), "第%d/%d段 · 选一段读", s_list_sel + 1, HELP_CHAPTER_N);
    lv_label_set_text(s_hint, hint);
}

void paint_help_topic(void)
{
    s_page = UI_HELP_TOPIC;
    show_home_chrome(false);
    if (s_help_chapter < 0) s_help_chapter = 0;
    if (s_help_chapter >= HELP_CHAPTER_N) s_help_chapter = HELP_CHAPTER_N - 1;
    int n = help_topic_count(s_help_chapter);
    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= n) s_list_sel = n - 1;
    if (s_list_sel < 0) s_list_sel = 0;
    lv_label_set_text(s_title, k_help_chapter_name[s_help_chapter]);
    const int view = ASHBOX_HELP_VIEW;
    int top = 0;
    if (s_list_sel >= view) top = s_list_sel - view + 1;
    if (top > n - view) top = n - view;
    if (top < 0) top = 0;
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < view; slot++) {
        int i = top + slot;
        if (i >= n) break;
        int line = help_topic_start(s_help_chapter) + i;
        bool sel = (s_list_sel == i);
        lv_obj_set_size(s_list_rows[slot], 216, ASHBOX_HELP_ROW_H);
        lv_obj_set_pos(s_list_rows[slot], 12, 8 + slot * ASHBOX_HELP_ROW_STEP);
        lv_obj_set_pos(s_list_labels[slot], 14, 4);
        lv_obj_add_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(s_list_labels[slot], ASHBOX_HELP_LABEL_W);
        lv_obj_set_height(s_list_labels[slot], ASHBOX_HELP_ROW_H - 8);
        lv_obj_set_style_text_align(s_list_labels[slot], LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(s_list_labels[slot], ashbox_help_line((unsigned)line));
        lv_obj_set_style_text_color(
            s_list_labels[slot],
            lv_color_hex(help_chapter_of_line(line) >= 0 && k_help_section[help_chapter_of_line(line)] == line
                             ? COL_AMBER
                             : COL_TEXT),
            0);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_list_rows[slot], lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot], lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, n, view);
    set_modal_visible(false);
    set_bar_dual(false);
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "返回", true, true);
    s_focus = 0;
    char hint[48];
    snprintf(hint, sizeof(hint), "第%d/%d行 · 确定返回", s_list_sel + 1, n);
    lv_label_set_text(s_hint, hint);
}
