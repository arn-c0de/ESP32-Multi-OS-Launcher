#include "esp_launcher.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

void launcher_ui_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_CLK) | 
                        (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_DT) | 
                        (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&cfg);
}

static void print_list(const firmware_entry_t *entries, size_t count, int selected)
{
    printf("\033[H\033[J"); // Clear Screen
    printf("=== SELECT OS FROM SD ===\n");
    for (size_t i = 0; i < count; i++) {
        printf("%s %d: %s (%u KB)\n", 
               (i == selected) ? "->" : "  ", 
               (int)i + 1, 
               entries[i].name, 
               (unsigned)entries[i].size / 1024);
    }
}

int launcher_ui_select(const firmware_entry_t *entries, size_t count)
{
    if (count == 0) return -1;

    int selected = 0;
    int last_clk = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_CLK);
    int last_sw = 1;

    print_list(entries, count, selected);

    while (1) {
        int clk = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_CLK);
        if (clk != last_clk && clk == 1) {
            if (gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_DT) != clk) {
                selected = (selected + 1) % (int)count;
            } else {
                selected = (selected - 1 + (int)count) % (int)count;
            }
            print_list(entries, count, selected);
        }
        last_clk = clk;

        int sw = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW);
        if (last_sw == 1 && sw == 0) { // Button Pressed
            vTaskDelay(pdMS_TO_TICKS(20)); // Debounce
            if (gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW) == 0) {
                while(gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW) == 0) vTaskDelay(1);
                return selected;
            }
        }
        last_sw = sw;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
