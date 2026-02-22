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
#if CONFIG_PPINJECTORUI_USE_THREAD
#include <freertos/semphr.h>
#endif
// END   --- FreeRTOS headers section ---

// BEGIN --- ESP-IDF headers section ---
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
// END   --- ESP-IDF headers section ---

// BEGIN --- Project configuration section ---
#include <PrjCfg.h>
// END   --- Project configuration section ---

// BEGIN --- Other project modules section ---
#include <TouchScreen.h>
// END   --- Other project modules section ---

// BEGIN --- Self-includes section ---
#include "PPInjectorUI.h"
#include "PPInjectorUI_netvars.h"
// END --- Self-includes section ---

// BEGIN --- Logging related variables
static const char *TAG = "PPInjectorUI";
static volatile int s_pending_system_action = PPInjectorUI_system_action_none;
static volatile bool s_wifi_credentials_available = false;
// END --- Logging related variables

// BEGIN --- Internal variables (DRE)
PPInjectorUI_dre_t PPInjectorUI_dre = {.enabled = true,
                                       .last_return_code = PPInjectorUI_ret_ok};
// END   --- Internal variables (DRE)

// BEGIN --- C++ bridge symbols ---
extern void PPInjectorUI_ui_init_bridge(void);
extern void PPInjectorUI_ui_tick_bridge(void);
extern void PPInjectorUI_comms_inject_line_bridge(const char *line);
extern void PPInjectorUI_comms_set_tx_callback_bridge(void (*cb)(const char *,
                                                                 void *),
                                                      void *ctx);
extern void PPInjectorUI_storage_selftest_bridge(void);
extern void PPInjectorUI_storage_read_dump_bridge(void);
// END   --- C++ bridge symbols ---

static bool s_ui_initialized = false;
static uint32_t s_spin_log_last_ms = 0;

#if CONFIG_PPINJECTORUI_UART_ENABLE
static bool s_uart_inited = false;
static char s_uart_line_buf[256];
static size_t s_uart_line_len = 0;
#endif

#if CONFIG_PPINJECTORUI_UART_ENABLE
static void PPInjectorUI_uart_tx_callback(const char *line, void *ctx) {
  (void)ctx;
  if (!s_uart_inited || !line || !line[0]) {
    return;
  }

  int port = CONFIG_PPINJECTORUI_UART_PORT;
  uart_write_bytes(port, line, strlen(line));
  uart_write_bytes(port, "\n", 1);
  ESP_LOGD(TAG, "TX> %s", line);
}

