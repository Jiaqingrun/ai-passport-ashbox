#include "ashbox_sfx.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "ash_sfx";

#define ASH_SFX_RATE     16000
#define ASH_SFX_CHUNK    256
#define ASH_SFX_Q_LEN    8

/*
 * 小喇叭几乎放不出 <250Hz；过尖易破音。
 * v10：壁炉床 = 真实木柴「瞬态门」PCM——只留噼啪，间隙近静音。
 * 全段连续火声嘶嘶在小喇叭上会被听成收音机底噪，故不再整段直通。
 */
#define RADIO_EVENT_MIN_S      (90u * ASH_SFX_RATE)
#define RADIO_EVENT_SPAN_S     (120u * ASH_SFX_RATE)
#define RADIO_EVENT_FIRST_MIN  (45u * ASH_SFX_RATE)
#define RADIO_EVENT_FIRST_SPAN (30u * ASH_SFX_RATE)
#define RADIO_EVENT_KINDS      5u
#define HEARTH_BED_GAIN        155

typedef struct {
    ash_sfx_id_t id;
} ash_sfx_evt_t;

typedef struct {
    uint16_t freq_hz;
    uint16_t ms;
    uint8_t  vol;
} ash_sfx_note_t;

typedef enum {
    RADIO_EVT_NONE = 0,
    RADIO_EVT_SCREAM_F,
    RADIO_EVT_SCREAM_M,
    RADIO_EVT_HELP,
    RADIO_EVT_SEARCH,
    RADIO_EVT_LAUGH,
} radio_evt_t;

static QueueHandle_t s_q;
static TaskHandle_t s_task;
static bool s_enabled = true;
static bool s_radio = true;
static uint8_t s_master = 80;
static uint32_t s_noise_seed = 0xA5C3E791u;
static uint32_t s_phase;
static int32_t s_tone_lpf;

static uint32_t s_radio_samples;
static uint32_t s_next_event_at;
static uint32_t s_hearth_pos;
static radio_evt_t s_evt;
static uint32_t s_evt_pos;
static uint32_t s_evt_len;
static bool s_radio_had_event;

/* 嵌入的真人录音：mono s16le @16kHz（CMake target_add_binary_data） */
extern const uint8_t radio_scream_f_pcm_start[] asm("_binary_radio_scream_f_pcm_start");
extern const uint8_t radio_scream_f_pcm_end[] asm("_binary_radio_scream_f_pcm_end");
extern const uint8_t radio_scream_m_pcm_start[] asm("_binary_radio_scream_m_pcm_start");
extern const uint8_t radio_scream_m_pcm_end[] asm("_binary_radio_scream_m_pcm_end");
extern const uint8_t radio_help_pcm_start[] asm("_binary_radio_help_pcm_start");
extern const uint8_t radio_help_pcm_end[] asm("_binary_radio_help_pcm_end");
extern const uint8_t radio_search_pcm_start[] asm("_binary_radio_search_pcm_start");
extern const uint8_t radio_search_pcm_end[] asm("_binary_radio_search_pcm_end");
extern const uint8_t radio_laugh_pcm_start[] asm("_binary_radio_laugh_pcm_start");
extern const uint8_t radio_laugh_pcm_end[] asm("_binary_radio_laugh_pcm_end");
extern const uint8_t hearth_fire_pcm_start[] asm("_binary_hearth_fire_pcm_start");
extern const uint8_t hearth_fire_pcm_end[] asm("_binary_hearth_fire_pcm_end");

static const uint8_t *s_samp_bytes; /* 勿直接强转 int16*：binary 嵌入常落在奇数地址 */
static uint32_t s_samp_n;
static uint8_t s_samp_gain; /* 额外增益，补偿部分素材响度 */

static uint32_t noise_u32(void)
{
    s_noise_seed ^= s_noise_seed << 13;
    s_noise_seed ^= s_noise_seed >> 17;
    s_noise_seed ^= s_noise_seed << 5;
    return s_noise_seed;
}

static int16_t noise_samp(void)
{
    return (int16_t)(((int32_t)(noise_u32() & 0xFFFFu) - 32768) / 48);
}

