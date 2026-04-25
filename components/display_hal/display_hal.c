#include <stdio.h>
#include "display_hal.h"
#include "esp_littlefs.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

#define PIN_NUM_MOSI 9
#define PIN_NUM_CLK  10
#define PIN_NUM_CS   2
#define PIN_NUM_DC   4
#define PIN_NUM_RST  5
#define PIN_NUM_BCKL -1

#define CHUNK_ROWS 40

// Minimalistic Font for Numbers and colon (ASCII 48 to 58)
static const uint8_t font_digits[11][8] = {
    {0x3C,0x66,0x66,0x6E,0x76,0x66,0x66,0x3C}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x3E}, // 1
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C}, // 3
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x1E}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x06,0x66,0x3C}, // 5
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x66,0x3C}, // 6
    {0x7E,0x06,0x06,0x0C,0x18,0x18,0x18,0x18}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C}, // 8
    {0x3C,0x66,0x66,0x66,0x3E,0x06,0x66,0x3C}, // 9
    {0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00}  // :
};

static SemaphoreHandle_t flush_sem = NULL;

static bool notify_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(flush_sem, &need_yield);
    return (need_yield == pdTRUE);
}

esp_lcd_panel_handle_t display_hal_init(void) {
    // 1. Mount LittleFS
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_vfs_littlefs_register(&conf);

    // 2. Init SPI
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // 3. Init IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = 40 * 1000 * 1000, 
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,               
        .trans_queue_depth = 10,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);

    // 4. Register DMA Callback
    flush_sem = xSemaphoreCreateBinary();
    const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = notify_flush_ready };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, NULL);

    // 5. Init Panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, 
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    return panel_handle;
}

void display_draw_bin_image(esp_lcd_panel_handle_t panel_handle, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return;

    size_t chunk_size = LCD_WIDTH * CHUNK_ROWS * 2; 
    uint8_t *buffer = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
    
    for (int y = 0; y < LCD_HEIGHT; y += CHUNK_ROWS) {
        fread(buffer, 1, chunk_size, f);
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y + CHUNK_ROWS, buffer);
        xSemaphoreTake(flush_sem, portMAX_DELAY);
    }
    free(buffer);
    fclose(f);
}

void display_fill_rect(esp_lcd_panel_handle_t panel_handle, int x, int y, int w, int h, uint16_t color) {
    size_t chunk_size = w * h * 2;
    uint16_t *buffer = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
    for (int i = 0; i < w * h; i++) buffer[i] = color;
    
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + w, y + h, buffer);
    xSemaphoreTake(flush_sem, portMAX_DELAY);
    free(buffer);
}

void display_draw_text(esp_lcd_panel_handle_t panel, int y, const char* text, int scale, uint16_t color, uint16_t bg) {
    int len = strlen(text);
    int char_w = 8 * scale;
    int total_w = len * char_w;
    int h = 8 * scale;
    int start_x = (LCD_WIDTH - total_w) / 2; // Center horizontally

    uint16_t *buf = heap_caps_malloc(total_w * h * 2, MALLOC_CAP_DMA);
    if (!buf) return;

    for (int i = 0; i < total_w * h; i++) buf[i] = bg; // Fill background

    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c < '0' || c > ':') continue; // Only process numbers and colon
        const uint8_t *glyph = font_digits[c - '0'];

        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < 8; px++) {
                if (glyph[py] & (1 << (7 - px))) {
                    // Draw scaled pixel block
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            int bx = (i * char_w) + (px * scale) + sx;
                            int by = (py * scale) + sy;
                            buf[by * total_w + bx] = color;
                        }
                    }
                }
            }
        }
    }
    
    // Push to screen
    esp_lcd_panel_draw_bitmap(panel, start_x, y, start_x + total_w, y + h, buf);
    
    // THIS LINE FIXES ALL GRAPHICAL BUGS: Wait for hardware to finish before freeing memory!
    xSemaphoreTake(flush_sem, portMAX_DELAY);
    free(buf);
}