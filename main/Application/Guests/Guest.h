#pragma once
#include <cstdint>
#include "DateTime.h"

struct Guest {
    uint8_t mac[6];  
    DateTime lastMessageTime;     
    uint32_t buttonPresses;      
};
