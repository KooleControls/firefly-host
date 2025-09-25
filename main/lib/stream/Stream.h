#pragma once
#include <cstddef>

class Stream {
public:
    virtual ~Stream() = default;
    virtual size_t write(const void* data, size_t len) = 0;
    virtual size_t read(void* buffer, size_t len) = 0;
    virtual void flush() = 0;
};
