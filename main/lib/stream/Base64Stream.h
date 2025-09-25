#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>
#include "Stream.h"

class Base64Stream : public Stream {
    Stream& out;
    uint8_t buf[3];
    size_t bufLen = 0;

    static constexpr const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    void encodeBlock(const uint8_t* block, size_t len) {
        uint32_t triple = (block[0] << 16) |
                          ((len > 1 ? block[1] : 0) << 8) |
                          (len > 2 ? block[2] : 0);

        char outbuf[4];
        outbuf[0] = table[(triple >> 18) & 0x3F];
        outbuf[1] = table[(triple >> 12) & 0x3F];
        outbuf[2] = (len > 1) ? table[(triple >> 6) & 0x3F] : '=';
        outbuf[3] = (len > 2) ? table[triple & 0x3F] : '=';

        out.write(outbuf, 4);
    }

public:
    explicit Base64Stream(Stream& s) : out(s) {}

    size_t write(const void* data, size_t len) override {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        size_t written = 0;

        for (size_t i = 0; i < len; ++i) {
            buf[bufLen++] = bytes[i];
            if (bufLen == 3) {
                encodeBlock(buf, 3);
                bufLen = 0;
            }
            ++written;
        }
        return written;
    }

    size_t read(void* buffer, size_t len) override {
        assert(false && "Base64Stream does not support read()");
        return 0; // not supported
    }

    void flush() override {
        if (bufLen > 0) {
            encodeBlock(buf, bufLen);
            bufLen = 0;
        }
        out.flush();
    }
};
