#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <PrjCfg.h>

// ------------------ BEGIN Return code ------------------
typedef enum {
    $$1_ret_error = -1,
    $$1_ret_ok    = 0
} $$1_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------

// ------------------ END   Datatypes ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    $$1_return_code_t last_return_code;

#include "$$1_netvar_types_fragment.h_"
} $$1_dre_t;

extern $$1_dre_t $$1_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
#if CONFIG_$#1_USE_THREAD
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
$$1_return_code_t $$1_start(void);

/**
 *  Stop background task gracefully.
 *  Idempotent. Safe to call if not running.
 */
$$1_return_code_t $$1_stop(void);

/**
 *  Thread-safe clone of current DRE state.
 */
$$1_return_code_t $$1_get_dre_clone($$1_dre_t *dst);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
$$1_return_code_t $$1_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t $$1_get_period_ms(void);


// ------------------ END   Public API (MULTITASKING)--------------------
#else
// ------------------ BEGIN Public API (SPIN)--------------------
/**
 *  Non-blocking step of this module (call it from your scheduler when
 *  CONFIG_$#1_USE_THREAD = n).
 */
$$1_return_code_t $$1_spin(void);

// ------------------ END   Public API (SPIN)--------------------
#endif // CONFIG_$#1_USE_THREAD

// ------------------ BEGIN Public API (COMMON)--------------------

/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
*/
void $$1_execute_function_safemode(void (*callback)());

/**
 *  Called at initialization time. Does minimal setup.
 */
$$1_return_code_t $$1_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
$$1_return_code_t $$1_enable(void);
$$1_return_code_t $$1_disable(void);

// ------------------ END Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
