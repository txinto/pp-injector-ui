// BEGIN --- Standard C headers section ---
#include <stdio.h>
#include <string.h>

// END   --- Standard C headers section ---

// BEGIN --- SDK config section---
#include <sdkconfig.h>
// END   --- SDK config section---

// BEGIN --- FreeRTOS headers section ---
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if CONFIG_TOUCHSCREEN_USE_THREAD
  #include <freertos/semphr.h>
#endif

// END   --- FreeRTOS headers section ---


// BEGIN --- ESP-IDF headers section ---
#include <esp_log.h>
#include "esp_err.h"

// END   --- ESP-IDF headers section ---

// BEGIN --- Project configuration section ---
#include <PrjCfg.h> // Including project configuration module 
// END   --- Project configuration section ---

// BEGIN --- Other project modules section ---

// END   --- Other project modules section  ---

// BEGIN --- Self-includes section ---
#include "TouchScreen.h"
#include "TouchScreen_netvars.h"
#include "TouchScreen_display.h"
#include "lvgl_port.h"
// END --- Self-includes section ---

// BEGIN --- Logging related variables
static const char *TAG = "TouchScreen";
// END --- Logging related variables

// BEGIN --- Internal variables (DRE)
TouchScreen_dre_t TouchScreen_dre = {
    .enabled = true,
    .last_return_code = TouchScreen_ret_ok,
};

// END   --- Internal variables (DRE)

// BEGIN --- Multitasking variables and handlers

#if CONFIG_TOUCHSCREEN_USE_THREAD
static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;
static uint32_t s_period_ms =
    #ifdef CONFIG_TOUCHSCREEN_PERIOD_MS
      CONFIG_TOUCHSCREEN_PERIOD_MS
    #else
      1000
    #endif
;
static SemaphoreHandle_t s_mutex = NULL;

static inline void _lock(void)   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
static inline void _unlock(void) { if (s_mutex) xSemaphoreGive(s_mutex); }

#ifdef CONFIG_TOUCHSCREEN_MINIMIZE_JITTER
    static TickType_t xLastWakeTime;
    static TickType_t xFrequency;
#endif

static TouchScreen_return_code_t TouchScreen_spin(void);  // In case we are using a thread, this function should not be part of the public API

static inline BaseType_t _create_mutex_once(void)
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return pdFAIL;
    }
    return pdPASS;
}

static inline BaseType_t _get_core_affinity(void)
{
    #if CONFIG_TOUCHSCREEN_PIN_CORE_ANY
        return tskNO_AFFINITY;
    #elif CONFIG_TOUCHSCREEN_PIN_CORE_0
        return 0;
    #elif CONFIG_TOUCHSCREEN_PIN_CORE_1
        return 1;
    #else
        return tskNO_AFFINITY;
    #endif
}

static void TouchScreen_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "task started (period=%u ms)", (unsigned)s_period_ms);
#ifdef CONFIG_TOUCHSCREEN_MINIMIZE_JITTER
    xLastWakeTime = xTaskGetTickCount();
    xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
    while (s_run) {
        TouchScreen_return_code_t ret = TouchScreen_spin();
        if (ret != TouchScreen_ret_ok)
        {
            ESP_LOGW(TAG, "Error in spin");
        }
#ifdef CONFIG_TOUCHSCREEN_MINIMIZE_JITTER
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
#else
        vTaskDelay(pdMS_TO_TICKS(s_period_ms));
#endif
    }
    ESP_LOGI(TAG, "task exit");
    vTaskDelete(NULL);
}

#endif // CONFIG_TOUCHSCREEN_USE_THREAD

// END   --- Multitasking variables and handlers

// BEGIN ------------------ Public API (MULTITASKING)------------------


#if CONFIG_TOUCHSCREEN_USE_THREAD

TouchScreen_return_code_t TouchScreen_start(void)
{
    if (_create_mutex_once() != pdPASS) {
        ESP_LOGE(TAG, "mutex creation failed");
        return TouchScreen_ret_error;
    }
    if (s_task) {
        // idempotente
        return TouchScreen_ret_ok;
    }
    s_run = true;

    BaseType_t core = _get_core_affinity();
    BaseType_t ok = xTaskCreatePinnedToCore(
        TouchScreen_task,
        "TouchScreen",
        CONFIG_TOUCHSCREEN_TASK_STACK,
        NULL,
        CONFIG_TOUCHSCREEN_TASK_PRIO,
        &s_task,
        core
    );
    if (ok != pdPASS) {
        s_task = NULL;
        s_run = false;
        ESP_LOGE(TAG, "xTaskCreatePinnedToCore failed");
        return TouchScreen_ret_error;
    }
    return TouchScreen_ret_ok;
}

