#pragma once
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

class WifiDriver {
    constexpr static const char* TAG = "WifiDriver";

public:
    WifiDriver() = default;

    void Init(const char* ssid = nullptr, const char* pass = nullptr)
    {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifiNetif = esp_netif_create_default_wifi_sta();

        // Pass `this` as context so callbacks can modify this instance
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiDriver::EventHandler, this, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiDriver::EventHandler, this, nullptr));

        if (ssid && pass) {
            wifi_config_t wifi_cfg = {};
            strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
            strncpy((char*)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        }

        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Wi-Fi initialized");
    }

    bool IsConnected() const { return connected; }

private:
    esp_netif_t* wifiNetif = nullptr;
    bool connected = false;

    static void EventHandler(void* arg, esp_event_base_t base, int32_t id, void* data)
    {
        auto* self = static_cast<WifiDriver*>(arg);

        if (base == WIFI_EVENT) {
            if (id == WIFI_EVENT_STA_START) {
                esp_wifi_connect();
                ESP_LOGI(TAG, "Wi-Fi started");
            } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
                ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
                esp_wifi_connect();
                self->connected = false;
            }
        } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
            auto* event = (ip_event_got_ip_t*)data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            self->connected = true;
        }
    }
};
