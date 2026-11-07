#pragma once

#include <stddef.h>
#include <stdint.h>

/** Reset in-memory NVS + optional partition image (host tests only). */
void nvs_host_reset(void);
/** Flip a byte inside a stored blob so CRC decode fails. */
void nvs_host_corrupt_key(const char *key);
void nvs_host_delete_key(const char *key);
int nvs_host_has_key(const char *key);
/** Feed a fake NVS partition dump for rescue/init path tests. */
void nvs_host_set_partition_image(const uint8_t *data, size_t len);
/** Next nvs_flash_init returns ESP_ERR_NVS_NO_FREE_PAGES once (triggers rescue). */
void nvs_host_force_no_free_pages_once(void);
