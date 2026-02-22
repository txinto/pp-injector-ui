#include <sdkconfig.h>

#include "TouchScreen_display.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_types.h"

#include "lvgl_port.h"

static const char *TAG = "TouchScreen4DS";

// GEN4-ESP32-43CT-CLB (4DSystems) pinout & timings (from GFX4dESP32 library)
#define GEN4_RGB_BK_LIGHT_ON_LEVEL 1
#define GEN4_RGB_PIN_NUM_BK_LIGHT  2

#define GEN4_RGB_PIN_NUM_HSYNC     39
#define GEN4_RGB_PIN_NUM_VSYNC     41
#define GEN4_RGB_PIN_NUM_DE        40
#define GEN4_RGB_PIN_NUM_PCLK      42
#define GEN4_RGB_PIN_NUM_DISP_EN   (-1)

#define GEN4_RGB_PIN_NUM_DATA0     8
#define GEN4_RGB_PIN_NUM_DATA1     3
#define GEN4_RGB_PIN_NUM_DATA2     46
#define GEN4_RGB_PIN_NUM_DATA3     9
#define GEN4_RGB_PIN_NUM_DATA4     1
#define GEN4_RGB_PIN_NUM_DATA5     5
#define GEN4_RGB_PIN_NUM_DATA6     6
#define GEN4_RGB_PIN_NUM_DATA7     7
#define GEN4_RGB_PIN_NUM_DATA8     15
#define GEN4_RGB_PIN_NUM_DATA9     16
#define GEN4_RGB_PIN_NUM_DATA10    4
#define GEN4_RGB_PIN_NUM_DATA11    45
#define GEN4_RGB_PIN_NUM_DATA12    48
#define GEN4_RGB_PIN_NUM_DATA13    47
#define GEN4_RGB_PIN_NUM_DATA14    21
#define GEN4_RGB_PIN_NUM_DATA15    14

#define GEN4_RGB_H_RES             800
#define GEN4_RGB_V_RES             480
#define GEN4_43CT_PIXEL_CLOCK_HZ   (16 * 1000 * 1000)

// Touch controller (from GFX4dESP32 sources): I2C @ 0x38, data starts at reg 0x02
#define GEN4_TOUCH_I2C_PORT        I2C_NUM_0
#define GEN4_TOUCH_I2C_SDA         17
#define GEN4_TOUCH_I2C_SCL         18
#define GEN4_TOUCH_I2C_FREQ_HZ     400000
#define GEN4_TOUCH_ADDR            0x38
#define GEN4_TOUCH_REG             0x02

// IO expander (from gfx4desp32_rgb_panel.h)
#define GEN4_IOX_ADDR              0x39
#define GEN4_IOX_INPUT_PORT_REG    0x00
#define GEN4_IOX_TOUCH_INT_BIT     6  // 1 = no touch, 0 = touch (active-low)

static bool s_i2c_inited = false;
static lv_indev_t *s_indev = NULL;

static esp_err_t touch_i2c_init(void)
{
    if (s_i2c_inited) return ESP_OK;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GEN4_TOUCH_I2C_SDA,
        .scl_io_num = GEN4_TOUCH_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = GEN4_TOUCH_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(GEN4_TOUCH_I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(GEN4_TOUCH_I2C_PORT, conf.mode, 0, 0, 0));
    s_i2c_inited = true;
    return ESP_OK;
}

static esp_err_t ioexp_touch_int_read(bool *out_notouch)
{
    if (!out_notouch) return ESP_ERR_INVALID_ARG;
    uint8_t reg = GEN4_IOX_INPUT_PORT_REG;
    uint8_t val = 0xFF;
    esp_err_t err = i2c_master_write_read_device(
        GEN4_TOUCH_I2C_PORT,
        GEN4_IOX_ADDR,
        &reg,
        1,
        &val,
        1,
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) return err;
    *out_notouch = ((val >> GEN4_IOX_TOUCH_INT_BIT) & 0x01) != 0;
    return ESP_OK;
}

