#include "esp_launcher.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "launcher_font_5x7.h"

#ifndef CONFIG_LAUNCHER_ENABLE_TFT_UI
#define CONFIG_LAUNCHER_ENABLE_TFT_UI 0
#endif

#ifndef CONFIG_LAUNCHER_TFT_WIDTH
#define CONFIG_LAUNCHER_TFT_WIDTH 320
#endif

#ifndef CONFIG_LAUNCHER_TFT_HEIGHT
#define CONFIG_LAUNCHER_TFT_HEIGHT 480
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_CS
#define CONFIG_LAUNCHER_PIN_TFT_CS 5
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_DC
#define CONFIG_LAUNCHER_PIN_TFT_DC 16
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_RST
#define CONFIG_LAUNCHER_PIN_TFT_RST 17
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_BL
#define CONFIG_LAUNCHER_PIN_TFT_BL 4
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_MOSI
#define CONFIG_LAUNCHER_PIN_TFT_MOSI 23
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_MISO
#define CONFIG_LAUNCHER_PIN_TFT_MISO 19
#endif

#ifndef CONFIG_LAUNCHER_PIN_TFT_SCK
#define CONFIG_LAUNCHER_PIN_TFT_SCK 18
#endif

#define ANSI_CLEAR "\033[H\033[J"
#define ANSI_RESET "\033[0m"
#define ANSI_CYAN "\033[36m"
#define ANSI_BLUE_BG "\033[44m"
#define ANSI_WHITE "\033[97m"
#define ANSI_BLACK "\033[30m"
#define ANSI_RED "\033[31m"
#define ANSI_DIM "\033[90m"
#define ANSI_REVERSE "\033[7m"

#define TAG "launcher_ui"

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800
#define TFT_GREEN 0x07E0
#define TFT_CYAN 0x07FF
#define TFT_YELLOW 0xFFE0
#define TFT_NAVY 0x000F
#define TFT_DARKGREY 0x7BEF

#define LAUNCHER_CHAR_SPACING 1
#define LAUNCHER_MENU_ITEM_HEIGHT 35
#define LAUNCHER_MENU_TOP 45
#define LAUNCHER_MENU_BOTTOM_MARGIN 18
#define LAUNCHER_MENU_SIDE_MARGIN 10
#define LAUNCHER_TITLE_BAR_HEIGHT 35
#define LAUNCHER_STATUS_ROWS 8

typedef void (*launcher_draw_cb_t)(int selected, void *ctx);

static bool s_tft_ready = false;
static bool s_tft_attempted = false;
static spi_device_handle_t s_tft_spi = NULL;
static uint16_t s_fill_line[CONFIG_LAUNCHER_TFT_WIDTH * LAUNCHER_STATUS_ROWS];

static inline uint16_t launcher_swap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static void launcher_serial_clear_screen(void)
{
    printf(ANSI_CLEAR);
}

static void launcher_serial_print_header(const char *title, const char *subtitle)
{
    printf(ANSI_BLUE_BG ANSI_CYAN " MULTI-OS LAUNCHER " ANSI_RESET "\n");
    printf(ANSI_CYAN "%s" ANSI_RESET "\n", title);
    if (subtitle && subtitle[0]) {
        printf(ANSI_DIM "%s" ANSI_RESET "\n", subtitle);
    }
    printf("\n");
}

static void launcher_serial_print_footer(const char *footer)
{
    printf("\n" ANSI_DIM "%s" ANSI_RESET "\n", footer);
}

static void launcher_serial_print_menu_item(int index, const char *label, bool active, const char *detail)
{
    if (active) {
        printf(ANSI_REVERSE ANSI_BLACK " > %d. %-32s " ANSI_RESET, index + 1, label);
    } else {
        printf(ANSI_WHITE "   %d. %-32s " ANSI_RESET, index + 1, label);
    }

    if (detail && detail[0]) {
        printf(ANSI_DIM " %s" ANSI_RESET, detail);
    }
    printf("\n");
}

static void launcher_serial_show_message(const char *title, const char *detail, bool is_error)
{
    launcher_serial_clear_screen();
    printf("%s%s MULTI-OS LAUNCHER %s\n",
           is_error ? ANSI_RED : ANSI_BLUE_BG,
           is_error ? "" : ANSI_CYAN,
           ANSI_RESET);
    printf("%s%s%s\n",
           is_error ? ANSI_RED : ANSI_CYAN,
           title ? title : "",
           ANSI_RESET);
    if (detail && detail[0]) {
        printf(ANSI_DIM "%s" ANSI_RESET "\n", detail);
    }
    fflush(stdout);
}

