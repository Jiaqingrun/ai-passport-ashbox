#include "ashbox_embackup.h"
#include "ashbox_embackup_proto.h"
#include "ashbox_game.h"
#include "ashbox_save.h"

#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "driver/usb_serial_jtag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "embackup";

#define JRNL_MAGIC 0x454d4252u /* EMBR */
#define TX_CHUNK   64u

typedef struct {
    uint32_t magic;
    uint32_t result;
    uint32_t crc;
} ash_em_jrnl_t;

static RTC_NOINIT_ATTR ash_em_jrnl_t s_jrnl;

static ash_em_proto_t *s_proto;
static uint8_t *s_img;
static SemaphoreHandle_t s_lock;
static ash_em_flush_fn s_flush;
static char s_device[13];
static char s_fw[65];
static bool s_started;

/* UI thread may already hold ashbox_runtime_lock(); never call flush/export there. */
static volatile bool s_pend_backup;
static volatile bool s_pend_import_on;
static volatile bool s_pend_import_off;
static volatile bool s_pend_confirm_yes;
static volatile bool s_pend_confirm_no;

/* Lock-free mirrors so UI never blocks on s_lock during USB TX. */
static volatile ash_em_ui_t s_ui_cache;
static volatile bool s_host_ok_cache;
static volatile bool s_busy_cache;
static volatile bool s_preamble_cache;
static volatile int s_pct_cache = -1;

