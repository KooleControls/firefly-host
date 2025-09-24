#include "NvsStorage.h"
#include "nvs_flash.h"
#include "esp_log.h"

NvsStorage::NvsStorage(const char *partition, const char *namespaceName)
    : partitionName(partition), namespaceName(namespaceName), handle(0)
{
    
}


bool NvsStorage::Open()
{
    esp_err_t err = nvs_open_from_partition(partitionName, namespaceName,
                                            NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS (partition=%s, namespace=%s) err=0x%x",
                 partitionName, namespaceName, err);
        return false;
    }
    return true;
}

bool NvsStorage::Commit()
{
    esp_err_t err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Commit failed err=0x%x", err);
        return false;
    }
    return true;
}

void NvsStorage::Close()
{
    if (handle != 0)
    {
        nvs_close(handle);
        handle = 0;
    }
}

// ---------------------- Scalars ----------------------

bool NvsStorage::Get(const char *key, int32_t &value)
{
    esp_err_t err = nvs_get_i32(handle, key, &value);
    return (err == ESP_OK);
}

bool NvsStorage::Set(const char *key, int32_t value)
{
    esp_err_t err = nvs_set_i32(handle, key, value);
    return (err == ESP_OK);
}

bool NvsStorage::Get(const char *key, float &value)
{
    size_t size = sizeof(value);
    esp_err_t err = nvs_get_blob(handle, key, &value, &size);
    return (err == ESP_OK && size == sizeof(value));
}

bool NvsStorage::Set(const char *key, float value)
{
    esp_err_t err = nvs_set_blob(handle, key, &value, sizeof(value));
    return (err == ESP_OK);
}

// ---------------------- String ----------------------

bool NvsStorage::GetString(const char *key, char *buffer, size_t bufferSize, size_t &dataSize)
{
    dataSize = bufferSize;
    esp_err_t err = nvs_get_str(handle, key, buffer, &dataSize);
    return (err == ESP_OK);
}

bool NvsStorage::SetString(const char *key, const char *value)
{
    esp_err_t err = nvs_set_str(handle, key, value);
    return (err == ESP_OK);
}

// ---------------------- Blob ----------------------

bool NvsStorage::GetBlob(const char *key, void *buffer, size_t bufferSize, size_t &dataSize)
{
    dataSize = bufferSize;
    esp_err_t err = nvs_get_blob(handle, key, buffer, &dataSize);
    return (err == ESP_OK);
}

bool NvsStorage::SetBlob(const char *key, const void *buffer, size_t dataSize)
{
    esp_err_t err = nvs_set_blob(handle, key, buffer, dataSize);
    return (err == ESP_OK);
}


void NvsStorage::InitNvsPartition(const char *partition)
{
    esp_err_t err = nvs_flash_init_partition(partition);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase_partition(partition));
        err = nvs_flash_init_partition(partition);
    }
    ESP_ERROR_CHECK(err);
}

void NvsStorage::PrintStats(const char *partition)
{
    nvs_stats_t stats;
    esp_err_t err = nvs_get_stats(partition, &stats);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "NVS partition '%s' stats:", partition);
        ESP_LOGI(TAG, "  Used entries: %d", stats.used_entries);
        ESP_LOGI(TAG, "  Free entries: %d", stats.free_entries);
        ESP_LOGI(TAG, "  Total entries: %d", stats.total_entries);
        ESP_LOGI(TAG, "  Namespace count: %d", stats.namespace_count);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get NVS stats for partition '%s' err=0x%x", partition, err);
    }
}