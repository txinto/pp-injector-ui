/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"


static const char *TAG = "waveshare_rgb_lcd_port";

// VSYNC event callback function
IRAM_ATTR static bool rgb_lcd_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    return lvgl_port_notify_rgb_vsync();
}

#if CONFIG_LCD_TOUCH_CONTROLLER_GT911
/**
 * @brief I2C master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // Configure I2C parameters
    i2c_param_config(i2c_master_port, &i2c_conf);

    // Install I2C driver
    return i2c_driver_install(i2c_master_port, i2c_conf.mode, 0, 0, 0);
}

// Reset the touch screen
void waveshare_esp32_s3_touch_reset()
{
#if CONFIG_TOUCHSCREEN_TOUCH_RESET_BY_CH422G
    #if CONFIG_TOUCHSCREEN_TOUCH_RESET_SEQ_GPIO >= 0
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_TOUCHSCREEN_TOUCH_RESET_SEQ_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    #endif

    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Reset the touch screen. It is recommended to reset the touch screen before using it.
    write_buf = 0x2C;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(100 * 1000);
    #if CONFIG_TOUCHSCREEN_TOUCH_RESET_SEQ_GPIO >= 0
    gpio_set_level(CONFIG_TOUCHSCREEN_TOUCH_RESET_SEQ_GPIO, 0);
    #endif
    esp_rom_delay_us(100 * 1000);
    write_buf = 0x2E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(200 * 1000);
#else
    ESP_LOGI(TAG, "touch reset sequence via CH422G disabled");
#endif
}

#endif

static void panel_backlight_enable(void)
{
#if CONFIG_TOUCHSCREEN_BACKLIGHT_CONTROL_CH422G
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    write_buf = 0x1E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
#elif CONFIG_TOUCHSCREEN_BACKLIGHT_CONTROL_GPIO
    #if CONFIG_TOUCHSCREEN_BACKLIGHT_GPIO >= 0
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_TOUCHSCREEN_BACKLIGHT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(CONFIG_TOUCHSCREEN_BACKLIGHT_GPIO, CONFIG_TOUCHSCREEN_BACKLIGHT_ON_LEVEL);
    #endif
#endif
}

// Initialize RGB LCD
esp_err_t waveshare_esp32_s3_rgb_lcd_init()
{
    ESP_LOGI(TAG, "Install RGB LCD panel driver"); // Log the start of the RGB LCD panel driver installation
    esp_lcd_panel_handle_t panel_handle = NULL;    // Declare a handle for the LCD panel
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT, // Set the clock source for the panel
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ, // Pixel clock frequency
            .h_res = LCD_H_RES,            // Horizontal resolution
            .v_res = LCD_V_RES,            // Vertical resolution
            .hsync_pulse_width = CONFIG_TOUCHSCREEN_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = CONFIG_TOUCHSCREEN_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = CONFIG_TOUCHSCREEN_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = CONFIG_TOUCHSCREEN_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = CONFIG_TOUCHSCREEN_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = CONFIG_TOUCHSCREEN_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = 1, // Active low pixel clock
            },
        },
        .data_width = RGB_DATA_WIDTH,                    // Data width for RGB
        .bits_per_pixel = RGB_BIT_PER_PIXEL,             // Bits per pixel
        .num_fbs = LVGL_PORT_LCD_RGB_BUFFER_NUMS,                // Number of frame buffers
        .bounce_buffer_size_px = RGB_BOUNCE_BUFFER_SIZE, // Bounce buffer size in pixels
        .sram_trans_align = 4,                                   // SRAM transaction alignment
        .psram_trans_align = 64,                                 // PSRAM transaction alignment
        .hsync_gpio_num = LCD_IO_RGB_HSYNC,              // GPIO number for horizontal sync
        .vsync_gpio_num = LCD_IO_RGB_VSYNC,              // GPIO number for vertical sync
        .de_gpio_num = LCD_IO_RGB_DE,                    // GPIO number for data enable
        .pclk_gpio_num = LCD_IO_RGB_PCLK,                // GPIO number for pixel clock
        .disp_gpio_num = LCD_IO_RGB_DISP,                // GPIO number for display
        .data_gpio_nums = {
            LCD_IO_RGB_DATA0,
            LCD_IO_RGB_DATA1,
            LCD_IO_RGB_DATA2,
            LCD_IO_RGB_DATA3,
            LCD_IO_RGB_DATA4,
            LCD_IO_RGB_DATA5,
            LCD_IO_RGB_DATA6,
            LCD_IO_RGB_DATA7,
            LCD_IO_RGB_DATA8,
            LCD_IO_RGB_DATA9,
            LCD_IO_RGB_DATA10,
            LCD_IO_RGB_DATA11,
            LCD_IO_RGB_DATA12,
            LCD_IO_RGB_DATA13,
            LCD_IO_RGB_DATA14,
            LCD_IO_RGB_DATA15,
        },
        .flags = {
            .fb_in_psram = 1, // Use PSRAM for framebuffer
        },
    };

    // Create a new RGB panel with the specified configuration
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");         // Log the initialization of the RGB LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // Initialize the LCD panel

    esp_lcd_touch_handle_t tp_handle = NULL; // Declare a handle for the touch panel
#if CONFIG_LCD_TOUCH_CONTROLLER_GT911
    ESP_LOGI(TAG, "Initialize I2C bus");   // Log the initialization of the I2C bus
    i2c_master_init();                     // Initialize the I2C master
    ESP_LOGI(TAG, "Initialize Touch LCD"); // Log touch LCD initialization
    waveshare_esp32_s3_touch_reset();      // Reset the touch panel

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;                                          // Declare a handle for touch panel I/O
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(); // Configure I2C for GT911 touch controller

    ESP_LOGI(TAG, "Initialize I2C panel IO");                                                                          // Log I2C panel I/O initialization
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config, &tp_io_handle)); // Create new I2C panel I/O

    ESP_LOGI(TAG, "Initialize touch controller GT911"); // Log touch controller initialization
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,                // Set maximum X coordinate
        .y_max = LCD_V_RES,                // Set maximum Y coordinate
        .rst_gpio_num = PIN_NUM_TOUCH_RST, // GPIO number for reset
        .int_gpio_num = PIN_NUM_TOUCH_INT, // GPIO number for interrupt
        .levels = {
            .reset = 0,     // Reset level
            .interrupt = 0, // Interrupt level
        },
        .flags = {
            .swap_xy = 0,  // No swap of X and Y
            .mirror_x = 0, // No mirroring of X
            .mirror_y = 0, // No mirroring of Y
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle)); // Create new I2C GT911 touch controller
#endif                                                                               // CONFIG_LCD_TOUCH_CONTROLLER_GT911

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle)); // Initialize LVGL with the panel and touch handles

    // Register callbacks for RGB panel events
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
#if RGB_BOUNCE_BUFFER_SIZE > 0
        .on_bounce_frame_finish = rgb_lcd_on_vsync_event, // Callback for bounce frame finish
#else
        .on_vsync = rgb_lcd_on_vsync_event, // Callback for vertical sync
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL)); // Register event callbacks

#if CONFIG_TOUCHSCREEN_BACKLIGHT_ENABLE_AT_INIT
    panel_backlight_enable();
#endif

    return ESP_OK; // Return success
}

/******************************* Turn on the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_on()
{
    panel_backlight_enable();
    return ESP_OK;
}

/******************************* Turn off the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_off()
{
#if CONFIG_TOUCHSCREEN_BACKLIGHT_CONTROL_CH422G
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    write_buf = 0x1A;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
#elif CONFIG_TOUCHSCREEN_BACKLIGHT_CONTROL_GPIO
    #if CONFIG_TOUCHSCREEN_BACKLIGHT_GPIO >= 0
    gpio_set_level(CONFIG_TOUCHSCREEN_BACKLIGHT_GPIO, !CONFIG_TOUCHSCREEN_BACKLIGHT_ON_LEVEL);
    #endif
#endif
    return ESP_OK;
}

void example_lvgl_demo_ui()
{
    // Reserved for optional manual demos. Not used by TouchScreen component runtime.
}
