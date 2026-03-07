#include "esp_launcher.h"
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

// Internal headers
esp_err_t launcher_sd_mount(int mosi, int miso, int sck, int cs);
void launcher_sd_unmount(void);
esp_err_t launcher_install_from_file(const char *path, size_t file_size);
void launcher_ui_init(void);
int launcher_ui_select(const firmware_entry_t *entries, size_t count);

#define TAG "launcher_core"
#define NVS_NAMESPACE "launcher"
#define NVS_KEY_BOOT_FLAG "boot_flag"

static bool check_boot_flag(void)
{
    nvs_handle_t handle;
    uint8_t flag = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, NVS_KEY_BOOT_FLAG, &flag);
        nvs_close(handle);
    }
    return (flag != 0);
}

static void clear_boot_flag(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_BOOT_FLAG, 0);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void esp_launcher_reboot_to_launcher(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_BOOT_FLAG, 1);
        nvs_commit(handle);
        nvs_close(handle);
    }
    esp_restart();
}

esp_err_t esp_launcher_check_and_run(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_LAUNCHER_PIN_BOOT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    if (gpio_get_level(CONFIG_LAUNCHER_PIN_BOOT_BUTTON) != 0 && !check_boot_flag()) {
        return ESP_OK;
    }

    clear_boot_flag();
    launcher_ui_init();

    if (launcher_sd_mount(CONFIG_LAUNCHER_PIN_SPI_MOSI, 
                          CONFIG_LAUNCHER_PIN_SPI_MISO, 
                          CONFIG_LAUNCHER_PIN_SPI_SCK, 
                          CONFIG_LAUNCHER_PIN_SD_CS) != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed. Check SD card.");
        return ESP_FAIL;
    }

    firmware_entry_t entries[MAX_FW_FILES];
    size_t count = 0;
    ret = esp_launcher_list_firmware(entries, &count);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No firmware found in SD:%s", CONFIG_LAUNCHER_FW_DIR);
        launcher_sd_unmount();
        return ret;
    }

    int selected = launcher_ui_select(entries, count);
    if (selected >= 0) {
        ESP_LOGI(TAG, "Installing %s...", entries[selected].name);
        ret = launcher_install_from_file(entries[selected].path, entries[selected].size);
    } else {
        ret = ESP_FAIL;
    }

    launcher_sd_unmount();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Success! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    return ret;
}
