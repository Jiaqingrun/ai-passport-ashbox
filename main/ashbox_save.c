#include "ashbox_save.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ash_save";

_Static_assert(sizeof(ash_game_t) == ASHBOX_SAVE_BLOB_SIZE,
               "ash_game_t size drifted — update migrators and ASHBOX_SAVE_BLOB_SIZE");

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
} ash_save_v1_head_t;

/* v3 layout: everything before v4 append, then crc */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint32_t crc;
} ash_game_v3_t;

/* v4 layout: through quest2_done, then crc (no stash_lv) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint32_t crc;
} ash_game_v4_t;

/* v5 layout: through reserved_v5, then crc (no Phase C fields) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t reserved_v5[3];
    uint32_t crc;
} ash_game_v5_t;

/* v6 layout: through chore_kind, then crc (no Phase D fields) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t reserved_v5[2];
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint32_t crc;
} ash_game_v6_t;

/* v7 layout: Phase D fields through trade2_extra_day, then crc (no door_bolt) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint32_t crc;
} ash_game_v7_t;

/* v8 layout: through door_bolt, then crc (no water fields) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint8_t door_bolt;
    uint32_t crc;
} ash_game_v8_t;

/* v9 layout: through crew_exhaust_remain_s, then crc (no iron / craft fields) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint8_t door_bolt;
    uint8_t shovel;
    uint8_t well_lv;
    uint8_t pad_v9;
    uint16_t water;
    uint16_t stash_water;
    uint16_t water_made_today;
    int16_t report_water_delta;
    uint32_t well_upgrade_remain_s;
    uint32_t crew_exhaust_remain_s;
    uint32_t crc;
} ash_game_v9_t;

/* v10 layout: through pad_v10b, then crc (no v11 day_latch_* fields) */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint8_t door_bolt;
    uint8_t shovel;
    uint8_t well_lv;
    uint8_t pad_v9;
    uint16_t water;
    uint16_t stash_water;
    uint16_t water_made_today;
    int16_t report_water_delta;
    uint32_t well_upgrade_remain_s;
    uint32_t crew_exhaust_remain_s;
    uint16_t iron;
    uint8_t bucket;
    uint8_t backpack;
    uint8_t crowbar;
    uint8_t pistol;
    uint8_t ammo;
    uint8_t craft_bench;
    uint8_t equip_backpack[ASHBOX_PERSON_CORE];
    uint8_t equip_pistol[ASHBOX_PERSON_CORE];
    uint8_t exp_crowbar;
    uint8_t exp_pistol;
    uint8_t exp_ammo;
    int16_t report_iron_delta;
    uint8_t report_waste_flags;
    uint8_t report_death_kind;
    uint8_t wanderer_pending;
    uint8_t wanderer_name_id;
    uint8_t wanderer_met_trade;
    uint8_t wanderer_rate_kind;
    uint8_t wanderer_ember_ok;
    uint8_t med_stim;
    uint32_t wanderer_next_s;
    uint8_t ember_trade_day;
    uint8_t med_milk;
    uint8_t med_abx;
    uint8_t pad_v10b;
    uint32_t crc;
} ash_game_v10_t;

/* Migration prefixes must stay layout-compatible with live ash_game_t (C-03).
 * Trailing padding after crc is allowed (ESP ABI); crc must still be last field. */
_Static_assert(offsetof(ash_game_t, water) + sizeof(uint16_t) <= offsetof(ash_game_t, crc),
               "ash_game_t.water must precede crc");
_Static_assert(offsetof(ash_game_t, crc) + sizeof(uint32_t) <= sizeof(ash_game_t),
               "ash_game_t.crc must be within the struct");
_Static_assert(offsetof(ash_game_t, stash_lv) == offsetof(ash_game_v4_t, crc),
               "v4->v5 migrate offsetof(stash_lv) mismatch");
_Static_assert(offsetof(ash_game_t, furn_lv) == offsetof(ash_game_v5_t, crc),
               "v5->v6 migrate offsetof(furn_lv) mismatch");
_Static_assert(offsetof(ash_game_t, ember_dry) == offsetof(ash_game_v6_t, crc),
               "v6->v7 migrate offsetof(ember_dry) mismatch");
_Static_assert(offsetof(ash_game_t, door_bolt) == offsetof(ash_game_v7_t, crc),
               "v7->v8 migrate offsetof(door_bolt) mismatch");
_Static_assert(offsetof(ash_game_t, water) == offsetof(ash_game_v8_t, crc),
               "v8->v9 migrate offsetof(water) mismatch");
_Static_assert(offsetof(ash_game_t, iron) == offsetof(ash_game_v9_t, crc),
               "v9->v10 migrate offsetof(iron) mismatch");
