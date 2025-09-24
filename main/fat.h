#pragma once
#include "esp_vfs_fat.h"
#include "wear_levelling.h"



static wl_handle_t wl_handle;

void mount_fatfs(void) {
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount("/fat", "fat", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE("FAT", "Failed to mount FATFS (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI("FAT", "FATFS mounted at /fat");
    }

    //mkdir("/fat/assets", 0777); // Create assets directory if it doesn't exist
}


