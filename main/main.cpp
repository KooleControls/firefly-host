#include <stdio.h>
#include "esp_log.h"
#include "AppContext.h"
#include "nvs_flash.h"
#include "esp_netif.h"


constexpr char *TAG = "Main";

AppContext appContext;

extern "C" void app_main(void)
{
    // Initialize the application context
    appContext.Init();

    // Enter the main application loop
    while(1) {
        appContext.Tick();
    }
}







