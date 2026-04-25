#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"

// ==========================================
// 1. PIN DEFINITIONS & SCREEN RESOLUTION
// ==========================================
// Change these to match your actual physical wiring!
#define PIN_NUM_MOSI 9
#define PIN_NUM_CLK  10
#define PIN_NUM_CS   2
#define PIN_NUM_DC   4
#define PIN_NUM_RST  5
#define PIN_NUM_BCKL -1

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define CHUNK_ROWS 40  // Number of rows to read into RAM at a time

// ==========================================
// 2. LITTLEFS INITIALIZATION
// ==========================================
void init_littlefs(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage", // Must match the label in partitions.csv
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    printf("Mounting LittleFS...\n");
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount or format filesystem\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("Failed to find LittleFS partition\n");
        } else {
            printf("Failed to initialize LittleFS (%s)\n", esp_err_to_name(ret));
        }
        return;
    }
    printf("LittleFS Mounted Successfully!\n");
}

// ==========================================
// 3. IMAGE DRAWING FUNCTION
// ==========================================
void draw_bin_image(esp_lcd_panel_handle_t panel_handle, const char* filename) {
    printf("Opening image file: %s\n", filename);
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        printf("Failed to open file! Check if it exists in littlefs_data folder.\n");
        return;
    }

    // Allocate a buffer in DMA-capable memory (required for SPI transfers)
    size_t chunk_size_bytes = LCD_WIDTH * CHUNK_ROWS * 2; // *2 because RGB565 is 2 bytes per pixel
    uint8_t *buffer = heap_caps_malloc(chunk_size_bytes, MALLOC_CAP_DMA);
    
    if (buffer == NULL) {
        printf("Failed to allocate DMA buffer!\n");
        fclose(f);
        return;
    }

    // Read and draw the image chunk by chunk from top to bottom
    for (int y = 0; y < LCD_HEIGHT; y += CHUNK_ROWS) {
        fread(buffer, 1, chunk_size_bytes, f);
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y + CHUNK_ROWS, buffer);

        //Stabilize the processor
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }

    free(buffer);
    fclose(f);
    printf("Image drawing complete!\n");
}

// ==========================================
// 4. MAIN APP
// ==========================================
void app_main(void)
{
    printf("Starting Desktop Companion Display Test...\n");

    // Turn on backlight (if your module requires it)
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BCKL, 1); 

    // --- SPI Bus Initialization ---
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1, // Not used for simple display output
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * CHUNK_ROWS * 2 + 8 // Must be at least the size of our chunk buffer
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // --- LCD I/O Initialization ---
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = 40 * 1000 * 1000, // 40 MHz SPI speed
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,               // ST7789 standard
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // --- ST7789 Panel Initialization ---
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, 
        .bits_per_pixel = 16, // RGB565 is 16-bit
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // Reset and initialize the panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // ST7789 displays often require color inversion to look correct
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // Turn the display on
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    printf("Display initialized!\n");

    // --- LittleFS & Image Loading ---
    init_littlefs();
    
    // Draw the image! Ensure the filename matches what is in your littlefs_data folder.
    draw_bin_image(panel_handle, "/littlefs/hk100.bin");

    // --- Infinite Loop ---
    // Keeps FreeRTOS running so the ESP32 doesn't restart
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}