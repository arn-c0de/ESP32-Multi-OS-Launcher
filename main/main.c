#include <stdio.h>
#include "esp_launcher.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    esp_launcher_set_app_label("Example App");

    // 1. Initial check: If button is held or software flag is set, enter launcher.
    // The shared launcher menu then offers "Boot <current app>" or the SD browser.
    esp_launcher_check_and_run();

    // 2. Normal application logic starts here
    ESP_LOGI("APP", "Welcome to your Multi-OS ESP32!");
    
    while(1) {
        // Your code...
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
