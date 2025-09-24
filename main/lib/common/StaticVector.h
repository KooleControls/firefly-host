#pragma once
#include <cassert>
#include <cstddef>

template <typename T>
class IStaticVector {
public:
    virtual ~IStaticVector() = default;

    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual bool empty() const = 0;
    virtual bool full() const = 0;

    virtual T& operator[](size_t i) = 0;
    virtual const T& operator[](size_t i) const = 0;

    virtual bool try_push_back(const T& value) = 0;
    virtual void push_back(const T& value) = 0;
    virtual void clear() = 0;
};

template <typename T, size_t Capacity>
class StaticVector : public IStaticVector<T> {
    size_t count = 0;
    T data[Capacity];

public:
    size_t size() const override { return count; }
    constexpr size_t capacity() const override { return Capacity; }
    bool empty() const override { return count == 0; }
    bool full() const override { return count == Capacity; }

    T& operator[](size_t i) override {
        assert(i < count && "StaticVector out of range!");
        return data[i];
    }

    const T& operator[](size_t i) const override {
        assert(i < count && "StaticVector out of range!");
        return data[i];
    }

    void push_back(const T& value) override {
        assert(try_push_back(value) && "StaticVector overflow!");
    }

    bool try_push_back(const T& value) override {
        if (count < Capacity) {
            data[count++] = value;
            return true;
        }
        return false;
    }

    void clear() override {
        count = 0; // if non-trivial T, destruct loop is safer
    }
};