_Static_assert(offsetof(ash_game_t, day_latch_trade) == offsetof(ash_game_v10_t, crc),
               "v10->v11 migrate offsetof(day_latch_trade) mismatch");
_Static_assert(sizeof(ash_game_v10_t) == 632u,
               "frozen v10 blob size must stay 632");

/* v11 layout: through pad_v11, then crc (no duty/meta append). */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint8_t door_bolt;
    uint8_t shovel;
    uint8_t well_lv;
    uint8_t pad_v9;
    uint16_t water;
    uint16_t stash_water;
    uint16_t water_made_today;
    int16_t report_water_delta;
    uint32_t well_upgrade_remain_s;
    uint32_t crew_exhaust_remain_s;
    uint16_t iron;
    uint8_t bucket;
    uint8_t backpack;
    uint8_t crowbar;
    uint8_t pistol;
    uint8_t ammo;
    uint8_t craft_bench;
    uint8_t equip_backpack[ASHBOX_PERSON_CORE];
    uint8_t equip_pistol[ASHBOX_PERSON_CORE];
    uint8_t exp_crowbar;
    uint8_t exp_pistol;
    uint8_t exp_ammo;
    int16_t report_iron_delta;
    uint8_t report_waste_flags;
    uint8_t report_death_kind;
    uint8_t wanderer_pending;
    uint8_t wanderer_name_id;
    uint8_t wanderer_met_trade;
    uint8_t wanderer_rate_kind;
    uint8_t wanderer_ember_ok;
    uint8_t med_stim;
    uint32_t wanderer_next_s;
    uint8_t ember_trade_day;
    uint8_t med_milk;
    uint8_t med_abx;
    uint8_t pad_v10b;
    uint16_t day_latch_trade;
    uint16_t day_latch_trade2;
    uint16_t day_latch_trade_extra;
    uint16_t day_latch_trade2_extra;
    uint16_t day_latch_duty;
    uint16_t day_latch_ember;
    uint16_t day_latch_abx;
    uint16_t pad_v11;
    uint32_t crc;
} ash_game_v11_t;

_Static_assert(offsetof(ash_game_t, person_ext) == offsetof(ash_game_v11_t, crc),
               "v11->v12 migrate offsetof(person_ext) mismatch");
_Static_assert(sizeof(ash_game_v11_t) == 648u,
               "frozen v11 blob size must stay 648");

/* v12 layout: through pad_v12, then crc (no pace/wanderer-trait append). */
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t food;
    uint16_t parts;
    uint16_t ember;
    uint8_t dorm_lv;
    uint8_t kitchen_lv;
    uint8_t workshop_lv;
    uint8_t gate_unlocked;
    ash_person_t person[ASHBOX_PERSON_CORE];
    uint8_t pop;
    ash_expedition_t exp;
    uint8_t threat;
    uint8_t short_probe_used_today;
    uint32_t day_key;
    uint32_t expedition_total;
    int64_t last_settle_unix;
    ash_report_t report;
    ash_crisis_t crisis;
    uint8_t quest_step[2];
    uint8_t quest_done[2];
    uint8_t threat_rise_num;
    uint8_t deep_loot_bonus;
    uint8_t hide_penalty_h;
    char journal[ASHBOX_JOURNAL_N][ASHBOX_JOURNAL_LEN];
    uint8_t journal_head;
    uint8_t journal_count;
    uint8_t night_order;
    uint8_t last_tag_id;
    uint8_t world_flags;
    uint8_t item[ASHBOX_ITEM_N];
    uint16_t stash_food;
    uint16_t stash_parts;
    ash_farm_plot_t farm[ASHBOX_FARM_PLOTS];
    uint8_t trade_day;
    uint8_t lore_shards;
    uint8_t quest2_step;
    uint8_t quest2_done;
    uint8_t stash_lv;
    uint8_t chore_done_exp;
    uint8_t duty_day;
    uint8_t duty_count;
    uint8_t furn_lv;
    uint8_t seed_root;
    uint8_t trade2_day;
    uint8_t chore_kind;
    uint8_t ember_dry;
    uint8_t gear_lv;
    uint8_t rep_shed;
    uint8_t rep_ferry;
    uint8_t quiet_gear;
    uint8_t lore_twist;
    uint8_t trade_extra_day;
    uint8_t trade2_extra_day;
    uint8_t door_bolt;
    uint8_t shovel;
    uint8_t well_lv;
    uint8_t pad_v9;
    uint16_t water;
    uint16_t stash_water;
    uint16_t water_made_today;
    int16_t report_water_delta;
    uint32_t well_upgrade_remain_s;
    uint32_t crew_exhaust_remain_s;
    uint16_t iron;
    uint8_t bucket;
    uint8_t backpack;
    uint8_t crowbar;
    uint8_t pistol;
    uint8_t ammo;
    uint8_t craft_bench;
    uint8_t equip_backpack[ASHBOX_PERSON_CORE];
    uint8_t equip_pistol[ASHBOX_PERSON_CORE];
    uint8_t exp_crowbar;
    uint8_t exp_pistol;
    uint8_t exp_ammo;
    int16_t report_iron_delta;
    uint8_t report_waste_flags;
    uint8_t report_death_kind;
    uint8_t wanderer_pending;
    uint8_t wanderer_name_id;
    uint8_t wanderer_met_trade;
    uint8_t wanderer_rate_kind;
    uint8_t wanderer_ember_ok;
    uint8_t med_stim;
    uint32_t wanderer_next_s;
    uint8_t ember_trade_day;
    uint8_t med_milk;
    uint8_t med_abx;
    uint8_t pad_v10b;
    uint16_t day_latch_trade;
    uint16_t day_latch_trade2;
    uint16_t day_latch_trade_extra;
    uint16_t day_latch_trade2_extra;
    uint16_t day_latch_duty;
    uint16_t day_latch_ember;
    uint16_t day_latch_abx;
    uint16_t pad_v11;
    ash_person_t person_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    uint8_t equip_backpack_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    uint8_t equip_pistol_ext[ASHBOX_PERSON_MAX - ASHBOX_PERSON_CORE];
    ash_farm_plot_t farm_ext[ASHBOX_FARM_PLOTS_MAX - ASHBOX_FARM_PLOTS];
    uint8_t person_job[ASHBOX_PERSON_MAX];
    uint8_t run_cleared;
    uint8_t peace_broken;
    uint8_t wipeout_reject_count;
    uint8_t game_over;
    uint8_t build_cap_bonus;
    uint8_t loot_bonus_perm_pct;
    uint8_t super_core;
    uint8_t farm_plots_unlocked;
    uint8_t clear_toast_pending;
    uint8_t joined_after_clear;
    uint16_t achieve_toast_mask;
    uint16_t stash_food_bonus;
    uint8_t end_toast_pending;
    uint8_t pad_v12[3];
    uint32_t crc;
} ash_game_v12_t;

