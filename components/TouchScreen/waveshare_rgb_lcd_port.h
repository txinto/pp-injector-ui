#ifndef _RGB_LCD_H_
#define _RGB_LCD_H_

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "lv_demos.h"
#include "lvgl_port.h"


#define I2C_MASTER_SCL_IO           CONFIG_TOUCHSCREEN_TOUCH_I2C_SCL_GPIO
#define I2C_MASTER_SDA_IO           CONFIG_TOUCHSCREEN_TOUCH_I2C_SDA_GPIO
#define I2C_MASTER_NUM              CONFIG_TOUCHSCREEN_TOUCH_I2C_PORT
#define I2C_MASTER_FREQ_HZ          CONFIG_TOUCHSCREEN_TOUCH_I2C_FREQ_HZ
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LCD_H_RES               (LVGL_PORT_H_RES)
#define LCD_V_RES               (LVGL_PORT_V_RES)

#define LCD_PIXEL_CLOCK_HZ      (CONFIG_TOUCHSCREEN_LCD_PIXEL_CLOCK_HZ)

#define LCD_BIT_PER_PIXEL       (16)
#define RGB_BIT_PER_PIXEL       (16)
#define RGB_DATA_WIDTH          (16)
#define RGB_BOUNCE_BUFFER_SIZE  (LCD_H_RES * CONFIG_LCD_RGB_BOUNCE_BUFFER_HEIGHT)
#define LCD_IO_RGB_DISP         (-1)             // -1 if not used
#define LCD_IO_RGB_VSYNC        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_VSYNC_GPIO)
#define LCD_IO_RGB_HSYNC        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_HSYNC_GPIO)
#define LCD_IO_RGB_DE           ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_DE_GPIO)
#define LCD_IO_RGB_PCLK         ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_PCLK_GPIO)
#define LCD_IO_RGB_DATA0        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D0_GPIO)
#define LCD_IO_RGB_DATA1        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D1_GPIO)
#define LCD_IO_RGB_DATA2        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D2_GPIO)
#define LCD_IO_RGB_DATA3        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D3_GPIO)
#define LCD_IO_RGB_DATA4        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D4_GPIO)
#define LCD_IO_RGB_DATA5        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D5_GPIO)
#define LCD_IO_RGB_DATA6        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D6_GPIO)
#define LCD_IO_RGB_DATA7        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D7_GPIO)
#define LCD_IO_RGB_DATA8        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D8_GPIO)
#define LCD_IO_RGB_DATA9        ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D9_GPIO)
#define LCD_IO_RGB_DATA10       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D10_GPIO)
#define LCD_IO_RGB_DATA11       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D11_GPIO)
#define LCD_IO_RGB_DATA12       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D12_GPIO)
#define LCD_IO_RGB_DATA13       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D13_GPIO)
#define LCD_IO_RGB_DATA14       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D14_GPIO)
#define LCD_IO_RGB_DATA15       ((gpio_num_t)CONFIG_TOUCHSCREEN_LCD_RGB_D15_GPIO)

#define LCD_IO_RST              (-1)             // -1 if not used
#define PIN_NUM_TOUCH_RST       CONFIG_TOUCHSCREEN_TOUCH_RESET_GPIO
#define PIN_NUM_TOUCH_INT       CONFIG_TOUCHSCREEN_TOUCH_INT_GPIO

bool example_lvgl_lock(int timeout_ms);
void example_lvgl_unlock(void);

esp_err_t waveshare_esp32_s3_rgb_lcd_init();
esp_err_t waveshare_esp32_s3_rgb_lcd_init_nolvgl();
esp_err_t waveshare_esp32_s3_rgb_lcd_clear(uint16_t color);
esp_err_t waveshare_esp32_s3_rgb_lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
esp_err_t waveshare_esp32_s3_rgb_lcd_draw_center_text(const char *text, uint16_t fg, uint16_t bg);
int waveshare_esp32_s3_rgb_lcd_width(void);
int waveshare_esp32_s3_rgb_lcd_height(void);

esp_err_t wavesahre_rgb_lcd_bl_on();
esp_err_t wavesahre_rgb_lcd_bl_off();

void example_lvgl_demo_ui();

#endif
