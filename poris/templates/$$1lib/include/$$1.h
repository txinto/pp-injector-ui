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
} $$1_dre_t;

extern $$1_dre_t $$1_dre;
// ------------------ END   DRE ------------------

// ------------------ BEGIN Public API (MULTITASKING)--------------------

// ------------------ BEGIN Public API (COMMON)--------------------

/**
 *  Called at initialization time. Does minimal setup.
 */
$$1_return_code_t $$1_setup(void);

// ------------------ END Public API (COMMON)--------------------

#ifdef __cplusplus
}
#endif
