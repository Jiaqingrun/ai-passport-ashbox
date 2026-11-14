#pragma once

#include <stdint.h>

/* M1 ambience: pure day-phase / tint / threat-vignette / ember-density rules.
 * Kept free of LVGL so the host tests can cover the numbers. */

typedef enum {
    ASH_AMB_DAWN = 0,
    ASH_AMB_DAY,
    ASH_AMB_DUSK,
    ASH_AMB_NIGHT,
} ash_amb_phase_t;

typedef struct {
    uint8_t phase;      /* ash_amb_phase_t */
    uint32_t tint_rgb;  /* 0xRRGGBB overlay colour */
    uint8_t tint_opa;   /* 0..255 */
    uint8_t ember_count;/* active rising-ember objects */
    uint8_t vignette;   /* red edge opacity, 0 = off */
} ash_ambience_t;

/** hour may be any int (normalised to 0..23); threat_pct clamps to 0..100. */
void ash_ambience_for(int hour, int threat_pct, int ember, ash_ambience_t *out);

/** Short player-facing day-phase word. Never NULL. */
const char *ash_ambience_phase_name(uint8_t phase);
