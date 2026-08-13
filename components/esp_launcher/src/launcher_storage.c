#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
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

    // ESP_ERR_INVALID_STATE means the bus is already initialized (e.g. after a
    // previous failed mount attempt) - that is fine, keep going.
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.host_id = SPI2_HOST;
    slot_cfg.gpio_cs = cs;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &slot_cfg, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        // Release the SPI bus so a later retry can re-initialize cleanly.
        spi_bus_free(SPI2_HOST);
        s_card = NULL;
        return ret;
    }
    return ESP_OK;
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
        // Match only names ending in ".bin" (case-insensitive), so that
        // e.g. "FW.BIN" is accepted but "x.bin.bak" is rejected.
        size_t n = strlen(ent->d_name);
        if (n < 4 || strcasecmp(ent->d_name + n - 4, ".bin") != 0) continue;

        int written = snprintf(entries[idx].path, sizeof(entries[idx].path), "%s/%s", dir_path, ent->d_name);
        if (written < 0 || (size_t)written >= sizeof(entries[idx].path)) continue;

        struct stat st;
        if (stat(entries[idx].path, &st) != 0) continue;

        snprintf(entries[idx].name, sizeof(entries[idx].name), "%s", ent->d_name);
        entries[idx].size = st.st_size;
        idx++;
    }
    closedir(dir);
    *count = idx;
    return (idx > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
