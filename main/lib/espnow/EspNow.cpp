#include "EspNow.h"

#include "EspNow.h"

esp_err_t EspNow::init()
{
    if (initGuard.IsReady())
        return ESP_OK;

    wifi_mode_t mode;
    esp_err_t wifiStatus = esp_wifi_get_mode(&mode);

    assert(wifiStatus == ESP_OK && "Wi-Fi must be initialized before ESP-NOW");
    assert((mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) &&
           "Wi-Fi must be in STA or APSTA mode for ESP-NOW");

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, myMac));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(&EspNow::recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(&EspNow::send_cb));

    instance = this;
    initGuard.SetReady();

    // Register broadcast peer
    static const uint8_t bcast[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ESP_ERROR_CHECK(registerPeer(bcast));

    ESP_LOGI(TAG, "ESP-NOW initialized");
    return ESP_OK;
}

esp_err_t EspNow::registerPeer(const uint8_t *address)
{
    REQUIRE_READY(initGuard);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, address, ESP_NOW_ETH_ALEN);
    peerInfo.channel = 0;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;

    if (esp_now_is_peer_exist(address))
        return ESP_OK;

    return esp_now_add_peer(&peerInfo);
}

esp_err_t EspNow::Send(const Package &pkg, TickType_t timeout)
{
    REQUIRE_READY(initGuard);

    Frame frame = ToFrame(pkg);
    ESP_ERROR_CHECK(registerPeer(pkg.destination));

    sendSemaphore.Take(timeout); // clear before sending

    esp_err_t err = esp_now_send(pkg.destination,
                                 reinterpret_cast<const uint8_t *>(&frame),
                                 sizeof(frame));
    if (err != ESP_OK)
        return err;

    if (!sendSemaphore.Take(timeout))
        return ESP_ERR_TIMEOUT;

    return sendStatus == ESP_NOW_SEND_SUCCESS ? ESP_OK : ESP_FAIL;
}

bool EspNow::Receive(Package &package, TickType_t timeout)
{
    REQUIRE_READY(initGuard);
    return receiveQueue.Pop(package, timeout);
}

// --- helpers ---
EspNow::Frame EspNow::ToFrame(const Package &pkg)
{
    Frame frame{};
    memcpy(frame.destination, pkg.destination, sizeof(frame.destination));

    // Copy only 4 bytes of commandId (ignore the null terminator in Package)
    memcpy(frame.commandId, pkg.commandId, 4);

    memset(frame.data, 0, sizeof(frame.data));
    memcpy(frame.data, pkg.data, std::min(pkg.dataSize, sizeof(frame.data)));

    return frame;
}

EspNow::Package EspNow::FromFrame(const Frame &frame, size_t dataSize, const uint8_t *sourceMac) const
{
    Package pkg{};
    memcpy(pkg.source, sourceMac, sizeof(pkg.source));
    memcpy(pkg.destination, frame.destination, sizeof(pkg.destination));

    // Copy raw 4 bytes into C-string with null terminator
    memcpy(pkg.commandId, frame.commandId, 4);
    pkg.commandId[4] = '\0';

    pkg.dataSize = std::min(dataSize, sizeof(frame.data));
    memcpy(pkg.data, frame.data, pkg.dataSize);

    pkg.isBroadcast = (memcmp(frame.destination, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0);
    pkg.isForMe = (memcmp(frame.destination, myMac, 6) == 0) || pkg.isBroadcast;

    return pkg;
}

// --- callbacks ---
void EspNow::recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (!instance || len < sizeof(Frame))
        return;

    const Frame *frame = reinterpret_cast<const Frame *>(data);
    Package pkg = instance->FromFrame(*frame, len - offsetof(Frame, data), recv_info->src_addr);

    BaseType_t hpw = pdFALSE;
    if (!instance->receiveQueue.PushFromIsr(pkg, &hpw))
        ESP_LOGW(TAG, "Receive queue full, dropping packet");
    portYIELD_FROM_ISR(hpw);
}

void EspNow::send_cb(const esp_now_send_info_t *send_info, esp_now_send_status_t status)
{
    if (!instance)
        return;
    instance->sendStatus = status;
    instance->sendSemaphore.GiveFromISR();
}