static void launcher_tft_write(bool data_mode, const void *data, size_t len)
{
    if (!s_tft_ready || !data || len == 0) {
        return;
    }

    gpio_set_level(CONFIG_LAUNCHER_PIN_TFT_DC, data_mode ? 1 : 0);

    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(s_tft_spi, &trans);
}

static void launcher_tft_write_cmd(uint8_t cmd)
{
    launcher_tft_write(false, &cmd, 1);
}

static void launcher_tft_write_data(const uint8_t *data, size_t len)
{
    launcher_tft_write(true, data, len);
}

static void launcher_tft_write_u16(uint16_t value)
{
    uint16_t swapped = launcher_swap16(value);
    launcher_tft_write(true, &swapped, sizeof(swapped));
}

static void launcher_tft_set_window(int x0, int y0, int x1, int y1)
{
    launcher_tft_write_cmd(0x2A);
    launcher_tft_write_u16((uint16_t)x0);
    launcher_tft_write_u16((uint16_t)x1);

    launcher_tft_write_cmd(0x2B);
    launcher_tft_write_u16((uint16_t)y0);
    launcher_tft_write_u16((uint16_t)y1);

    launcher_tft_write_cmd(0x2C);
}

static void launcher_tft_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_tft_ready || w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > CONFIG_LAUNCHER_TFT_WIDTH) {
        w = CONFIG_LAUNCHER_TFT_WIDTH - x;
    }
    if (y + h > CONFIG_LAUNCHER_TFT_HEIGHT) {
        h = CONFIG_LAUNCHER_TFT_HEIGHT - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint16_t color_be = launcher_swap16(color);
    int chunk_rows = LAUNCHER_STATUS_ROWS;
    int pixels_per_chunk = CONFIG_LAUNCHER_TFT_WIDTH * chunk_rows;
    int required_pixels = w * chunk_rows;
    if (required_pixels > pixels_per_chunk) {
        chunk_rows = pixels_per_chunk / w;
        if (chunk_rows < 1) {
            chunk_rows = 1;
        }
    }

    for (int i = 0; i < w * chunk_rows; ++i) {
        s_fill_line[i] = color_be;
    }

    int rows_left = h;
    int current_y = y;
    while (rows_left > 0) {
        int rows = rows_left > chunk_rows ? chunk_rows : rows_left;
        launcher_tft_set_window(x, current_y, x + w - 1, current_y + rows - 1);
        launcher_tft_write(true, s_fill_line, (size_t)(w * rows * sizeof(uint16_t)));
        current_y += rows;
        rows_left -= rows;
    }
}

static void launcher_tft_draw_pixel(int x, int y, uint16_t color)
{
    launcher_tft_fill_rect(x, y, 1, 1, color);
}

static int launcher_tft_text_width(const char *text, int scale)
{
    if (!text) {
        return 0;
    }
    return (int)strlen(text) * (LAUNCHER_FONT_WIDTH + LAUNCHER_CHAR_SPACING) * scale;
}

static void launcher_tft_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    uint8_t ch = (uint8_t)c;
    if (ch < LAUNCHER_FONT_FIRST_CHAR || ch >= LAUNCHER_FONT_LAST_CHAR || scale < 1) {
        ch = '?';
    }

    const uint8_t *glyph = &s_launcher_font_5x7[(ch - LAUNCHER_FONT_FIRST_CHAR) * LAUNCHER_FONT_WIDTH];
    for (int col = 0; col < LAUNCHER_FONT_WIDTH; ++col) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < LAUNCHER_FONT_HEIGHT; ++row) {
            uint16_t color = (bits & (1U << row)) ? fg : bg;
            if (scale == 1) {
                launcher_tft_draw_pixel(x + col, y + row, color);
            } else {
                launcher_tft_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }

    if (bg != fg) {
        int advance = LAUNCHER_FONT_WIDTH * scale;
        launcher_tft_fill_rect(x + advance, y, LAUNCHER_CHAR_SPACING * scale, LAUNCHER_FONT_HEIGHT * scale, bg);
    }
}

static void launcher_tft_draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg, int scale)
{
    if (!text) {
        return;
    }

    int cursor_x = x;
    int advance = (LAUNCHER_FONT_WIDTH + LAUNCHER_CHAR_SPACING) * scale;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        launcher_tft_draw_char(cursor_x, y, text[i], fg, bg, scale);
        cursor_x += advance;
    }
}

