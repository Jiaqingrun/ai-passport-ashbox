#include "ashbox_runtime.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ash_rt";
static SemaphoreHandle_t s_mu;

void ashbox_runtime_init(void)
{
    if (s_mu) return;
    s_mu = xSemaphoreCreateMutex();
    if (!s_mu) {
        ESP_LOGE(TAG, "runtime mutex alloc failed");
    }
}

void ashbox_runtime_lock(void)
{
    if (!s_mu) return;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
}

void ashbox_runtime_unlock(void)
{
    if (!s_mu) return;
    (void)xSemaphoreGive(s_mu);
}