static int16_t soft_osc(uint32_t phase)
{
    /* phase 为 16.16 定点数：低 16 位是波形相位。
     * 旧代码误用 phase>>16，步进 (freq<<16)/rate 时整段 UI 音几乎是直流包络。 */
    uint32_t p = phase & 0xFFFFu;
    int32_t x = (p < 0x8000u) ? ((int32_t)p - 16384) : (49152 - (int32_t)p);
    int32_t n = x / 64;
    int32_t y = n - (n * n * n) / (3 * 256 * 256);
    return (int16_t)(y * 64);
}

static int16_t clamp16(int32_t v, int32_t lim)
{
    if (v > lim) return (int16_t)lim;
    if (v < -lim) return (int16_t)-lim;
    return (int16_t)v;
}

/* gain: master*vol → 满档约 10000；除数越小越响 */
static int32_t tone_gain(uint8_t vol)
{
    int32_t g = (int32_t)s_master * (int32_t)vol;
    if (g < 800) g = 800;
    return g;
}

/* 可听短音：500Hz+，接近满幅，轻低通防刺耳 */
static void write_tone_heavy(uint16_t freq_hz, uint16_t ms, uint8_t vol)
{
    if (!s_enabled || freq_hz == 0 || ms == 0) return;
    if (freq_hz < 360) freq_hz = 360;
    const uint32_t total = (uint32_t)ASH_SFX_RATE * ms / 1000;
    const uint32_t step = ((uint32_t)freq_hz << 16) / ASH_SFX_RATE;
    const int32_t gain = tone_gain(vol);
    const uint32_t atk = ASH_SFX_RATE * 4 / 1000;
    const uint32_t rel = ASH_SFX_RATE * 28 / 1000;
    int16_t buf[ASH_SFX_CHUNK];
    uint32_t done = 0;
    while (done < total) {
        uint32_t n = total - done;
        if (n > ASH_SFX_CHUNK) n = ASH_SFX_CHUNK;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t t = i + done;
            int32_t env = 256;
            if (t < atk) env = (int32_t)(t * 256 / (atk ? atk : 1));
            else if (total - t < rel) env = (int32_t)((total - t) * 256 / (rel ? rel : 1));
            int32_t s0 = (int32_t)soft_osc(s_phase);
            int32_t s1 = (int32_t)soft_osc(s_phase * 2u) / 5;
            s_phase += step;
            /* soft_osc≈±16k；gain/4500 在 master60/vol95 时接近满幅 */
            int32_t raw = (s0 + s1) * gain / 4500 * env / 256;
            s_tone_lpf += (raw - s_tone_lpf) / 4;
            buf[i] = clamp16(s_tone_lpf, 22000);
        }
        (void)bsp_audio_write(buf, n * sizeof(int16_t));
        done += n;
    }
}

/* 短促咔嗒：600Hz 主峰，明显比旧版沉闷叩击响 */
static void write_thump(uint16_t ms, uint8_t vol)
{
    if (!s_enabled || ms == 0) return;
    const uint32_t total = (uint32_t)ASH_SFX_RATE * ms / 1000;
    const uint32_t step = (620u << 16) / ASH_SFX_RATE;
    const int32_t gain = tone_gain(vol);
    int16_t buf[ASH_SFX_CHUNK];
    uint32_t done = 0;
    uint32_t ph = s_phase;
    while (done < total) {
        uint32_t n = total - done;
        if (n > ASH_SFX_CHUNK) n = ASH_SFX_CHUNK;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t t = i + done;
            int32_t env = (int32_t)((total - t) * 256 / (total ? total : 1));
            env = (env * env) / 256;
            int32_t tone = (int32_t)soft_osc(ph);
            ph += step;
            int32_t nse = (int32_t)noise_samp();
            int32_t raw = (tone * 7 / 8 + nse / 4) * gain / 4200 * env / 256;
            s_tone_lpf += (raw - s_tone_lpf) / 3;
            buf[i] = clamp16(s_tone_lpf, 24000);
        }
        (void)bsp_audio_write(buf, n * sizeof(int16_t));
        done += n;
    }
    s_phase = ph;
}

