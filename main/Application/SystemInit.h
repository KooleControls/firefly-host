#pragma once

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_err.h"

namespace SystemInit
{
    inline void InitNvs()
    {
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    inline void InitNetworkStack()
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
    }

    inline void InitNtp()
    {
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_init();
    }
}