_Static_assert(offsetof(ash_game_t, pace_era) == offsetof(ash_game_v12_t, crc),
               "v12->v13 migrate offsetof(pace_era) mismatch");
_Static_assert(sizeof(ash_game_v12_t) == 744u,
               "frozen v12 blob size must stay 744");
_Static_assert(sizeof(((ash_game_t *)0)->pad_v13) == 6u,
               "pad_v13 must seal alignment before crc");
_Static_assert(offsetof(ash_game_t, crc) == offsetof(ash_game_t, pad_v13) + 6u,
               "no anonymous padding allowed between pad_v13 and crc");
_Static_assert(offsetof(ash_game_t, crc) + sizeof(uint32_t) == sizeof(ash_game_t),
               "crc must be the final field with no trailing padding");
_Static_assert(sizeof(ash_game_t) % 8u == 0u,
               "ash_game_t size must stay 8-byte aligned");

#define ASHBOX_MAGIC_V1 0x48534131u /* ASH1 */

static uint32_t crc32_bytes_local(const uint8_t *data, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~c;
}

/** Pre-seal CRC: full struct copy with crc zeroed (may include dirty pad bytes). */
static uint32_t ash_game_crc_legacy_raw(const ash_game_t *g)
{
    static ash_game_t s_legacy_tmp;
    s_legacy_tmp = *g;
    s_legacy_tmp.crc = 0;
    return crc32_bytes_local((const uint8_t *)&s_legacy_tmp, sizeof(s_legacy_tmp));
}

static bool ash_game_crc_ok(const ash_game_t *g)
{
    uint32_t expect = g->crc;
    return ash_game_crc(g) == expect || ash_game_crc_legacy_raw(g) == expect;
}

static void ash_game_seal_for_store(ash_game_t *g)
{
    g->magic = ASHBOX_MAGIC;
    g->ver = ASHBOX_SAVE_VER;
    memset(g->pad_v12, 0, sizeof(g->pad_v12));
    memset(g->pad_v13, 0, sizeof(g->pad_v13));
    g->pad_v13a = 0;
    g->crc = ash_game_crc(g);
}

