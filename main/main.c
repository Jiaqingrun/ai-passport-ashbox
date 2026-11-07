#include "ashbox_embackup.h"
#include "ashbox_game.h"
#include "ashbox_runtime.h"
#include "ashbox_save.h"
#include "ashbox_settings.h"
#include "ashbox_sfx.h"
#include "ashbox_sleep.h"
#include "ashbox_ui.h"
#include "key_guard.h"

#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <sys/time.h>
#include <time.h>

static const char *TAG = "ashbox";

#define INPUT_QUEUE_DEPTH 16
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)
#define DT_MAX_S (36LL * 3600)
#define ASH_SLEEP_RTC_MAGIC 0xA54B51EEu

/* Survives deep sleep (not power cut). Used when wall RTC rewound vs save timeline. */
static RTC_DATA_ATTR uint32_t s_sleep_rtc_magic;
static RTC_DATA_ATTR int64_t s_sleep_rtc_enter;

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static ash_game_t s_game;
static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;
static bool s_session_trusted;
static int64_t s_boot_unix;

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (s_key_guard.armed) {
        /* Any key action ends the post-wake quiet UI immediately; keep
           swallowing until release/timeout so the held wake key cannot
           fire a LONG→sleep or other unintended action. */
        if (ashbox_ui_wake_guard_active()) {
            ESP_LOGI(TAG, "wake quiet ended by input");
            /* Callback context: never take LVGL here (C-02). */
            ashbox_ui_request_clear_wake_guard();
        }
        if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
            return;
        }
    }
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = {.btn = btn, .event = ev};
    (void)xQueueSend(s_input_queue, &input, 0);
}

static void input_queue_drain(void)
{
    input_event_t dump;
    while (s_input_queue && xQueueReceive(s_input_queue, &dump, 0) == pdTRUE) {
    }
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            ashbox_ui_on_key(input.btn, input.event);
            if (ashbox_ui_take_input_drain_request()) {
                input_queue_drain();
            }
        }
    }
}

