#include "ashbox_embackup_proto.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char last[384];
    char begin[384];
    char end[384];
    char ready[384];
    char error[384];
    int n;
    int data_n;
    uint8_t img[ASH_EMBACKUP_NVS_SIZE];
    uint32_t ms;
    bool committed;
    uint32_t commit_crc;
    int reboots;
    bool saw_begin;
    bool saw_end;
    bool saw_ready;
    bool saw_open_import;
    bool saw_no_host;
} harness_t;

static bool emit_cb(const char *line, void *user)
{
    harness_t *h = user;
    h->n++;
    snprintf(h->last, sizeof(h->last), "%s", line);
    if (strncmp(line, "!EMBACKUP READY ", 16) == 0) {
        h->saw_ready = true;
        snprintf(h->ready, sizeof(h->ready), "%s", line);
    } else if (strncmp(line, "!EMBACKUP BEGIN ", 16) == 0) {
        h->saw_begin = true;
        snprintf(h->begin, sizeof(h->begin), "%s", line);
    } else if (strncmp(line, "!EMBACKUP DATA ", 15) == 0) {
        h->data_n++;
    } else if (strncmp(line, "!EMBACKUP END ", 14) == 0) {
        h->saw_end = true;
        snprintf(h->end, sizeof(h->end), "%s", line);
    } else if (strcmp(line, "!EMBACKUP ERROR OPEN_IMPORT") == 0) {
        h->saw_open_import = true;
    } else if (strcmp(line, "!EMBACKUP ERROR NO_HOST") == 0) {
        h->saw_no_host = true;
    } else if (strncmp(line, "!EMBACKUP ERROR ", 16) == 0) {
        snprintf(h->error, sizeof(h->error), "%s", line);
    }
    return true;
}

static bool snap_cb(uint8_t *out, size_t len, void *user)
{
    harness_t *h = user;
    assert(len == ASH_EMBACKUP_NVS_SIZE);
    memcpy(out, h->img, len);
    return true;
}

static bool commit_cb(const uint8_t *data, size_t len, uint32_t crc, void *user)
{
    harness_t *h = user;
    assert(len == ASH_EMBACKUP_NVS_SIZE);
    assert(ash_em_crc32(data, len) == crc);
    h->committed = true;
    h->commit_crc = crc;
    memcpy(h->img, data, len);
    return true;
}

static void reboot_cb(void *user)
{
    harness_t *h = user;
    h->reboots++;
}

static uint32_t now_cb(void *user)
{
    harness_t *h = user;
    return h->ms;
}

static void feed_str(ash_em_proto_t *p, const char *s)
{
    for (const char *c = s; *c; c++) {
        ash_em_proto_feed(p, *c);
    }
    ash_em_proto_feed(p, '\n');
}

static void test_crc_matches_js_vector(void)
{
    uint8_t b[4] = {1, 2, 3, 4};
    /* Independently computed with the same algorithm as backup.mjs */
    assert(ash_em_crc32(b, 4) == 0xb63cfbcdu);
}

static void test_hello_ready_and_backup(void)
{
    harness_t h;
    memset(&h, 0, sizeof(h));
    for (size_t i = 0; i < sizeof(h.img); i++) {
        h.img[i] = (uint8_t)(i & 0xff);
    }
    uint8_t *scratch = malloc(ASH_EMBACKUP_NVS_SIZE);
    assert(scratch);
    ash_em_proto_t *p = calloc(1, ash_em_proto_bytes());
    assert(p);

    ash_em_hooks_t hooks = {
        .emit = emit_cb,
        .snapshot = snap_cb,
        .commit = commit_cb,
        .reboot = reboot_cb,
        .flush = NULL,
        .user = &h,
        .save_version = 8,
        .now_ms = now_cb,
    };
    memcpy(hooks.device_id, "aabbccddeeff", 13);
    memset(hooks.firmware, 'a', 64);
    hooks.firmware[64] = '\0';
    ash_em_proto_init(p, &hooks);
    ash_em_proto_set_recv_buf(p, scratch, ASH_EMBACKUP_NVS_SIZE);

    feed_str(p, "!EMBACKUP HELLO 0123456789abcdef0123456789abcdef");
    assert(h.saw_ready);
    assert(ash_em_proto_host_ok(p));

    ash_em_proto_request_export(p, false);
    for (int i = 0; i < 400; i++) {
        ash_em_proto_tick(p);
    }
    assert(h.saw_begin);
    assert(h.data_n == (int)(ASH_EMBACKUP_NVS_SIZE / 128));
    assert(h.saw_end);
    {
        char expect[16];
        snprintf(expect, sizeof(expect), "%08x", ash_em_crc32(h.img, ASH_EMBACKUP_NVS_SIZE));
        assert(strstr(h.begin, expect) != NULL);
        assert(strstr(h.end, expect) != NULL);
    }

    /* ACK */
    char ack[80];
    snprintf(ack, sizeof(ack),
             "!EMBACKUP ACK 0123456789abcdef0123456789abcdef 1");
    feed_str(p, ack);
    assert(ash_em_proto_ui(p) == ASH_EM_UI_BACKUP_OK);

    free(scratch);
    free(p);
}

