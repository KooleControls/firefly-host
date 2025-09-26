#pragma once
#include "StaticVector.h"
#include "Guest.h"
#include "Mutex.h"
#include "DateTime.h"
#include "Semaphore.h"
#include <cstring> // memcpy, memcmp

class GuestManager {
    constexpr static const char *TAG = "GuestManager";
    static constexpr size_t MAX_GUESTS = 50;

public:
    GuestManager() = default;

    void init() {
    }

    void ReportGuest(const uint8_t mac[6], uint32_t buttonPresses) {
        LOCK(mutex);

        if (guests.full())
            return;

        Guest *g = findGuest(mac);
        if (g) {
            updateGuest(*g, buttonPresses);
        } else {
            createGuest(mac, buttonPresses);
        }

        updateSem.Give();  // notify listeners
    }

    template <typename Func>
    void ForEachGuest(Func func) {
        LOCK(mutex);
        for (int i = 0; i < guests.size(); i++) {
            func(guests[i]);
        }
    }

    /// Block until guests are updated
    bool waitForUpdate(TickType_t timeout = portMAX_DELAY) {
        return updateSem.Take(timeout);
    }

private:
    Mutex mutex;
    StaticVector<Guest, MAX_GUESTS> guests;
    Semaphore updateSem;   // wrapped FreeRTOS semaphore

    Guest *findGuest(const uint8_t mac[6]) {
        for (int i = 0; i < guests.size(); i++) {
            if (memcmp(guests[i].mac, mac, 6) == 0) {
                return &guests[i];
            }
        }
        return nullptr;
    }

    void updateGuest(Guest &g, uint32_t buttonPresses) {
        g.lastMessageTime = DateTime::Now();
        g.buttonPresses = buttonPresses;
    }

    void createGuest(const uint8_t mac[6], uint32_t buttonPresses) {
        Guest newGuest{};
        memcpy(newGuest.mac, mac, 6);
        newGuest.lastMessageTime = DateTime::Now();
        newGuest.buttonPresses = buttonPresses;
        guests.push_back(newGuest);
    }
};
