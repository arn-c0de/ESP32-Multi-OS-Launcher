#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#define TAG "launcher_ota"
#define BUF_SIZE 4096

esp_err_t launcher_install_from_file(const char *path, size_t file_size)
{
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found!");
        return ESP_ERR_NOT_FOUND;
    }

    // Never write into the partition we are currently running from.
    if (update_partition == esp_ota_get_running_partition()) {
        ESP_LOGE(TAG, "Update partition equals running partition - aborting to avoid conflict");
        return ESP_ERR_INVALID_STATE;
    }

    // Reject images that cannot fit into the target partition.
    if (file_size > update_partition->size) {
        ESP_LOGE(TAG, "Firmware (%u bytes) larger than partition (%u bytes)",
                 (unsigned)file_size, (unsigned)update_partition->size);
        return ESP_ERR_INVALID_SIZE;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        fclose(f);
        return err;
    }

    uint8_t *buffer = malloc(BUF_SIZE);
    if (!buffer) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t total_written = 0;
    while (1) {
        size_t n = fread(buffer, 1, BUF_SIZE, f);
        if (n == 0) {
            // Distinguish a genuine read error from a clean EOF.
            if (ferror(f)) {
                ESP_LOGE(TAG, "Read error while streaming firmware");
                err = ESP_FAIL;
            }
            break;
        }

        err = esp_ota_write(update_handle, buffer, n);
        if (err != ESP_OK) break;
        total_written += n;

        // Short log to save space
        if (total_written % (64 * 1024) == 0) printf(".");
    }
    printf("\n");

    free(buffer);
    fclose(f);

    // Ensure we actually wrote the whole announced image.
    if (err == ESP_OK && file_size && total_written != file_size) {
        ESP_LOGE(TAG, "Wrote %u of %u bytes - size mismatch",
                 (unsigned)total_written, (unsigned)file_size);
        err = ESP_ERR_INVALID_SIZE;
    }

    if (err == ESP_OK) {
        err = esp_ota_end(update_handle);
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(update_partition);
        }
    } else {
        esp_ota_abort(update_handle);
    }

    return err;
}
