#pragma once
#include "EspNow.h"
#include "rtos.h"
#include "InitGuard.h"
#include "GuestManager.h"

class EspNowManager
{
    static constexpr uint8_t BROADCAST[ESP_NOW_ETH_ALEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

public:
    EspNowManager(GuestManager &guestMgr) : guestManager(guestMgr) {}
    ~EspNowManager() = default;

    void init()
    {
        if (initGuard.IsReady())
            return;

        LOCK(mutex);

        task.Init("EspNow", 7, 1024 * 4);
        task.SetHandler([this]()
                        { work(); });

        espnow.init();
        task.Run();
        initGuard.SetReady();
    }

private:
    GuestManager &guestManager;
    InitGuard initGuard;
    Mutex mutex;
    EspNow espnow;
    Task task;

    void work()
    {
        EspNow::Package rxPackage;
        while (1)
        {
            if (espnow.Receive(rxPackage, portMAX_DELAY))
            {
                processReceivedPackage(rxPackage);
            }
        }
    }

    // --- Command routing types ---
    enum class MatchFlags : uint8_t
    {
        None = 0,
        Broadcast = 1 << 0,
        ForMe = 1 << 1,
        OnlyForMe = 1 << 2,
        Any = Broadcast | ForMe | OnlyForMe
    };

    friend constexpr MatchFlags operator|(MatchFlags a, MatchFlags b)
    {
        return static_cast<MatchFlags>(
            static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    friend constexpr bool operator&(MatchFlags a, MatchFlags b)
    {
        return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
    }

    struct CommandRoute
    {
        char commandId[EspNow::COMMAND_SIZE + 1];
        MatchFlags flags;
        void (EspNowManager::*handler)(const EspNow::Package &package);
    };

    // --- Handlers ---
    void handleGuestDiscovery(const EspNow::Package &package)
    {
        guestManager.ReportGuest(package.source, 0);
        EspNow::Package response = EspNow::Package::Make(package.source, "RDSC");
        espnow.Send(response);
    }

    void handleGuestButton(const EspNow::Package &package)
    {
        uint32_t buttonPresses = 0;
        if (!package.GetData(buttonPresses))
        {
            ESP_LOGW("EspNowManager", "Invalid button press data size: %u", package.dataSize);
            return;
        }
        guestManager.ReportGuest(package.source, buttonPresses);
    }

    constexpr static CommandRoute commandRoutes[] = {
        {"CDSC", MatchFlags::Broadcast, &EspNowManager::handleGuestDiscovery},
        {"CBUT", MatchFlags::OnlyForMe, &EspNowManager::handleGuestButton},
    };

    // --- Dispatch ---
    void processReceivedPackage(const EspNow::Package &package)
    {
        LOCK(mutex);
        for (auto &entry : commandRoutes)
        {
            if (strncmp(package.commandId, entry.commandId, EspNow::COMMAND_SIZE) != 0)
                continue;

            if (!matchesFlags(package, entry.flags))
                break;

            (this->*entry.handler)(package);
            return;
        }
        ESP_LOGW("EspNowManager", "Unknown command: %.4s", package.commandId);
    }

    static bool matchesFlags(const EspNow::Package &pkg, MatchFlags flags)
    {
        if ((flags & MatchFlags::Broadcast) && pkg.isBroadcast)
            return true;
        if ((flags & MatchFlags::ForMe) && pkg.isForMe)
            return true;
        if ((flags & MatchFlags::OnlyForMe) && pkg.isOnlyForMe())
            return true;
        return false;
    }
};
