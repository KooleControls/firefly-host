#pragma once
#include "HttpEndpoint.h"
#include "Mutex.h"
#include "Stream.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <sys/socket.h>

class HttpSseEndpointBase {
protected:
    struct ClientSlot {
        httpd_handle_t server = nullptr;
        int sockfd = -1;
        bool active = false;
    };

    class SocketStream : public Stream {
    public:
        SocketStream(httpd_handle_t server, int sockfd)
            : server(server), sockfd(sockfd) {}

        size_t write(const void* data, size_t len) override {
            if (!server || sockfd < 0) return 0;
            int sent = httpd_socket_send(server, sockfd, data, len, 0);
            return sent < 0 ? 0 : sent;
        }

        size_t read(void* buffer, size_t len) override {
            if (sockfd < 0) return 0;
            int r = ::recv(sockfd, buffer, len, MSG_DONTWAIT);
            return (r <= 0) ? 0 : r;
        }

        void flush() override {}

    private:
        httpd_handle_t server;
        int sockfd;
    };
};

template<size_t MaxClients>
class HttpSseEndpoint : public HttpEndpoint, protected HttpSseEndpointBase {
public:
    HttpSseEndpoint(const char* /*taskName*/, uint32_t /*stackSize*/, UBaseType_t /*prio*/) {
        // Could spawn a keepalive task here if desired
    }

    esp_err_t handle(httpd_req_t* req) override {
        int fd = httpd_req_to_sockfd(req);
        if (fd < 0) {
            ESP_LOGE(TAG, "Failed to get sockfd");
            return ESP_FAIL;
        }

        const char* hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "\r\n";

        if (httpd_socket_send(req->handle, fd, hdr, strlen(hdr), 0) < 0) {
            ESP_LOGE(TAG, "Failed to send SSE headers");
            return ESP_FAIL;
        }

        LOCK(clientMutex);
        for (size_t i = 0; i < MaxClients; i++) {
            if (!clients[i].active) {
                clients[i].server = req->handle;
                clients[i].sockfd = fd;
                clients[i].active = true;
                ESP_LOGI(TAG, "SSE client %d connected (fd=%d)", (int)i, fd);

                // Call OnConnect hook
                SocketStream stream(clients[i].server, clients[i].sockfd);
                OnConnect(stream);

                break;
            }
        }

        return ESP_OK;
    }

    template<typename FUNC>
    void ForEachClient(FUNC&& func) {
        LOCK(clientMutex);
        for (size_t i = 0; i < MaxClients; i++) {
            if (clients[i].active) {
                SocketStream stream(clients[i].server, clients[i].sockfd);
                if (func(stream) == false) {
                    cleanup(clients[i]);
                }
            }
        }
    }

protected:
    virtual void OnConnect(Stream& /*s*/) {
        // Default: nothing. Derived class can override.
    }

    void cleanup(ClientSlot& c) {
        ESP_LOGI(TAG, "Cleaning up client fd=%d", c.sockfd);
        c.active = false;
        c.server = nullptr;
        c.sockfd = -1;
    }

private:
    ClientSlot clients[MaxClients];
    Mutex clientMutex;
    static constexpr const char* TAG = "HttpSSE";
};