TouchScreen_return_code_t TouchScreen_stop(void)
{
    if (!s_task) return TouchScreen_ret_ok; // idempotente
    s_run = false;
    // Espera una vuelta de scheduler para que el loop salga y se autodelete
    vTaskDelay(pdMS_TO_TICKS(1));
    // Si aún vive por cualquier motivo, fuerza delete
    if (s_task) {
        TaskHandle_t t = s_task;
        s_task = NULL;
        vTaskDelete(t);
    }
    ESP_LOGI(TAG, "stopped");
    return TouchScreen_ret_ok;
}

TouchScreen_return_code_t TouchScreen_get_dre_clone(TouchScreen_dre_t *dst)
{
    if (!dst) return TouchScreen_ret_error;
    _lock();
    memcpy(dst, &TouchScreen_dre, sizeof(TouchScreen_dre));
    _unlock();
    return TouchScreen_ret_ok;
}

TouchScreen_return_code_t TouchScreen_set_period_ms(uint32_t period_ms)
{
    if (period_ms < 10) period_ms = 10;
    _lock();
    s_period_ms = period_ms;
#ifdef CONFIG_TOUCHSCREEN_MINIMIZE_JITTER    
    xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
    _unlock();
    ESP_LOGI(TAG, "period set to %u ms", (unsigned)period_ms);
    return TouchScreen_ret_ok;
}

uint32_t TouchScreen_get_period_ms(void)
{
    _lock();
    uint32_t v = s_period_ms;
    _unlock();
    return v;
}

/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
*/
void TouchScreen_execute_function_safemode(void (*callback)())
{
    _lock();
    callback();
    _unlock();
}

#endif // CONFIG_TOUCHSCREEN_USE_THREAD

// END   ------------------ Public API (MULTITASKING)------------------

static bool s_display_inited = false;
static bool s_lvgl_ready = false;

static void touchscreen_main(void)
{
    if (s_display_inited) return;

    esp_err_t init_ret = TouchScreen_display_init();
    if (init_ret != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(init_ret));
        return;
    }
    s_display_inited = true;
    s_lvgl_ready = true;
}


// BEGIN ------------------ Public API (COMMON + SPIN)------------------

TouchScreen_return_code_t TouchScreen_setup(void)
{
    // Init liviano; no arranca tarea.
    ESP_LOGD(TAG, "setup()");
    // Loading values from NVS
    TouchScreen_netvars_nvs_load();    
#if CONFIG_TOUCHSCREEN_USE_THREAD
    if (_create_mutex_once() != pdPASS) {
        ESP_LOGE(TAG, "mutex creation failed");
        return TouchScreen_ret_error;
    }
#endif
    TouchScreen_dre.last_return_code = TouchScreen_ret_ok;

        ESP_LOGI(TAG, "Launching touchscreen_main()");
        touchscreen_main();

    return TouchScreen_ret_ok;
}

#if CONFIG_TOUCHSCREEN_USE_THREAD
static  // In case we are using a thread, this function should not be part of the public API
#endif
TouchScreen_return_code_t TouchScreen_spin(void)
{
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _lock();
#endif
    TouchScreen_dre_t snapshot = TouchScreen_dre;
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _unlock();
#endif

    if (!snapshot.enabled)
    {
        return TouchScreen_ret_ok;
    }

    TouchScreen_nvs_spin();

    return TouchScreen_ret_ok;
}

TouchScreen_return_code_t TouchScreen_enable(void)
{
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _lock();
#endif
    TouchScreen_dre.enabled = true;
    TouchScreen_dre.last_return_code = TouchScreen_ret_ok;
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _unlock();
#endif
    return TouchScreen_ret_ok;
}

TouchScreen_return_code_t TouchScreen_disable(void)
{
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _lock();
#endif
    TouchScreen_dre.enabled = false;
    TouchScreen_dre.last_return_code = TouchScreen_ret_ok;
#if CONFIG_TOUCHSCREEN_USE_THREAD
    _unlock();
#endif
    return TouchScreen_ret_ok;
}

// END ------------------ Public API (COMMON)------------------

bool TouchScreen_lvgl_ready(void)
{
    return s_lvgl_ready;
}

bool TouchScreen_lvgl_lock(int timeout_ms)
{
    if (!s_lvgl_ready) return false;
    return lvgl_port_lock(timeout_ms);
}

void TouchScreen_lvgl_unlock(void)
{
    if (!s_lvgl_ready) return;
    lvgl_port_unlock();
}