static void play_notes_heavy(const ash_sfx_note_t *notes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        write_tone_heavy(notes[i].freq_hz, notes[i].ms, notes[i].vol);
        if (i + 1 < count) vTaskDelay(pdMS_TO_TICKS(12));
    }
}

static void play_id(ash_sfx_id_t id)
{
    s_tone_lpf = 0;
    switch (id) {
    case ASH_SFX_CLICK:
        write_thump(55, 100);
        break;
    case ASH_SFX_MOVE: {
        const ash_sfx_note_t n[] = {{520, 36, 100}, {680, 44, 100}};
        write_thump(32, 90);
        play_notes_heavy(n, 2);
        break;
    }
    case ASH_SFX_CONFIRM: {
        const ash_sfx_note_t n[] = {{480, 50, 100}, {600, 60, 100}, {760, 80, 100}};
        write_thump(36, 95);
        play_notes_heavy(n, 3);
        break;
    }
    case ASH_SFX_BACK: {
        const ash_sfx_note_t n[] = {{700, 45, 95}, {520, 70, 90}};
        play_notes_heavy(n, 2);
        break;
    }
    case ASH_SFX_DEPART: {
        const ash_sfx_note_t n[] = {{440, 80, 100}, {560, 90, 100}, {720, 120, 100}};
        write_thump(48, 95);
        play_notes_heavy(n, 3);
        break;
    }
    case ASH_SFX_REWARD: {
        /* 完成回波：中频双声呐，烂喇叭也能听清「叮—咚」 */
        const ash_sfx_note_t n[] = {{620, 70, 100}, {780, 120, 100}};
        write_thump(28, 85);
        play_notes_heavy(n, 2);
        break;
    }
    case ASH_SFX_WARN: {
        /* 警报：中低频双拍，避免尖啸 */
        const ash_sfx_note_t n[] = {{540, 90, 100}, {460, 130, 100}};
        write_thump(40, 100);
        play_notes_heavy(n, 2);
        break;
    }
    case ASH_SFX_SLEEP: {
        const ash_sfx_note_t n[] = {{560, 100, 85}, {440, 130, 75}, {360, 170, 65}};
        play_notes_heavy(n, 3);
        break;
    }
    case ASH_SFX_BELL: {
        const ash_sfx_note_t n[] = {{640, 70, 100}, {520, 110, 95}};
        write_thump(30, 90);
        play_notes_heavy(n, 2);
        break;
    }
    default:
        break;
    }
}

static void schedule_next_radio_event(void)
{
    uint32_t min_s = s_radio_had_event ? RADIO_EVENT_MIN_S : RADIO_EVENT_FIRST_MIN;
    uint32_t span = s_radio_had_event ? RADIO_EVENT_SPAN_S : RADIO_EVENT_FIRST_SPAN;
    uint32_t jitter = noise_u32() % (span ? span : 1u);
    s_next_event_at = s_radio_samples + min_s + jitter;
}

static void bind_sample(const uint8_t *start, const uint8_t *end, uint8_t gain)
{
    size_t bytes = (size_t)(end - start);
    s_samp_bytes = start;
    s_samp_n = (uint32_t)(bytes / sizeof(int16_t));
    s_samp_gain = gain ? gain : 100;
    s_evt_pos = 0;
    s_evt_len = s_samp_n;
}