static void launcher_tft_draw_text_centered(int y, const char *text, uint16_t fg, uint16_t bg, int scale)
{
    int width = launcher_tft_text_width(text, scale);
    int x = (CONFIG_LAUNCHER_TFT_WIDTH - width) / 2;
    if (x < 0) {
        x = 0;
    }
    launcher_tft_draw_text(x, y, text, fg, bg, scale);
}

static void launcher_tft_copy_with_ellipsis(char *dest, size_t dest_size, const char *src, int max_chars)
{
    if (dest_size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t len = strlen(src);
    if ((int)len <= max_chars) {
        snprintf(dest, dest_size, "%s", src);
        return;
    }

    if (max_chars <= 3) {
        snprintf(dest, dest_size, "%.*s", max_chars, src);
        return;
    }

    snprintf(dest, dest_size, "%.*s...", max_chars - 3, src);
}

static void launcher_tft_draw_header(const char *title)
{
    launcher_tft_fill_rect(0, 0, CONFIG_LAUNCHER_TFT_WIDTH, LAUNCHER_TITLE_BAR_HEIGHT, TFT_NAVY);
    launcher_tft_draw_text_centered(10, title, TFT_CYAN, TFT_NAVY, 2);
}

static void launcher_tft_draw_footer(const char *footer)
{
    int y = CONFIG_LAUNCHER_TFT_HEIGHT - 15;
    launcher_tft_fill_rect(0, y - 2, CONFIG_LAUNCHER_TFT_WIDTH, 14, TFT_BLACK);
    launcher_tft_draw_text_centered(y, footer, TFT_DARKGREY, TFT_BLACK, 1);
}

static void launcher_tft_draw_menu_item(int y, bool active, const char *label)
{
    const uint16_t bg = active ? TFT_CYAN : TFT_BLACK;
    const uint16_t fg = active ? TFT_BLACK : TFT_WHITE;
    char display_label[48];
    int max_chars = (CONFIG_LAUNCHER_TFT_WIDTH - 50) / ((LAUNCHER_FONT_WIDTH + LAUNCHER_CHAR_SPACING) * 2);

    launcher_tft_copy_with_ellipsis(display_label, sizeof(display_label), label, max_chars);
    launcher_tft_fill_rect(LAUNCHER_MENU_SIDE_MARGIN, y, CONFIG_LAUNCHER_TFT_WIDTH - (LAUNCHER_MENU_SIDE_MARGIN * 2), 30, bg);
    launcher_tft_draw_text(25, y + 7, active ? "> " : "  ", fg, bg, 2);
    launcher_tft_draw_text(49, y + 7, display_label, fg, bg, 2);
}

static int launcher_tft_visible_items(void)
{
    int footer_top = CONFIG_LAUNCHER_TFT_HEIGHT - LAUNCHER_MENU_BOTTOM_MARGIN;
    int available = footer_top - LAUNCHER_MENU_TOP;
    int visible = available / LAUNCHER_MENU_ITEM_HEIGHT;
    return visible > 0 ? visible : 1;
}

static void launcher_tft_draw_menu(const char *title,
                                   const char *footer,
                                   const char *const *items,
                                   int item_count,
                                   int selected)
{
    launcher_tft_fill_rect(0, 0, CONFIG_LAUNCHER_TFT_WIDTH, CONFIG_LAUNCHER_TFT_HEIGHT, TFT_BLACK);
    launcher_tft_draw_header(title);

    int visible = launcher_tft_visible_items();
    int first_visible = 0;
    if (selected >= visible) {
        first_visible = selected - visible + 1;
    }

    for (int row = 0; row < visible && (first_visible + row) < item_count; ++row) {
        int index = first_visible + row;
        int y = LAUNCHER_MENU_TOP + row * LAUNCHER_MENU_ITEM_HEIGHT;
        launcher_tft_draw_menu_item(y, index == selected, items[index]);
    }

    launcher_tft_draw_footer(footer);
}

static void launcher_tft_show_message(const char *title, const char *detail, uint16_t color)
{
    launcher_tft_fill_rect(0, 0, CONFIG_LAUNCHER_TFT_WIDTH, CONFIG_LAUNCHER_TFT_HEIGHT, TFT_BLACK);
    launcher_tft_draw_text_centered(CONFIG_LAUNCHER_TFT_HEIGHT / 2 - 20, title ? title : "", color, TFT_BLACK, 2);
    if (detail && detail[0]) {
        char clipped[48];
        int max_chars = (CONFIG_LAUNCHER_TFT_WIDTH - 20) / (LAUNCHER_FONT_WIDTH + LAUNCHER_CHAR_SPACING);
        launcher_tft_copy_with_ellipsis(clipped, sizeof(clipped), detail, max_chars);
        launcher_tft_draw_text_centered(CONFIG_LAUNCHER_TFT_HEIGHT / 2 + 10, clipped, TFT_DARKGREY, TFT_BLACK, 1);
    }
}

static esp_err_t launcher_tft_send_init_sequence(void)
{
    static const uint8_t cmd_f0_c3[] = { 0xC3 };
    static const uint8_t cmd_f0_96[] = { 0x96 };
    static const uint8_t cmd_36[] = { 0x28 };
    static const uint8_t cmd_3a[] = { 0x55 };
    static const uint8_t cmd_b4[] = { 0x01 };
    static const uint8_t cmd_b7[] = { 0xC6 };
    static const uint8_t cmd_e8[] = { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 };
    static const uint8_t cmd_c1[] = { 0x06 };
    static const uint8_t cmd_c2[] = { 0xA7 };
    static const uint8_t cmd_c5[] = { 0x18 };
    static const uint8_t cmd_e0[] = { 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B };
    static const uint8_t cmd_e1[] = { 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B };

    launcher_tft_write_cmd(0xF0);
    launcher_tft_write_data(cmd_f0_c3, sizeof(cmd_f0_c3));
    launcher_tft_write_cmd(0xF0);
    launcher_tft_write_data(cmd_f0_96, sizeof(cmd_f0_96));
    launcher_tft_write_cmd(0x36);
    launcher_tft_write_data(cmd_36, sizeof(cmd_36));
    launcher_tft_write_cmd(0x3A);
    launcher_tft_write_data(cmd_3a, sizeof(cmd_3a));
    launcher_tft_write_cmd(0xB4);
    launcher_tft_write_data(cmd_b4, sizeof(cmd_b4));
    launcher_tft_write_cmd(0xB7);
    launcher_tft_write_data(cmd_b7, sizeof(cmd_b7));
    launcher_tft_write_cmd(0xE8);
    launcher_tft_write_data(cmd_e8, sizeof(cmd_e8));
    launcher_tft_write_cmd(0xC1);
    launcher_tft_write_data(cmd_c1, sizeof(cmd_c1));
    launcher_tft_write_cmd(0xC2);
    launcher_tft_write_data(cmd_c2, sizeof(cmd_c2));
    launcher_tft_write_cmd(0xC5);
    launcher_tft_write_data(cmd_c5, sizeof(cmd_c5));
    launcher_tft_write_cmd(0xE0);
    launcher_tft_write_data(cmd_e0, sizeof(cmd_e0));
    launcher_tft_write_cmd(0xE1);
    launcher_tft_write_data(cmd_e1, sizeof(cmd_e1));

    launcher_tft_write_cmd(0x21);
    launcher_tft_write_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    launcher_tft_write_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static esp_err_t launcher_tft_init(void)
{
    if (s_tft_attempted) {
        return s_tft_ready ? ESP_OK : ESP_FAIL;
    }
    s_tft_attempted = true;

#if !CONFIG_LAUNCHER_ENABLE_TFT_UI
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_err_t ret;

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_LAUNCHER_PIN_TFT_DC)
                        | (1ULL << CONFIG_LAUNCHER_PIN_TFT_RST)
                        | (1ULL << CONFIG_LAUNCHER_PIN_TFT_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&out_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Display GPIO init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = CONFIG_LAUNCHER_PIN_TFT_MOSI,
        .miso_io_num = CONFIG_LAUNCHER_PIN_TFT_MISO,
        .sclk_io_num = CONFIG_LAUNCHER_PIN_TFT_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CONFIG_LAUNCHER_TFT_WIDTH * LAUNCHER_STATUS_ROWS * sizeof(uint16_t),
    };
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Display SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CONFIG_LAUNCHER_PIN_TFT_CS,
        .queue_size = 1,
    };
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &s_tft_spi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Display SPI device init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(CONFIG_LAUNCHER_PIN_TFT_BL, 0);
    gpio_set_level(CONFIG_LAUNCHER_PIN_TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(CONFIG_LAUNCHER_PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    s_tft_ready = true;
    ret = launcher_tft_send_init_sequence();
    if (ret != ESP_OK) {
        s_tft_ready = false;
        return ret;
    }

    gpio_set_level(CONFIG_LAUNCHER_PIN_TFT_BL, 1);
    launcher_tft_fill_rect(0, 0, CONFIG_LAUNCHER_TFT_WIDTH, CONFIG_LAUNCHER_TFT_HEIGHT, TFT_BLACK);
    return ESP_OK;
#endif
}

void launcher_ui_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_CLK)
                        | (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_DT)
                        | (1ULL << CONFIG_LAUNCHER_PIN_ENCODER_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&cfg);

    esp_err_t ret = launcher_tft_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Falling back to Serial launcher UI");
        s_tft_ready = false;
    }
}

