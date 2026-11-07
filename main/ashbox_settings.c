#include "ashbox_settings.h"

#include "ashbox_save.h"
#include "ashbox_sfx.h"
#include "bsp_display.h"

#include "esp_log.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "ash_cfg";
static const char *KEY = "ui_cfg2";

typedef struct {
    uint8_t ver;
    uint8_t brightness;
    uint8_t volume;
    uint8_t radio_on;
} ash_settings_v2_t;

ash_settings_t ashbox_settings_defaults(void)
{
    ash_settings_t s = {
        .ver = 4,
        .brightness = 80,
        .volume = 60,
        .radio_on = 1,
        .guide_step = 0,
        .coach_seen = 0,
    };
    return s;
}

static uint8_t clamp_step10(uint8_t v, uint8_t min_v, uint8_t max_v)
{
    if (v > max_v) {
        v = max_v;
    }
    if (v < min_v) {
        v = min_v;
    }
    v = (uint8_t)((v / 10u) * 10u);
    if (v < min_v) {
        v = min_v;
    }
    return v;
}

uint8_t ashbox_settings_step_up(uint8_t v, uint8_t min_v)
{
    if (v >= 100) {
        return min_v;
    }
    v = (uint8_t)(v + 10);
    if (v > 100) {
        v = 100;
    }
    return v;
}

uint8_t ashbox_settings_step_down(uint8_t v, uint8_t min_v)
{
    if (v <= min_v) {
        return 100;
    }
    if (v < 10) {
        return min_v;
    }
    return (uint8_t)(v - 10);
}

esp_err_t ashbox_settings_load(ash_settings_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = ashbox_settings_defaults();
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }
    uint8_t raw[16];
    size_t len = sizeof(raw);
    err = nvs_get_blob(h, KEY, raw, &len);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *out = ashbox_settings_defaults();
        return ESP_OK;
    }
    if (err != ESP_OK || len < 4) {
        *out = ashbox_settings_defaults();
        return ESP_OK;
    }
    if (len >= sizeof(ash_settings_t) && raw[0] == 4) {
        memcpy(out, raw, sizeof(*out));
    } else if (raw[0] == 3 && len >= 5) {
        out->ver = 4;
        out->brightness = raw[1];
        out->volume = raw[2];
        out->radio_on = raw[3];
        out->guide_step = raw[4];
        out->coach_seen = 0;
    } else if (raw[0] == 2 && len >= sizeof(ash_settings_v2_t)) {
        ash_settings_v2_t v2;
        memcpy(&v2, raw, sizeof(v2));
        out->ver = 4;
        out->brightness = v2.brightness;
        out->volume = v2.volume;
        out->radio_on = v2.radio_on;
        out->guide_step = 0; /* UI start 会对老档自动标完成 */
        out->coach_seen = 0;
    } else {
        *out = ashbox_settings_defaults();
        return ESP_OK;
    }
    out->brightness = clamp_step10(out->brightness, 10, 100);
    out->volume = clamp_step10(out->volume, 0, 100);
    out->radio_on = out->radio_on ? 1 : 0;
    if (out->guide_step > ASH_GUIDE_DONE) out->guide_step = ASH_GUIDE_DONE;
    out->ver = 4;
    return ESP_OK;
}

esp_err_t ashbox_settings_save(const ash_settings_t *in)
{
    if (!in) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs open %s", esp_err_to_name(err));
        return err;
    }
    ash_settings_t store = *in;
    store.ver = 4;
    store.brightness = clamp_step10(store.brightness, 10, 100);
    store.volume = clamp_step10(store.volume, 0, 100);
    store.radio_on = store.radio_on ? 1 : 0;
    if (store.guide_step > ASH_GUIDE_DONE) store.guide_step = ASH_GUIDE_DONE;
    err = nvs_set_blob(h, KEY, &store, sizeof(store));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

void ashbox_settings_apply(const ash_settings_t *in)
{
    if (!in) {
        return;
    }
    uint8_t bl = clamp_step10(in->brightness, 10, 100);
    bsp_display_backlight(bl);
    uint8_t vol = clamp_step10(in->volume, 0, 100);
    ashbox_sfx_set_volume(vol);
    ashbox_sfx_set_enabled(vol > 0);
    ashbox_sfx_set_radio(vol > 0 && in->radio_on);
}
