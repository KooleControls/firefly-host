#pragma once
#include "IMutex.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define LOCK(mutex) ContextLock lock(mutex)

class ContextLock
{
    constexpr const static char* TAG = "ContextLock";
    const IMutex& mutex;
    bool taken = false;
public:
    ContextLock(const IMutex& mutex) : mutex(mutex)
    {
        taken = mutex.Take(pdMS_TO_TICKS(10000));
        assert(taken);
    }
    
    ~ContextLock()
    {
        if(taken)
            mutex.Give();
    }
};

