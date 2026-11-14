#include "ashbox_digest.h"

#include "ashbox_save.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "ash_digest";
static const char *KEY = "away";

static void snap_from_game(const ash_game_t *g, int64_t now, ash_digest_snap_t *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = ASH_DIGEST_MAGIC;
    s->ver = ASH_DIGEST_VER;
    s->bytes = (uint16_t)sizeof(*s);
    s->unix = now;
    s->food = ash_game_total_food(g);
    s->parts = ash_game_total_parts(g);
    s->iron = g->iron;
    s->water = ash_game_total_water(g);
    s->ember = g->ember;
    s->threat = g->threat;
    s->pop = g->pop;
}

esp_err_t ashbox_digest_store(const ash_game_t *g, int64_t now)
{
    if (!g) {
        return ESP_ERR_INVALID_ARG;
    }
    if (now <= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    ash_digest_snap_t s;
    snap_from_game(g, now, &s);

    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs open %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(h, KEY, &s, sizeof(s));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "store %s", esp_err_to_name(err));
    }
    return err;
}

void ashbox_digest_discard(void)
{
    nvs_handle_t h;
    if (nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_erase_key(h, KEY);
    (void)nvs_commit(h);
    nvs_close(h);
}

bool ashbox_digest_take(const ash_game_t *g, int64_t now, ash_digest_delta_t *out)
{
    if (!g || !out) {
        return false;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return false;
    }
    ash_digest_snap_t s;
    size_t len = sizeof(s);
    err = nvs_get_blob(h, KEY, &s, &len);
    nvs_close(h);
    if (err != ESP_OK || len != sizeof(s)) {
        return false;
    }

    bool ok = ash_digest_calc(&s, now, ash_game_total_food(g), ash_game_total_parts(g),
                              g->iron, ash_game_total_water(g), g->ember, g->threat, out);
    /* Consume either way: a rejected blob (stale or from an older build) must not
     * reappear on every boot. */
    if (nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        (void)nvs_erase_key(h, KEY);
        (void)nvs_commit(h);
        nvs_close(h);
    }
    return ok;
}
