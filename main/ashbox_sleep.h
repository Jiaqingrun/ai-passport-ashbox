#pragma once

#include "esp_err.h"
#include <stdint.h>

/* Enter deep sleep with GPIO key wake + optional RTC alarm (seconds, 0=GPIO only). */
esp_err_t ashbox_enter_deep_sleep(uint32_t rtc_alarm_s);
