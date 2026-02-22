// BEGIN --- Standard C headers section ---
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// END   --- Standard C headers section ---

// BEGIN --- SDK config section---
#include <sdkconfig.h>
// END   --- SDK config section---

// BEGIN --- FreeRTOS headers section ---
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
// END   --- FreeRTOS headers section ---

// BEGIN --- ESP-IDF headers section ---
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_heap_caps.h>

#if CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
#include "esp_efuse.h"
#endif

#if CONFIG_OTA_CONNECT_WIFI
#include "esp_wifi.h"
#endif

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
#include "ble_api.h"
#endif

// END   --- ESP-IDF headers section ---

// BEGIN --- Project configuration section ---
#include <PrjCfg.h> // Including project configuration module
// END   --- Project configuration section ---

// BEGIN --- Other project modules section ---

// END   --- Other project modules section  ---

// BEGIN --- Self-includes section ---
#include "OTA.h"
#include "OTA_netvars.h"

// END --- Self-includes section ---

// BEGIN --- Logging related variables
static const char *TAG = "OTA";
// END --- Logging related variables

// BEGIN --- Internal variables (DRE)
OTA_dre_t OTA_dre = {
    .enabled = true,
    .running = false,
    .finished = false,
    .last_return_code = OTA_ret_ok
};

// END   --- Internal variables (DRE)



// BEGIN --- Functional variables and handlers

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

