#pragma once
#include "InitGuard.h"
#include "HardwareManager.h"
#include "FtpManager.h"
#include "WebManager.h"
#include "SystemInit.h"
#include "EspnowManager.h"

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
        ftpManager.init();
        espNowManager.Init();
        webManager.init();

       // hardwareManager.GetEthernetDriver().SetStaticIp("192.168.1.50", "192.168.1.1", "255.255.255.0");

        initGuard.SetReady();
    }

    void Tick()
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

private:

    InitGuard initGuard;

    HardwareManager hardwareManager;
    FtpManager ftpManager;

    EspNowManager espNowManager;
    WebManager webManager {espNowManager};


};





