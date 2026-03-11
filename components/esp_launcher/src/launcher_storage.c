#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "esp_launcher.h"

#define TAG "launcher_sd"
#define MOUNT_POINT "/sdcard"
#define SD_MAX_TRANSFER_SZ 4096

static sdmmc_card_t *s_card = NULL;

esp_err_t launcher_sd_mount(int mosi, int miso, int sck, int cs)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SD_MAX_TRANSFER_SZ,
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

static bool has_bin_extension(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + len - 4;
    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 'b' &&
            tolower((unsigned char)ext[2]) == 'i' &&
            tolower((unsigned char)ext[3]) == 'n');
}

static int compare_entries(const void *a, const void *b)
{
    return strcmp(((const firmware_entry_t *)a)->name,
                  ((const firmware_entry_t *)b)->name);
}

esp_err_t esp_launcher_list_firmware(firmware_entry_t *entries, size_t *count)
{
    const char *dir_path = MOUNT_POINT CONFIG_LAUNCHER_FW_DIR;
    DIR *dir = opendir(dir_path);
    if (!dir) return ESP_FAIL;

    const esp_partition_t *ota_part = esp_ota_get_next_update_partition(NULL);

    size_t idx = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && idx < MAX_FW_FILES) {
        if (ent->d_type != DT_REG) continue;
        if (!has_bin_extension(ent->d_name)) continue;

        snprintf(entries[idx].name, sizeof(entries[idx].name), "%s", ent->d_name);
        snprintf(entries[idx].path, sizeof(entries[idx].path), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(entries[idx].path, &st) != 0) {
            ESP_LOGW(TAG, "stat failed for %s, skipping", entries[idx].name);
            continue;
        }

        if (ota_part != NULL && (size_t)st.st_size > ota_part->size) {
            ESP_LOGW(TAG, "Skipping %s: file too large for OTA partition", entries[idx].name);
            continue;
        }

        entries[idx].size = st.st_size;
        idx++;
    }
    closedir(dir);

    if (idx > 1) qsort(entries, idx, sizeof(firmware_entry_t), compare_entries);

    *count = idx;
    return (idx > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
