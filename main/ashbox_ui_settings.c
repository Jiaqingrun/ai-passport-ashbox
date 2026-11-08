/* Settings page and its confirm modal. Page unit split out of ashbox_ui.c (C-UI1). */
#include "ashbox_ui_page.h"

#include "ashbox_embackup.h"

#include <stdio.h>


void paint_settings(void)
{
    s_page = UI_SETTINGS;
    show_home_chrome(false);
    lv_label_set_text(s_title, "设置");

    ash_em_ui_t em = ashbox_embackup_ui_state();
    if (em == ASH_EM_UI_IMPORT_CONFIRM) {
        paint_settings_confirm();
        return;
    }

    char titles[SETTINGS_COUNT][48];
    char subs[SETTINGS_COUNT][40];
    snprintf(titles[0], sizeof(titles[0]), "亮度  %u%%", (unsigned)s_cfg.brightness);
    snprintf(subs[0], sizeof(subs[0]), "确定 +10%%  上下选");
    snprintf(titles[1], sizeof(titles[1]), "音量  %u%%", (unsigned)s_cfg.volume);
    snprintf(subs[1], sizeof(subs[1]), "音效音量");
    snprintf(titles[2], sizeof(titles[2]), "%s", s_cfg.radio_on ? "电台声  开" : "电台声  关");
    snprintf(subs[2], sizeof(subs[2]), "背景噪声");
    snprintf(titles[3], sizeof(titles[3]), "重看开局说明");
    snprintf(subs[3], sizeof(subs[3]), "再走一回入门");
    snprintf(titles[4], sizeof(titles[4]), "备份存档");
    snprintf(subs[4], sizeof(subs[4]), "%s",
             ashbox_embackup_host_ok() ? "导出到存档管家" : "需先连接存档管家");
    snprintf(titles[5], sizeof(titles[5]), "导入存档");
    snprintf(subs[5], sizeof(subs[5]), "%s",
             ashbox_embackup_host_ok() ? "等待管家发来后确认" : "需先连接存档管家");
    snprintf(titles[6], sizeof(titles[6]), "重新开始");
    snprintf(subs[6], sizeof(subs[6]), "只留成就 · 本局清空");

    char status_buf[40];
    const char *status = NULL;
    int pct = ashbox_embackup_progress_pct();
    if (em == ASH_EM_UI_NO_HOST) {
        status = "请先打开存档管家并连接";
    } else if (em == ASH_EM_UI_BACKUP_BUSY) {
        if (pct >= 0) {
            snprintf(status_buf, sizeof(status_buf), "正在备份 %d%%", pct);
            status = status_buf;
        } else {
            status = "正在备份";
        }
    } else if (em == ASH_EM_UI_BACKUP_OK) {
        status = "备份完成";
    } else if (em == ASH_EM_UI_BACKUP_FAIL) {
        status = "备份未完成";
    } else if (em == ASH_EM_UI_IMPORT_WAIT) {
        status = "等待管家发来文件";
    } else if (em == ASH_EM_UI_IMPORT_BUSY) {
        if (ashbox_embackup_import_preamble()) {
            if (pct >= 0) {
                snprintf(status_buf, sizeof(status_buf), "备份当前 %d%%", pct);
                status = status_buf;
            } else {
                status = "备份当前档";
            }
        } else if (pct >= 0) {
            snprintf(status_buf, sizeof(status_buf), "正在导入 %d%%", pct);
            status = status_buf;
        } else {
            status = "正在导入";
        }
    } else if (em == ASH_EM_UI_IMPORT_DONE_HINT) {
        status = "写入完成 · 即将复位";
    } else if (em == ASH_EM_UI_IMPORT_CONFIRM) {
        status = "确认覆盖 · 默认取消";
    }

    if (s_list_sel < 0) s_list_sel = 0;
    if (s_list_sel >= SETTINGS_COUNT) s_list_sel = SETTINGS_COUNT - 1;
    int set_view = SETTINGS_VIEW;
    if (SETTINGS_COUNT < set_view) set_view = SETTINGS_COUNT;
    int set_top = list_scroll_top(s_list_sel, SETTINGS_COUNT, set_view);
    for (int slot = 0; slot < ASH_LIST_MAX; slot++) lv_obj_add_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
    for (int slot = 0; slot < set_view; slot++) {
        int i = set_top + slot;
        if (i >= SETTINGS_COUNT) break;
        lv_obj_set_size(s_list_rows[slot], 216, 32);
        lv_obj_set_pos(s_list_rows[slot], 12, 10 + slot * 36);
        lv_obj_set_pos(s_list_labels[slot], 10, 2);
        lv_obj_set_pos(s_list_subs[slot], 10, 16);
        lv_label_set_text(s_list_labels[slot], titles[i]);
        lv_label_set_text(s_list_subs[slot], subs[i]);
        lv_label_set_long_mode(s_list_labels[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_long_mode(s_list_subs[slot], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(s_list_labels[slot], 196);
        lv_obj_set_width(s_list_subs[slot], 196);
        lv_obj_clear_flag(s_list_subs[slot], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_list_rows[slot], LV_OBJ_FLAG_HIDDEN);
        bool sel = (s_list_sel == i);
        lv_obj_set_style_border_color(s_list_rows[slot],
                                      lv_color_hex(sel ? COL_AMBER : COL_LINE), 0);
        lv_obj_set_style_bg_color(s_list_rows[slot],
                                  lv_color_hex(sel ? COL_SEL : COL_SURF), 0);
    }
    paint_hub_scrollbar(s_list_sel, SETTINGS_COUNT, set_view);
    set_modal_visible(false);
    lv_obj_clear_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
    set_bar_dual(false);
    const char *cta = "调节";
    if (em == ASH_EM_UI_BACKUP_BUSY || em == ASH_EM_UI_IMPORT_BUSY) {
        cta = "进行中";
    } else if (s_list_sel == 3) {
        cta = "开始";
    } else if (s_list_sel == 4) {
        cta = "备份";
    } else if (s_list_sel == 5) {
        cta = "导入";
    }
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), cta, true, true);
    s_focus = 0;
    lv_label_set_text(s_hint, status ? status : "确定长按 · 返回");
}

void paint_settings_confirm(void)
{
    show_home_chrome(false);
    lv_label_set_text(s_title, "导入存档");
    for (int i = 0; i < ASH_LIST_MAX; i++) lv_obj_add_flag(s_list_rows[i], LV_OBJ_FLAG_HIDDEN);
    set_modal_visible(true);
    lv_label_set_text(s_modal_lines[0], "确认覆盖当前进度");
    lv_label_set_text(s_modal_lines[1], "会先备份到存档管家");
    lv_label_set_text(s_modal_lines[2], "写入后设备会复位");
    lv_label_set_text(s_modal_lines[3], "");
    lv_label_set_text(s_modal_lines[4], "");
    lv_obj_add_flag(s_modal_btn, LV_OBJ_FLAG_HIDDEN);
    set_bar_dual(true);
    /* 默认焦点在取消，避免误覆盖 */
    if (s_focus != 0 && s_focus != 1) s_focus = 0;
    set_btn(s_btn_pri, lv_obj_get_child(s_btn_pri, 0), "取消", true, s_focus == 0);
    set_btn(s_btn_sec, lv_obj_get_child(s_btn_sec, 0), "覆盖", true, s_focus == 1);
    if (s_hint) {
        lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_hint, "上下选 · 确定");
    }
}
