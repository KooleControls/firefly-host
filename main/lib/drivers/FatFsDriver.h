#pragma once
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "esp_log.h"

class FatfsDriver {
    constexpr static const char* TAG = "FatfsDriver";

public:
    FatfsDriver(const char* basePath = "/fat", const char* partitionLabel = "fat")
        : basePath(basePath), partitionLabel(partitionLabel) {}

    esp_err_t Init()
    {
        esp_vfs_fat_mount_config_t mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG();
        mount_config.format_if_mount_failed = true,
        mount_config.max_files = 5,
        mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

        esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
            basePath,
            partitionLabel,
            &mount_config,
            &wl_handle);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "FATFS mounted at %s", basePath);
            mounted = true;
        }
        return err;
    }

    void Unmount()
    {
        if (mounted) {
            esp_vfs_fat_spiflash_unmount_rw_wl(basePath, wl_handle);
            ESP_LOGI(TAG, "FATFS unmounted from %s", basePath);
            mounted = false;
        }
    }

    bool IsMounted() const { return mounted; }

    ~FatfsDriver() { Unmount(); }

private:
    const char* basePath;
    const char* partitionLabel;
    wl_handle_t wl_handle = WL_INVALID_HANDLE;
    bool mounted = false;
};
