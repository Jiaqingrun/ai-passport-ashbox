#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef uint32_t nvs_handle_t;
#define NVS_READONLY 1
#define NVS_READWRITE 2

esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *buf, size_t *len);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *buf, size_t len);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);
