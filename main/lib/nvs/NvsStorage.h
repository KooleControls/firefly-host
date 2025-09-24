#pragma once
#include "nvs.h"

class NvsStorage
{
    constexpr static const char *TAG = "NvsStorage";

public:
    NvsStorage(const char *partition, const char *namespaceName);

    bool Open();
    bool Commit();
    void Close();

    bool Get(const char *key, int32_t &value);
    bool Set(const char *key, int32_t value);

    bool Get(const char *key, float &value);
    bool Set(const char *key, float value);

    bool GetString(const char *key, char *buffer, size_t bufferSize, size_t &dataSize);
    bool SetString(const char *key, const char *value);

    bool GetBlob(const char *key, void *buffer, size_t bufferSize, size_t &dataSize);
    bool SetBlob(const char *key, const void *buffer, size_t dataSize);


    static void InitNvsPartition(const char *partition);
    static void PrintStats(const char *partition);


private:
    const char *partitionName;
    const char *namespaceName;
    nvs_handle_t handle;
};
