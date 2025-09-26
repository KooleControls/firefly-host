#pragma once
#include "HttpEndpoint.h"
#include "GuestManager.h"
#include "rtos.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>     // strlen
#include <sys/socket.h> // send()

class SocketStream : public Stream
{
public:
    SocketStream(httpd_handle_t server, int sockfd)
        : server(server), sockfd(sockfd) {}

    size_t write(const void *data, size_t len) override
    {
        if (!server || sockfd < 0)
            return 0;
        int sent = httpd_socket_send(server, sockfd, (const char *)data, len, 0);
        if (sent < 0)
        {
            ESP_LOGW(TAG, "Send failed on fd=%d", sockfd);
            return 0;
        }
        return sent;
    }

    size_t read(void *buffer, size_t len) override
    {
        if (sockfd < 0)
            return 0;
        int r = ::recv(sockfd, buffer, len, MSG_DONTWAIT);
        if (r <= 0)
            return 0;
        return r;
    }

    void flush() override
    {
        // No buffering — nothing to flush
    }

private:
    httpd_handle_t server;
    int sockfd;
    static constexpr const char *TAG = "SocketStream";
};

class GuestSseEndpoint : public HttpEndpoint
{
public:
    static constexpr int MAX_CLIENTS = 8;

    explicit GuestSseEndpoint(GuestManager &guestManager)
        : guestManager(guestManager)
    {
        pushTask.Init("SSEPushTask", 5, 8192);
        pushTask.SetHandler([this]()
                            { loop(); });
        pushTask.Run();
    }

    esp_err_t handle(httpd_req_t *req) override
    {
        int fd = httpd_req_to_sockfd(req);
        if (fd < 0)
        {
            ESP_LOGE(TAG, "Failed to get sockfd");
            return ESP_FAIL;
        }

        // Send raw SSE headers manually over the socket
        const char *hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "\r\n";
        if (httpd_socket_send(req->handle, fd, hdr, strlen(hdr), 0) < 0)
        {
            ESP_LOGE(TAG, "Failed to send SSE headers");
            return ESP_FAIL;
        }

        // Register client in fixed array
        LOCK(clientMutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].active)
            {
                clients[i].server = req->handle;
                clients[i].sockfd = fd;
                clients[i].active = true;
                ESP_LOGI(TAG, "SSE client %d connected (fd=%d)", i, fd);
                break;
            }
        }

        // Important: return immediately — httpd will not manage this socket anymore
        return ESP_OK;
    }

private:
    struct ClientSlot
    {
        httpd_handle_t server = nullptr;
        int sockfd = -1;
        bool active = false;
    };

    GuestManager &guestManager;
    ClientSlot clients[MAX_CLIENTS];
    Mutex clientMutex;
    Task pushTask;
    static constexpr const char *TAG = "GuestSSE";

    void loop()
    {
        while (true)
        {
            if (!guestManager.waitForUpdate(pdMS_TO_TICKS(30000)))
            {
                sendKeepAlive();
                continue;
            }
            sendGuestListToAll();
        }
    }

    void sendKeepAlive()
    {
        static const char *msg = ": keepalive\n\n";
        LOCK(clientMutex);
        ForEachClient([&](ClientSlot &c)
                      {
            if (httpd_socket_send(c.server, c.sockfd, msg, strlen(msg), 0) < 0) {
                ESP_LOGI(TAG, "SSE client disconnected (keepalive)");
                c.active = false;
            } });
    }

    void sendGuestListToAll()
    {
        LOCK(clientMutex);
        ForEachClient([&](ClientSlot &c)
                      {
        SocketStream stream(c.server, c.sockfd);

        bool ok = true;

        if (stream.write("data: ", 6) == 0) {
            ok = false;
        }

        if (ok) {
            JsonArrayWriter::create(stream, [&](JsonArrayWriter &arr) {
                guestManager.ForEachGuest([&](const Guest &g) {
                    arr.withObject([&](JsonObjectWriter &guestObj) {
                        writeGuest(guestObj, g);
                    });
                });
            });
        }

        if (ok && stream.write("\n\n", 2) == 0) {
            ok = false;
        }

        stream.flush();

        if (!ok) {
            ESP_LOGI(TAG, "SSE client disconnected (send fail, fd=%d)", c.sockfd);
            c.active = false;
            c.server = nullptr;
            c.sockfd = -1;
        } });
    }

    template <typename FUNC>
    void ForEachClient(FUNC func)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].active)
                func(clients[i]);
        }
    }

    void writeGuest(JsonObjectWriter &obj, const Guest &g)
    {
        char timeBuf[32];
        g.lastMessageTime.ToStringUtc(timeBuf, sizeof(timeBuf), DateTime::FormatIso8601);
        obj.fieldData("mac", g.mac, 6);
        obj.field("lastMessageTime", timeBuf);
        obj.field("buttonPresses", static_cast<uint64_t>(g.buttonPresses));
    }
};
