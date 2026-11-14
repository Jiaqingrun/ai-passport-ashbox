#include "ashbox_digest.h"

bool ash_digest_calc(const ash_digest_snap_t *snap, int64_t now, uint16_t food,
                     uint16_t parts, uint16_t iron, uint16_t water, uint16_t ember,
                     uint8_t threat, ash_digest_delta_t *out)
{
    if (!snap || !out) {
        return false;
    }
    if (snap->magic != ASH_DIGEST_MAGIC || snap->ver != ASH_DIGEST_VER ||
        snap->bytes != (uint16_t)sizeof(*snap)) {
        return false;
    }
    /* Any positive clock is usable: devices without a set RTC run on
     * epoch+uptime, and the game treats that timeline as valid too. */
    if (snap->unix <= 0 || now < snap->unix) {
        return false;
    }
    int64_t away = now - snap->unix;
    if (away < ASH_DIGEST_MIN_AWAY_S || away > ASH_DIGEST_MAX_AWAY_S) {
        return false;
    }
    out->away_s = away;
    /* Resources can fall while away (food eaten); keep the signed net. */
    out->food = (int32_t)food - (int32_t)snap->food;
    out->parts = (int32_t)parts - (int32_t)snap->parts;
    out->iron = (int32_t)iron - (int32_t)snap->iron;
    out->water = (int32_t)water - (int32_t)snap->water;
    out->ember = (int32_t)ember - (int32_t)snap->ember;
    out->threat = (int16_t)threat - (int16_t)snap->threat;
    return true;
}
