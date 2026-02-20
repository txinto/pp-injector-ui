#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <PrjCfg.h>

// ------------------ BEGIN Return code ------------------
typedef enum {
    PPInjectorUI_ret_error = -1,
    PPInjectorUI_ret_ok    = 0
} PPInjectorUI_return_code_t;

typedef enum {
    PPInjectorUI_system_action_none = 0,
    PPInjectorUI_system_action_ota = 1,
    PPInjectorUI_system_action_reprovision = 2
} PPInjectorUI_system_action_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------

// ------------------ END   Datatypes ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    PPInjectorUI_return_code_t last_return_code;

#include "PPInjectorUI_netvar_types_fragment.h_"
} PPInjectorUI_dre_t;

extern PPInjectorUI_dre_t PPInjectorUI_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
#if CONFIG_PPINJECTORUI_USE_THREAD
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
PPInjectorUI_return_code_t PPInjectorUI_start(void);

/**
 *  Stop background task gracefully.
 *  Idempotent. Safe to call if not running.
 */
PPInjectorUI_return_code_t PPInjectorUI_stop(void);

/**
 *  Thread-safe clone of current DRE state.
 */
PPInjectorUI_return_code_t PPInjectorUI_get_dre_clone(PPInjectorUI_dre_t *dst);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
PPInjectorUI_return_code_t PPInjectorUI_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t PPInjectorUI_get_period_ms(void);


// ------------------ END   Public API (MULTITASKING)--------------------
#else
// ------------------ BEGIN Public API (SPIN)--------------------
/**
 *  Non-blocking step of this module (call it from your scheduler when
 *  CONFIG_PPINJECTORUI_USE_THREAD = n).
 */
PPInjectorUI_return_code_t PPInjectorUI_spin(void);

// ------------------ END   Public API (SPIN)--------------------
#endif // CONFIG_PPINJECTORUI_USE_THREAD

// ------------------ BEGIN Public API (COMMON)--------------------

typedef void (*PPInjectorUI_machine_tx_cb_t)(const char *line, void *ctx);

/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
*/
void PPInjectorUI_execute_function_safemode(void (*callback)());

/**
 *  Called at initialization time. Does minimal setup.
 */
PPInjectorUI_return_code_t PPInjectorUI_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
PPInjectorUI_return_code_t PPInjectorUI_enable(void);
PPInjectorUI_return_code_t PPInjectorUI_disable(void);

/**
 * Feed one machine/protocol line into PPInjectorUI parser (RX path).
 */
void PPInjectorUI_feed_machine_line(const char *line);

/**
 * Register callback for outbound machine/protocol lines (TX path).
 */
void PPInjectorUI_set_machine_tx_callback(PPInjectorUI_machine_tx_cb_t cb, void *ctx);

void PPInjectorUI_request_system_action(PPInjectorUI_system_action_t action);
PPInjectorUI_system_action_t PPInjectorUI_take_system_action(void);
void PPInjectorUI_set_wifi_credentials_available(bool available);
bool PPInjectorUI_wifi_credentials_available(void);

// ------------------ END Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
