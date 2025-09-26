#pragma once
#include "esp_err.h"
#include "esp_http_server.h"
#include "HttpEndpoint.h"

class WebServer
{
public:
    WebServer() = default;
    ~WebServer() { stop(); }

    WebServer(const WebServer &) = delete;
    WebServer &operator=(const WebServer &) = delete;
    WebServer(WebServer &&) = delete;
    WebServer &operator=(WebServer &&) = delete;

    void start()
    {
        if (server)
            return;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_open_sockets = 8;  // allow 8 concurrent sockets
        config.max_uri_handlers = 16; // more endpoints

        config.uri_match_fn = httpd_uri_match_wildcard;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
    }

    void stop()
    {
        if (!server)
            return;
        httpd_stop(server);
        server = nullptr;
    }

    void EnableCors()
    {
        // Handle OPTIONS for CORS preflight
        httpd_uri_t api_options = {
            .uri = "/*",
            .method = HTTP_OPTIONS,
            .handler = api_options_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &api_options);
    }

    void registerHandler(const char *uri, httpd_method_t method, HttpEndpoint &handler)
    {
        httpd_uri_t config = {
            .uri = uri,
            .method = method,
            .handler = &WebServer::trampoline,
            .user_ctx = &handler};
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config));
    }

private:
    httpd_handle_t server = nullptr;

    static void set_cors_headers(httpd_req_t *req)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    }

    static esp_err_t trampoline(httpd_req_t *req)
    {
        set_cors_headers(req);  // Always inject CORS headers
        auto *h = static_cast<HttpEndpoint *>(req->user_ctx);
        return h->handle(req);
    }

    static esp_err_t api_options_handler(httpd_req_t *req)
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, NULL, 0);
    }
};