const esp_partition_t *running_partition;

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT)
    {
        switch (event_id)
        {
        case ESP_HTTPS_OTA_START:
            ESP_LOGI(TAG, "OTA started");
            break;
        case ESP_HTTPS_OTA_CONNECTED:
            ESP_LOGI(TAG, "Connected to server");
            break;
        case ESP_HTTPS_OTA_GET_IMG_DESC:
            ESP_LOGI(TAG, "Reading Image Description");
            break;
        case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
            ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
            break;
        case ESP_HTTPS_OTA_DECRYPT_CB:
            ESP_LOGI(TAG, "Callback to decrypt function");
            break;
        case ESP_HTTPS_OTA_WRITE_FLASH:
            ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
            break;
        case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
            ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
            break;
        case ESP_HTTPS_OTA_FINISH:
            ESP_LOGI(TAG, "OTA finish");
            break;
        case ESP_HTTPS_OTA_ABORT:
            ESP_LOGI(TAG, "OTA abort");
            break;
        }
    }
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running_partition, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_OTA_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0)
    {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version)
    {
        ESP_LOGW(TAG, "New firmware security version is less than eFuse programmed, %" PRIu32 " < %" PRIu32, new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

// END --- Functional variables and handlers

// BEGIN --- Multitasking variables and handlers

static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;

static SemaphoreHandle_t s_mutex = NULL;
static volatile int s_img_len_read = 0;
static volatile int s_img_total_len = 0;
static volatile esp_err_t s_last_ota_err = ESP_OK;
static char s_running_version[32] = "unknown";
static char s_target_version[32] = "unknown";

static inline void _lock(void)
{
    if (s_mutex)
        xSemaphoreTake(s_mutex, portMAX_DELAY);
}
static inline void _unlock(void)
{
    if (s_mutex)
        xSemaphoreGive(s_mutex);
}

static inline BaseType_t _create_mutex_once(void)
{
    if (!s_mutex)
    {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex)
            return pdFAIL;
    }
    return pdPASS;
}

static inline BaseType_t _get_core_affinity(void)
{
#if CONFIG_OTA_PIN_CORE_ANY
    return tskNO_AFFINITY;
#elif CONFIG_OTA_PIN_CORE_0
    return 0;
#elif CONFIG_OTA_PIN_CORE_1
    return 1;
#else
    return tskNO_AFFINITY;
#endif
}

static void ota_set_runtime_state(bool running, bool finished, OTA_return_code_t rc)
{
    _lock();
    OTA_dre.running = running;
    OTA_dre.finished = finished;
    OTA_dre.last_return_code = rc;
    if (!running)
    {
        s_task = NULL;
    }
    _unlock();
}

static void ota_set_progress(int read_len, int total_len)
{
    _lock();
    s_img_len_read = read_len;
    s_img_total_len = total_len;
    _unlock();
}

static void ota_set_last_error(esp_err_t err)
{
    _lock();
    s_last_ota_err = err;
    _unlock();
}

static void ota_set_running_version(const char *version)
{
    _lock();
    snprintf(s_running_version, sizeof(s_running_version), "%s",
             (version && version[0] != '\0') ? version : "unknown");
    _unlock();
}

static void ota_set_target_version(const char *version)
{
    _lock();
    snprintf(s_target_version, sizeof(s_target_version), "%s",
             (version && version[0] != '\0') ? version : "unknown");
    _unlock();
}
#ifdef CONFIG_OTA_FWSERVER_URL

#include <esp_mac.h>

#endif

static void OTA_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Starting OTA process");
    ota_set_runtime_state(true, false, OTA_ret_ok);
    ota_set_progress(0, 0);
    ota_set_last_error(ESP_OK);
    ota_set_target_version("unknown");

    const esp_partition_t *running_part = esp_ota_get_running_partition();
    esp_app_desc_t running_desc = {0};
    if (running_part && esp_ota_get_partition_description(running_part, &running_desc) == ESP_OK)
    {
        ota_set_running_version(running_desc.version);
    }
    else
    {
        ota_set_running_version("unknown");
    }

    esp_err_t ota_finish_err = ESP_OK;
#ifdef CONFIG_OTA_FWSERVER_URL

    char url_buf[OTA_URL_SIZE];
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        ESP_LOGW(TAG,"Could not obtain MAC address");
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG, "My IP: " IPSTR "", IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "My GW: " IPSTR "", IP2STR(&ip_info.gw));
    ESP_LOGI(TAG, "My NETMASK: " IPSTR "", IP2STR(&ip_info.netmask));
    sprintf(url_buf,
        "%s?mac=%02x%02x%02x%02x%02x%02x%02x%02x&model=%s&fw=%s&ip=" IPSTR,
        CONFIG_OTA_FIRMWARE_UPGRADE_URL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7],
        CONFIG_OTA_FWSERVER_MODEL, running_app_info.version, IP2STR(&ip_info.ip));
    esp_http_client_config_t config = {
        .url = url_buf,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
#else
    esp_http_client_config_t config = {
        .url = CONFIG_OTA_FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
#endif

#ifdef CONFIG_OTA_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0)
    {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    }
    else
    {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_OTA_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
#ifdef CONFIG_OTA_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_OTA_HTTP_REQUEST_SIZE,
#endif
    };

    ESP_LOGI(TAG,"OTA URL is %s",config.url);
    ESP_LOGI(TAG,
             "Heap before OTA begin: free=%" PRIu32 " min=%" PRIu32
             " int_free=%" PRIu32 " int_largest=%" PRIu32
             " 8bit_free=%" PRIu32 " 8bit_largest=%" PRIu32,
             (uint32_t)esp_get_free_heap_size(),
             (uint32_t)esp_get_minimum_free_heap_size(),
             (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        ota_set_last_error(err);
        ota_set_runtime_state(false, true, OTA_ret_error);
        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        ota_set_last_error(err);
        goto ota_end;
    }
    ota_set_target_version(app_desc.version);
    err = validate_image_header(&app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "image header verification failed");
        ota_set_last_error(err);
        goto ota_end;
    }

    while (1)
    {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        int read_len = esp_https_ota_get_image_len_read(https_ota_handle);
        ota_set_progress(read_len, (int)running_partition->size);
        ESP_LOGI(TAG, "Image bytes read: %d / %lu", read_len, running_partition->size);
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
    {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
        ota_set_last_error(ESP_ERR_INVALID_SIZE);
    }
    else
    {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK))
        {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            ota_set_runtime_state(false, true, OTA_ret_ok);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            ota_set_last_error(ota_finish_err);
            ota_set_runtime_state(false, true, OTA_ret_error);
            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    ota_set_last_error(err);
    ota_set_runtime_state(false, true, OTA_ret_error);
    vTaskDelete(NULL);
}

// END   --- Multitasking variables and handlers

// BEGIN ------------------ Public API (MULTITASKING)------------------

OTA_return_code_t OTA_start(void)
{
    if (OTA_dre.enabled)
    {
        if (_create_mutex_once() != pdPASS)
        {
            ESP_LOGE(TAG, "mutex creation failed");
            return OTA_ret_error;
        }
        if (s_task)
        {
            // idempotente
            return OTA_ret_ok;
        }
        s_run = true;
        ota_set_runtime_state(false, false, OTA_ret_ok);

        ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
         * Read "Establishing Wi-Fi or Ethernet Connection" section in
         * examples/protocols/README.md for more information about this function.
         */
        // ESP_ERROR_CHECK(example_connect());

#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
        /**
         * We are treating successful WiFi connection as a checkpoint to cancel rollback
         * process and mark newly updated firmware image as active. For production cases,
         * please tune the checkpoint behavior per end application requirement.
         */
        running_partition = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running_partition, &ota_state) == ESP_OK)
        {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
            {
                if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
                {
                    ESP_LOGI(TAG, "App is valid, rollback cancelled successfully");
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to cancel rollback");
                }
            }
        }
#endif

#if CONFIG_OTA_CONNECT_WIFI
#if !CONFIG_BT_ENABLED
        /* Ensure to disable any WiFi power save mode, this allows best throughput
         * and hence timings for overall OTA operation.
         */
        esp_wifi_set_ps(WIFI_PS_NONE);
#else
        /* WIFI_PS_MIN_MODEM is the default mode for WiFi Power saving. When both
         * WiFi and Bluetooth are running, WiFI modem has to go down, hence we
         * need WIFI_PS_MIN_MODEM. And as WiFi modem goes down, OTA download time
         * increases.
         */
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
#endif // CONFIG_BT_ENABLED
#endif // CONFIG_OTA_CONNECT_WIFI

#if CONFIG_BT_CONTROLLER_ENABLED && (CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED)
        // ESP_ERROR_CHECK(esp_ble_helper_init());
#endif

        BaseType_t core = _get_core_affinity();
        BaseType_t ok = xTaskCreatePinnedToCore(
            OTA_task,
            "OTA",
            CONFIG_OTA_TASK_STACK,
            NULL,
            CONFIG_OTA_TASK_PRIO,
            &s_task,
            core);
        if (ok != pdPASS)
        {
            s_task = NULL;
            s_run = false;
            ota_set_runtime_state(false, true, OTA_ret_error);
            ESP_LOGE(TAG, "xTaskCreatePinnedToCore failed");
            return OTA_ret_error;
        }
        return OTA_ret_ok;
    }
    else
    {
        ESP_LOGE(TAG, "OTA NOT ENABLED");
    }
    return OTA_ret_error;
}

