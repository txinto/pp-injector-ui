#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// BEGIN --- Standard C headers section ---
#define max(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

#include <stdbool.h>

// END   --- Standard C headers section ---

// BEGIN --- SDK config section---
#include <sdkconfig.h>
// END   --- SDK config section---

// BEGIN --- FreeRTOS headers section ---

// END   --- FreeRTOS headers section ---


// BEGIN --- ESP-IDF headers section ---
#include <esp_netif.h>

// END   --- ESP-IDF headers section ---


// ------------------ BEGIN Return code ------------------
typedef enum {
    PrjCfg_ret_error = -1,
    PrjCfg_ret_ok    = 0
} PrjCfg_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------

#define PRJCFG_MAC_LEN (6)
#define PRJCFG_UNIQUEID_BASE_LEN (0)
#define PRJCFG_UNIQUEID_MAX_LEN (PRJCFG_UNIQUEID_BASE_LEN + (PRJCFG_MAC_LEN * 2) + 1)
#define PRJCFG_IPADDRESS_MAX_LEN ((4 * 4) + 1)
#define PRJCFG_SSID_MAX_LEN (32 + 1)

// ------------------ END   Datatypes ------------------

// ------------------ BEGIN Project configuration constants ------------------

#define MAIN_CYCLE_PERIOD_MS 100

#define MQTTCOMM_CYCLE_PERIOD_MS 10000

#define MEASUREMENT_CYCLE_PERIOD_MS 1000

#ifdef CONFIG_MEASUREMENT_ENABLE_SIMULATION
#define MEASUREMENT_HEATER_CYCLE_MS (20*1000)
#endif

// ------------------ END   Project configuration constants ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    PrjCfg_return_code_t last_return_code;


#include "PrjCfg_netvar_types_fragment.h_"

} PrjCfg_dre_t;

extern PrjCfg_dre_t PrjCfg_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
#if CONFIG_PRJCFG_USE_THREAD
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
PrjCfg_return_code_t PrjCfg_start(void);

/**
 *  Stop background task gracefully.
 *  Idempotent. Safe to call if not running.
 */
PrjCfg_return_code_t PrjCfg_stop(void);

/**
 *  Thread-safe clone of current DRE state.
 */
PrjCfg_return_code_t PrjCfg_get_dre_clone(PrjCfg_dre_t *dst);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
PrjCfg_return_code_t PrjCfg_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t PrjCfg_get_period_ms(void);



// ------------------ END   Public API (MULTITASKING)--------------------
#else
// ------------------ BEGIN Public API (SPIN)--------------------
/**
 *  Non-blocking step of this module (call it from your scheduler when
 *  CONFIG_PRJCFG_USE_THREAD = n).
 */
PrjCfg_return_code_t PrjCfg_spin(void);

// ------------------ END   Public API (SPIN)--------------------
#endif // CONFIG_PRJCFG_USE_THREAD

// ------------------ BEGIN Public API (COMMON)--------------------
/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
 */
void PrjCfg_execute_function_safemode(void (*callback)());

/**
 *  Called at initialization time. Does minimal setup.
 */
PrjCfg_return_code_t PrjCfg_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
PrjCfg_return_code_t PrjCfg_enable(void);
PrjCfg_return_code_t PrjCfg_disable(void);

// ------------------ END Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
