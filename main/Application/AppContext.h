#pragma once
#include "InitGuard.h"
#include "HardwareManager.h"
#include "FtpManager.h"
#include "WebManager.h"
#include "GuestManager.h"
#include "esp_sntp.h"

class AppContext
{
public:
    AppContext() = default;
    ~AppContext() = default;
    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

    void Init()
    {
        if(initGuard.IsReady())
            return;

        hardwareManager.init();

        // Setup NTP
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        sntp_init();

        ftpManager.init();
        guestManager.init();

        webManager.init();

        initGuard.SetReady();
    }



private:

    InitGuard initGuard;

    HardwareManager hardwareManager;
    FtpManager ftpManager;
    GuestManager guestManager;

    WebManager webManager{guestManager};


    static void time_sync_notification_cb(struct timeval *tv)
    {
        ESP_LOGI("AppContext", "Time synchronized");
    }

};