static esp_err_t input_dispatch_init(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    /* Above app_main loop so key paints are not stuck behind settle/SPI work. */
    if (xTaskCreate(input_task, "ash_input", 4096, NULL, 6, &s_input_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static int64_t wall_unix_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec;
}

static void boot_settle(uint32_t wake_causes)
{
    ash_clock_info_t clk = {0};
    int64_t now = wall_unix_now();
    s_boot_unix = now;

    bool deep = (wake_causes & (1u << ESP_SLEEP_WAKEUP_TIMER)) != 0 ||
                (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) != 0;
    /* Deep-sleep wake: prefer wall delta when RTC still tracks the save timeline;
     * otherwise use the RTC-domain stamp taken at sleep entry (power-cut safe for
     * the sleep itself; full power-cut clears RTC_DATA and skips catch-up). */
    if (deep) {
        int64_t dt = -1;
        if (ash_game_wall_unix_usable(&s_game, now) && s_game.last_settle_unix > 0) {
            dt = now - s_game.last_settle_unix;
        } else if (s_sleep_rtc_magic == ASH_SLEEP_RTC_MAGIC) {
            dt = now - s_sleep_rtc_enter;
        }
        if (dt >= 0) {
            if (dt > DT_MAX_S) dt = DT_MAX_S;
            if (dt > 0) {
                clk.trusted = true;
                clk.in_deep_sleep_segment = true;
                clk.dt_s = dt;
                s_session_trusted = ash_game_wall_unix_usable(&s_game, now);
            }
        }
    }
    s_sleep_rtc_magic = 0;

    if (!clk.trusted) {
        /* Power-on / RTC rewind: do not economy-advance, and never smash a good
         * save timeline with epoch RTC (that reset day_key and 日勤). */
        clk.trusted = false;
        clk.dt_s = 0;
        s_session_trusted = ash_game_wall_unix_usable(&s_game, now);
        if (s_game.last_settle_unix <= 0) {
            s_game.last_settle_unix = now;
        } else if (ash_game_wall_unix_usable(&s_game, now) && now < s_game.last_settle_unix) {
            /* Mild trusted rollback (E-03): re-anchor without inventing progress. */
            s_game.last_settle_unix = now;
        }
    }
    ashbox_runtime_lock();
    ash_game_settle(&s_game, &clk);
    if (ash_game_wall_unix_usable(&s_game, now)) {
        ash_game_note_wall_time(&s_game, now);
    }
    ESP_LOGI(TAG, "boot_settle deep=%d dt=%lld day=%u settle=%lld wall=%lld usable=%d",
             (int)deep, (long long)clk.dt_s, (unsigned)s_game.day_key,
             (long long)s_game.last_settle_unix, (long long)now,
             (int)ash_game_wall_unix_usable(&s_game, now));
    (void)ashbox_save_store_mirrored(&s_game);
    ashbox_runtime_unlock();
}

static bool key_is_held(void)
{
    const int mv = bsp_button_read_mv();
    return mv >= 0 && mv < BSP_BTN_MV_RELEASED_MIN;
}

static esp_err_t try_sleep(void)
{
    /* settle to now before sleep */
    int64_t now = wall_unix_now();
    uint32_t alarm = 0;
    ashbox_runtime_lock();
    if (s_session_trusted && ash_game_wall_unix_usable(&s_game, now) &&
        s_game.last_settle_unix > 0) {
        ash_clock_info_t clk = {
            .trusted = true,
            .in_deep_sleep_segment = false,
            .dt_s = now - s_game.last_settle_unix,
        };
        if (clk.dt_s < 0) clk.dt_s = 0;
        if (clk.dt_s > DT_MAX_S) clk.dt_s = DT_MAX_S;
        ash_game_settle(&s_game, &clk);
    }
    ash_game_note_wall_time(&s_game, now); /* no-op if RTC rewound vs save */
    s_sleep_rtc_enter = now;
    s_sleep_rtc_magic = ASH_SLEEP_RTC_MAGIC;
    (void)ashbox_save_store_mirrored(&s_game);
    if (s_game.exp.active && s_game.exp.remaining_s > 0) {
        alarm = s_game.exp.remaining_s;
        if (alarm > 28800) alarm = 28800;
    }
    {
        uint32_t w = ash_game_wanderer_next_wake_s(&s_game);
        if (w != UINT32_MAX && w > 0) {
            if (w > 28800) w = 28800;
            if (alarm == 0 || w < alarm) alarm = w;
        }
    }
    ashbox_runtime_unlock();
    ESP_LOGI(TAG, "entering deep sleep alarm=%u", (unsigned)alarm);
    return ashbox_enter_deep_sleep(alarm);
}

void app_main(void)
{
    ESP_LOGI(TAG, "余烬 Ashbox starting");
    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & ~(1u << ESP_SLEEP_WAKEUP_UNDEFINED)) {
        ESP_LOGI(TAG, "wakeup causes: 0x%02x", (unsigned)wake_causes);
    }
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "GPIO wake: arm key guard");
    }

    ESP_ERROR_CHECK(ashbox_save_init());
    ashbox_runtime_init();
    if (ashbox_save_load(&s_game) != ESP_OK) {
        ESP_LOGE(TAG, "save missing/corrupt — starting NEW run (progress reset)");
        ash_game_new(&s_game);
        s_game.last_settle_unix = wall_unix_now();
        ashbox_runtime_lock();
        (void)ashbox_save_store_mirrored(&s_game);
        ashbox_runtime_unlock();
    } else {
        ESP_LOGI(TAG, "resume food=%u pop=%u day=%u", (unsigned)s_game.food,
                 (unsigned)s_game.pop, (unsigned)s_game.day_key);
        if (s_game.exp.active) {
            ESP_LOGI(TAG, "loaded expedition rem=%us / %us tier=%u",
                     (unsigned)s_game.exp.remaining_s, (unsigned)s_game.exp.duration_s,
                     (unsigned)s_game.exp.tier);
        }
    }

    boot_settle(wake_causes);

    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    (void)bsp_battery_init();
    (void)ashbox_sfx_init();

    ESP_ERROR_CHECK(input_dispatch_init());
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) {
            ESP_LOGI(TAG, "wake key already up before button init");
        }
    }

    ashbox_ui_start(&s_game, s_key_guard.armed);
    ashbox_embackup_start(ashbox_ui_flush_persistent);
    s_input_ready = true;

    for (;;) {
        ashbox_ui_tick();
        if (ashbox_ui_wants_deep_sleep()) {
            /* Long-press OK arms sleep while the key is still down. Wait for
               release; any new press cancels via ashbox_ui_on_key. Soft-mute
               only after the key is clear — prepare_deep_sleep is irreversible. */
            if (key_is_held()) {
                /* keep request pending */
            } else {
                ashbox_sfx_stop_all();
                const esp_err_t err = try_sleep();
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "deep sleep refused: %s — restore audio",
                             esp_err_to_name(err));
                    ashbox_ui_on_deep_sleep_aborted();
                }
            }
        }
        /* Adaptive: ~40 ms while awake (snappy keys/anim), slower when dim/blank. */
        int delay_ms = ashbox_ui_tick_delay_ms();
        if (delay_ms < 20) delay_ms = 20;
        if (delay_ms > 200) delay_ms = 200;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