/* Scan a raw NVS partition dump for a CRC-valid ASH2 blob (largest/current ver). */
static bool rescue_scan_save(const uint8_t *dump, size_t dump_len, ash_game_t *out)
{
    /* Static scratch: two ash_game_t on ~4KiB main stack overflows (boot panic). */
    static ash_game_t s_best;
    static ash_game_t s_cand;
    bool found = false;
    memset(&s_best, 0, sizeof(s_best));
    for (size_t off = 0; off + 8u <= dump_len; off++) {
        uint32_t magic = 0;
        memcpy(&magic, dump + off, sizeof(magic));
        if (magic != ASHBOX_MAGIC) continue;
        uint16_t ver = 0;
        memcpy(&ver, dump + off + 4, sizeof(ver));
        size_t try_len = 0;
        if (ver == ASHBOX_SAVE_VER) try_len = sizeof(ash_game_t);
        else if (ver == 12) try_len = sizeof(ash_game_v12_t);
        else if (ver == 11) try_len = sizeof(ash_game_v11_t);
        else if (ver == 10) try_len = sizeof(ash_game_v10_t);
        else continue;
        if (off + try_len > dump_len) continue;
        if (ashbox_save_decode(dump + off, try_len, &s_cand) != ESP_OK) continue;
        if (!found || s_cand.ver > s_best.ver ||
            (s_cand.ver == s_best.ver && s_cand.day_key >= s_best.day_key)) {
            s_best = s_cand;
            found = true;
        }
    }
    if (found && out) *out = s_best;
    return found;
}

bool ashbox_save_scan_dump(const uint8_t *dump, size_t dump_len, ash_game_t *out)
{
    return rescue_scan_save(dump, dump_len, out);
}

static bool rescue_scan_meta(const uint8_t *dump, size_t dump_len, uint8_t *out, size_t out_len)
{
    if (!out || out_len < 32u) return false;
    for (size_t off = 0; off + 32u <= dump_len; off++) {
        uint32_t magic = 0;
        memcpy(&magic, dump + off, sizeof(magic));
        if (magic != 0x4D485341u) continue; /* ASHM */
        uint16_t ver = 0;
        memcpy(&ver, dump + off + 4, sizeof(ver));
        if (ver != 1) continue;
        uint8_t blob[32];
        memcpy(blob, dump + off, 32);
        uint32_t expect = 0;
        memcpy(&expect, blob + 28, 4);
        memset(blob + 28, 0, 4);
        uint32_t got = crc32_bytes_local(blob, 32);
        if (got != expect) continue;
        memcpy(blob + 28, &expect, 4);
        memcpy(out, blob, 32);
        return true;
    }
    return false;
}

static esp_err_t ashbox_nvs_reinit_preserving(esp_err_t why)
{
    const char *why_name = esp_err_to_name(why);
    (void)why_name;
    ESP_LOGE(TAG, "NVS erase required (%s) — attempting save/meta rescue", why_name);

    static ash_game_t s_rescued_game;
    bool have_game = false;
    uint8_t rescued_meta[32];
    bool have_meta = false;

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    if (part && part->size > 0 && part->size <= 0x20000u) {
        uint8_t *dump = (uint8_t *)malloc(part->size);
        if (dump && esp_partition_read(part, 0, dump, part->size) == ESP_OK) {
            have_game = rescue_scan_save(dump, part->size, &s_rescued_game);
            have_meta = rescue_scan_meta(dump, part->size, rescued_meta, sizeof(rescued_meta));
            ESP_LOGW(TAG, "NVS dump rescue: save=%d meta=%d", (int)have_game, (int)have_meta);
        } else {
            ESP_LOGE(TAG, "NVS dump read failed — progress may be lost");
        }
        free(dump);
    } else {
        ESP_LOGE(TAG, "NVS partition unavailable for rescue");
    }

    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;

    if (have_game) {
        esp_err_t st = ashbox_save_store_mirrored(&s_rescued_game);
        const char *st_name = esp_err_to_name(st);
        (void)st_name;
        ESP_LOGW(TAG, "restored rescued save: %s", st_name);
    }
    if (have_meta) {
        nvs_handle_t h;
        if (nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
            esp_err_t st = nvs_set_blob(h, "meta", rescued_meta, sizeof(rescued_meta));
            if (st == ESP_OK) st = nvs_commit(h);
            nvs_close(h);
            const char *st_name = esp_err_to_name(st);
            (void)st_name;
            ESP_LOGW(TAG, "restored rescued meta: %s", st_name);
        }
    }
    if (!have_game) {
        ESP_LOGE(TAG, "no rescueable save found — next boot may start a new run");
    }
    return ESP_OK;
}

esp_err_t ashbox_save_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = ashbox_nvs_reinit_preserving(err);
    }
    ESP_LOGI(TAG, "ash_game_t size=%u (expect %u)", (unsigned)sizeof(ash_game_t),
             (unsigned)ASHBOX_SAVE_BLOB_SIZE);
    return err;
}

