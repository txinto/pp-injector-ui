#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <PrjCfg.h>

// ------------------ BEGIN Return code ------------------
typedef enum {
    TouchScreen_ret_error = -1,
    TouchScreen_ret_ok    = 0
} TouchScreen_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------

// ------------------ END   Datatypes ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    TouchScreen_return_code_t last_return_code;

#include "TouchScreen_netvar_types_fragment.h_"
} TouchScreen_dre_t;

extern TouchScreen_dre_t TouchScreen_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
#if CONFIG_TOUCHSCREEN_USE_THREAD
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
TouchScreen_return_code_t TouchScreen_start(void);

/**
 *  Stop background task gracefully.
 *  Idempotent. Safe to call if not running.
 */
TouchScreen_return_code_t TouchScreen_stop(void);

/**
 *  Thread-safe clone of current DRE state.
 */
TouchScreen_return_code_t TouchScreen_get_dre_clone(TouchScreen_dre_t *dst);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
TouchScreen_return_code_t TouchScreen_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t TouchScreen_get_period_ms(void);

/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
*/
void TouchScreen_execute_function_safemode(void (*callback)());


// ------------------ END   Public API (MULTITASKING)--------------------
#else
// ------------------ BEGIN Public API (SPIN)--------------------
/**
 *  Non-blocking step of this module (call it from your scheduler when
 *  CONFIG_TOUCHSCREEN_USE_THREAD = n).
 */
TouchScreen_return_code_t TouchScreen_spin(void);

// ------------------ END   Public API (SPIN)--------------------
#endif // CONFIG_TOUCHSCREEN_USE_THREAD

// ------------------ BEGIN Public API (COMMON)--------------------

/**
 *  Called at initialization time. Does minimal setup.
 */
TouchScreen_return_code_t TouchScreen_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
TouchScreen_return_code_t TouchScreen_enable(void);
TouchScreen_return_code_t TouchScreen_disable(void);

/**
 * LVGL base services (provided by TouchScreen):
 * - TouchScreen_setup() initializes the display + LVGL port.
 * - UI components should call TouchScreen_lvgl_ready() before using LVGL.
 * - LVGL is not thread-safe, so UI components must use the lock wrappers.
 */
bool TouchScreen_lvgl_ready(void);
bool TouchScreen_lvgl_lock(int timeout_ms);
void TouchScreen_lvgl_unlock(void);

// ------------------ END   Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
