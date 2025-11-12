#pragma once
#include "HttpEndpoint.h"
#include "ResponseStream.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>
#include <array>
#include "EspNowManager.h"
#include "utils.h"

class PostScoreEndpoint : public HttpEndpoint
{
public:
    PostScoreEndpoint(EspNowManager& espNowManager)
        : espNowManager(espNowManager)
    {
    }
    esp_err_t handle(httpd_req_t *req) override
    {
        static constexpr const char *TAG = "PostScoreEndpoint";

        char body[256] = {0};
        if (!receiveBody(req, body, sizeof(body)))
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }

        int score = 0;
        char mac[18] = {0};
        if (!parseJson(body, score, mac, sizeof(mac)))
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }

        handleData(score, mac);

        ResponseStream stream(req);
        const char *response = "{\"status\":\"ok\"}";
        stream.write(response, strlen(response));
        stream.close();

        return ESP_OK;
    }

private:
    EspNowManager& espNowManager;
    bool receiveBody(httpd_req_t *req, char *out, size_t maxLen)
    {
        int total_len = req->content_len;
        int received = 0;
        while (received < total_len && received < (int)(maxLen - 1))
        {
            int ret = httpd_req_recv(req, out + received, maxLen - 1 - received);
            if (ret <= 0)
            {
                ESP_LOGE("PostScoreEndpoint", "Failed to receive POST data");
                return false;
            }
            received += ret;
        }
        out[received] = '\0';
        return true;
    }

    bool parseJson(const char *json, int &score, char *mac, size_t macLen)
    {
        cJSON *root = cJSON_Parse(json);
        if (!root)
        {
            ESP_LOGE("PostScoreEndpoint", "JSON parse error");
            return false;
        }

        cJSON *scoreItem = cJSON_GetObjectItem(root, "score");
        cJSON *macItem = cJSON_GetObjectItem(root, "mac");

        bool ok = cJSON_IsNumber(scoreItem) && cJSON_IsString(macItem);
        if (ok)
        {
            score = scoreItem->valueint;
            strncpy(mac, macItem->valuestring, macLen - 1);
            mac[macLen - 1] = '\0';
        }
        else
        {
            ESP_LOGE("PostScoreEndpoint", "Missing or invalid JSON fields");
        }

        cJSON_Delete(root);
        return ok;
    }

    /// Base64 decoder (no dynamic memory)
    static bool base64Decode(const char *in, uint8_t *out, size_t outSize)
    {
        static const uint8_t d[256] = {
            64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
            64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
            64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,52,
            53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,
            64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
            64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64};

        size_t len = strlen(in);
        size_t i = 0, j = 0;
        uint32_t v = 0;
        size_t outCount = 0;

        while (i < len && outCount < outSize)
        {
            uint8_t c = in[i++];
            if (c == '=') break;
            uint8_t x = (c < 128) ? d[c] : 64;
            if (x == 64) continue; // skip invalid
            v = (v << 6) | x;
            if (++j == 4)
            {
                out[outCount++] = (v >> 16) & 0xFF;
                if (outCount < outSize) out[outCount++] = (v >> 8) & 0xFF;
                if (outCount < outSize) out[outCount++] = v & 0xFF;
                j = 0;
                v = 0;
            }
        }
        if (j && outCount < outSize)
        {
            v <<= (4 - j) * 6;
            out[outCount++] = (v >> 16) & 0xFF;
            if (j >= 3 && outCount < outSize)
                out[outCount++] = (v >> 8) & 0xFF;
        }
        return true;
    }

    void handleData(int score, const char *mac)
    {
        uint8_t macBytes[6] = {0};
        MacUtils::FromString(mac, macBytes);

        espnow_message_t msg = {};
        msg.event = ESPNOW_MESSAGE_EVENT_SCORE_UPDATE;
        msg.value = score;
        memcpy(msg.destinationMac, macBytes, 6);
        espNowManager.Send(msg);
    }
};
