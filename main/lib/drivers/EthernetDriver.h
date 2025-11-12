#pragma once
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

class EthernetDriver {
    constexpr static const char* TAG = "EthernetDriver";

public:
    EthernetDriver() = default;

    void Init(int phy_addr = 0, int mdc_gpio = 23, int mdio_gpio = 18, int rst_gpio = -1)
    {
        eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
        phy_cfg.phy_addr = phy_addr;
        phy_cfg.reset_gpio_num = rst_gpio;

        eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
        emac_cfg.smi_mdc_gpio_num  = mdc_gpio;
        emac_cfg.smi_mdio_gpio_num = mdio_gpio;

        esp_eth_mac_t* mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
        esp_eth_phy_t* phy = esp_eth_phy_new_lan87xx(&phy_cfg);
        esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

        ESP_ERROR_CHECK(esp_eth_driver_install(&config, &ethHandle));

        esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
        ethNetif = esp_netif_new(&netif_cfg);
        ESP_ERROR_CHECK(esp_netif_attach(ethNetif, esp_eth_new_netif_glue(ethHandle)));

        // Register event handlers
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetDriver::EventHandler, this, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetDriver::EventHandler, this, nullptr));

        ESP_ERROR_CHECK(esp_eth_start(ethHandle));
        ESP_LOGI(TAG, "Ethernet initialized");
    }

    /**
     * @brief Configure a static IP address (disable DHCP)
     *
     * @param ip      Static IP address, e.g., "192.168.1.100"
     * @param gateway Gateway address, e.g., "192.168.1.1"
     * @param netmask Netmask, e.g., "255.255.255.0"
     */
    void SetStaticIp(const char* ip, const char* gateway, const char* netmask)
    {
        assert(ethNetif != nullptr && "Ethernet interface not initialized");

        esp_netif_ip_info_t ip_info = {};
        ESP_ERROR_CHECK(esp_netif_str_to_ip4(ip, &ip_info.ip));
        ESP_ERROR_CHECK(esp_netif_str_to_ip4(gateway, &ip_info.gw));
        ESP_ERROR_CHECK(esp_netif_str_to_ip4(netmask, &ip_info.netmask));

        // Stop DHCP client if running
        esp_err_t err = esp_netif_dhcpc_stop(ethNetif);
        if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            ESP_LOGW(TAG, "DHCP already stopped");
        } else {
            ESP_ERROR_CHECK(err);
        }

        // Set static IP configuration
        ESP_ERROR_CHECK(esp_netif_set_ip_info(ethNetif, &ip_info));
        ESP_LOGI(TAG, "Static IP set to: %s", ip);
    }

    bool IsConnected() const { return connected; }

private:
    esp_netif_t* ethNetif = nullptr;
    esp_eth_handle_t ethHandle = nullptr;
    bool connected = false;

    static void EventHandler(void* arg, esp_event_base_t base, int32_t id, void* data)
    {
        auto* self = static_cast<EthernetDriver*>(arg);

        if (base == ETH_EVENT) {
            if (id == ETHERNET_EVENT_CONNECTED) {
                ESP_LOGI(TAG, "Ethernet link up");
            } else if (id == ETHERNET_EVENT_DISCONNECTED) {
                ESP_LOGW(TAG, "Ethernet link down");
                self->connected = false;
            }
        } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
            auto* event = (ip_event_got_ip_t*)data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            self->connected = true;
        }
    }
};
