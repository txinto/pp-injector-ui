#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

// ------------------ BEGIN Constants ------------------
#define OTA_URL_SIZE 256
// ------------------ END Constants ------------------

// ------------------ BEGIN Return code ------------------
typedef enum {
    OTA_ret_error = -1,
    OTA_ret_ok    = 0
} OTA_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------

// ------------------ END   Datatypes ------------------

// ------------------ BEGIN DRE ------------------
typedef struct {
    bool enabled;
    bool running;
    bool finished;
    OTA_return_code_t last_return_code;
#include "OTA_netvar_types_fragment.h_"

} OTA_dre_t;

extern OTA_dre_t OTA_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------
/**
 *  Start background task that calls spin() every period.
 *  Idempotent. Returns error if task creation fails.
 */
OTA_return_code_t OTA_start(void);

/**
 *  Thread-safe clone of current DRE state.
 */
OTA_return_code_t OTA_get_dre_clone(OTA_dre_t *dst);

/**
 * True while OTA task is in progress.
 */
bool OTA_is_running(void);

/**
 * True when OTA task has already ended (success or failure) since last OTA_start().
 */
bool OTA_has_finished(void);

int OTA_get_image_len_read(void);
int OTA_get_image_total_len(void);
esp_err_t OTA_get_last_error(void);
bool OTA_get_running_version(char *dst, size_t dst_len);
bool OTA_get_target_version(char *dst, size_t dst_len);

/**
 *  Change the periodic interval at runtime (ms).
 *  Clamped internamente a >= 10 ms.
 */
OTA_return_code_t OTA_set_period_ms(uint32_t period_ms);

/**
 *  Get current period in ms.
 */
uint32_t OTA_get_period_ms(void);

// ------------------ END   Public API (MULTITASKING)--------------------

// ------------------ BEGIN Public API (COMMON)--------------------
/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
 */
void OTA_execute_function_safemode(void (*callback)());



/**
 *  Called at initialization time. Does minimal setup.
 */
OTA_return_code_t OTA_setup(void);

/**
 *  Enable/disable from user code (thread-safe if internal thread is enabled).
 */
OTA_return_code_t OTA_enable(void);
OTA_return_code_t OTA_disable(void);

// ------------------ END Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
