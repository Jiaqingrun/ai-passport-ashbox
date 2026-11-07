#include "ashbox_embackup_proto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 512
#define ACK_TIMEOUT_MS 60000u
#define RECV_IDLE_TIMEOUT_MS 30000u
#define CHUNK 128u

typedef enum {
    ST_IDLE = 0,
    ST_EXPORT_SEND,
    ST_EXPORT_WAIT_ACK,
    ST_IMPORT_WAIT_OFFER,
    ST_IMPORT_CONFIRM,
    ST_IMPORT_RECV,
} ash_em_st_t;

struct ash_em_proto {
    ash_em_hooks_t hooks;
    ash_em_st_t st;
    ash_em_ui_t ui;
    bool host_ok;
    char session[33];
    char line[LINE_MAX];
    size_t line_len;
    uint32_t export_id;
    uint32_t ack_deadline_ms;
    uint32_t export_crc;
    uint32_t export_off;
    bool export_for_import;
    bool import_mode;
    char offer_device[13];
    char offer_fw[65];
    unsigned offer_ver;
    uint32_t offer_size;
    uint32_t offer_crc;
    uint8_t *recv_buf;
    size_t recv_cap;
    uint32_t recv_off;
    uint32_t upload_id;
    uint32_t recv_deadline_ms;
    unsigned pending_restored_result;
    uint32_t pending_restored_crc;
    bool pending_restored;
};

size_t ash_em_proto_bytes(void)
{
    return sizeof(struct ash_em_proto);
}

uint32_t ash_em_crc32(const uint8_t *data, size_t len)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < len; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int)(c & 1u);
            c = (c >> 1) ^ (0xedb88320u & mask);
        }
    }
    return c ^ 0xffffffffu;
}

static uint32_t now_ms(ash_em_proto_t *p)
{
    return p->hooks.now_ms ? p->hooks.now_ms(p->hooks.user) : 0;
}

