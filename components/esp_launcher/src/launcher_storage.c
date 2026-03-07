#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "esp_launcher.h"

#define TAG "launcher_sd"
#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;

esp_err_t launcher_sd_mount(int mosi, int miso, int sck, int cs)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) return ret;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.host_id = SPI2_HOST;
    slot_cfg.gpio_cs = cs;

    return esp_vfs_fat_sdspi_mount(MOUNT_POINT, &slot_cfg, &mount_cfg, &s_card);
}

void launcher_sd_unmount(void)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        spi_bus_free(SPI2_HOST);
        s_card = NULL;
    }
}

esp_err_t esp_launcher_list_firmware(firmware_entry_t *entries, size_t *count)
{
    const char *dir_path = MOUNT_POINT CONFIG_LAUNCHER_FW_DIR;
    DIR *dir = opendir(dir_path);
    if (!dir) return ESP_FAIL;

    size_t idx = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && idx < MAX_FW_FILES) {
        if (strstr(ent->d_name, ".bin")) {
            snprintf(entries[idx].name, sizeof(entries[idx].name), "%s", ent->d_name);
            snprintf(entries[idx].path, sizeof(entries[idx].path), "%s/%s", dir_path, ent->d_name);
            
            struct stat st;
            stat(entries[idx].path, &st);
            entries[idx].size = st.st_size;
            idx++;
        }
    }
    closedir(dir);
    *count = idx;
    return (idx > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
