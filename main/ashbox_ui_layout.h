#pragma once

/*
 * LAYOUT_LOCK geometry — authoritative numeric contract for the home stack.
 * Details: docs/ashbox/LAYOUT_LOCK.md. Do not change without an explicit redesign.
 */

#define ASHBOX_UI_SAFE_TOP 2
#define ASHBOX_UI_HDR_H 18
#define ASHBOX_UI_STAGE_H 176
#define ASHBOX_UI_NOW_BAND_H 52
#define ASHBOX_UI_LIST_LAYER_H (ASHBOX_UI_STAGE_H + ASHBOX_UI_NOW_BAND_H) /* 228 */
#define ASHBOX_UI_CONTENT_Y (ASHBOX_UI_SAFE_TOP + ASHBOX_UI_HDR_H)        /* 20 */
