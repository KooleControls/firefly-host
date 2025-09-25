#pragma once
#include "HttpEndpoint.h"
#include "GuestManager.h"
#include "ResponseStream.h"
#include "json.h"

class GuestGetAllEndpoint : public HttpEndpoint {
public:
    explicit GuestGetAllEndpoint(GuestManager& guestManager)
        : guestManager(guestManager) {}

    esp_err_t handle(httpd_req_t* req) override {
        httpd_resp_set_type(req, "application/json");

        ResponseStream respStream(req);
        JsonArrayWriter::create(respStream, [&](JsonArrayWriter& arr) {
            WriteResponse(arr);
        });

        return ESP_OK;
    }

private:
    GuestManager& guestManager;

    void WriteResponse(JsonArrayWriter& arr) {
        guestManager.ForEachGuest([&](const Guest& g) {
            arr.withObject([&](JsonObjectWriter& guestObj) {
                WriteGuest(guestObj, g);
            });
        });
    }

    void WriteGuest(JsonObjectWriter& obj, const Guest& g) {
        char timeBuf[32];
        g.lastMessageTime.ToStringUtc(timeBuf, sizeof(timeBuf), DateTime::FormatIso8601);

        obj.fieldData("mac", g.mac, 6);
        obj.field("lastMessageTime", timeBuf);
        obj.field("buttonPresses", static_cast<uint64_t>(g.buttonPresses));
    }
};
