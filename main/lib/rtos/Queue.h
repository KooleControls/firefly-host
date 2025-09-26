#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

template<typename T>
class Queue
{
    constexpr static const char* TAG = "Queue";
    QueueHandle_t handle = nullptr;
    size_t capacity = 0;

public:
    explicit Queue(size_t capacity)
        : capacity(capacity)
    {
        handle = xQueueCreate(capacity, sizeof(T));
    }

    ~Queue()
    {
        if (handle != nullptr)
            vQueueDelete(handle);
    }

    bool PushFromIsr(const T& item, BaseType_t* higherPriorityTaskWoken = nullptr)
    {
        return xQueueSendToBackFromISR(handle, &item, higherPriorityTaskWoken) == pdTRUE;
    }

    bool Push(const T& item, TickType_t timeout = 0)
    {
        return xQueueSendToBack(handle, &item, timeout) == pdTRUE;
    }

    bool Pop(T& out, TickType_t timeout = 0)
    {
        return xQueueReceive(handle, &out, timeout) == pdTRUE;
    }

    bool Peek(T& out, TickType_t timeout = 0)
    {
        return xQueuePeek(handle, &out, timeout) == pdTRUE;
    }

    bool IsEmpty() const
    {
        return uxQueueMessagesWaiting(handle) == 0;
    }

    bool IsFull() const
    {
        return uxQueueSpacesAvailable(handle) == 0;
    }

    size_t Size() const
    {
        return uxQueueMessagesWaiting(handle);
    }

    size_t Capacity() const
    {
        return capacity;
    }
};
