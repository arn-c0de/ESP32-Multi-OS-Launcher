#include "esp_launcher.h"
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

// Internal headers
esp_err_t launcher_sd_mount(int mosi, int miso, int sck, int cs);
void launcher_sd_unmount(void);
esp_err_t launcher_install_from_file(const char *path, size_t file_size);
void launcher_ui_init(void);
int launcher_ui_select_main_menu(const char *app_label);
int launcher_ui_select_firmware(const firmware_entry_t *entries, size_t count);
void launcher_ui_show_message(const char *title, const char *detail, bool is_error);

#define TAG "launcher_core"
#define NVS_NAMESPACE "launcher"
#define NVS_KEY_BOOT_FLAG "boot_flag"

static char s_app_label[64] = "Current Firmware";

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

void esp_launcher_set_app_label(const char *label)
{
    if (!label || !label[0]) {
        snprintf(s_app_label, sizeof(s_app_label), "%s", "Current Firmware");
        return;
    }
    snprintf(s_app_label, sizeof(s_app_label), "%s", label);
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

    while (true) {
        int main_action = launcher_ui_select_main_menu(s_app_label);
        if (main_action == 0) {
            return ESP_OK;
        }

        launcher_ui_show_message("Mounting SD", CONFIG_LAUNCHER_FW_DIR, false);
        ret = launcher_sd_mount(CONFIG_LAUNCHER_PIN_SPI_MOSI,
                                CONFIG_LAUNCHER_PIN_SPI_MISO,
                                CONFIG_LAUNCHER_PIN_SPI_SCK,
                                CONFIG_LAUNCHER_PIN_SD_CS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SD mount failed. Check SD card.");
            launcher_ui_show_message("SD mount failed", "Check card and wiring", true);
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        firmware_entry_t entries[MAX_FW_FILES];
        size_t count = 0;
        ret = esp_launcher_list_firmware(entries, &count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "No firmware found in SD:%s", CONFIG_LAUNCHER_FW_DIR);
            launcher_ui_show_message("No firmware found", CONFIG_LAUNCHER_FW_DIR, true);
            launcher_sd_unmount();
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        int selected = launcher_ui_select_firmware(entries, count);
        if (selected < 0) {
            launcher_sd_unmount();
            continue;
        }

        ESP_LOGI(TAG, "Installing %s...", entries[selected].name);
        launcher_ui_show_message("Flashing firmware", entries[selected].name, false);
        ret = launcher_install_from_file(entries[selected].path, entries[selected].size);
        launcher_sd_unmount();

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Success! Rebooting...");
            launcher_ui_show_message("Flash complete", "Rebooting...", false);
            vTaskDelay(pdMS_TO_TICKS(800));
            esp_restart();
        }

        launcher_ui_show_message("Flashing failed", entries[selected].name, true);
        vTaskDelay(pdMS_TO_TICKS(1800));
    }

    return ret;
}