static bool emitf(ash_em_proto_t *p, const char *fmt, ...)
{
    char buf[LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (!p->hooks.emit) {
        return false;
    }
    return p->hooks.emit(buf, p->hooks.user);
}

static void emit_error(ash_em_proto_t *p, const char *code)
{
    (void)emitf(p, "!EMBACKUP ERROR %s", code);
}

static void emit_ready(ash_em_proto_t *p)
{
    (void)emitf(p, "!EMBACKUP READY %s %s", p->session, p->hooks.device_id);
    if (p->pending_restored) {
        (void)emitf(p, "!EMBACKUP RESTORED %s %u %08x", p->session,
                     p->pending_restored_result, (unsigned)p->pending_restored_crc);
        p->pending_restored = false;
        p->ui = ASH_EM_UI_IMPORT_DONE_HINT;
    }
}

static bool hex_nibble(char c, uint8_t *out)
{
    if (c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        *out = (uint8_t)(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *out = (uint8_t)(c - 'A' + 10);
        return true;
    }
    return false;
}

static bool parse_hex_bytes(const char *hex, size_t hex_len, uint8_t *out, size_t out_len)
{
    if (hex_len != out_len * 2) {
        return false;
    }
    for (size_t i = 0; i < out_len; i++) {
        uint8_t hi, lo;
        if (!hex_nibble(hex[i * 2], &hi) || !hex_nibble(hex[i * 2 + 1], &lo)) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static void bytes_to_hex(const uint8_t *in, size_t n, char *out)
{
    static const char *dig = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = dig[in[i] >> 4];
        out[i * 2 + 1] = dig[in[i] & 0xf];
    }
    out[n * 2] = '\0';
}

static void set_ui(ash_em_proto_t *p, ash_em_ui_t ui)
{
    p->ui = ui;
}

static void begin_export(ash_em_proto_t *p)
{
    if (!p->host_ok) {
        emit_error(p, "NO_HOST");
        set_ui(p, ASH_EM_UI_NO_HOST);
        return;
    }
    if (!p->recv_buf || p->recv_cap < ASH_EMBACKUP_NVS_SIZE) {
        emit_error(p, "MEMORY");
        set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        p->st = ST_IDLE;
        return;
    }
    if (p->hooks.flush) {
        p->hooks.flush();
    }
    if (!p->hooks.snapshot ||
        !p->hooks.snapshot(p->recv_buf, ASH_EMBACKUP_NVS_SIZE, p->hooks.user)) {
        emit_error(p, "SNAPSHOT");
        set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        p->st = ST_IDLE;
        return;
    }
    p->export_crc = ash_em_crc32(p->recv_buf, ASH_EMBACKUP_NVS_SIZE);
    p->export_id += 1u;
    if (p->export_id == 0) {
        p->export_id = 1;
    }
    p->export_off = 0;
    p->st = ST_EXPORT_SEND;
    set_ui(p, p->export_for_import ? ASH_EM_UI_IMPORT_BUSY : ASH_EM_UI_BACKUP_BUSY);
    if (!emitf(p, "!EMBACKUP BEGIN %s %u %s %s %u %u %u %08x", p->session,
               (unsigned)p->export_id, p->hooks.device_id, p->hooks.firmware,
               p->hooks.save_version, (unsigned)ASH_EMBACKUP_NVS_OFFSET,
               (unsigned)ASH_EMBACKUP_NVS_SIZE, (unsigned)p->export_crc)) {
        emit_error(p, "SNAPSHOT");
        set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        p->st = ST_IDLE;
    }
}

static void pump_export(ash_em_proto_t *p)
{
    if (p->st != ST_EXPORT_SEND || !p->recv_buf) {
        return;
    }
    if (p->export_off >= ASH_EMBACKUP_NVS_SIZE) {
        uint32_t chk = ash_em_crc32(p->recv_buf, ASH_EMBACKUP_NVS_SIZE);
        if (chk != p->export_crc) {
            emit_error(p, "CHECKSUM");
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            p->st = ST_IDLE;
            return;
        }
        if (!emitf(p, "!EMBACKUP END %u %08x", (unsigned)p->export_id,
                   (unsigned)p->export_crc)) {
            return; /* retry END next tick */
        }
        p->st = ST_EXPORT_WAIT_ACK;
        p->ack_deadline_ms = now_ms(p) + ACK_TIMEOUT_MS;
        return;
    }
    char hex[CHUNK * 2 + 1];
    bytes_to_hex(p->recv_buf + p->export_off, CHUNK, hex);
    if (!emitf(p, "!EMBACKUP DATA %u %u %s", (unsigned)p->export_id,
               (unsigned)p->export_off, hex)) {
        return; /* keep offset; retry same chunk */
    }
    p->export_off += CHUNK;
}

static void on_ack(ash_em_proto_t *p, uint32_t id, bool ok)
{
    if (p->st != ST_EXPORT_WAIT_ACK || id != p->export_id) {
        return;
    }
    if (!ok) {
        p->st = ST_IDLE;
        set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        if (p->export_for_import) {
            p->import_mode = false;
        }
        return;
    }
    if (p->export_for_import) {
        p->upload_id = p->export_id;
        p->recv_off = 0;
        memset(p->recv_buf, 0, ASH_EMBACKUP_NVS_SIZE);
        p->st = ST_IMPORT_RECV;
        p->recv_deadline_ms = now_ms(p) + RECV_IDLE_TIMEOUT_MS;
        set_ui(p, ASH_EM_UI_IMPORT_BUSY);
        (void)emitf(p, "!EMBACKUP UPLOAD %u", (unsigned)p->upload_id);
        return;
    }
    p->st = ST_IDLE;
    set_ui(p, ASH_EM_UI_BACKUP_OK);
}

static void handle_line(ash_em_proto_t *p, char *line)
{
    if (strncmp(line, "!EMBACKUP ", 10) != 0) {
        return;
    }
    char *tok[12];
    int ntok = 0;
    char *save = NULL;
    for (char *t = strtok_r(line, " ", &save); t && ntok < 12;
         t = strtok_r(NULL, " ", &save)) {
        tok[ntok++] = t;
    }
    if (ntok < 2) {
        return;
    }

    if (strcmp(tok[1], "HELLO") == 0 && ntok == 3) {
        if (strlen(tok[2]) != 32) {
            return;
        }
        memcpy(p->session, tok[2], 33);
        p->host_ok = true;
        emit_ready(p);
        return;
    }
    if (strcmp(tok[1], "PING") == 0 && ntok == 3) {
        (void)tok;
        return;
    }
    if (strcmp(tok[1], "BYE") == 0) {
        p->host_ok = false;
        p->session[0] = '\0';
        if (p->st == ST_IMPORT_WAIT_OFFER || p->st == ST_IMPORT_CONFIRM) {
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_IDLE);
        }
        return;
    }
    if (strcmp(tok[1], "ACK") == 0 && ntok == 4) {
        if (!p->host_ok || strcmp(tok[2], p->session) != 0) {
            return;
        }
        on_ack(p, (uint32_t)strtoul(tok[3], NULL, 10), true);
        return;
    }
    if (strcmp(tok[1], "FAIL") == 0 && ntok == 4) {
        if (!p->host_ok || strcmp(tok[2], p->session) != 0) {
            return;
        }
        on_ack(p, (uint32_t)strtoul(tok[3], NULL, 10), false);
        return;
    }
    if (strcmp(tok[1], "PULL") == 0 && ntok == 3) {
        if (!p->host_ok || strcmp(tok[2], p->session) != 0) {
            return;
        }
        /* One-click backup from the save manager; same path as device menu. */
        ash_em_proto_request_export(p, false);
        return;
    }
    if (strcmp(tok[1], "OFFER") == 0 && ntok == 8) {
        if (!p->host_ok || strcmp(tok[2], p->session) != 0) {
            return;
        }
        /* Accept while idle or already waiting — no need to pre-open Import. */
        if (p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK ||
            p->st == ST_IMPORT_RECV || p->st == ST_IMPORT_CONFIRM) {
            emit_error(p, "BUSY");
            return;
        }
        if (p->st != ST_IDLE && p->st != ST_IMPORT_WAIT_OFFER) {
            emit_error(p, "BUSY");
            return;
        }
        if (strlen(tok[3]) != 12 || strcmp(tok[3], p->hooks.device_id) != 0) {
            emit_error(p, "INCOMPATIBLE");
            return;
        }
        unsigned ver = (unsigned)strtoul(tok[5], NULL, 10);
        uint32_t size = (uint32_t)strtoul(tok[6], NULL, 10);
        uint32_t crc = (uint32_t)strtoul(tok[7], NULL, 16);
        if (ver < 1 || ver > 0xffffu || ver > p->hooks.save_version ||
            size != ASH_EMBACKUP_NVS_SIZE || strlen(tok[4]) != 64) {
            emit_error(p, "INCOMPATIBLE");
            return;
        }
        memcpy(p->offer_device, tok[3], 13);
        memcpy(p->offer_fw, tok[4], 65);
        p->offer_ver = ver;
        p->offer_size = size;
        p->offer_crc = crc;
        p->import_mode = true;
        p->st = ST_IMPORT_CONFIRM;
        set_ui(p, ASH_EM_UI_IMPORT_CONFIRM);
        (void)emitf(p, "!EMBACKUP OFFERED");
        return;
    }
    if (strcmp(tok[1], "RDATA") == 0 && ntok == 6) {
        if (p->st != ST_IMPORT_RECV || !p->recv_buf) {
            return;
        }
        if (strcmp(tok[2], p->session) != 0) {
            return;
        }
        uint32_t id = (uint32_t)strtoul(tok[3], NULL, 10);
        uint32_t off = (uint32_t)strtoul(tok[4], NULL, 10);
        if (id != p->upload_id || off != p->recv_off ||
            p->recv_off + CHUNK > ASH_EMBACKUP_NVS_SIZE ||
            strlen(tok[5]) != CHUNK * 2) {
            emit_error(p, "SEQUENCE");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            return;
        }
        if (!parse_hex_bytes(tok[5], CHUNK * 2, p->recv_buf + p->recv_off, CHUNK)) {
            emit_error(p, "SEQUENCE");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            return;
        }
        p->recv_off += CHUNK;
        p->recv_deadline_ms = now_ms(p) + RECV_IDLE_TIMEOUT_MS;
        (void)emitf(p, "!EMBACKUP NEXT %u %u", (unsigned)p->upload_id,
                    (unsigned)p->recv_off);
        return;
    }
    if (strcmp(tok[1], "RFIN") == 0 && ntok == 4) {
        if (p->st != ST_IMPORT_RECV || !p->recv_buf) {
            return;
        }
        if (strcmp(tok[2], p->session) != 0) {
            return;
        }
        uint32_t id = (uint32_t)strtoul(tok[3], NULL, 10);
        if (id != p->upload_id || p->recv_off != ASH_EMBACKUP_NVS_SIZE) {
            emit_error(p, "SEQUENCE");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            return;
        }
        uint32_t crc = ash_em_crc32(p->recv_buf, ASH_EMBACKUP_NVS_SIZE);
        if (crc != p->offer_crc) {
            emit_error(p, "CHECKSUM");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            return;
        }
        if (!p->hooks.commit ||
            !p->hooks.commit(p->recv_buf, ASH_EMBACKUP_NVS_SIZE, crc, p->hooks.user)) {
            (void)emitf(p, "!EMBACKUP ERROR STAGE FLASH");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            /* Reboot so we do not keep running with NVS deinitialized/half-written. */
            if (p->hooks.reboot) {
                p->hooks.reboot(p->hooks.user);
            }
            return;
        }
        (void)emitf(p, "!EMBACKUP STAGED %08x", (unsigned)crc);
        p->st = ST_IDLE;
        p->import_mode = false;
        set_ui(p, ASH_EM_UI_IMPORT_DONE_HINT);
        if (p->hooks.reboot) {
            p->hooks.reboot(p->hooks.user);
        }
    }
}

void ash_em_proto_init(ash_em_proto_t *p, const ash_em_hooks_t *hooks)
{
    memset(p, 0, sizeof(*p));
    if (hooks) {
        p->hooks = *hooks;
    }
    p->st = ST_IDLE;
    p->ui = ASH_EM_UI_IDLE;
}

void ash_em_proto_set_recv_buf(ash_em_proto_t *p, uint8_t *buf, size_t cap)
{
    p->recv_buf = buf;
    p->recv_cap = cap;
}

void ash_em_proto_set_flush(ash_em_proto_t *p, ash_em_flush_fn fn)
{
    p->hooks.flush = fn;
}

void ash_em_proto_feed(ash_em_proto_t *p, char c)
{
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        if (p->line_len > 0 && p->line_len < LINE_MAX) {
            p->line[p->line_len] = '\0';
            handle_line(p, p->line);
        }
        p->line_len = 0;
        return;
    }
    if (p->line_len + 1 < LINE_MAX) {
        p->line[p->line_len++] = c;
    } else {
        p->line_len = 0;
    }
}

void ash_em_proto_tick(ash_em_proto_t *p)
{
    if (p->st == ST_EXPORT_SEND) {
        /* A few chunks per tick; emit must not hold locks across USB waits. */
        for (int i = 0; i < 8; i++) {
            if (p->st != ST_EXPORT_SEND) {
                break;
            }
            uint32_t before = p->export_off;
            pump_export(p);
            if (p->st == ST_EXPORT_SEND && p->export_off == before) {
                break; /* TX backpressure — try again next tick */
            }
        }
    }
    if (p->st == ST_EXPORT_WAIT_ACK) {
        uint32_t t = now_ms(p);
        if ((int32_t)(t - p->ack_deadline_ms) >= 0) {
            emit_error(p, "TIMEOUT");
            p->st = ST_IDLE;
            if (p->export_for_import) {
                p->import_mode = false;
            }
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        }
    }
    if (p->st == ST_IMPORT_RECV) {
        uint32_t t = now_ms(p);
        if ((int32_t)(t - p->recv_deadline_ms) >= 0) {
            emit_error(p, "TIMEOUT");
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
        }
    }
}

void ash_em_proto_request_export(ash_em_proto_t *p, bool for_import_preamble)
{
    if (ash_em_proto_busy(p)) {
        emit_error(p, "BUSY");
        return;
    }
    if (!p->host_ok) {
        emit_error(p, "NO_HOST");
        set_ui(p, ASH_EM_UI_NO_HOST);
        return;
    }
    p->export_for_import = for_import_preamble;
    begin_export(p);
}

void ash_em_proto_set_import_mode(ash_em_proto_t *p, bool on)
{
    if (on) {
        /* Already in import / transfer: ignore re-open (no BUSY spam). */
        if (p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK ||
            p->st == ST_IMPORT_RECV || p->st == ST_IMPORT_WAIT_OFFER ||
            p->st == ST_IMPORT_CONFIRM) {
            return;
        }
        if (!p->host_ok) {
            emit_error(p, "NO_HOST");
            set_ui(p, ASH_EM_UI_NO_HOST);
            return;
        }
        p->import_mode = true;
        p->st = ST_IMPORT_WAIT_OFFER;
        set_ui(p, ASH_EM_UI_IMPORT_WAIT);
        (void)emitf(p, "!EMBACKUP HINT 可在管家选择文件发送");
    } else {
        if (p->st == ST_IMPORT_CONFIRM) {
            (void)emitf(p, "!EMBACKUP CANCELLED");
        }
        if (p->st == ST_IMPORT_CONFIRM || p->st == ST_IMPORT_WAIT_OFFER) {
            p->st = ST_IDLE;
            p->import_mode = false;
            set_ui(p, ASH_EM_UI_IDLE);
            return;
        }
        /* Mid-transfer leave is blocked in UI; keep as a safety net. */
        if ((p->export_for_import &&
             (p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK)) ||
            p->st == ST_IMPORT_RECV) {
            emit_error(p, "TIMEOUT");
            p->st = ST_IDLE;
            p->import_mode = false;
            p->export_for_import = false;
            set_ui(p, ASH_EM_UI_BACKUP_FAIL);
            return;
        }
        p->import_mode = false;
        if (p->ui == ASH_EM_UI_IMPORT_WAIT || p->ui == ASH_EM_UI_IMPORT_CONFIRM) {
            set_ui(p, ASH_EM_UI_IDLE);
        }
    }
}

void ash_em_proto_confirm_import(ash_em_proto_t *p, bool accept)
{
    if (p->st != ST_IMPORT_CONFIRM) {
        return;
    }
    if (!accept) {
        (void)emitf(p, "!EMBACKUP CANCELLED");
        p->st = ST_IDLE;
        p->import_mode = false;
        set_ui(p, ASH_EM_UI_IDLE);
        return;
    }
    p->export_for_import = true;
    begin_export(p);
}

bool ash_em_proto_host_ok(const ash_em_proto_t *p)
{
    return p->host_ok;
}

bool ash_em_proto_busy(const ash_em_proto_t *p)
{
    return p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK ||
           p->st == ST_IMPORT_RECV;
}

ash_em_ui_t ash_em_proto_ui(const ash_em_proto_t *p)
{
    return p->ui;
}

int ash_em_proto_progress_pct(const ash_em_proto_t *p)
{
    if (p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK) {
        if (p->st == ST_EXPORT_WAIT_ACK) {
            return 100;
        }
        return (int)((p->export_off * 100u) / ASH_EMBACKUP_NVS_SIZE);
    }
    if (p->st == ST_IMPORT_RECV) {
        return (int)((p->recv_off * 100u) / ASH_EMBACKUP_NVS_SIZE);
    }
    return -1;
}

bool ash_em_proto_import_preamble(const ash_em_proto_t *p)
{
    return p->export_for_import &&
           (p->st == ST_EXPORT_SEND || p->st == ST_EXPORT_WAIT_ACK);
}

void ash_em_proto_clear_ui_pulse(ash_em_proto_t *p)
{
    if (p->ui == ASH_EM_UI_BACKUP_OK || p->ui == ASH_EM_UI_BACKUP_FAIL ||
        p->ui == ASH_EM_UI_NO_HOST || p->ui == ASH_EM_UI_IMPORT_DONE_HINT) {
        p->ui = ASH_EM_UI_IDLE;
    }
}

void ash_em_proto_set_pending_restored(ash_em_proto_t *p, unsigned result, uint32_t crc)
{
    p->pending_restored = true;
    p->pending_restored_result = result;
    p->pending_restored_crc = crc;
}