static esp_err_t touch_read(uint16_t *out_x, uint16_t *out_y, bool *out_pressed)
{
    if (!out_x || !out_y || !out_pressed) return ESP_ERR_INVALID_ARG;
    *out_pressed = false;

    // Fast path: if the IO expander says "no touch" and we're not already pressed,
    // skip reading the touch controller (saves I2C bandwidth/CPU).
    static bool last_pressed = false;
    bool no_touch = false;
    if (ioexp_touch_int_read(&no_touch) == ESP_OK && no_touch && !last_pressed) {
        return ESP_OK;
    }

    uint8_t reg = GEN4_TOUCH_REG;
    uint8_t buf[6] = {0};
    esp_err_t err = i2c_master_write_read_device(
        GEN4_TOUCH_I2C_PORT,
        GEN4_TOUCH_ADDR,
        &reg,
        1,
        buf,
        sizeof(buf),
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) return err;

    uint8_t tps = buf[0];
    if (tps == 0xFF || tps == 0) {
        last_pressed = false;
        return ESP_OK;
    }

    uint16_t y = (uint16_t)(((buf[1] & 0x0F) << 8) | buf[2]);
    uint16_t x = (uint16_t)(((buf[3] & 0x0F) << 8) | buf[4]);

    if (x >= GEN4_RGB_H_RES) x = GEN4_RGB_H_RES - 1;
    if (y >= GEN4_RGB_V_RES) y = GEN4_RGB_V_RES - 1;

    *out_x = x;
    *out_y = y;
    *out_pressed = true;
    last_pressed = true;
    return ESP_OK;
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    uint16_t x = 0, y = 0;
    bool pressed = false;
    if (touch_read(&x, &y, &pressed) == ESP_OK && pressed) {
        last_x = x;
        last_y = y;
        data->state = (lv_indev_state_t)1;
    } else {
        data->state = (lv_indev_state_t)0;
    }
    data->point.x = (lv_coord_t)last_x;
    data->point.y = (lv_coord_t)last_y;
    data->continue_reading = false;
}

IRAM_ATTR static bool rgb_lcd_on_vsync_event(esp_lcd_panel_handle_t panel,
                                            const esp_lcd_rgb_panel_event_data_t *edata,
                                            void *user_ctx)
{
    (void)panel;
    (void)edata;
    (void)user_ctx;
    return lvgl_port_notify_rgb_vsync();
}

static void backlight_on(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GEN4_RGB_PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(GEN4_RGB_PIN_NUM_BK_LIGHT, GEN4_RGB_BK_LIGHT_ON_LEVEL);
}

esp_err_t TouchScreen_display_init(void)
{
    ESP_LOGI(TAG, "Install RGB LCD panel driver (4DSystems GEN4-ESP32-43CT-CLB)");
    esp_lcd_panel_handle_t panel_handle = NULL;

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = GEN4_43CT_PIXEL_CLOCK_HZ,
            .h_res = GEN4_RGB_H_RES,
            .v_res = GEN4_RGB_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags = {
                .vsync_idle_low = 1,
                .pclk_active_neg = 1,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = LVGL_PORT_LCD_RGB_BUFFER_NUMS,
        .bounce_buffer_size_px = GEN4_RGB_H_RES * CONFIG_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = GEN4_RGB_PIN_NUM_HSYNC,
        .vsync_gpio_num = GEN4_RGB_PIN_NUM_VSYNC,
        .de_gpio_num = GEN4_RGB_PIN_NUM_DE,
        .pclk_gpio_num = GEN4_RGB_PIN_NUM_PCLK,
        .disp_gpio_num = GEN4_RGB_PIN_NUM_DISP_EN,
        .data_gpio_nums = {
            GEN4_RGB_PIN_NUM_DATA0,
            GEN4_RGB_PIN_NUM_DATA1,
            GEN4_RGB_PIN_NUM_DATA2,
            GEN4_RGB_PIN_NUM_DATA3,
            GEN4_RGB_PIN_NUM_DATA4,
            GEN4_RGB_PIN_NUM_DATA5,
            GEN4_RGB_PIN_NUM_DATA6,
            GEN4_RGB_PIN_NUM_DATA7,
            GEN4_RGB_PIN_NUM_DATA8,
            GEN4_RGB_PIN_NUM_DATA9,
            GEN4_RGB_PIN_NUM_DATA10,
            GEN4_RGB_PIN_NUM_DATA11,
            GEN4_RGB_PIN_NUM_DATA12,
            GEN4_RGB_PIN_NUM_DATA13,
            GEN4_RGB_PIN_NUM_DATA14,
            GEN4_RGB_PIN_NUM_DATA15,
        },
        .flags = {
            .refresh_on_demand = 0,
            .fb_in_psram = 1,
            .double_fb = 0,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    backlight_on();

    ESP_ERROR_CHECK(touch_i2c_init());

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, NULL));
    // Register LVGL touch input device (polling I2C controller at 0x38)
    if (lvgl_port_lock(1000)) {
        s_indev = lv_indev_create();
        if (!s_indev) {
            ESP_LOGW(TAG, "lv_indev_create failed");
        } else {
            lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(s_indev, lvgl_touch_read_cb);
        }
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "lvgl_port_lock failed, touch not registered");
    }

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = rgb_lcd_on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));

    return ESP_OK;
}

esp_err_t TouchScreen_display_boot_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t TouchScreen_display_boot_clear(uint16_t color)
{
    (void)color;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t TouchScreen_display_boot_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t TouchScreen_display_boot_draw_center_text(const char *text, uint16_t fg, uint16_t bg)
{
    (void)text; (void)fg; (void)bg;
    return ESP_ERR_NOT_SUPPORTED;
}

int TouchScreen_display_boot_width(void)
{
    return 0;
}

int TouchScreen_display_boot_height(void)
{
    return 0;
}