OTA_return_code_t OTA_get_dre_clone(OTA_dre_t *dst)
{
    if (!dst)
        return OTA_ret_error;
    _lock();
    memcpy(dst, &OTA_dre, sizeof(OTA_dre));
    _unlock();
    return OTA_ret_ok;
}

bool OTA_is_running(void)
{
    bool running;
    _lock();
    running = OTA_dre.running;
    _unlock();
    return running;
}

bool OTA_has_finished(void)
{
    bool finished;
    _lock();
    finished = OTA_dre.finished;
    _unlock();
    return finished;
}

int OTA_get_image_len_read(void)
{
    int v;
    _lock();
    v = s_img_len_read;
    _unlock();
    return v;
}

int OTA_get_image_total_len(void)
{
    int v;
    _lock();
    v = s_img_total_len;
    _unlock();
    return v;
}

esp_err_t OTA_get_last_error(void)
{
    esp_err_t v;
    _lock();
    v = s_last_ota_err;
    _unlock();
    return v;
}

bool OTA_get_running_version(char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0)
    {
        return false;
    }
    _lock();
    snprintf(dst, dst_len, "%s", s_running_version);
    _unlock();
    return true;
}

bool OTA_get_target_version(char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0)
    {
        return false;
    }
    _lock();
    snprintf(dst, dst_len, "%s", s_target_version);
    _unlock();
    return true;
}

// END   ------------------ Public API (MULTITASKING)------------------

// BEGIN ------------------ Public API (COMMON + SPIN)------------------
/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
 */
void OTA_execute_function_safemode(void (*callback)())
{
    _lock();
    callback();
    _unlock();
}



OTA_return_code_t OTA_setup(void)
{
    // Init liviano; no arranca tarea.
    ESP_LOGD(TAG, "setup()");
    if (_create_mutex_once() != pdPASS)
    {
        ESP_LOGE(TAG, "mutex creation failed");
        return OTA_ret_error;
    }
    _lock();
    OTA_dre.running = false;
    OTA_dre.finished = false;
    OTA_dre.last_return_code = OTA_ret_ok;
    snprintf(s_running_version, sizeof(s_running_version), "%s", "unknown");
    snprintf(s_target_version, sizeof(s_target_version), "%s", "unknown");
    _unlock();
    return OTA_ret_ok;
}

OTA_return_code_t OTA_enable(void)
{
    _lock();
    OTA_dre.enabled = true;
    OTA_dre.last_return_code = OTA_ret_ok;
    _unlock();
    return OTA_ret_ok;
}

OTA_return_code_t OTA_disable(void)
{
    _lock();
    OTA_dre.enabled = false;
    OTA_dre.last_return_code = OTA_ret_ok;
    _unlock();
    return OTA_ret_ok;
}

// END ------------------ Public API (COMMON)------------------
