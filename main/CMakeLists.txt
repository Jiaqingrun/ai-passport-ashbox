idf_component_register(
    SRCS "main.c"
         "ashbox_game.c"
         "ashbox_game_loot.c"
         "ashbox_content.c"
         "ashbox_save.c"
         "ashbox_meta.c"
         "ashbox_settings.c"
         "ashbox_sfx.c"
         "ashbox_sleep.c"
         "ashbox_runtime.c"
         "ashbox_ui.c"
         "ashbox_ui_help.c"
         "ashbox_ui_trade.c"
         "ashbox_ui_stash.c"
         "ashbox_ui_equip.c"
         "ashbox_ui_pick.c"
         "ashbox_ui_build.c"
         "ashbox_ui_survivors.c"
         "ashbox_ui_settings.c"
         "ashbox_ui_farm.c"
         "ashbox_help_copy.c"
         "ashbox_sprites.c"
         "ashbox_embackup_proto.c"
         "ashbox_embackup.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash esp_timer esp_hw_support app_update driver esp_partition
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/ashbox_font_16.c"
)

# 广播事件：真人录音 PCM（mono s16le @16kHz），见 assets/ashbox/sfx/SOURCES.txt
set(_ASH_SFX_DIR "${CMAKE_CURRENT_LIST_DIR}/../assets/ashbox/sfx")
foreach(_pcm radio_scream_f radio_scream_m radio_help radio_search radio_laugh hearth_fire)
    target_add_binary_data(${COMPONENT_LIB} "${_ASH_SFX_DIR}/${_pcm}.pcm" BINARY)
endforeach()

# 场景精灵 indexed+RLE（ASR1）；由 tools/ashbox_sprites_encode.py 生成
set(_ASH_SPR_DIR "${CMAKE_CURRENT_LIST_DIR}/../assets/ashbox/sprites_rle")
set(_ASH_SPR_SCENES idle away weak threat return night gate duo farm stash trade watch)
foreach(_sc ${_ASH_SPR_SCENES})
    foreach(_fi 0 1 2)
        target_add_binary_data(${COMPONENT_LIB} "${_ASH_SPR_DIR}/${_sc}_f${_fi}.bin" BINARY)
    endforeach()
endforeach()
