#pragma once
#include "esp_err.h"
#include "esp_http_server.h"
#include "HttpEndpoint.h"

class WebServer {
public:
    WebServer() = default;
    ~WebServer() { stop(); }

    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    WebServer(WebServer&&) = delete;
    WebServer& operator=(WebServer&&) = delete;

    void start() {
        if (server) return;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
    }

    void stop() {
        if (!server) return;
        httpd_stop(server);
        server = nullptr;
    }

    void registerHandler(const char* uri, httpd_method_t method, HttpEndpoint& handler) {
        httpd_uri_t config = {
            .uri      = uri,
            .method   = method,
            .handler  = &WebServer::trampoline,
            .user_ctx = &handler
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config));
    }

private:
    httpd_handle_t server = nullptr;

    static esp_err_t trampoline(httpd_req_t* req) {
        auto* h = static_cast<HttpEndpoint*>(req->user_ctx);
        return h->handle(req);
    }
};
