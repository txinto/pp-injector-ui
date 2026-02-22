#include <sdkconfig.h>

#include "TouchScreen_display.h"

#include "waveshare_rgb_lcd_port.h"

esp_err_t TouchScreen_display_init(void)
{
    return waveshare_esp32_s3_rgb_lcd_init();
}

esp_err_t TouchScreen_display_boot_init(void)
{
    return waveshare_esp32_s3_rgb_lcd_init_nolvgl();
}

esp_err_t TouchScreen_display_boot_clear(uint16_t color)
{
    return waveshare_esp32_s3_rgb_lcd_clear(color);
}

esp_err_t TouchScreen_display_boot_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    return waveshare_esp32_s3_rgb_lcd_fill_rect(x, y, w, h, color);
}

esp_err_t TouchScreen_display_boot_draw_center_text(const char *text, uint16_t fg, uint16_t bg)
{
    return waveshare_esp32_s3_rgb_lcd_draw_center_text(text, fg, bg);
}

int TouchScreen_display_boot_width(void)
{
    return waveshare_esp32_s3_rgb_lcd_width();
}

int TouchScreen_display_boot_height(void)
{
    return waveshare_esp32_s3_rgb_lcd_height();
}
