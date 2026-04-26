#pragma once
#include "esp_lcd_panel_ops.h"
#include <stddef.h>
#include <stdint.h>

// Screen Dimensions
#define LCD_WIDTH  240
#define LCD_HEIGHT 240

// Initialize LittleFS and the ST7789 Panel
esp_lcd_panel_handle_t display_hal_init(void);

// Draw the full-screen image from LittleFS
void display_draw_bin_image(esp_lcd_panel_handle_t panel_handle, const char* filename);

// Draw the indexed user photo from the generated assets partition
void display_draw_asset_image(esp_lcd_panel_handle_t panel_handle, size_t photo_index);

// Draw a solid rectangle (used for clearing the screen or backgrounds)
void display_fill_rect(esp_lcd_panel_handle_t panel_handle, int x, int y, int w, int h, uint16_t color);

void display_draw_text(esp_lcd_panel_handle_t panel, int y, const char* text, int scale, uint16_t color, uint16_t bg);

void display_draw_text_on_photo(esp_lcd_panel_handle_t panel, size_t photo_index,
                                int y, const char *text, int scale,
                                uint16_t color);
