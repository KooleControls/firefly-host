#pragma once
#include <cstddef>
#include <cstdint>

class IStreamWriter {
public:
    virtual ~IStreamWriter() = default;

    virtual void writeBool(bool v) = 0;
    virtual void writeInt(int64_t v) = 0;
    virtual void writeUInt(uint64_t v) = 0;
    virtual void writeFloat(float v) = 0;
    virtual void writeDouble(double v) = 0;
    virtual void writeString(const char* v) = 0;
    virtual void writeData(const void* data, size_t len) = 0;
};


