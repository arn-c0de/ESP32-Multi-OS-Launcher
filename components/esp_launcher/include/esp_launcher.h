#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_FW_FILES 32
#define MAX_PATH_LEN 128

typedef struct {
    char path[MAX_PATH_LEN];
    char name[64];
    size_t size;
} firmware_entry_t;

/**
 * @brief Check if launcher should be active (button press or NVS flag).
 * 
 * Call this at the very beginning of app_main() or setup().
 */
esp_err_t esp_launcher_check_and_run(void);

/**
 * @brief Force the launcher to start on the next reboot.
 */
void esp_launcher_reboot_to_launcher(void);

/**
 * @brief Get a list of firmware files from the SD card.
 */
esp_err_t esp_launcher_list_firmware(firmware_entry_t *entries, size_t *count);
