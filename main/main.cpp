#include <stdio.h>
#include "esp_log.h"
#include "AppContext.h"
#include "nvs_flash.h"
#include "esp_netif.h"


constexpr char *TAG = "Main";

AppContext appContext;

extern "C" void app_main(void)
{
    // Setup NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Setup the network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());



    // Initialize the application context
    appContext.Init();

}





