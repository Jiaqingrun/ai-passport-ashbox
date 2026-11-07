#include "ashbox_meta.h"
#include "ashbox_save.h"

#include "esp_log.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "ash_meta";

static uint32_t crc32_bytes(const uint8_t *data, size_t n)
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

void ashbox_meta_new(ashbox_meta_t *m)
{
    memset(m, 0, sizeof(*m));
    m->magic = ASHBOX_META_MAGIC;
    m->ver = ASHBOX_META_VER;
    m->crc = ashbox_meta_crc(m);
}

uint32_t ashbox_meta_crc(const ashbox_meta_t *m)
{
    ashbox_meta_t tmp = *m;
    tmp.crc = 0;
    return crc32_bytes((const uint8_t *)&tmp, sizeof(tmp));
}

esp_err_t ashbox_meta_load(ashbox_meta_t *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ashbox_meta_new(m);
        return err;
    }
    ashbox_meta_t raw;
    size_t len = sizeof(raw);
    err = nvs_get_blob(h, ASHBOX_NVS_META_KEY, &raw, &len);
    nvs_close(h);
    if (err != ESP_OK || len != sizeof(raw)) {
        ashbox_meta_new(m);
        return (err == ESP_OK) ? ESP_ERR_INVALID_ARG : err;
    }
    if (raw.magic != ASHBOX_META_MAGIC || raw.ver != ASHBOX_META_VER) {
        ashbox_meta_new(m);
        return ESP_ERR_INVALID_CRC;
    }
    uint32_t expect = raw.crc;
    if (ashbox_meta_crc(&raw) != expect) {
        ashbox_meta_new(m);
        return ESP_ERR_INVALID_CRC;
    }
    *m = raw;
    return ESP_OK;
}

esp_err_t ashbox_meta_store(const ashbox_meta_t *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    ashbox_meta_t tmp = *m;
    tmp.magic = ASHBOX_META_MAGIC;
    tmp.ver = ASHBOX_META_VER;
    tmp.crc = ashbox_meta_crc(&tmp);
    nvs_handle_t h;
    esp_err_t err = nvs_open(ASHBOX_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, ASHBOX_NVS_META_KEY, &tmp, sizeof(tmp));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "meta store failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool ashbox_meta_has(const ashbox_meta_t *m, uint16_t bit)
{
    return m && (m->unlocked & bit) != 0;
}

uint16_t ashbox_meta_unlock(ashbox_meta_t *m, uint16_t bits)
{
    if (!m) return 0;
    uint16_t neu = (uint16_t)(bits & ~m->unlocked);
    m->unlocked = (uint16_t)(m->unlocked | (bits & ASH_ACH_ALL_MASK));
    return neu;
}

void ash_game_mark_peace_broken(ash_game_t *g)
{
    if (g) g->peace_broken = 1;
}

uint16_t ash_game_eval_clear_achievements(ash_game_t *g, ashbox_meta_t *m)
{
    if (!g || !m) return 0;
    if (!g->run_cleared) return 0;
    uint16_t bits = 0;
    if (g->pop == 1) bits |= ASH_ACH_LONE;
    if (g->pop == 2 || g->pop == 3) bits |= ASH_ACH_TOGETHER;
    if (g->pop >= 5) bits |= ASH_ACH_CROWD;
    if (!g->peace_broken) bits |= ASH_ACH_PACIFIST;
    if (g->pop == 0) bits |= ASH_ACH_NOBODY;
    uint16_t neu = ashbox_meta_unlock(m, bits);
    if (neu) {
        g->achieve_toast_mask = (uint16_t)(g->achieve_toast_mask | neu);
        g->clear_toast_pending = 1;
        m->clear_count = (uint16_t)(m->clear_count + 1u);
    }
    return neu;
}

void ash_game_apply_meta_grants(ash_game_t *g, ashbox_meta_t *m)
{
    if (!g || !m) return;
    if (ashbox_meta_has(m, ASH_ACH_LONE)) {
        g->pistol = (uint8_t)(g->pistol + 1u);
    }
    if (ashbox_meta_has(m, ASH_ACH_TOGETHER)) {
        g->backpack = (uint8_t)(g->backpack + 2u);
    }
    if (ashbox_meta_has(m, ASH_ACH_CROWD)) {
        g->pistol = (uint8_t)(g->pistol + 2u);
        g->ammo = (uint8_t)(g->ammo + 10u);
    }
    if (ashbox_meta_has(m, ASH_ACH_PACIFIST)) {
        g->stash_food_bonus = 50;
    }
    if (ashbox_meta_has(m, ASH_ACH_NOBODY) && !m->super_core_granted &&
        !m->super_core_consumed) {
        g->super_core = 1;
        m->super_core_granted = 1;
    }
    g->crc = ash_game_crc(g);
}

void ash_game_restart_with_meta(ash_game_t *g, ashbox_meta_t *m)
{
    if (!g) return;
    ash_game_new(g);
    if (m) ash_game_apply_meta_grants(g, m);
}

const char *ash_ach_name(uint16_t bit)
{
    switch (bit) {
    case ASH_ACH_LONE:
        return "孤勇者";
    case ASH_ACH_TOGETHER:
        return "结伴而行";
    case ASH_ACH_CROWD:
        return "人多势众";
    case ASH_ACH_PACIFIST:
        return "和平主义者";
    case ASH_ACH_NOBODY:
        return "无人生还";
    default:
        return "成就";
    }
}

const char *ash_ach_cond(uint16_t bit)
{
    switch (bit) {
    case ASH_ACH_LONE:
        return "通关时站里只剩1人";
    case ASH_ACH_TOGETHER:
        return "通关时站里有2人或3人";
    case ASH_ACH_CROWD:
        return "通关时站里至少5人";
    case ASH_ACH_PACIFIST:
        return "通关前从未拿手枪或子弹";
    case ASH_ACH_NOBODY:
        return "通关时站里一个人都没有";
    default:
        return "";
    }
}

const char *ash_ach_grant(uint16_t bit)
{
    switch (bit) {
    case ASH_ACH_LONE:
        return "重新开始后：开局多1把手枪";
    case ASH_ACH_TOGETHER:
        return "重新开始后：开局多2个背包";
    case ASH_ACH_CROWD:
        return "重新开始后：开局多2手枪和10发子弹";
    case ASH_ACH_PACIFIST:
        return "重新开始后：仓库口粮容量多50";
    case ASH_ACH_NOBODY:
        return "重新开始后：得到1个超级核心（生涯一次）";
    default:
        return "";
    }
}