static void launcher_print_main_menu(const char *app_label, int selected)
{
    char boot_label[96];
    const char *items[2];

    snprintf(boot_label, sizeof(boot_label), "Boot %s", app_label && app_label[0] ? app_label : "Current Firmware");
    items[0] = boot_label;
    items[1] = "SD Card Browser";

    if (s_tft_ready) {
        launcher_tft_draw_menu("OS LAUNCHER", "ROT: Navigate | CLICK: Confirm", items, 2, selected);
        return;
    }

    launcher_serial_clear_screen();
    launcher_serial_print_header("OS Launcher", "Standard launcher menu");
    launcher_serial_print_menu_item(0, boot_label, selected == 0, "Start the currently installed app");
    launcher_serial_print_menu_item(1, "SD Card Browser", selected == 1, "Flash another .bin from the SD card");
    launcher_serial_print_footer("Rotate: Select | Press: Confirm");
}

static void launcher_print_firmware_menu(const firmware_entry_t *entries, size_t count, int selected)
{
    const char *items[MAX_FW_FILES + 1];
    items[0] = "Back";
    for (size_t i = 0; i < count; ++i) {
        items[i + 1] = entries[i].name;
    }

    if (s_tft_ready) {
        launcher_tft_draw_menu("SELECT FIRMWARE", "ROT: Navigate | CLICK: Flash", items, (int)count + 1, selected);
        return;
    }

    launcher_serial_clear_screen();
    launcher_serial_print_header("Select Firmware", CONFIG_LAUNCHER_FW_DIR);
    launcher_serial_print_menu_item(0, "Back", selected == 0, "Return to launcher start menu");

    for (size_t i = 0; i < count; i++) {
        char size_label[32];
        snprintf(size_label, sizeof(size_label), "%u KB", (unsigned)(entries[i].size / 1024));
        launcher_serial_print_menu_item((int)i + 1, entries[i].name, selected == (int)i + 1, size_label);
    }

    launcher_serial_print_footer("Rotate: Select | Press: Flash");
}

