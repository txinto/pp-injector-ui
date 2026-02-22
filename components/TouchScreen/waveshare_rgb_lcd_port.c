/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include <string.h>
#include "esp_err.h"


static const char *TAG = "waveshare_rgb_lcd_port";
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static bool s_panel_initialized = false;
static bool s_lvgl_initialized = false;

/* Keep boot/no-LVGL display as light as possible on internal DMA RAM usage. */
#define BOOT_RGB_NUM_FBS               1
#define BOOT_RGB_BOUNCE_BUFFER_LINES   2
#define BOOT_RGB_BOUNCE_BUFFER_SIZE_PX (LCD_H_RES * BOOT_RGB_BOUNCE_BUFFER_LINES)

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
    if (s_panel_initialized && s_lvgl_initialized) {
        return ESP_OK;
    }

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
    if (!s_panel_initialized) {
        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

        ESP_LOGI(TAG, "Initialize RGB LCD panel");         // Log the initialization of the RGB LCD panel
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // Initialize the LCD panel
        s_panel_handle = panel_handle;
        s_panel_initialized = true;
    } else {
        panel_handle = s_panel_handle;
    }

    if (s_lvgl_initialized) {
        panel_backlight_enable();
        return ESP_OK;
    }

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
            .swap_xy = 0,  // Keep X/Y axis
            .mirror_x = 1, // Rotate 180º: mirror X
            .mirror_y = 1, // Rotate 180º: mirror Y
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle)); // Create new I2C GT911 touch controller
#endif                                                                               // CONFIG_LCD_TOUCH_CONTROLLER_GT911

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle)); // Initialize LVGL with the panel and touch handles
    /* Rotate display output 180º (top<->bottom and left<->right). */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

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
    s_lvgl_initialized = true;

    return ESP_OK; // Return success
}

