#pragma once
#include "HttpSseEndpoint.h"
#include "EspNowManager.h"
#include "json.h"
#include "utils.h"


class GuestSseEndpoint : public HttpSseEndpoint<8>
{
    constexpr static const char* TAG = "GuestSseEndpoint";
public:
    GuestSseEndpoint(EspNowManager& espNowManager)
        : espNowManager(espNowManager)
    {
        // spawn background task
        task.Init("SSEPushTask", 5, 8192);
        task.SetHandler([this]() { runLoop(); });
        task.Run();
    }

protected:
    void OnConnect(Stream& s) override {
        // I dont have anything to send here
        
    }

private:
    EspNowManager& espNowManager;
    Task task;

    void runLoop() {
        while (true) {
            EspNowManager::Packet pkt;
            if (espNowManager.Read(pkt, pdMS_TO_TICKS(30000))) {
                handlePackage(pkt);
            }
            // handle keepalive + cleanup
            SendKeepAlive();
        }
    }

    void handlePackage(const EspNowManager::Packet &pkt)
    {
        if (pkt.len < sizeof(espnow_message_t))
        {
            ESP_LOGW("GuestSseEndpoint", "Received short packet (%d bytes)", pkt.len);
            return;
        }

        // Interpret packet as espnow_message_t
        const espnow_message_t *message = reinterpret_cast<const espnow_message_t *>(pkt.data);


        
        //ESP_LOGI(TAG, "Sent event=%s value=%ld name=%s", event_to_string(message->event), (long)message->value, message->name);

        // Push event to all connected SSE clients
        pushToAllClients(pkt.mac, *message);
    }

    void pushToAllClients(const uint8_t *mac, const espnow_message_t &message)
    {
        ForEachClient([&](Stream &s) {
            s.write("data: ", 6);
            char macId[18];
            MacUtils::ToString(mac, macId, sizeof(macId));
            JsonObjectWriter::create(s, [&](JsonObjectWriter &obj) {
                obj.field("mac", macId);
                obj.field("event", EventToString(message.event));
                obj.field("value", (int64_t)message.value);
                obj.field("name", message.name);
            });
            s.write("\n\n", 2);
            s.flush();
            return true; // keep client alive
        });
    }
};


