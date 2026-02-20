// BEGIN --- Standard C headers section ---
#include <stdio.h>
#include <string.h>

// END   --- Standard C headers section ---

// BEGIN --- SDK config section---
#include <sdkconfig.h>
// END   --- SDK config section---

// BEGIN --- ESP-IDF headers section ---
#include <esp_log.h>

// END   --- ESP-IDF headers section ---

// BEGIN --- Project configuration section ---
#include <PrjCfg.h> // Including project configuration module 
// END   --- Project configuration section ---

// BEGIN --- Other project modules section ---

// END   --- Other project modules section  ---

// BEGIN --- Self-includes section ---
#include "$$1.h"
// END --- Self-includes section ---

// BEGIN --- Logging related variables
static const char *TAG = "$$1";
// END --- Logging related variables

// BEGIN --- Internal variables (DRE)
$$1_dre_t $$1_dre = {
    .enabled = true,
    .last_return_code = $$1_ret_ok
};
// END   --- Internal variables (DRE)

// BEGIN ------------------ Public API (COMMON + SPIN)------------------

$$1_return_code_t $$1_setup(void)
{
    // Init liviano; no arranca tarea.
    ESP_LOGD(TAG, "setup()");
    $$1_dre.last_return_code = $$1_ret_ok;
    return $$1_ret_ok;
}

// END ------------------ Public API (COMMON)------------------
