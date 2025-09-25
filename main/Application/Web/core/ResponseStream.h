#pragma once
#include "Stream.h"
#include "esp_http_server.h"

class ResponseStream : public Stream
{
    httpd_req_t *req;
    bool closed = false;

public:
    explicit ResponseStream(httpd_req_t *r) : req(r) {}
    ~ResponseStream()
    {
        if (!closed)
        {
            close();
        }
    }

    size_t write(const void *data, size_t len) override
    {
        int ret = httpd_resp_send_chunk(req, (const char *)data, len);
        if (ret == ESP_OK)
        {
            return len;
        }
        return 0;
    }

    size_t read(void *buffer, size_t len) override
    {
        assert(false && "ResponseStream does not support read()");
        return 0; // not supported
    }

    void flush() override
    {
        // do nothing, just keep interface happy
    }

    void close()
    {
        httpd_resp_send_chunk(req, nullptr, 0); // end of response
        closed = true;
    }
};
