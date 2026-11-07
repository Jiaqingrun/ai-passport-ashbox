#include "ashbox_sleep.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ash_sleep";

static void warn(const char *step, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "deep sleep continue: %s failed: %s", step, esp_err_to_name(err));
    }
}

esp_err_t ashbox_enter_deep_sleep(uint32_t rtc_alarm_s)
{
    /* Refuse while any key is held — caller must wait for release and must not
       tear down audio before this returns ESP_OK-bound irreversible steps. */
    int mv = bsp_button_read_mv();
    if (mv >= 0 && mv < BSP_BTN_MV_RELEASED_MIN) {
        ESP_LOGW(TAG, "key held (%d mV), refuse sleep", mv);
        return ESP_ERR_INVALID_STATE;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    /* Re-check after settle: long-press OK still held would otherwise pass the
       first sample then wedge into mute with sleep refused. */
    mv = bsp_button_read_mv();
    if (mv >= 0 && mv < BSP_BTN_MV_RELEASED_MIN) {
        ESP_LOGW(TAG, "key re-held (%d mV), refuse sleep", mv);
        return ESP_ERR_INVALID_STATE;
    }

    if (rtc_alarm_s > 0) {
        /* 过夜档约 8h；深睡闹钟上限对齐最长远征 */
        if (rtc_alarm_s > 28800) rtc_alarm_s = 28800;
        esp_err_t err = esp_sleep_enable_timer_wakeup((uint64_t)rtc_alarm_s * 1000000ULL);
        if (err != ESP_OK) {
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            return err;
        }
    }

    int level = 0;
    esp_err_t err = bsp_button_prepare_deep_sleep(&level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "button handoff failed: %s", esp_err_to_name(err));
        esp_restart();
    }
    if (level != 1) {
        /* Button driver already handed off; cannot return to UI — continue.
           Immediate GPIO wake is acceptable over a wedged input path. */
        ESP_LOGW(TAG, "key still low before sleep");
    }

    warn("GPIO wake arm",
         esp_deep_sleep_enable_gpio_wakeup(1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW));

    warn("CW2017 suspend", bsp_battery_sleep());
    warn("ES8311 suspend", bsp_audio_sleep());
    /* Irreversible in this run: only after every refusal check has passed. */
    warn("I2S pin release", bsp_audio_prepare_deep_sleep());
    warn("shared I2C pin release", bsp_i2c_prepare_deep_sleep());

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "cannot lock LVGL before sleep");
        esp_restart();
    }
    warn("ST7789 suspend", bsp_display_prepare_deep_sleep());

    esp_deep_sleep_start();
    ESP_LOGE(TAG, "esp_deep_sleep_start returned");
    esp_restart();
    return ESP_FAIL;
}
