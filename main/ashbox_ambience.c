#include "ashbox_ambience.h"

#include <stddef.h>

typedef struct {
    uint32_t rgb;
    uint8_t opa;
} tint_t;

/* Four windows; overlays stay translucent so the pixel art keeps its contrast. */
static const tint_t k_tint[4] = {
    [ASH_AMB_DAWN] = {0xFF9A5A, 50},
    [ASH_AMB_DAY] = {0xFFE0B0, 8},
    [ASH_AMB_DUSK] = {0xE06028, 60},
    [ASH_AMB_NIGHT] = {0x101840, 95},
};

static const char *const k_phase_name[4] = {"天亮", "白天", "晚", "夜里"};

void ash_ambience_for(int hour, int threat_pct, int ember, ash_ambience_t *out)
{
    if (!out) {
        return;
    }
    hour %= 24;
    if (hour < 0) {
        hour += 24;
    }
    uint8_t phase;
    if (hour >= 5 && hour < 7) {
        phase = ASH_AMB_DAWN;
    } else if (hour >= 7 && hour < 17) {
        phase = ASH_AMB_DAY;
    } else if (hour >= 17 && hour < 20) {
        phase = ASH_AMB_DUSK;
    } else {
        phase = ASH_AMB_NIGHT;
    }

    if (threat_pct < 0) threat_pct = 0;
    if (threat_pct > 100) threat_pct = 100;
    uint8_t vignette = 0;
    if (threat_pct >= 80) {
        vignette = 90;
    } else if (threat_pct >= 50) {
        vignette = 55;
    }

    int count = 10;
    if (ember <= 0) {
        count = 6;
    } else if (ember >= 10) {
        count = 14;
    }

    out->phase = phase;
    out->tint_rgb = k_tint[phase].rgb;
    out->tint_opa = k_tint[phase].opa;
    out->ember_count = (uint8_t)count;
    out->vignette = vignette;
}

const char *ash_ambience_phase_name(uint8_t phase)
{
    if (phase > ASH_AMB_NIGHT) {
        phase = ASH_AMB_NIGHT;
    }
    return k_phase_name[phase];
}
