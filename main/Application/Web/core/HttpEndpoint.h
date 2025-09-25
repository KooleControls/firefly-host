#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

class HttpEndpoint {
public:
    HttpEndpoint() = default;
    virtual ~HttpEndpoint() = default;

    HttpEndpoint(const HttpEndpoint&) = delete;
    HttpEndpoint& operator=(const HttpEndpoint&) = delete;
    HttpEndpoint(HttpEndpoint&&) = delete;
    HttpEndpoint& operator=(HttpEndpoint&&) = delete;

    virtual esp_err_t handle(httpd_req_t* req) = 0;
};