static void lock(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

static void refresh_cache_locked(void)
{
    if (!s_proto) {
        return;
    }
    s_ui_cache = ash_em_proto_ui(s_proto);
    s_host_ok_cache = ash_em_proto_host_ok(s_proto);
    s_busy_cache = ash_em_proto_busy(s_proto);
    s_preamble_cache = ash_em_proto_import_preamble(s_proto);
    s_pct_cache = ash_em_proto_progress_pct(s_proto);
}

static uint32_t hook_now_ms(void *user)
{
    (void)user;
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool usb_write_all(const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t off = 0;
    int idle = 0;
    while (off < n) {
        size_t todo = n - off;
        if (todo > TX_CHUNK) {
            todo = TX_CHUNK;
        }
        int w = usb_serial_jtag_write_bytes(p + off, todo, pdMS_TO_TICKS(100));
        if (w < 0) {
            return false;
        }
        if (w == 0) {
            if (++idle > 250) {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        idle = 0;
        off += (size_t)w;
    }
    return true;
}

static bool hook_emit(const char *line, void *user)
{
    (void)user;
    if (!line) {
        return false;
    }
    size_t n = strlen(line);
    char nl = '\n';
    /*
     * Protocol lock must not be held across USB waits — UI also takes it for
     * status reads, and flush needs the runtime lock (opposite order).
     */
    unlock();
    bool ok = usb_write_all(line, n) && usb_write_all(&nl, 1);
    lock();
    return ok;
}

static bool hook_snapshot(uint8_t *out, size_t len, void *user)
{
    (void)user;
    if (!out || len != ASH_EMBACKUP_NVS_SIZE) {
        return false;
    }
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    if (!part || part->size < ASH_EMBACKUP_NVS_SIZE) {
        return false;
    }
    return esp_partition_read(part, 0, out, ASH_EMBACKUP_NVS_SIZE) == ESP_OK;
}

static bool hook_commit(const uint8_t *data, size_t len, uint32_t crc, void *user)
{
    (void)user;
    if (!data || len != ASH_EMBACKUP_NVS_SIZE) {
        return false;
    }
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    if (!part || part->size < ASH_EMBACKUP_NVS_SIZE) {
        return false;
    }

    (void)nvs_flash_deinit();

    esp_err_t err = esp_partition_erase_range(part, 0, part->size);
    if (err != ESP_OK) {
        /* Erase did not start/complete — original NVS should still be intact. */
        ESP_LOGE(TAG, "nvs erase failed: %s", esp_err_to_name(err));
        (void)nvs_flash_init();
        return false;
    }
    err = esp_partition_write(part, 0, data, ASH_EMBACKUP_NVS_SIZE);
    if (err != ESP_OK) {
        /* Erase already destroyed the live save; no on-device rollback possible.
         * Journal result 2 means: restore from the PC before-import backup. */
        ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(err));
        s_jrnl.magic = JRNL_MAGIC;
        s_jrnl.result = 2;
        s_jrnl.crc = crc;
        return false;
    }

    err = nvs_flash_init();
    unsigned result = 1;
    if (err != ESP_OK) {
        result = 3;
    } else if (!ashbox_save_nvs_readable()) {
        /* Must use ASHBOX_NVS_NS ("ashbox"), not the ASH2 magic word. */
        ESP_LOGE(TAG, "post-import verify failed: namespace %s / key %s unreadable",
                 ASHBOX_NVS_NS, ASHBOX_NVS_SAVE_KEY);
        result = 3;
    } else {
        /* B20: blob present is not enough — decode/CRC must succeed.
         * Static scratch: ash_game_t is 776B; avoid stacking on a tight task. */
        static ash_game_t s_verify;
        if (ashbox_save_load(&s_verify) != ESP_OK) {
            ESP_LOGE(TAG, "post-import verify failed: save decode/CRC");
            result = 3;
        }
    }

    s_jrnl.magic = JRNL_MAGIC;
    s_jrnl.result = result;
    s_jrnl.crc = crc;
    /* B11: only STAGED+reboot when verify passed (result==1). */
    return result == 1u;
}

static void hook_reboot(void *user)
{
    (void)user;
    vTaskDelay(pdMS_TO_TICKS(80));
    esp_restart();
}

static void fill_ids(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        memset(mac, 0, sizeof(mac));
    }
    snprintf(s_device, sizeof(s_device), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);

    const esp_app_desc_t *desc = esp_app_get_description();
    const uint8_t *sha = desc ? desc->app_elf_sha256 : NULL;
    if (sha) {
        for (int i = 0; i < 32; i++) {
            static const char *dig = "0123456789abcdef";
            s_fw[i * 2] = dig[sha[i] >> 4];
            s_fw[i * 2 + 1] = dig[sha[i] & 0xf];
        }
        s_fw[64] = '\0';
    } else {
        memset(s_fw, '0', 64);
        s_fw[64] = '\0';
    }
}

static void usb_task(void *arg)
{
    (void)arg;
    bool quiet = false;
    for (;;) {
        bool do_backup = s_pend_backup;
        bool do_yes = s_pend_confirm_yes;
        bool do_no = s_pend_confirm_no;
        bool do_in = s_pend_import_on;
        bool do_out = s_pend_import_off;
        if (do_backup) {
            s_pend_backup = false;
        }
        if (do_yes) {
            s_pend_confirm_yes = false;
        }
        if (do_no) {
            s_pend_confirm_no = false;
        }
        if (do_in) {
            s_pend_import_on = false;
        }
        if (do_out) {
            s_pend_import_off = false;
        }

        /* Flush without s_lock — opposite lock order vs UI (runtime then s_lock). */
        if (do_backup || do_yes) {
            esp_log_level_set("*", ESP_LOG_ERROR);
            quiet = true;
            if (s_flush) {
                s_flush();
            }
        }

        bool need_fast = s_busy_cache || do_backup || do_yes || do_in ||
                         s_ui_cache == ASH_EM_UI_IMPORT_WAIT ||
                         s_ui_cache == ASH_EM_UI_IMPORT_CONFIRM ||
                         s_ui_cache == ASH_EM_UI_IMPORT_BUSY;
        int wait_ms = need_fast ? 5 : 20;
        uint8_t rx[256];
        int n = usb_serial_jtag_read_bytes(rx, sizeof(rx), pdMS_TO_TICKS(wait_ms));

        lock();
        if (s_proto) {
            ash_em_proto_set_flush(s_proto, NULL); /* already flushed above when needed */
            if (do_out) {
                ash_em_proto_set_import_mode(s_proto, false);
            }
            if (do_in) {
                ash_em_proto_set_import_mode(s_proto, true);
            }
            if (do_no) {
                ash_em_proto_confirm_import(s_proto, false);
            }
            if (do_yes) {
                ash_em_proto_confirm_import(s_proto, true);
            }
            if (do_backup) {
                ash_em_proto_request_export(s_proto, false);
            }
            for (int i = 0; i < n; i++) {
                ash_em_proto_feed(s_proto, (char)rx[i]);
            }
            ash_em_proto_tick(s_proto);
            refresh_cache_locked();
            if (!s_busy_cache && s_ui_cache != ASH_EM_UI_IMPORT_BUSY &&
                s_ui_cache != ASH_EM_UI_IMPORT_WAIT &&
                s_ui_cache != ASH_EM_UI_IMPORT_CONFIRM && quiet) {
                esp_log_level_set("*", ESP_LOG_INFO);
                quiet = false;
            }
        }
        unlock();
    }
}

void ashbox_embackup_start(ash_em_flush_fn flush)
{
    if (s_started) {
        return;
    }
    s_flush = flush;
    s_lock = xSemaphoreCreateMutex();
    s_img = (uint8_t *)malloc(ASH_EMBACKUP_NVS_SIZE);
    s_proto = (ash_em_proto_t *)calloc(1, ash_em_proto_bytes());
    if (!s_lock || !s_img || !s_proto) {
        ESP_LOGE(TAG, "out of memory for USB backup");
        return;
    }

    fill_ids();

    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "usb_serial_jtag_driver_install: %s", esp_err_to_name(err));
    }

    ash_em_hooks_t hooks = {
        .emit = hook_emit,
        .snapshot = hook_snapshot,
        .commit = hook_commit,
        .reboot = hook_reboot,
        .flush = NULL, /* USB task flushes before export/confirm */
        .user = NULL,
        .save_version = ASHBOX_SAVE_VER,
        .now_ms = hook_now_ms,
    };
    memcpy(hooks.device_id, s_device, sizeof(hooks.device_id));
    memcpy(hooks.firmware, s_fw, sizeof(hooks.firmware));
    ash_em_proto_init(s_proto, &hooks);
    ash_em_proto_set_recv_buf(s_proto, s_img, ASH_EMBACKUP_NVS_SIZE);

    if (s_jrnl.magic == JRNL_MAGIC) {
        ash_em_proto_set_pending_restored(s_proto, (unsigned)s_jrnl.result, s_jrnl.crc);
        s_jrnl.magic = 0;
    }

    if (xTaskCreate(usb_task, "em_usb", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "usb task create failed");
        return;
    }
    s_started = true;
    ESP_LOGI(TAG, "USB save protocol ready (device %s)", s_device);
}