esp_err_t waveshare_esp32_s3_rgb_lcd_init_nolvgl()
{
    if (s_panel_initialized) {
        panel_backlight_enable();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Install RGB LCD panel driver (no LVGL)");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = CONFIG_TOUCHSCREEN_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = CONFIG_TOUCHSCREEN_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = CONFIG_TOUCHSCREEN_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = CONFIG_TOUCHSCREEN_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = CONFIG_TOUCHSCREEN_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = CONFIG_TOUCHSCREEN_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width = RGB_DATA_WIDTH,
        .bits_per_pixel = RGB_BIT_PER_PIXEL,
        .num_fbs = BOOT_RGB_NUM_FBS,
        .bounce_buffer_size_px = BOOT_RGB_BOUNCE_BUFFER_SIZE_PX,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = LCD_IO_RGB_HSYNC,
        .vsync_gpio_num = LCD_IO_RGB_VSYNC,
        .de_gpio_num = LCD_IO_RGB_DE,
        .pclk_gpio_num = LCD_IO_RGB_PCLK,
        .disp_gpio_num = LCD_IO_RGB_DISP,
        .data_gpio_nums = {
            LCD_IO_RGB_DATA0, LCD_IO_RGB_DATA1, LCD_IO_RGB_DATA2, LCD_IO_RGB_DATA3,
            LCD_IO_RGB_DATA4, LCD_IO_RGB_DATA5, LCD_IO_RGB_DATA6, LCD_IO_RGB_DATA7,
            LCD_IO_RGB_DATA8, LCD_IO_RGB_DATA9, LCD_IO_RGB_DATA10, LCD_IO_RGB_DATA11,
            LCD_IO_RGB_DATA12, LCD_IO_RGB_DATA13, LCD_IO_RGB_DATA14, LCD_IO_RGB_DATA15,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot display: esp_lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_lcd_panel_init(panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot display: esp_lcd_panel_init failed: %s", esp_err_to_name(err));
        return err;
    }
    /* Boot display must match the same 180º orientation as LVGL mode. */
    err = esp_lcd_panel_mirror(panel_handle, true, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot display: esp_lcd_panel_mirror failed: %s", esp_err_to_name(err));
        return err;
    }

    s_panel_handle = panel_handle;
    s_panel_initialized = true;
    panel_backlight_enable();
    return ESP_OK;
}

static esp_err_t lcd_draw_hline(int x, int y, int w, uint16_t color)
{
    if (!s_panel_initialized || !s_panel_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (w <= 0) return ESP_OK;

    static uint16_t *row = NULL;
    static int row_cap = 0;
    if (row_cap < w) {
        uint16_t *new_row = (uint16_t *)heap_caps_realloc(row, sizeof(uint16_t) * w, MALLOC_CAP_8BIT);
        if (!new_row) {
            return ESP_ERR_NO_MEM;
        }
        row = new_row;
        row_cap = w;
    }
    for (int i = 0; i < w; i++) row[i] = color;
    return esp_lcd_panel_draw_bitmap(s_panel_handle, x, y, x + w, y + 1, row);
}

esp_err_t waveshare_esp32_s3_rgb_lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_panel_initialized || !s_panel_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (w <= 0 || h <= 0) return ESP_OK;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= LCD_H_RES || y >= LCD_V_RES) return ESP_OK;
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;
    if (w <= 0 || h <= 0) return ESP_OK;

    for (int yy = y; yy < y + h; yy++) {
        esp_err_t err = lcd_draw_hline(x, yy, w, color);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t waveshare_esp32_s3_rgb_lcd_clear(uint16_t color)
{
    return waveshare_esp32_s3_rgb_lcd_fill_rect(0, 0, LCD_H_RES, LCD_V_RES, color);
}

static const uint8_t *font5x7(char c)
{
    static const uint8_t SPACE[7] = {0,0,0,0,0,0,0};
    static const uint8_t DASH[7]  = {0,0,0,0x1F,0,0,0};
    static const uint8_t UNDERSCORE[7] = {0,0,0,0,0,0,0x1F};
    static const uint8_t DOT[7]   = {0,0,0,0,0,0x0C,0x0C};
    static const uint8_t COLON[7] = {0,0x0C,0x0C,0,0x0C,0x0C,0};
    static const uint8_t SLASH[7] = {0x01,0x02,0x04,0x08,0x10,0,0};
    static const uint8_t ZERO[7]  = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
    static const uint8_t ONE[7]   = {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F};
    static const uint8_t TWO[7]   = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
    static const uint8_t THREE[7] = {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E};
    static const uint8_t FOUR[7]  = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
    static const uint8_t FIVE[7]  = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E};
    static const uint8_t SIX[7]   = {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E};
    static const uint8_t SEVEN[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x10};
    static const uint8_t EIGHT[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
    static const uint8_t NINE[7]  = {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E};
    static const uint8_t A[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const uint8_t B[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
    static const uint8_t C[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
    static const uint8_t D[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    static const uint8_t E[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    static const uint8_t F[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
    static const uint8_t G[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0E};
    static const uint8_t H[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const uint8_t I[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
    static const uint8_t J[7]={0x1F,0x02,0x02,0x02,0x12,0x12,0x0C};
    static const uint8_t K[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    static const uint8_t L[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
    static const uint8_t M[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
    static const uint8_t N[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11};
    static const uint8_t O[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const uint8_t P[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
    static const uint8_t Q[7]={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
    static const uint8_t R[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    static const uint8_t S[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    static const uint8_t T[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
    static const uint8_t U[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const uint8_t V[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
    static const uint8_t W[7]={0x11,0x11,0x11,0x15,0x15,0x15,0x0A};
    static const uint8_t X[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
    static const uint8_t Y[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
    static const uint8_t Z[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};

    switch (c) {
        case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D;
        case 'E': return E; case 'F': return F; case 'G': return G; case 'H': return H;
        case 'I': return I; case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O; case 'P': return P;
        case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
        case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X;
        case 'Y': return Y; case 'Z': return Z;
        case '-': return DASH; case '_': return UNDERSCORE; case '.': return DOT; case ':': return COLON; case '/': return SLASH;
        case '0': return ZERO; case '1': return ONE; case '2': return TWO; case '3': return THREE; case '4': return FOUR;
        case '5': return FIVE; case '6': return SIX; case '7': return SEVEN; case '8': return EIGHT; case '9': return NINE;
        case ' ': return SPACE;
        default: return SPACE;
    }
}

esp_err_t waveshare_esp32_s3_rgb_lcd_draw_center_text(const char *text, uint16_t fg, uint16_t bg)
{
    if (!text) return ESP_ERR_INVALID_ARG;
    esp_err_t err = waveshare_esp32_s3_rgb_lcd_clear(bg);
    if (err != ESP_OK) return err;

    size_t len = strlen(text);
    if (len == 0) return ESP_OK;

    int lines = 1;
    int cur_len = 0;
    int max_line_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            if (cur_len > max_line_len) max_line_len = cur_len;
            cur_len = 0;
            lines++;
        } else {
            cur_len++;
        }
    }
    if (cur_len > max_line_len) max_line_len = cur_len;
    if (max_line_len <= 0) return ESP_OK;

    int scale_w = (LCD_H_RES - 20) / ((max_line_len * 6) - 1);
    int total_rows = (lines * 7) + (lines - 1);
    int scale_h = (LCD_V_RES - 20) / total_rows;
    int scale = scale_w < scale_h ? scale_w : scale_h;
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;

    int text_h = (lines * 7 * scale) + ((lines - 1) * scale);
    int y0 = (LCD_V_RES - text_h) / 2;
    if (y0 < 0) y0 = 0;

    int line = 0;
    int line_len = 0;
    int line_start = 0;
    for (size_t i = 0; i <= len; i++) {
        bool line_end = (i == len) || (text[i] == '\n');
        if (!line_end) {
            line_len++;
            continue;
        }

        int text_w = (line_len > 0) ? ((line_len * 6 * scale) - scale) : 0;
        int x0 = (LCD_H_RES - text_w) / 2;
        if (x0 < 0) x0 = 0;
        int y_line = y0 + line * (8 * scale);

        for (int j = 0; j < line_len; j++) {
            char c = text[line_start + j];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            const uint8_t *g = font5x7(c);
            int gx = x0 + j * (6 * scale);
            for (int r = 0; r < 7; r++) {
                uint8_t row = g[r];
                for (int col = 0; col < 5; col++) {
                    if (row & (1 << (4 - col))) {
                        err = waveshare_esp32_s3_rgb_lcd_fill_rect(gx + col * scale, y_line + r * scale, scale, scale, fg);
                        if (err != ESP_OK) return err;
                    }
                }
            }
        }

        line++;
        line_start = (int)i + 1;
        line_len = 0;
    }
    return ESP_OK;
}

int waveshare_esp32_s3_rgb_lcd_width(void)
{
    return LCD_H_RES;
}

int waveshare_esp32_s3_rgb_lcd_height(void)
{
    return LCD_V_RES;
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
