#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#define ESP_PARTITION_TYPE_DATA 0x01
#define ESP_PARTITION_SUBTYPE_DATA_NVS 0x02

typedef struct {
    uint32_t type;
    uint32_t subtype;
    uint32_t address;
    uint32_t size;
    const char *label;
} esp_partition_t;

const esp_partition_t *esp_partition_find_first(uint32_t type, uint32_t subtype,
                                                const char *label);
esp_err_t esp_partition_read(const esp_partition_t *part, size_t offset, void *dst,
                             size_t size);