bool ashbox_embackup_host_ok(void)
{
    return s_host_ok_cache;
}

bool ashbox_embackup_busy(void)
{
    return s_busy_cache || s_pend_backup || s_pend_confirm_yes;
}

ash_em_ui_t ashbox_embackup_ui_state(void)
{
    ash_em_ui_t ui = s_ui_cache;
    if (ui == ASH_EM_UI_IDLE && (s_pend_backup || s_pend_confirm_yes)) {
        return ASH_EM_UI_BACKUP_BUSY;
    }
    if (ui == ASH_EM_UI_IDLE && s_pend_import_on) {
        return ASH_EM_UI_IMPORT_WAIT;
    }
    return ui;
}

int ashbox_embackup_progress_pct(void)
{
    return s_pct_cache;
}

bool ashbox_embackup_import_preamble(void)
{
    return s_preamble_cache;
}

void ashbox_embackup_clear_ui_pulse(void)
{
    lock();
    if (s_proto) {
        ash_em_proto_clear_ui_pulse(s_proto);
        refresh_cache_locked();
    }
    unlock();
}

void ashbox_embackup_request_backup(void)
{
    s_pend_backup = true;
}

void ashbox_embackup_enter_import(void)
{
    s_pend_import_off = false;
    s_pend_import_on = true;
}

void ashbox_embackup_leave_import(void)
{
    s_pend_import_on = false;
    s_pend_import_off = true;
}

void ashbox_embackup_confirm_overwrite(bool accept)
{
    if (accept) {
        s_pend_confirm_no = false;
        s_pend_confirm_yes = true;
    } else {
        s_pend_confirm_yes = false;
        s_pend_confirm_no = true;
    }
}
