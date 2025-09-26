#pragma once
#include "HttpSseEndpoint.h"
#include "GuestManager.h"

class GuestSseEndpoint : public HttpSseEndpoint<8>
{
public:
    explicit GuestSseEndpoint(GuestManager& gm) 
        : guestManager(gm)
    {
        // spawn background task
        task.Init("SSEPushTask", 5, 8192);
        task.SetHandler([this]() { runLoop(); });
        task.Run();
    }

    // Called by external events if you want manual push
    void UpdateData() {
        ForEachClient([&](Stream& s) {
            s.write("data: ", 6);
            JsonArrayWriter::create(s, [&](JsonArrayWriter& arr) {
                guestManager.ForEachGuest([&](const Guest& g) {
                    arr.withObject([&](JsonObjectWriter& obj) {
                        writeGuest(obj, g);
                    });
                });
            });
            s.write("\n\n", 2);
            s.flush();
            return true; // keep client alive
        });
    }

protected:
    void OnConnect(Stream& s) override {
        // Send initial snapshot immediately
        s.write("data: ", 6);
        JsonArrayWriter::create(s, [&](JsonArrayWriter& arr) {
            guestManager.ForEachGuest([&](const Guest& g) {
                arr.withObject([&](JsonObjectWriter& obj) {
                    writeGuest(obj, g);
                });
            });
        });
        s.write("\n\n", 2);
        s.flush();
    }

private:
    GuestManager& guestManager;
    Task task;

    void runLoop() {
        while (true) {
            // wait for guests update or timeout for keepalive
            if (guestManager.waitForUpdate(pdMS_TO_TICKS(30000))) {
                UpdateData();
            }
            // handle keepalive + cleanup
            tick();
        }
    }

    void writeGuest(JsonObjectWriter& obj, const Guest& g) {
        char timeBuf[32];
        g.lastMessageTime.ToStringUtc(timeBuf, sizeof(timeBuf), DateTime::FormatIso8601);
        obj.fieldData("mac", g.mac, 6);
        obj.field("lastMessageTime", timeBuf);
        obj.field("buttonPresses", static_cast<uint64_t>(g.buttonPresses));
    }
};
