#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>

class MacUtils
{
public:
    /// Convert 6-byte MAC array → string "AA:BB:CC:DD:EE:FF"
    static void ToString(const uint8_t mac[6], char *out, size_t outSize)
    {
        if (!mac || !out || outSize < 18)
            return;

        snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    /// Convert string "AA:BB:CC:DD:EE:FF" → 6-byte MAC array
    /// Accepts both ':' and '-' separators, and is case-insensitive.
    /// Returns true on success.
    static bool FromString(const char *str, uint8_t mac[6])
    {
        if (!str || !mac)
            return false;

        size_t len = strlen(str);
        if (len < 11) // too short to be a MAC
            return false;

        unsigned int values[6];
        int converted = sscanf(str, "%2x%*[:\\-]%2x%*[:\\-]%2x%*[:\\-]%2x%*[:\\-]%2x%*[:\\-]%2x",
                               &values[0], &values[1], &values[2],
                               &values[3], &values[4], &values[5]);

        if (converted != 6)
            return false;

        for (int i = 0; i < 6; ++i)
            mac[i] = static_cast<uint8_t>(values[i]);

        return true;
    }
};
