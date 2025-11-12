#pragma once
#include "rtos.h"
#include "InitGuard.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <cstring>


// --- Broadcast address ---
static constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Enum for event types ---
enum espnow_message_event_t : uint8_t
{
    ESPNOW_MESSAGE_EVENT_BUTTON_PRESS = 0,
    ESPNOW_MESSAGE_EVENT_STARTUP = 1,
    ESPNOW_MESSAGE_EVENT_SCORE_UPDATE = 2,
};

// --- Compact binary message struct ---
typedef struct __attribute__((packed))
{
    char name[8];
    espnow_message_event_t event;
    int32_t value;
    uint8_t destinationMac[6];
} espnow_message_t;

static const char *EventToString(espnow_message_event_t e)
{
    switch (e)
    {
    case ESPNOW_MESSAGE_EVENT_BUTTON_PRESS:
        return "button";
    case ESPNOW_MESSAGE_EVENT_STARTUP:
        return "startup";
    case ESPNOW_MESSAGE_EVENT_SCORE_UPDATE:
        return "score";
    default:
        return "UNKNOWN";
    }
}

class EspNowManager
{
    constexpr static const char *TAG = "EspNowManager";

public:
    // --- Packet structure for queue ---
    struct Packet
    {
        uint8_t mac[6];
        uint8_t data[250];
        int len;
    };

    EspNowManager()
    {
        instance = this;
    }

    ~EspNowManager() = default;

    void Init()
    {
        if (initGuard.IsReady())
            return;

        LOCK(mutex);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, myMac));
        ESP_LOGI(TAG, "My MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 myMac[0], myMac[1], myMac[2],
                 myMac[3], myMac[4], myMac[5]);

        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_register_recv_cb(OnReceive));

        // Add broadcast peer
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
        peerInfo.channel = 1;
        peerInfo.ifidx = WIFI_IF_STA;
        peerInfo.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

        recvQueue = xQueueCreate(8, sizeof(Packet));

        task.Init("EspNow", 7, 4096);
        task.SetHandler([this]() { Work(); });
        task.Run();

        initGuard.SetReady();

        ESP_LOGI(TAG, "ESP-NOW initialized and ready.");
    }

    bool Read(Packet &package, TickType_t timeoutTicks)
    {
        REQUIRE_READY(initGuard);
        return xQueueReceive(recvQueue, &package, timeoutTicks) == pdTRUE;
    }

    // --- Sending functions ---
    bool Send(const espnow_message_t &msg)
    {
        esp_err_t result = esp_now_send(
            BROADCAST_MAC,
            reinterpret_cast<const uint8_t *>(&msg),
            sizeof(msg));

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Sent event=%s value=%ld name=%s, dstMac=%02X:%02X:%02X:%02X:%02X:%02X",
                     EventToString(msg.event), (long)msg.value, msg.name,
                     msg.destinationMac[0], msg.destinationMac[1], msg.destinationMac[2],
                     msg.destinationMac[3], msg.destinationMac[4], msg.destinationMac[5]);
                     
            return true;
        }

        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(result));
        return false;
    }

    bool SendEvent(espnow_message_event_t event, int32_t value,
                   const char *name, const uint8_t *dest = BROADCAST_MAC)
    {
        espnow_message_t msg = {};
        strncpy(msg.name, name, sizeof(msg.name) - 1);
        msg.event = event;
        msg.value = value;
        memcpy(msg.destinationMac, dest, 6);
        return Send(msg);
    }

    const uint8_t *GetMacAddress() const { return myMac; }

private:
    InitGuard initGuard;
    Mutex mutex;
    Task task;
    QueueHandle_t recvQueue = nullptr;
    uint8_t myMac[6] = {0};

    static inline EspNowManager *instance = nullptr;

    static void OnReceive(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
    {
        if (!recv_info || !data || len <= 0)
            return;

        if (!instance || !instance->recvQueue)
            return;

        // Filter self
        if (memcmp(recv_info->src_addr, instance->myMac, 6) == 0)
            return;

        // Enqueue packet
        Packet pkt{};
        memcpy(pkt.mac, recv_info->src_addr, 6);
        pkt.len = len < sizeof(pkt.data) ? len : sizeof(pkt.data);
        memcpy(pkt.data, data, pkt.len);
        xQueueSend(instance->recvQueue, &pkt, 0);
    }

    void Work()
    {
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
};
