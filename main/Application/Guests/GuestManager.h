#pragma once
#include "StaticVector.h"
#include "Guest.h"
#include "Mutex.h"
#include "DateTime.h"
#include <cstring> // for memcpy, memcmp

class GuestManager
{
    constexpr static const char *TAG = "GuestManager";
    static constexpr size_t MAX_GUESTS = 50;

public:
    GuestManager() = default;
    ~GuestManager() = default;
    GuestManager(const GuestManager &) = delete;
    GuestManager &operator=(const GuestManager &) = delete;

    void init()
    {
        uint8_t mac1[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
        uint8_t mac2[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
        uint8_t mac3[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x03};

        createGuest(mac1, 0);
        createGuest(mac2, 5);
        createGuest(mac3, 10);
    }

    void ReportGuest(const uint8_t mac[6], uint32_t buttonPresses)
    {
        LOCK(mutex);

        if(guests.full())
            return;

        Guest *g = findGuest(mac);
        if (g) {
            updateGuest(*g, buttonPresses);
            return;
        }
        
        createGuest(mac, buttonPresses);
    }

    template <typename Func>
    void ForEachGuest(Func func)
    {
        LOCK(mutex);
        for (int i = 0; i < guests.size(); i++)
        {
            func(guests[i]);
        }
    }

private:
    Mutex mutex;
    StaticVector<Guest, MAX_GUESTS> guests;

    Guest *findGuest(const uint8_t mac[6])
    {
        for (int i = 0; i < guests.size(); i++)
        {
            if (memcmp(guests[i].mac, mac, 6) == 0)
            {
                return &guests[i];
            }
        }
        return nullptr;
    }

    void updateGuest(Guest &g, uint32_t buttonPresses)
    {
        g.lastMessageTime = DateTime::Now();
        g.buttonPresses = buttonPresses;
    }

    void createGuest(const uint8_t mac[6], uint32_t buttonPresses)
    {
        Guest newGuest{};
        memcpy(newGuest.mac, mac, 6);
        newGuest.lastMessageTime = DateTime::Now();
        newGuest.buttonPresses = buttonPresses;
        guests.push_back(newGuest);
    }
};
