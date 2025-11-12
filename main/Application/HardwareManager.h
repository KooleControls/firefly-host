#pragma once
#include "esp_log.h"
#include "InitGuard.h"
#include "WifiDriver.h"
#include "EthernetDriver.h"
#include "FatFsDriver.h"
#include "SystemInit.h"

class HardwareManager {
    constexpr static const char* TAG = "HardwareManager";

public:
    HardwareManager() = default;
    ~HardwareManager() = default;
    HardwareManager(const HardwareManager &) = delete;
    HardwareManager &operator=(const HardwareManager &) = delete;

    void init()
    {
        if(initGuard.IsReady())
            return;
            
        wifiDriver.Init();
        ethernetDriver.Init();
        fatFsDriver.Init();

        initGuard.SetReady();
    }

    WifiDriver& GetWifiDriver() { 
        REQUIRE_READY(initGuard);
        return wifiDriver; 
    }
    EthernetDriver& GetEthernetDriver() { 
        REQUIRE_READY(initGuard);
        return ethernetDriver; 
    }

private:
    InitGuard initGuard;
    WifiDriver wifiDriver;
    EthernetDriver ethernetDriver;
    FatfsDriver fatFsDriver{"/fat", "fat"};

};