bool ashbox_save_nvs_readable(void)
{
    nvs_handle_t h;
    if (nvs_open(ASHBOX_NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = 0;
    esp_err_t err = nvs_get_blob(h, ASHBOX_NVS_SAVE_KEY, NULL, &len);
    nvs_close(h);
    return err == ESP_OK && len >= sizeof(ash_save_v1_head_t);
}

static void migrate_from_v1_head(ash_game_t *g, const ash_save_v1_head_t *h)
{
    ash_game_new(g);
    g->food = h->food;
    g->parts = h->parts;
    g->ember = h->ember;
    if (h->dorm_lv >= 1 && h->dorm_lv <= 3) g->dorm_lv = h->dorm_lv;
    if (h->kitchen_lv >= 1 && h->kitchen_lv <= 3) g->kitchen_lv = h->kitchen_lv;
    if (h->workshop_lv >= 1 && h->workshop_lv <= 3) g->workshop_lv = h->workshop_lv;
    g->gate_unlocked = h->gate_unlocked ? 1 : 0;
    if (g->kitchen_lv >= 2) {
        g->pop = 2;
        g->person[1].id = 1;
        g->person[1].state = ASH_PERSON_HEALTHY;
        g->world_flags |= (uint8_t)ASH_WORLD_FARM;
    }
    if (g->workshop_lv >= 2) {
        g->pop = 3;
        g->person[2].id = 2;
        g->person[2].state = ASH_PERSON_HEALTHY;
    }
    ash_game_log(g, "旧档已迁移 · 深度内容重置");
    ash_game_after_migrate(g);
}

static void migrate_from_v3(ash_game_t *g, const ash_game_v3_t *v3)
{
    memset(g, 0, sizeof(*g));
    g->magic = v3->magic;
    g->ver = ASHBOX_SAVE_VER;
    g->food = v3->food;
    g->parts = v3->parts;
    g->ember = v3->ember;
    g->dorm_lv = v3->dorm_lv;
    g->kitchen_lv = v3->kitchen_lv;
    g->workshop_lv = v3->workshop_lv;
    g->gate_unlocked = v3->gate_unlocked;
    memcpy(g->person, v3->person, sizeof(g->person));
    g->pop = v3->pop;
    g->exp = v3->exp;
    /* 旧档若卡在已废除的档位枚举外，收回 */
    if ((unsigned)g->exp.tier >= ASH_TIER_COUNT) {
        g->exp.active = false;
        g->exp.tier = ASH_TIER_SHORT;
    }
    g->threat = v3->threat;
    g->short_probe_used_today = v3->short_probe_used_today;
    g->day_key = v3->day_key;
    g->expedition_total = v3->expedition_total;
    g->last_settle_unix = v3->last_settle_unix;
    g->report = v3->report;
    g->crisis = v3->crisis;
    memcpy(g->quest_step, v3->quest_step, sizeof(g->quest_step));
    memcpy(g->quest_done, v3->quest_done, sizeof(g->quest_done));
    g->threat_rise_num = v3->threat_rise_num;
    g->deep_loot_bonus = v3->deep_loot_bonus;
    g->hide_penalty_h = v3->hide_penalty_h;
    memcpy(g->journal, v3->journal, sizeof(g->journal));
    g->journal_head = v3->journal_head;
    g->journal_count = v3->journal_count;
    g->night_order = v3->night_order;
    g->last_tag_id = v3->last_tag_id;
    g->trade_day = 0xFF;
    g->trade2_day = 0xFF;
    g->stash_lv = 1;
    g->furn_lv = 0;
    g->seed_root = 0;
    ash_game_log(g, "存档升至正式骨架");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v4(ash_game_t *g, const ash_game_v4_t *v4)
{
    memset(g, 0, sizeof(*g));
    /* v4 前缀与现行结构一致直至 quest2_done */
    memcpy(g, v4, offsetof(ash_game_t, stash_lv));
    g->ver = ASHBOX_SAVE_VER;
    g->stash_lv = 1;
    g->trade2_day = 0xFF;
    g->furn_lv = 0;
    g->seed_root = 0;
    ash_game_log(g, "仓库改为可独立扩容");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v5(ash_game_t *g, const ash_game_v5_t *v5)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v5, offsetof(ash_game_t, furn_lv));
    g->ver = ASHBOX_SAVE_VER;
    g->furn_lv = 0;
    g->seed_root = 0;
    g->trade2_day = 0xFF;
    g->chore_kind = ASHBOX_CHORE_NONE;
    g->chore_done_exp = 0;
    g->trade_extra_day = 0xFF;
    g->trade2_extra_day = 0xFF;
    ash_game_log(g, "可装货架 · 渡口种更饱薯");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v6(ash_game_t *g, const ash_game_v6_t *v6)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v6, offsetof(ash_game_t, ember_dry));
    g->ver = ASHBOX_SAVE_VER;
    g->ember_dry = 0;
    g->gear_lv = 0;
    g->rep_shed = 0;
    g->rep_ferry = 0;
    g->quiet_gear = 0;
    g->lore_twist = 0;
    g->trade_extra_day = 0xFF;
    g->trade2_extra_day = 0xFF;
    g->door_bolt = 0;
    ash_game_log(g, "猎装与声望已就绪");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v7(ash_game_t *g, const ash_game_v7_t *v7)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v7, offsetof(ash_game_t, door_bolt));
    g->ver = ASHBOX_SAVE_VER;
    g->door_bolt = 0;
    ash_game_log(g, "可装门扣 · 威胁更慢修门更省");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v8(ash_game_t *g, const ash_game_v8_t *v8)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v8, offsetof(ash_game_t, water));
    g->ver = ASHBOX_SAVE_VER;
    g->water = 40;
    g->stash_water = 0;
    g->shovel = 0;
    g->well_lv = 0;
    g->water_made_today = 0;
    g->report_water_delta = 0;
    g->well_upgrade_remain_s = 0;
    g->crew_exhaust_remain_s = 0;
    g->ember_trade_day = 0xFF;
    ash_game_log(g, "据点开始存水 · 先护口渴");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v9(ash_game_t *g, const ash_game_v9_t *v9)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v9, offsetof(ash_game_t, iron));
    g->ver = ASHBOX_SAVE_VER;
    g->iron = 0;
    g->bucket = 0;
    g->backpack = 0;
    g->crowbar = 0;
    g->pistol = 0;
    g->ammo = 0;
    g->craft_bench = 0;
    memset(g->equip_backpack, 0, sizeof(g->equip_backpack));
    memset(g->equip_pistol, 0, sizeof(g->equip_pistol));
    g->exp_crowbar = 0;
    g->exp_pistol = 0;
    g->exp_ammo = 0;
    g->report_iron_delta = 0;
    g->report_waste_flags = 0;
    g->report_death_kind = ASH_DEATH_NONE;
    g->wanderer_pending = 0;
    g->wanderer_name_id = 0;
    g->wanderer_met_trade = 0;
    g->wanderer_rate_kind = ASH_WANDER_RATE_FAIR;
    g->wanderer_ember_ok = 0;
    g->wanderer_next_s = 0;
    g->ember_trade_day = 0xFF;
    ash_game_log(g, "铁与仓位就绪 · 可等过路客");
    ash_game_widen_day_latches(g);
    ash_game_after_migrate(g);
}

