#pragma once
#include "InitGuard.h"
#include "HardwareManager.h"

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

        initGuard.SetReady();
    }

    

private:

    InitGuard initGuard;

    HardwareManager hardwareManager;

};





