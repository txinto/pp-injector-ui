#include <sdkconfig.h>

#include "TouchScreen_display.h"

#include "waveshare_rgb_lcd_port.h"

esp_err_t TouchScreen_display_init(void)
{
    return waveshare_esp32_s3_rgb_lcd_init();
}