static void PPInjectorUI_uart_init_once(void) {
  if (s_uart_inited) {
    return;
  }

  const int port = CONFIG_PPINJECTORUI_UART_PORT;
  const uart_config_t uart_cfg = {
      .baud_rate = CONFIG_PPINJECTORUI_UART_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  ESP_ERROR_CHECK(uart_param_config(port, &uart_cfg));
  ESP_ERROR_CHECK(uart_set_pin(port, CONFIG_PPINJECTORUI_UART_TX_GPIO,
                               CONFIG_PPINJECTORUI_UART_RX_GPIO,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(
      uart_driver_install(port, CONFIG_PPINJECTORUI_UART_RX_BUF_SIZE,
                          CONFIG_PPINJECTORUI_UART_TX_BUF_SIZE, 0, NULL, 0));

  s_uart_line_len = 0;
  s_uart_inited = true;
  PPInjectorUI_set_machine_tx_callback(PPInjectorUI_uart_tx_callback, NULL);
  ESP_LOGI(TAG, "UART ready: uart=%d tx=%d rx=%d baud=%d", port,
           CONFIG_PPINJECTORUI_UART_TX_GPIO, CONFIG_PPINJECTORUI_UART_RX_GPIO,
           CONFIG_PPINJECTORUI_UART_BAUD);
}

static inline void PPInjectorUI_uart_handle_line(const char *line) {
  if (!line || !line[0]) {
    return;
  }
  // ESP_LOGD(TAG, "RX< %s", line);
  PPInjectorUI_feed_machine_line(line);
}

static void PPInjectorUI_uart_poll(void) {
  if (!s_uart_inited) {
    return;
  }

  const int port = CONFIG_PPINJECTORUI_UART_PORT;
  uint8_t rx_tmp[64];
  int got = 0;

  while ((got = uart_read_bytes(port, rx_tmp, sizeof(rx_tmp), 0)) > 0) {
    for (int i = 0; i < got; ++i) {
      const char c = (char)rx_tmp[i];
      if (c == '\n' || c == '\r') {
        if (s_uart_line_len > 0) {
          s_uart_line_buf[s_uart_line_len] = '\0';
          PPInjectorUI_uart_handle_line(s_uart_line_buf);
          s_uart_line_len = 0;
        }
        continue;
      }

      if (s_uart_line_len < sizeof(s_uart_line_buf) - 1) {
        s_uart_line_buf[s_uart_line_len++] = c;
      } else {
        ESP_LOGW(TAG, "UART RX line overflow, dropping partial line");
        s_uart_line_len = 0;
      }
    }
  }
}
#endif

static void PPInjectorUI_try_init_ui(void) {
  if (s_ui_initialized) {
    return;
  }
  if (!TouchScreen_lvgl_ready()) {
    return;
  }
  if (!TouchScreen_lvgl_lock(1000)) {
    return;
  }

  PPInjectorUI_ui_init_bridge();
  TouchScreen_lvgl_unlock();
  s_ui_initialized = true;
  ESP_LOGI(TAG, "EEZ UI initialized");
}

// BEGIN --- Multitasking variables and handlers
#if CONFIG_PPINJECTORUI_USE_THREAD
static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;
static uint32_t s_period_ms =
#ifdef CONFIG_PPINJECTORUI_PERIOD_MS
    CONFIG_PPINJECTORUI_PERIOD_MS
#else
    1000
#endif
    ;
static SemaphoreHandle_t s_mutex = NULL;

static inline void _lock(void) {
  if (s_mutex)
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}
static inline void _unlock(void) {
  if (s_mutex)
    xSemaphoreGive(s_mutex);
}

#ifdef CONFIG_PPINJECTORUI_MINIMIZE_JITTER
static TickType_t xLastWakeTime;
static TickType_t xFrequency;
#endif

static PPInjectorUI_return_code_t PPInjectorUI_spin(void);

static inline BaseType_t _create_mutex_once(void) {
  if (!s_mutex) {
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
      return pdFAIL;
  }
  return pdPASS;
}

static inline BaseType_t _get_core_affinity(void) {
#if CONFIG_PPINJECTORUI_PIN_CORE_ANY
  return tskNO_AFFINITY;
#elif CONFIG_PPINJECTORUI_PIN_CORE_0
  return 0;
#elif CONFIG_PPINJECTORUI_PIN_CORE_1
  return 1;
#else
  return tskNO_AFFINITY;
#endif
}

static void PPInjectorUI_task(void *arg) {
  (void)arg;
  ESP_LOGD(TAG, "task started (period=%u ms)", (unsigned)s_period_ms);
#ifdef CONFIG_PPINJECTORUI_MINIMIZE_JITTER
  xLastWakeTime = xTaskGetTickCount();
  xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
  while (s_run) {
    PPInjectorUI_return_code_t ret = PPInjectorUI_spin();
    if (ret != PPInjectorUI_ret_ok) {
      ESP_LOGW(TAG, "Error in spin");
    }
#ifdef CONFIG_PPINJECTORUI_MINIMIZE_JITTER
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
#else
    vTaskDelay(pdMS_TO_TICKS(s_period_ms));
#endif
  }
  ESP_LOGD(TAG, "task exit");
  vTaskDelete(NULL);
}
#endif // CONFIG_PPINJECTORUI_USE_THREAD
// END   --- Multitasking variables and handlers

// BEGIN ------------------ Public API (MULTITASKING)------------------
#if CONFIG_PPINJECTORUI_USE_THREAD
PPInjectorUI_return_code_t PPInjectorUI_start(void) {
  if (_create_mutex_once() != pdPASS) {
    ESP_LOGE(TAG, "mutex creation failed");
    return PPInjectorUI_ret_error;
  }
  if (s_task) {
    return PPInjectorUI_ret_ok;
  }
  s_run = true;

  BaseType_t core = _get_core_affinity();
  BaseType_t ok = xTaskCreatePinnedToCore(
      PPInjectorUI_task, "PPInjectorUI", CONFIG_PPINJECTORUI_TASK_STACK, NULL,
      CONFIG_PPINJECTORUI_TASK_PRIO, &s_task, core);
  if (ok != pdPASS) {
    s_task = NULL;
    s_run = false;
    ESP_LOGE(TAG, "xTaskCreatePinnedToCore failed");
    return PPInjectorUI_ret_error;
  }
  return PPInjectorUI_ret_ok;
}

PPInjectorUI_return_code_t PPInjectorUI_stop(void) {
  if (!s_task)
    return PPInjectorUI_ret_ok;
  s_run = false;
  vTaskDelay(pdMS_TO_TICKS(1));
  if (s_task) {
    TaskHandle_t t = s_task;
    s_task = NULL;
    vTaskDelete(t);
  }
  ESP_LOGD(TAG, "stopped");
  return PPInjectorUI_ret_ok;
}

PPInjectorUI_return_code_t PPInjectorUI_get_dre_clone(PPInjectorUI_dre_t *dst) {
  if (!dst)
    return PPInjectorUI_ret_error;
  _lock();
  memcpy(dst, &PPInjectorUI_dre, sizeof(PPInjectorUI_dre));
  _unlock();
  return PPInjectorUI_ret_ok;
}

PPInjectorUI_return_code_t PPInjectorUI_set_period_ms(uint32_t period_ms) {
  if (period_ms < 10)
    period_ms = 10;
  _lock();
  s_period_ms = period_ms;
#ifdef CONFIG_PPINJECTORUI_MINIMIZE_JITTER
  xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
  _unlock();
  ESP_LOGD(TAG, "period set to %u ms", (unsigned)period_ms);
  return PPInjectorUI_ret_ok;
}

uint32_t PPInjectorUI_get_period_ms(void) {
  _lock();
  uint32_t v = s_period_ms;
  _unlock();
  return v;
}
#endif // CONFIG_PPINJECTORUI_USE_THREAD
// END   ------------------ Public API (MULTITASKING)------------------

// BEGIN ------------------ Public API (COMMON + SPIN)------------------
void PPInjectorUI_execute_function_safemode(void (*callback)()) {
#if CONFIG_PPINJECTORUI_USE_THREAD
  _lock();
#endif
  callback();
#if CONFIG_PPINJECTORUI_USE_THREAD
  _unlock();
#endif
}

PPInjectorUI_return_code_t PPInjectorUI_setup(void) {
  ESP_LOGD(TAG, "setup()");
  PPInjectorUI_netvars_nvs_load();
#if CONFIG_PPINJECTORUI_UART_ENABLE
  PPInjectorUI_uart_init_once();
#endif
#if CONFIG_PPINJECTORUI_USE_THREAD
  if (_create_mutex_once() != pdPASS) {
    ESP_LOGE(TAG, "mutex creation failed");
    return PPInjectorUI_ret_error;
  }
#endif
#if CONFIG_PPINJECTORUI_STORAGE_SELFTEST
  ESP_LOGI(TAG, "running storage self-test");
  PPInjectorUI_storage_selftest_bridge();
#endif
  PPInjectorUI_storage_read_dump_bridge();
  PPInjectorUI_try_init_ui();
  PPInjectorUI_dre.last_return_code = PPInjectorUI_ret_ok;
  return PPInjectorUI_ret_ok;
}

#if CONFIG_PPINJECTORUI_USE_THREAD
static
#endif
    PPInjectorUI_return_code_t
    PPInjectorUI_spin(void) {
#if CONFIG_PPINJECTORUI_USE_THREAD
  _lock();
#endif
  bool en = PPInjectorUI_dre.enabled;
#if CONFIG_PPINJECTORUI_USE_THREAD
  _unlock();
#endif
  if (!en) {
    return PPInjectorUI_ret_ok;
  }

#if CONFIG_PPINJECTORUI_UART_ENABLE
  PPInjectorUI_uart_poll();
#endif

  PPInjectorUI_try_init_ui();
  if (!s_ui_initialized) {
    return PPInjectorUI_ret_ok;
  }

  if (TouchScreen_lvgl_lock(10)) {
    // LVGL timer handling already runs in TouchScreen LVGL task.
    PPInjectorUI_ui_tick_bridge();
    TouchScreen_lvgl_unlock();
  }

  PPInjectorUI_nvs_spin();

  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if ((now_ms - s_spin_log_last_ms) >= 1000U) {
    s_spin_log_last_ms = now_ms;
    ESP_LOGI(TAG, "UI Heartbeat (1s)");
  }

  return PPInjectorUI_ret_ok;
}

PPInjectorUI_return_code_t PPInjectorUI_enable(void) {
#if CONFIG_PPINJECTORUI_USE_THREAD
  _lock();
#endif
  PPInjectorUI_dre.enabled = true;
  PPInjectorUI_dre.last_return_code = PPInjectorUI_ret_ok;
#if CONFIG_PPINJECTORUI_USE_THREAD
  _unlock();
#endif
  return PPInjectorUI_ret_ok;
}

PPInjectorUI_return_code_t PPInjectorUI_disable(void) {
#if CONFIG_PPINJECTORUI_USE_THREAD
  _lock();
#endif
  PPInjectorUI_dre.enabled = false;
  PPInjectorUI_dre.last_return_code = PPInjectorUI_ret_ok;
#if CONFIG_PPINJECTORUI_USE_THREAD
  _unlock();
#endif
  return PPInjectorUI_ret_ok;
}

void PPInjectorUI_feed_machine_line(const char *line) {
  if (!line) {
    return;
  }
  PPInjectorUI_comms_inject_line_bridge(line);
}

void PPInjectorUI_set_machine_tx_callback(PPInjectorUI_machine_tx_cb_t cb,
                                          void *ctx) {
  PPInjectorUI_comms_set_tx_callback_bridge(cb, ctx);
}

void PPInjectorUI_request_system_action(PPInjectorUI_system_action_t action) {
  if (action <= PPInjectorUI_system_action_none) {
    return;
  }
  // Keep the highest priority action if multiple are requested.
  if ((int)action > s_pending_system_action) {
    s_pending_system_action = (int)action;
  }
}

PPInjectorUI_system_action_t PPInjectorUI_take_system_action(void) {
  int action = s_pending_system_action;
  s_pending_system_action = PPInjectorUI_system_action_none;
  return (PPInjectorUI_system_action_t)action;
}

void PPInjectorUI_set_wifi_credentials_available(bool available) {
  s_wifi_credentials_available = available;
}

bool PPInjectorUI_wifi_credentials_available(void) {
  return s_wifi_credentials_available;
}
// END ------------------ Public API (COMMON)------------------
