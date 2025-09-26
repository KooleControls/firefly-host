#pragma once
#include "InitGuard.h"
#include "HardwareManager.h"
#include "FtpManager.h"
#include "WebManager.h"
#include "GuestManager.h"
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
        SystemInit::InitNtp();
        ftpManager.init();
        guestManager.init();
        espnowManager.init();
        webManager.init();

        initGuard.SetReady();
    }

    void Main()
    {

    }

private:

    InitGuard initGuard;

    HardwareManager hardwareManager;
    FtpManager ftpManager;
    GuestManager guestManager;

    EspNowManager espnowManager{guestManager};
    WebManager webManager{guestManager};


};





