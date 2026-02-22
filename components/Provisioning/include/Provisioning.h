#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <esp_netif.h>
// ------------------ BEGIN Return code ------------------
typedef enum {
    Provisioning_ret_error = -1,
    Provisioning_ret_ok    = 0
} Provisioning_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------
#define PROVISIONING_IP_STR_MAX_LEN 16
#define PROVISIONING_SSID_MAX_LEN (32 + 1)
// ------------------ END   Datatypes ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    Provisioning_return_code_t last_return_code;
#include "Provisioning_netvar_types_fragment.h_"
    esp_ip4_addr_t ip;

} Provisioning_dre_t;

extern Provisioning_dre_t Provisioning_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
#if CONFIG_PROVISIONING_USE_THREAD
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
Provisioning_return_code_t Provisioning_start(void);

/**
 *  Stop background task gracefully.
 *  Idempotent. Safe to call if not running.
 */
Provisioning_return_code_t Provisioning_stop(void);

/**
 *  Thread-safe clone of current DRE state.
 */
Provisioning_return_code_t Provisioning_get_dre_clone(Provisioning_dre_t *dst);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
Provisioning_return_code_t Provisioning_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t Provisioning_get_period_ms(void);



// ------------------ END   Public API (MULTITASKING)--------------------
#else
// ------------------ BEGIN Public API (SPIN)--------------------
/**
 *  Non-blocking step of this module (call it from your scheduler when
 *  CONFIG_PROVISIONING_USE_THREAD = n).
 */
Provisioning_return_code_t Provisioning_spin(void);

// ------------------ END   Public API (SPIN)--------------------
#endif // CONFIG_PROVISIONING_USE_THREAD

// ------------------ BEGIN Public API (COMMON)--------------------
/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
 */
void Provisioning_execute_function_safemode(void (*callback)());

/**
 *  Called at initialization time. Does minimal setup.
 */
Provisioning_return_code_t Provisioning_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
Provisioning_return_code_t Provisioning_enable(void);
Provisioning_return_code_t Provisioning_disable(void);

// ------------------ END Public API (COMMON)--------------------

void Provisioning_forget(void);

bool Provisioning_ip_valid(void);
bool Provisioning_is_provisioned(void);

typedef void (*Provisioning_qr_payload_callback_t)(const char *payload);
void Provisioning_set_qr_payload_callback(Provisioning_qr_payload_callback_t callback);
bool Provisioning_get_last_qr_payload(char *dst, size_t dst_len);

typedef void (*Provisioning_status_callback_t)(const char *status);
void Provisioning_set_status_callback(Provisioning_status_callback_t callback);

#ifdef __cplusplus
}
#endif