static int launcher_wait_for_selection(int item_count, launcher_draw_cb_t draw_fn, void *ctx)
{
    if (item_count <= 0) {
        return -1;
    }
    int selected = 0;
    int last_clk = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_CLK);
    int last_sw = 1;

    draw_fn(selected, ctx);

    while (1) {
        int clk = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_CLK);
        if (clk != last_clk && clk == 1) {
            if (gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_DT) != clk) {
                selected = (selected + 1) % item_count;
            } else {
                selected = (selected - 1 + item_count) % item_count;
            }
            draw_fn(selected, ctx);
        }
        last_clk = clk;

        int sw = gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW);
        if (last_sw == 1 && sw == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW) == 0) {
                while (gpio_get_level(CONFIG_LAUNCHER_PIN_ENCODER_SW) == 0) {
                    vTaskDelay(1);
                }
                return selected;
            }
        }
        last_sw = sw;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

typedef struct {
    const char *app_label;
} main_menu_ctx_t;

typedef struct {
    const firmware_entry_t *entries;
    size_t count;
} firmware_menu_ctx_t;

static void draw_main_menu_cb(int selected, void *ctx)
{
    main_menu_ctx_t *menu = (main_menu_ctx_t *)ctx;
    launcher_print_main_menu(menu->app_label, selected);
}

static void draw_firmware_menu_cb(int selected, void *ctx)
{
    firmware_menu_ctx_t *menu = (firmware_menu_ctx_t *)ctx;
    launcher_print_firmware_menu(menu->entries, menu->count, selected);
}

int launcher_ui_select_main_menu(const char *app_label)
{
    main_menu_ctx_t ctx = {
        .app_label = app_label,
    };
    return launcher_wait_for_selection(2, draw_main_menu_cb, &ctx);
}

int launcher_ui_select_firmware(const firmware_entry_t *entries, size_t count)
{
    if (count == 0) {
        return -1;
    }

    firmware_menu_ctx_t ctx = {
        .entries = entries,
        .count = count,
    };
    int selected = launcher_wait_for_selection((int)count + 1, draw_firmware_menu_cb, &ctx);
    return selected == 0 ? -1 : selected - 1;
}

void launcher_ui_show_message(const char *title, const char *detail, bool is_error)
{
    if (s_tft_ready) {
        launcher_tft_show_message(title, detail, is_error ? TFT_RED : TFT_CYAN);
    }
    launcher_serial_show_message(title, detail, is_error);
}