static void migrate_from_v10(ash_game_t *g, const ash_game_v10_t *v10)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v10, offsetof(ash_game_t, day_latch_trade));
    g->ver = ASHBOX_SAVE_VER;
    ash_game_widen_day_latches(g);
    ash_game_log(g, "日限改按完整天数记");
    ash_game_after_migrate(g);
}

static void migrate_from_v11(ash_game_t *g, const ash_game_v11_t *v11)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v11, offsetof(ash_game_t, person_ext));
    g->ver = ASHBOX_SAVE_VER;
    g->farm_plots_unlocked = ASHBOX_FARM_PLOTS;
    memset(g->person_job, 0, sizeof(g->person_job));
    g->pace_era = ASHBOX_PACE_LEGACY;
    for (int i = 0; i < ASHBOX_QUEST_N; i++) g->quest_beat_day[i] = ASHBOX_DAY_LATCH_NEVER;
    ash_game_log(g, "可记成就 · 宿舍能再扩床");
    ash_game_after_migrate(g);
}

static void migrate_from_v12(ash_game_t *g, const ash_game_v12_t *v12)
{
    memset(g, 0, sizeof(*g));
    memcpy(g, v12, offsetof(ash_game_t, pace_era));
    g->ver = ASHBOX_SAVE_VER;
    g->pace_era = ASHBOX_PACE_LEGACY;
    g->wanderer_trait = ASHBOX_TRAIT_NONE;
    g->wanderer_fired = 0;
    g->pad_v13a = 0;
    for (int i = 0; i < ASHBOX_QUEST_N; i++) g->quest_beat_day[i] = ASHBOX_DAY_LATCH_NEVER;
    g->wanderer_sched_day = 0;
    g->wanderer_t0_min = 0;
    g->wanderer_t1_min = 0;
    memset(g->person_trait, 0, sizeof(g->person_trait));
    memset(g->farm_labor_h, 0, sizeof(g->farm_labor_h));
    memset(g->pad_v13, 0, sizeof(g->pad_v13));
    ash_game_log(g, "旧节奏保留 · 菜地余烬照旧");
    ash_game_after_migrate(g);
}

size_t ashbox_save_v3_bytes(void)
{
    return sizeof(ash_game_v3_t);
}

size_t ashbox_save_v8_bytes(void)
{
    return sizeof(ash_game_v8_t);
}

size_t ashbox_save_v9_bytes(void)
{
    return sizeof(ash_game_v9_t);
}

size_t ashbox_save_v10_bytes(void)
{
    return sizeof(ash_game_v10_t);
}

size_t ashbox_save_v11_bytes(void)
{
    return sizeof(ash_game_v11_t);
}

