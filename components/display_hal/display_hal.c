#include <stdio.h>
#include "display_hal.h"
#include "user_assets.h"
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

static const uint8_t font_dash[8]  = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00};
static const uint8_t font_slash[8] = {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00};
static const uint8_t font_space[8] = {0};
static const uint8_t font_star[8]  = {0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00};
static const uint8_t font_letters[26][8] = {
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
};

static SemaphoreHandle_t flush_sem = NULL;

static const uint8_t *get_glyph(char c) {
    if (c >= '0' && c <= '9') return font_digits[c - '0'];
    if (c == ':') return font_digits[10];
    if (c == '-') return font_dash;
    if (c == '/') return font_slash;
    if (c == ' ') return font_space;
    if (c == '*') return font_star;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return font_letters[c - 'A'];
    return NULL;
}

static int glyph_width(char c) {
    if (c == ':') return 2;
    if (c == '/') return 4;
    if (c == ' ' || c == '*') return 5;
    return 6;
}

static int text_width_px(const char *text, int scale) {
    int width = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        width += (glyph_width(text[i]) + 1) * scale;
    }
    return width > 0 ? width - scale : 0;
}

static void dim_rgb565_buffer(uint8_t *buffer, size_t pixel_count, uint8_t brightness_percent) {
    for (size_t i = 0; i < pixel_count; i++) {
        uint16_t raw = ((uint16_t)buffer[i * 2] << 8) | buffer[i * 2 + 1];
        uint16_t r = ((raw >> 11) & 0x1F) * brightness_percent / 100;
        uint16_t g = ((raw >> 5) & 0x3F) * brightness_percent / 100;
        uint16_t b = (raw & 0x1F) * brightness_percent / 100;
        uint16_t dimmed = (r << 11) | (g << 5) | b;
        buffer[i * 2] = (uint8_t)(dimmed >> 8);
        buffer[i * 2 + 1] = (uint8_t)(dimmed & 0xFF);
    }
}

static bool notify_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(flush_sem, &need_yield);
    return (need_yield == pdTRUE);
}

esp_lcd_panel_handle_t display_hal_init(void) {
    // 1. Init SPI
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // 2. Init IO
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

    // 3. Register DMA Callback
    flush_sem = xSemaphoreCreateBinary();
    const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = notify_flush_ready };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, NULL);

    // 4. Init Panel
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

void display_draw_asset_image(esp_lcd_panel_handle_t panel_handle, size_t photo_index) {
    if (!user_assets_ready() || user_assets_photo_count() == 0) {
        display_fill_rect(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0x0000);
        return;
    }

    size_t chunk_size = LCD_WIDTH * CHUNK_ROWS * 2;
    uint8_t *buffer = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
    if (!buffer) return;

    for (int y = 0; y < LCD_HEIGHT; y += CHUNK_ROWS) {
        uint16_t rows = (LCD_HEIGHT - y < CHUNK_ROWS) ? (LCD_HEIGHT - y) : CHUNK_ROWS;
        size_t bytes = LCD_WIDTH * rows * 2;
        if (user_assets_read_photo_rows(photo_index, y, rows, buffer, bytes) != ESP_OK) {
            break;
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y + rows, buffer);
        xSemaphoreTake(flush_sem, portMAX_DELAY);
    }
    free(buffer);
}

void display_draw_asset_image_dimmed(esp_lcd_panel_handle_t panel_handle,
                                     size_t photo_index, uint8_t brightness_percent) {
    if (!user_assets_ready() || user_assets_photo_count() == 0) {
        display_fill_rect(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0x0000);
        return;
    }

    size_t chunk_size = LCD_WIDTH * CHUNK_ROWS * 2;
    uint8_t *buffer = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA);
    if (!buffer) return;

    for (int y = 0; y < LCD_HEIGHT; y += CHUNK_ROWS) {
        uint16_t rows = (LCD_HEIGHT - y < CHUNK_ROWS) ? (LCD_HEIGHT - y) : CHUNK_ROWS;
        size_t bytes = LCD_WIDTH * rows * 2;
        if (user_assets_read_photo_rows(photo_index, y, rows, buffer, bytes) != ESP_OK) {
            break;
        }
        dim_rgb565_buffer(buffer, LCD_WIDTH * rows, brightness_percent);
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y + rows, buffer);
        xSemaphoreTake(flush_sem, portMAX_DELAY);
    }
    free(buffer);
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
        const uint8_t *glyph = get_glyph(text[i]);
        if (!glyph) continue;

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

void display_draw_text_on_photo(esp_lcd_panel_handle_t panel, size_t photo_index,
                                int y, const char *text, int scale,
                                uint16_t color) {
    display_draw_text_on_photo_dimmed(panel, photo_index, y, text, scale, color, 100);
}

void display_draw_text_on_photo_dimmed(esp_lcd_panel_handle_t panel, size_t photo_index,
                                       int y, const char *text, int scale,
                                       uint16_t color, uint8_t brightness_percent) {
    int len = strlen(text);
    int total_w = text_width_px(text, scale);
    int h = 8 * scale;
    int start_x = (LCD_WIDTH - total_w) / 2;
    if (start_x < 0 || y < 0 || y + h > LCD_HEIGHT) return;

    uint16_t *buf = heap_caps_malloc(total_w * h * 2, MALLOC_CAP_DMA);
    uint16_t *rows = heap_caps_malloc(LCD_WIDTH * h * 2, MALLOC_CAP_DMA);
    if (!buf || !rows) {
        free(buf);
        free(rows);
        return;
    }

    if (user_assets_ready() && user_assets_photo_count() > 0 &&
        user_assets_read_photo_rows(photo_index, y, h, rows, LCD_WIDTH * h * 2) == ESP_OK) {
        for (int py = 0; py < h; py++) {
            memcpy(&buf[py * total_w], &rows[py * LCD_WIDTH + start_x], total_w * 2);
        }
        dim_rgb565_buffer((uint8_t *)buf, total_w * h, brightness_percent);
    } else {
        for (int i = 0; i < total_w * h; i++) buf[i] = 0x0000;
    }

    int cursor_x = 0;
    for (int i = 0; i < len; i++) {
        const uint8_t *glyph = get_glyph(text[i]);
        int glyph_w = glyph_width(text[i]);
        if (!glyph) {
            cursor_x += (glyph_w + 1) * scale;
            continue;
        }
        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < glyph_w; px++) {
                if (glyph[py] & (1 << (7 - px))) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            int bx = cursor_x + (px * scale) + sx;
                            int by = (py * scale) + sy;
                            buf[by * total_w + bx] = color;
                        }
                    }
                }
            }
        }
        cursor_x += (glyph_w + 1) * scale;
    }

    esp_lcd_panel_draw_bitmap(panel, start_x, y, start_x + total_w, y + h, buf);
    xSemaphoreTake(flush_sem, portMAX_DELAY);
    free(rows);
    free(buf);
}