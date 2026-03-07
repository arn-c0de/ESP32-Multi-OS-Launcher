#include <stdio.h>
#include "esp_launcher.h"
#include "esp_log.h"

void app_main(void)
{
    // 1. Initial check: If button is held or software flag is set, enter launcher.
    // This handles SD mounting, UI, and OTA flashing.
    esp_launcher_check_and_run();

    // 2. Normal application logic starts here
    ESP_LOGI("APP", "Welcome to your Multi-OS ESP32!");
    
    while(1) {
        // Your code...
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