esp_err_t ashbox_save_decode(const uint8_t *raw, size_t len, ash_game_t *g)
{
    if (!raw || !g) return ESP_ERR_INVALID_ARG;
    if (len < sizeof(ash_save_v1_head_t)) return ESP_ERR_INVALID_CRC;

    uint32_t magic = 0;
    uint16_t ver = 0;
    memcpy(&magic, raw, sizeof(magic));
    memcpy(&ver, raw + sizeof(magic), sizeof(ver));

    if (magic == ASHBOX_MAGIC && ver == ASHBOX_SAVE_VER && len == sizeof(ash_game_t)) {
        memcpy(g, raw, sizeof(*g));
        if (!ash_game_crc_ok(g)) return ESP_ERR_INVALID_CRC;
        /* Re-seal pads so subsequent stores use the stable CRC. */
        memset(g->pad_v12, 0, sizeof(g->pad_v12));
        memset(g->pad_v13, 0, sizeof(g->pad_v13));
        g->pad_v13a = 0;
        g->crc = ash_game_crc(g);
        ash_game_after_migrate(g);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 12 && len == sizeof(ash_game_v12_t)) {
        /* One v12 blob only — dual copies blew the ~4KiB main task stack. */
        static ash_game_v12_t s_v12;
        memcpy(&s_v12, raw, sizeof(s_v12));
        uint32_t expect = s_v12.crc;
        s_v12.crc = 0;
        memset(s_v12.pad_v12, 0, sizeof(s_v12.pad_v12));
        uint32_t got = crc32_bytes_local((const uint8_t *)&s_v12, sizeof(s_v12));
        if (expect != got) {
            /* Pre-seal raw CRC (dirty pad bytes included, crc zeroed). */
            memcpy(&s_v12, raw, sizeof(s_v12));
            s_v12.crc = 0;
            got = crc32_bytes_local((const uint8_t *)&s_v12, sizeof(s_v12));
            if (expect != got) return ESP_ERR_INVALID_CRC;
        }
        s_v12.crc = expect;
        migrate_from_v12(g, &s_v12);
        ESP_LOGW(TAG, "migrated v12 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 11 && len == sizeof(ash_game_v11_t)) {
        ash_game_v11_t v11;
        memcpy(&v11, raw, sizeof(v11));
        uint32_t expect = v11.crc;
        v11.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v11, sizeof(v11));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v11.crc = expect;
        migrate_from_v11(g, &v11);
        ESP_LOGW(TAG, "migrated v11 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 10 && len == sizeof(ash_game_v10_t)) {
        ash_game_v10_t v10;
        memcpy(&v10, raw, sizeof(v10));
        uint32_t expect = v10.crc;
        v10.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v10, sizeof(v10));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v10.crc = expect;
        migrate_from_v10(g, &v10);
        ESP_LOGW(TAG, "migrated v10 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 9 && len == sizeof(ash_game_v9_t)) {
        ash_game_v9_t v9;
        memcpy(&v9, raw, sizeof(v9));
        uint32_t expect = v9.crc;
        v9.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v9, sizeof(v9));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v9.crc = expect;
        migrate_from_v9(g, &v9);
        ESP_LOGW(TAG, "migrated v9 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 8 && len == sizeof(ash_game_v8_t)) {
        ash_game_v8_t v8;
        memcpy(&v8, raw, sizeof(v8));
        uint32_t expect = v8.crc;
        v8.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v8, sizeof(v8));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v8.crc = expect;
        migrate_from_v8(g, &v8);
        ESP_LOGW(TAG, "migrated v8 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 7 && len == sizeof(ash_game_v7_t)) {
        ash_game_v7_t v7;
        memcpy(&v7, raw, sizeof(v7));
        uint32_t expect = v7.crc;
        v7.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v7, sizeof(v7));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v7.crc = expect;
        migrate_from_v7(g, &v7);
        ESP_LOGW(TAG, "migrated v7 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 6 && len == sizeof(ash_game_v6_t)) {
        ash_game_v6_t v6;
        memcpy(&v6, raw, sizeof(v6));
        uint32_t expect = v6.crc;
        v6.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v6, sizeof(v6));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v6.crc = expect;
        migrate_from_v6(g, &v6);
        ESP_LOGW(TAG, "migrated v6 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 5 && len == sizeof(ash_game_v5_t)) {
        ash_game_v5_t v5;
        memcpy(&v5, raw, sizeof(v5));
        uint32_t expect = v5.crc;
        v5.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v5, sizeof(v5));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v5.crc = expect;
        migrate_from_v5(g, &v5);
        ESP_LOGW(TAG, "migrated v5 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 4 && len == sizeof(ash_game_v4_t)) {
        ash_game_v4_t v4;
        memcpy(&v4, raw, sizeof(v4));
        uint32_t expect = v4.crc;
        v4.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v4, sizeof(v4));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v4.crc = expect;
        migrate_from_v4(g, &v4);
        ESP_LOGW(TAG, "migrated v4 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    if (magic == ASHBOX_MAGIC && ver == 3 && len == sizeof(ash_game_v3_t)) {
        ash_game_v3_t v3;
        memcpy(&v3, raw, sizeof(v3));
        uint32_t expect = v3.crc;
        v3.crc = 0;
        uint32_t got = crc32_bytes_local((const uint8_t *)&v3, sizeof(v3));
        if (expect != got) return ESP_ERR_INVALID_CRC;
        v3.crc = expect;
        migrate_from_v3(g, &v3);
        ESP_LOGW(TAG, "migrated v3 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    /* Legacy v2: only accept a full v3-sized blob with a matching CRC.
     * B18: never fall back to head-only (silent pop/expedition/journal loss). */
    if (magic == ASHBOX_MAGIC && ver == 2) {
        if (len == sizeof(ash_game_v3_t)) {
            ash_game_v3_t v3;
            memcpy(&v3, raw, sizeof(v3));
            uint32_t expect = v3.crc;
            v3.crc = 0;
            uint32_t got = crc32_bytes_local((const uint8_t *)&v3, sizeof(v3));
            if (expect != got) return ESP_ERR_INVALID_CRC;
            v3.crc = expect;
            migrate_from_v3(g, &v3);
            ESP_LOGW(TAG, "migrated v2(as-v3-layout) save to v%d", ASHBOX_SAVE_VER);
            return ESP_OK;
        }
        return ESP_ERR_INVALID_CRC;
    }

    if (magic == ASHBOX_MAGIC_V1 && ver == 1) {
        ash_save_v1_head_t head;
        memcpy(&head, raw, sizeof(head));
        migrate_from_v1_head(g, &head);
        ESP_LOGW(TAG, "migrated v1 save to v%d", ASHBOX_SAVE_VER);
        return ESP_OK;
    }

    return ESP_ERR_INVALID_CRC;
}

static esp_err_t load_key(nvs_handle_t h, const char *key, ash_game_t *g)
{
    static uint8_t s_raw[sizeof(ash_game_t) + 64];
    size_t len = sizeof(s_raw);
    esp_err_t err = nvs_get_blob(h, key, s_raw, &len);
    if (err != ESP_OK) return err;
    return ashbox_save_decode(s_raw, len, g);
}

esp_err_t ashbox_save_load(ash_game_t *g)
{
    if (!g) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open for load failed: %s", esp_err_to_name(err));
        return err;
    }

    err = load_key(h, ASHBOX_NVS_SAVE_KEY, g);
    if (err == ESP_OK) {
        nvs_close(h);
        ESP_LOGI(TAG, "loaded primary food=%u pop=%u day=%u", (unsigned)g->food,
                 (unsigned)g->pop, (unsigned)g->day_key);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "primary save unreadable (%s) — trying backup", esp_err_to_name(err));

    esp_err_t bak = load_key(h, ASHBOX_NVS_SAVE_BAK_KEY, g);
    nvs_close(h);
    if (bak != ESP_OK) {
        ESP_LOGE(TAG, "backup save unreadable (%s) — progress not loaded", esp_err_to_name(bak));
        return bak;
    }

    ESP_LOGW(TAG, "recovered from backup food=%u pop=%u day=%u — healing primary",
             (unsigned)g->food, (unsigned)g->pop, (unsigned)g->day_key);
    (void)ashbox_save_store_mirrored(g);
    return ESP_OK;
}

static esp_err_t store_blob(const ash_game_t *g, bool mirror_backup)
{
    if (!g) return ESP_ERR_INVALID_ARG;
    static ash_game_t s_store_tmp;
    s_store_tmp = *g;
    ash_game_seal_for_store(&s_store_tmp);

    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open for store failed: %s", esp_err_to_name(err));
        return err;
    }

    if (mirror_backup) {
        /* Backup first: a crash during primary write still leaves sv2 intact. */
        esp_err_t bak = nvs_set_blob(h, ASHBOX_NVS_SAVE_BAK_KEY, &s_store_tmp, sizeof(s_store_tmp));
        if (bak == ESP_OK) bak = nvs_commit(h);
        if (bak != ESP_OK) {
            ESP_LOGW(TAG, "backup slot store failed: %s", esp_err_to_name(bak));
        }
    }

    err = nvs_set_blob(h, ASHBOX_NVS_SAVE_KEY, &s_store_tmp, sizeof(s_store_tmp));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "primary store failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t ashbox_save_store(const ash_game_t *g)
{
    return store_blob(g, false);
}

esp_err_t ashbox_save_store_mirrored(const ash_game_t *g)
{
    return store_blob(g, true);
}