/* 小端 s16：按字节拼装，兼容未对齐 Flash 嵌入 */
static int16_t le_s16_at(const uint8_t *base, uint32_t i)
{
    const uint8_t *p = base + (size_t)i * 2u;
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t samp_at(uint32_t i)
{
    return le_s16_at(s_samp_bytes, i);
}

static uint32_t hearth_sample_count(void)
{
    size_t bytes = (size_t)(hearth_fire_pcm_end - hearth_fire_pcm_start);
    return (uint32_t)(bytes / sizeof(int16_t));
}

static int32_t sample_hearth_bed(void)
{
    uint32_t n = hearth_sample_count();
    if (n == 0) return 0;
    if (s_hearth_pos >= n) s_hearth_pos = 0;
    int32_t raw = (int32_t)le_s16_at(hearth_fire_pcm_start, s_hearth_pos);
    s_hearth_pos++;
    /* 首尾交叉淡化，减轻 12s 循环接点 */
    uint32_t fade = ASH_SFX_RATE / 8; /* 125ms */
    int32_t env = 256;
    if (s_hearth_pos < fade) {
        env = (int32_t)(s_hearth_pos * 256 / fade);
    } else if (n - s_hearth_pos < fade) {
        env = (int32_t)((n - s_hearth_pos) * 256 / fade);
    }
    return raw * HEARTH_BED_GAIN / 100 * env / 256;
}

static void start_radio_event(void)
{
    uint32_t kind = (noise_u32() % RADIO_EVENT_KINDS) + 1u;
    s_evt = (radio_evt_t)kind;
    switch (s_evt) {
    case RADIO_EVT_SCREAM_F:
        /* 素材峰值偏低，抬增益 */
        bind_sample(radio_scream_f_pcm_start, radio_scream_f_pcm_end, 220);
        break;
    case RADIO_EVT_SCREAM_M:
        bind_sample(radio_scream_m_pcm_start, radio_scream_m_pcm_end, 110);
        break;
    case RADIO_EVT_HELP:
        bind_sample(radio_help_pcm_start, radio_help_pcm_end, 220);
        break;
    case RADIO_EVT_SEARCH:
        bind_sample(radio_search_pcm_start, radio_search_pcm_end, 120);
        break;
    case RADIO_EVT_LAUGH:
        bind_sample(radio_laugh_pcm_start, radio_laugh_pcm_end, 110);
        break;
    default:
        s_evt = RADIO_EVT_NONE;
        s_samp_bytes = NULL;
        s_samp_n = 0;
        s_evt_len = 0;
        break;
    }
}

static int32_t sample_radio_voice(void)
{
    if (!s_samp_bytes || s_evt_pos >= s_samp_n) return 0;
    int32_t raw = (int32_t)samp_at(s_evt_pos);
    s_evt_pos++;
    /* 首尾淡入淡出，避免咔哒 */
    uint32_t fade = ASH_SFX_RATE / 40; /* 25ms */
    int32_t env = 256;
    if (s_evt_pos < fade) env = (int32_t)(s_evt_pos * 256 / fade);
    else if (s_samp_n - s_evt_pos < fade) env = (int32_t)((s_samp_n - s_evt_pos) * 256 / fade);
    raw = raw * (int32_t)s_samp_gain / 100 * env / 256;
    /* 壁炉场景不再叠 noise_samp「静电感」——那正是收音机底噪感的来源 */
    return raw;
}

static void write_radio_chunk(void)
{
    if (!s_radio || !s_enabled) return;
    if (s_next_event_at == 0) schedule_next_radio_event();

    int16_t buf[ASH_SFX_CHUNK];
    const int32_t amp = (int32_t)s_master;
    if (amp <= 0) return;

    for (int i = 0; i < ASH_SFX_CHUNK; i++) {
        uint32_t abs_i = s_radio_samples + (uint32_t)i;
        int32_t bed = sample_hearth_bed();

        int32_t voice = 0;
        if (s_evt != RADIO_EVT_NONE) {
            voice = sample_radio_voice();
            if (s_evt_pos >= s_evt_len) {
                s_evt = RADIO_EVT_NONE;
                s_evt_pos = 0;
                s_samp_bytes = NULL;
                s_radio_had_event = true;
                schedule_next_radio_event();
            }
        } else if (abs_i >= s_next_event_at) {
            start_radio_event();
            if (s_evt != RADIO_EVT_NONE) voice = sample_radio_voice();
        }

        /* 人声事件时压低壁炉，避免盖过求救/笑声 */
        int32_t bed_scale = (s_evt != RADIO_EVT_NONE) ? 18 : 55;
        int32_t voice_scale = (s_evt != RADIO_EVT_NONE) ? 140 : 0;
        int32_t v = (bed * bed_scale / 100 + voice * voice_scale / 100) * amp / 75;
        buf[i] = clamp16(v, 22000);
    }

    s_radio_samples += ASH_SFX_CHUNK;
    (void)bsp_audio_write(buf, sizeof(buf));
}

static void apply_stop_locked(void)
{
    s_radio = false;
    s_enabled = false;
    s_evt = RADIO_EVT_NONE;
    s_evt_pos = 0;
    s_samp_bytes = NULL;
    s_samp_n = 0;
}

static void sfx_task(void *arg)
{
    (void)arg;
    ash_sfx_evt_t ev;
    for (;;) {
        if (xQueueReceive(s_q, &ev, pdMS_TO_TICKS(18)) == pdTRUE) {
            if (ev.id == ASH_SFX_CMD_STOP) {
                apply_stop_locked();
                continue;
            }
            if (!s_enabled) continue;
            play_id(ev.id);
        } else if (s_radio && s_enabled) {
            write_radio_chunk();
        } else {
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
}

esp_err_t ashbox_sfx_init(void)
{
    if (s_q) return ESP_OK;
    s_q = xQueueCreate(ASH_SFX_Q_LEN, sizeof(ash_sfx_evt_t));
    if (!s_q) return ESP_ERR_NO_MEM;
    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "audio init %s", esp_err_to_name(err));
        return err;
    }
    (void)bsp_audio_set_format(ASH_SFX_RATE, 16, 1);
    bsp_audio_set_volume(90);
    s_next_event_at = 0;
    s_hearth_pos = 0;
    s_evt = RADIO_EVT_NONE;
    s_radio_had_event = false;
    BaseType_t ok = xTaskCreate(sfx_task, "ash_sfx", 4608, NULL, 4, &s_task);
    if (ok != pdPASS) return ESP_ERR_NO_MEM;
    /* 开机三连音：一听就知道新固件音效已上电 */
    ash_sfx_evt_t boot = {.id = ASH_SFX_CONFIRM};
    (void)xQueueSend(s_q, &boot, 0);
    ESP_LOGI(TAG, "sfx ready v10 hearth-sparse (%u samples)", (unsigned)hearth_sample_count());
    return ESP_OK;
}

void ashbox_sfx_set_enabled(bool on)
{
    s_enabled = on;
}

void ashbox_sfx_set_radio(bool on)
{
    s_radio = on;
    if (!on) {
        s_evt = RADIO_EVT_NONE;
        s_evt_pos = 0;
        s_samp_bytes = NULL;
    } else if (s_next_event_at == 0 || s_next_event_at < s_radio_samples) {
        schedule_next_radio_event();
    }
}

void ashbox_sfx_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_master = percent == 0 ? 0 : (percent < 40 ? 50 : percent);
    bsp_audio_set_volume(percent == 0 ? 0 : (percent < 40 ? 55 : (percent < 70 ? 80 : 95)));
}

bool ashbox_sfx_enabled(void)
{
    return s_enabled;
}

bool ashbox_sfx_radio_on(void)
{
    return s_radio;
}

void ashbox_sfx_play(ash_sfx_id_t id)
{
    if (!s_q || !s_enabled) return;
    ash_sfx_evt_t ev = {.id = id};
    (void)xQueueSend(s_q, &ev, 0);
}

void ashbox_sfx_stop_all(void)
{
    /* Soft mute only. bsp_audio_prepare_deep_sleep() is irreversible in this
       run (I2S pins go high-Z); ashbox_enter_deep_sleep() owns that step
       after all refusal checks pass. Calling it here left the app silent
       forever when deep sleep was refused (e.g. key still held after long-press).
       Clear sample pointers on the sfx task (C-05), not racing write_radio_chunk. */
    if (!s_q) {
        apply_stop_locked();
        return;
    }
    xQueueReset(s_q);
    ash_sfx_evt_t stop = {.id = ASH_SFX_CMD_STOP};
    (void)xQueueSend(s_q, &stop, 0);
    /* Let sfx_task drain STOP before caller continues toward deep sleep. */
    vTaskDelay(pdMS_TO_TICKS(50));
}
