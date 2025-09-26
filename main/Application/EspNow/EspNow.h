#pragma once
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "rtos.h"
#include "InitGuard.h"
#include <string.h>
#include <algorithm>

class EspNow
{
    static constexpr const char *TAG = "EspNow";

public:
    static constexpr size_t COMMAND_SIZE = 4;               // raw bytes in frame
    static constexpr size_t MAX_DATA_SIZE = 16;             // payload bytes

private:
    struct Frame
    {
        uint8_t destination[ESP_NOW_ETH_ALEN];
        uint8_t commandId[COMMAND_SIZE];   // raw bytes, no null terminator
        uint8_t data[MAX_DATA_SIZE];
    } __attribute__((packed));

public:
    struct Package
    {
        char commandId[COMMAND_SIZE + 1]; // C-string (4 chars + '\0')
        uint8_t data[MAX_DATA_SIZE];
        size_t dataSize{};
        uint8_t source[ESP_NOW_ETH_ALEN]{};
        uint8_t destination[ESP_NOW_ETH_ALEN]{};
        bool isBroadcast{false};
        bool isForMe{false};

        bool isOnlyForMe() const { return isForMe && !isBroadcast; }
        static Package Make(const uint8_t *destination, const char commandId[COMMAND_SIZE]);

        template<typename T>
        static Package Make(const uint8_t *destination, const char commandId[COMMAND_SIZE], const T& data);

        template<typename T>
        bool GetData(T &out) const;
    };

    esp_err_t init();
    esp_err_t registerPeer(const uint8_t *address);
    esp_err_t Send(const Package &pkg, TickType_t timeout = 0);
    bool Receive(Package &package, TickType_t timeout = portMAX_DELAY);

private:
    InitGuard initGuard;
    Queue<Package> receiveQueue{10};
    Semaphore sendSemaphore;
    uint8_t myMac[ESP_NOW_ETH_ALEN]{0};

    volatile esp_now_send_status_t sendStatus = ESP_NOW_SEND_FAIL;
    static EspNow *instance;

    // Helpers
    static Frame ToFrame(const Package &pkg);
    Package FromFrame(const Frame &frame, size_t dataSize, const uint8_t *sourceMac) const;

    // Callbacks
    static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
    static void send_cb(const esp_now_send_info_t *send_info, esp_now_send_status_t status);
};

inline EspNow *EspNow::instance = nullptr;


// Make with no payload
inline EspNow::Package EspNow::Package::Make(const uint8_t *destination, const char commandId[EspNow::COMMAND_SIZE])
{
    EspNow::Package pkg{};
    memcpy(pkg.destination, destination, ESP_NOW_ETH_ALEN);

    memcpy(pkg.commandId, commandId, EspNow::COMMAND_SIZE);
    pkg.commandId[EspNow::COMMAND_SIZE] = '\0';

    pkg.dataSize = 0;
    pkg.isBroadcast = (memcmp(destination, "\xFF\xFF\xFF\xFF\xFF\xFF", ESP_NOW_ETH_ALEN) == 0);
    pkg.isForMe = false;

    return pkg;
}

// Make with struct payload
template <typename T>
inline EspNow::Package EspNow::Package::Make(const uint8_t *destination, const char commandId[EspNow::COMMAND_SIZE], const T &value)
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

    Package pkg = Make(destination, commandId);
    pkg.dataSize = std::min(sizeof(T), MAX_DATA_SIZE);
    memcpy(pkg.data, &value, pkg.dataSize);
    return pkg;
}

// Extract payload
template <typename T>
inline bool EspNow::Package::GetData(T &out) const
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

    if (dataSize >= sizeof(T))
        return false;
    
    memcpy(&out, data, sizeof(T));
    return true;
}

