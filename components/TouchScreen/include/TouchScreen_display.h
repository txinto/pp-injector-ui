#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the physical display + touch + LVGL port bindings for the current
 * board backend.
 *
 * On success, LVGL must be usable and lvgl_port_lock()/unlock() must work.
 */
esp_err_t TouchScreen_display_init(void);

/**
 * Initialize display backend in a lightweight mode (no LVGL, no touch).
 */
esp_err_t TouchScreen_display_boot_init(void);

/**
 * Fill full display with a solid RGB565 color.
 */
esp_err_t TouchScreen_display_boot_clear(uint16_t color);

/**
 * Draw a filled rectangle in RGB565.
 */
esp_err_t TouchScreen_display_boot_fill_rect(int x, int y, int w, int h, uint16_t color);

/**
 * Draw centered text using a tiny built-in font.
 */
esp_err_t TouchScreen_display_boot_draw_center_text(const char *text, uint16_t fg, uint16_t bg);

int TouchScreen_display_boot_width(void);
int TouchScreen_display_boot_height(void);

#ifdef __cplusplus
}
#endif
