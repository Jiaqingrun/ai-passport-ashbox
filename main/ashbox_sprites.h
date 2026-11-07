#pragma once
#include "lvgl.h"

#define ASHBOX_SPRITE_FRAMES 3

typedef enum {
    ASHBOX_SCENE_IDLE = 0,
    ASHBOX_SCENE_AWAY,
    ASHBOX_SCENE_WEAK,
    ASHBOX_SCENE_THREAT,
    ASHBOX_SCENE_RETURN,
    ASHBOX_SCENE_NIGHT,
    ASHBOX_SCENE_GATE,
    ASHBOX_SCENE_DUO,
    /* Phase G+：状态自动聚焦（菜地/仓/交易/守望） */
    ASHBOX_SCENE_FARM,
    ASHBOX_SCENE_STASH,
    ASHBOX_SCENE_TRADE,
    ASHBOX_SCENE_WATCH,
    ASHBOX_SCENE_COUNT
} ashbox_scene_t;

const lv_image_dsc_t *ashbox_sprite_get(ashbox_scene_t scene);
const lv_image_dsc_t *ashbox_sprite_get_frame(ashbox_scene_t scene, int frame);