static void test_offer_rejects_wrong_device(void)
{
    harness_t h;
    memset(&h, 0, sizeof(h));
    uint8_t *scratch = malloc(ASH_EMBACKUP_NVS_SIZE);
    ash_em_proto_t *p = calloc(1, ash_em_proto_bytes());
    ash_em_hooks_t hooks = {
        .emit = emit_cb,
        .snapshot = snap_cb,
        .commit = commit_cb,
        .reboot = reboot_cb,
        .user = &h,
        .save_version = 8,
        .now_ms = now_cb,
    };
    memcpy(hooks.device_id, "aabbccddeeff", 13);
    memset(hooks.firmware, 'b', 64);
    hooks.firmware[64] = '\0';
    ash_em_proto_init(p, &hooks);
    ash_em_proto_set_recv_buf(p, scratch, ASH_EMBACKUP_NVS_SIZE);
    feed_str(p, "!EMBACKUP HELLO 0123456789abcdef0123456789abcdef");
    feed_str(p, "!EMBACKUP OFFER 0123456789abcdef0123456789abcdef "
                "001122334455 "
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb "
                "8 24576 deadbeef");
    assert(strstr(h.error, "INCOMPATIBLE") != NULL);
    assert(ash_em_proto_ui(p) != ASH_EM_UI_IMPORT_CONFIRM);
    free(scratch);
    free(p);
}

static void test_no_host_backup(void)
{
    harness_t h;
    memset(&h, 0, sizeof(h));
    uint8_t *scratch = malloc(ASH_EMBACKUP_NVS_SIZE);
    ash_em_proto_t *p = calloc(1, ash_em_proto_bytes());
    ash_em_hooks_t hooks = {
        .emit = emit_cb,
        .snapshot = snap_cb,
        .user = &h,
        .save_version = 8,
        .now_ms = now_cb,
    };
    memcpy(hooks.device_id, "aabbccddeeff", 13);
    memset(hooks.firmware, 'c', 64);
    hooks.firmware[64] = '\0';
    ash_em_proto_init(p, &hooks);
    ash_em_proto_set_recv_buf(p, scratch, ASH_EMBACKUP_NVS_SIZE);
    ash_em_proto_request_export(p, false);
    assert(h.saw_no_host);
    assert(ash_em_proto_ui(p) == ASH_EM_UI_NO_HOST);
    free(scratch);
    free(p);
}

static void test_import_offer_confirm(void)
{
    harness_t h;
    memset(&h, 0, sizeof(h));
    for (size_t i = 0; i < sizeof(h.img); i++) {
        h.img[i] = (uint8_t)(0xa5 ^ (i & 0xff));
    }
    uint8_t *scratch = malloc(ASH_EMBACKUP_NVS_SIZE);
    ash_em_proto_t *p = calloc(1, ash_em_proto_bytes());
    ash_em_hooks_t hooks = {
        .emit = emit_cb,
        .snapshot = snap_cb,
        .commit = commit_cb,
        .reboot = reboot_cb,
        .user = &h,
        .save_version = 8,
        .now_ms = now_cb,
    };
    memcpy(hooks.device_id, "aabbccddeeff", 13);
    memset(hooks.firmware, 'd', 64);
    hooks.firmware[64] = '\0';
    ash_em_proto_init(p, &hooks);
    ash_em_proto_set_recv_buf(p, scratch, ASH_EMBACKUP_NVS_SIZE);

    feed_str(p, "!EMBACKUP HELLO 0123456789abcdef0123456789abcdef");
    /* OFFER must work without pre-opening import mode. */
    uint32_t crc = ash_em_crc32(h.img, ASH_EMBACKUP_NVS_SIZE);
    char offer[256];
    snprintf(offer, sizeof(offer),
             "!EMBACKUP OFFER 0123456789abcdef0123456789abcdef "
             "aabbccddeeff "
             "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd "
             "8 24576 %08x",
             crc);
    feed_str(p, offer);
    assert(ash_em_proto_ui(p) == ASH_EM_UI_IMPORT_CONFIRM);
    assert(strstr(h.last, "!EMBACKUP OFFERED") != NULL);
    h.error[0] = '\0';
    ash_em_proto_set_import_mode(p, true); /* re-tap must not emit BUSY */
    assert(h.error[0] == '\0');

    ash_em_proto_confirm_import(p, true);
    for (int i = 0; i < 400; i++) {
        ash_em_proto_tick(p);
    }
    assert(h.saw_begin);
    assert(h.saw_end);
    assert(ash_em_proto_busy(p) || strstr(h.last, "!EMBACKUP END") != NULL);

    free(scratch);
    free(p);
}

int main(void)
{
    test_crc_matches_js_vector();
    test_hello_ready_and_backup();
    test_offer_rejects_wrong_device();
    test_import_offer_confirm();
    test_no_host_backup();
    puts("test_ashbox_embackup: PASS");
    return 0;
}
