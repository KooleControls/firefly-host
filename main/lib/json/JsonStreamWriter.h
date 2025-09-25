#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Stream.h"
#include "IStreamWriter.h"
#include "Base64Stream.h"
#include "JsonEscapedStream.h"

class JsonStreamWriter : public IStreamWriter
{
    Stream &out;

public:
    explicit JsonStreamWriter(Stream &s) : out(s) {}

    void writeBool(bool v) override
    {
        const char *str = v ? "true" : "false";
        out.write(str, std::strlen(str));
    }

    void writeInt(int64_t v) override
    {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%lld",
                         static_cast<long long>(v));
        out.write(buf, n);
    }

    void writeUInt(uint64_t v) override
    {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%llu",
                         static_cast<unsigned long long>(v));
        out.write(buf, n);
    }

    void writeFloat(float v) override
    {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "%g",
                         static_cast<double>(v));
        out.write(buf, n);
    }

    void writeDouble(double v) override
    {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "%g", v);
        out.write(buf, n);
    }

    void writeString(const char *v) override
    {
        out.write("\"", 1);
        JsonEscapedStream esc(out);
        esc.write(v, std::strlen(v));
        esc.flush();
        out.write("\"", 1);
    }

    void writeData(const void *data, size_t len) override
    {
        out.write("\"", 1);
        Base64Stream b64(out);
        b64.write(data, len);
        b64.flush();
        out.write("\"", 1);
    }
};
