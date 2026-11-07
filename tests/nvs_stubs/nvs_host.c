#include "nvs.h"
#include "nvs_flash.h"
#include "esp_partition.h"

#include <string.h>

#define NVS_HOST_MAX_KEYS 16
#define NVS_HOST_BLOB_MAX 1024
#define NVS_HOST_PART_MAX 4096

typedef struct {
    char key[16];
    uint8_t data[NVS_HOST_BLOB_MAX];
    size_t len;
    int used;
} nvs_host_slot_t;

static nvs_host_slot_t s_slots[NVS_HOST_MAX_KEYS];
static int s_opened;
static uint8_t s_part_image[NVS_HOST_PART_MAX];
static size_t s_part_size;
static int s_part_ready;
static int s_force_no_free_pages;
static esp_partition_t s_part = {
    .type = ESP_PARTITION_TYPE_DATA,
    .subtype = ESP_PARTITION_SUBTYPE_DATA_NVS,
    .address = 0x9000,
    .size = NVS_HOST_PART_MAX,
    .label = "nvs",
};

void nvs_host_reset(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_opened = 0;
    memset(s_part_image, 0, sizeof(s_part_image));
    s_part_size = 0;
    s_part_ready = 0;
    s_force_no_free_pages = 0;
}

void nvs_host_force_no_free_pages_once(void)
{
    s_force_no_free_pages = 1;
}

void nvs_host_corrupt_key(const char *key)
{
    if (!key) return;
    for (int i = 0; i < NVS_HOST_MAX_KEYS; i++) {
        if (!s_slots[i].used) continue;
        if (strncmp(s_slots[i].key, key, sizeof(s_slots[i].key)) != 0) continue;
        if (s_slots[i].len > 8) s_slots[i].data[8] ^= 0xA5u;
        return;
    }
}

void nvs_host_delete_key(const char *key)
{
    if (!key) return;
    for (int i = 0; i < NVS_HOST_MAX_KEYS; i++) {
        if (!s_slots[i].used) continue;
        if (strncmp(s_slots[i].key, key, sizeof(s_slots[i].key)) != 0) continue;
        memset(&s_slots[i], 0, sizeof(s_slots[i]));
        return;
    }
}

int nvs_host_has_key(const char *key)
{
    if (!key) return 0;
    for (int i = 0; i < NVS_HOST_MAX_KEYS; i++) {
        if (!s_slots[i].used) continue;
        if (strncmp(s_slots[i].key, key, sizeof(s_slots[i].key)) == 0) return 1;
    }
    return 0;
}

void nvs_host_set_partition_image(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > sizeof(s_part_image)) {
        s_part_ready = 0;
        s_part_size = 0;
        return;
    }
    memcpy(s_part_image, data, len);
    if (len < sizeof(s_part_image)) {
        memset(s_part_image + len, 0, sizeof(s_part_image) - len);
    }
    s_part_size = sizeof(s_part_image);
    s_part.size = (uint32_t)s_part_size;
    s_part_ready = 1;
}

esp_err_t nvs_flash_init(void)
{
    if (s_force_no_free_pages) {
        s_force_no_free_pages = 0;
        return ESP_ERR_NVS_NO_FREE_PAGES;
    }
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    return ESP_OK;
}

esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out)
{
    (void)ns;
    (void)mode;
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = 1;
    s_opened = 1;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *buf, size_t *len)
{
    (void)h;
    if (!key || !len) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < NVS_HOST_MAX_KEYS; i++) {
        if (!s_slots[i].used) continue;
        if (strncmp(s_slots[i].key, key, sizeof(s_slots[i].key)) != 0) continue;
        if (!buf) {
            *len = s_slots[i].len;
            return ESP_OK;
        }
        if (*len < s_slots[i].len) {
            *len = s_slots[i].len;
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(buf, s_slots[i].data, s_slots[i].len);
        *len = s_slots[i].len;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *buf, size_t len)
{
    (void)h;
    if (!key || !buf || len == 0 || len > NVS_HOST_BLOB_MAX) return ESP_ERR_INVALID_ARG;
    int free_i = -1;
    for (int i = 0; i < NVS_HOST_MAX_KEYS; i++) {
        if (s_slots[i].used && strncmp(s_slots[i].key, key, sizeof(s_slots[i].key)) == 0) {
            memcpy(s_slots[i].data, buf, len);
            s_slots[i].len = len;
            return ESP_OK;
        }
        if (!s_slots[i].used && free_i < 0) free_i = i;
    }
    if (free_i < 0) return ESP_ERR_NO_MEM;
    memset(&s_slots[free_i], 0, sizeof(s_slots[free_i]));
    strncpy(s_slots[free_i].key, key, sizeof(s_slots[free_i].key) - 1);
    memcpy(s_slots[free_i].data, buf, len);
    s_slots[free_i].len = len;
    s_slots[free_i].used = 1;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h;
    return ESP_OK;
}

void nvs_close(nvs_handle_t h)
{
    (void)h;
    s_opened = 0;
}

const esp_partition_t *esp_partition_find_first(uint32_t type, uint32_t subtype, const char *label)
{
    (void)label;
    if (!s_part_ready) return NULL;
    if (type != ESP_PARTITION_TYPE_DATA || subtype != ESP_PARTITION_SUBTYPE_DATA_NVS) return NULL;
    return &s_part;
}

esp_err_t esp_partition_read(const esp_partition_t *part, size_t offset, void *dst, size_t size)
{
    if (!part || !dst) return ESP_ERR_INVALID_ARG;
    if (!s_part_ready) return ESP_FAIL;
    if (offset + size > s_part_size) return ESP_ERR_INVALID_SIZE;
    memcpy(dst, s_part_image + offset, size);
    return ESP_OK;
}
