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

#ifdef __cplusplus
}
#endif

