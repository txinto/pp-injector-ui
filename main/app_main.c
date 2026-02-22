/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <cJSON.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include <esp_log.h>
#include "qrcode.h"

// Include project configuration
#include <PrjCfg.h>

// Include components
#ifdef CONFIG_PORIS_ENABLE_WIFI
#include <Wifi.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
#include <OTA.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
#include <MQTTComm.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
#include <Measurement.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
#include <DualLED.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
#include <DualLedTester.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
#include <Relays.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
#include <RelaysTest.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
#include <TouchScreen.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
#ifdef CONFIG_PORIS_ENABLE_WIFI
#error "Provisioning component can not coexist with Wifi component"
#else
#include <Provisioning.h>
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_UIDEMO
#include <UIDemo.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
#include <BlePeripheral.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_I2CMANAGER
#include <i2cManager.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_MCP3426
#include <Mcp3426.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
#include <ADCProbes.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_SDMMCFS
#include <SdMmcFS.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
#include <LCDFlag.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_SECUREBOX
#include <SecureBox.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
#include <PPInjectorUI.h>
#endif
// [PORIS_INTEGRATION_INCLUDE]

// Include comms callbacks
#ifdef CONFIG_PORIS_ENABLE_PRJCFG
#include <PrjCfg_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
#include <Measurement_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
#include <MQTTComm_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
#include <OTA_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
#include <DualLED_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
#include <DualLedTester_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
#include <Relays_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
#include <RelaysTest_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_UDPCOMM
#include <UDPComm_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
#include <Wifi_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
#include <TouchScreen_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
#include <BlePeripheral_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
#include <ADCProbes_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
#include <LCDFlag_netvars.h>
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
#include <PPInjectorUI_netvars.h>
#endif
// [PORIS_INTEGRATION_NETVARS_INCLUDE]

typedef enum
{
    app_main_ret_error = -1,
    app_main_ret_ok = 0
} app_main_return_code;

static char TAG[] = "main";

static bool s_network_started = false;

typedef enum
{
    BOOT_STATE_NORMAL = 0
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    ,
    BOOT_STATE_PROV_START = 1,
    BOOT_STATE_PROV_END = 2
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    ,
    BOOT_STATE_OTA = 3
#endif
} boot_state_t;

/* Keep across esp_restart(). On non-software reset we force NORMAL. */
__NOINIT_ATTR static uint32_t boot_state_noinit;
static uint32_t boot_state = BOOT_STATE_NORMAL;
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
static bool provisioning_enabled = false;
#endif
static bool ui_enabled = true;

#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
static void main_get_provisioning_service_name(char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0)
    {
        return;
    }
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        snprintf(dst, dst_len, "%s%02X%02X%02X",
                 CONFIG_PROVISIONING_SERVICE_NAME_PREFIX, mac[3], mac[4], mac[5]);
    }
    else
    {
        snprintf(dst, dst_len, "%s??????", CONFIG_PROVISIONING_SERVICE_NAME_PREFIX);
    }
}
#endif

static bool boot_state_is_valid(uint32_t st)
{
    if (st == BOOT_STATE_NORMAL)
    {
        return true;
    }
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    if (st == BOOT_STATE_PROV_START || st == BOOT_STATE_PROV_END)
    {
        return true;
    }
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    if (st == BOOT_STATE_OTA)
    {
        return true;
    }
#endif
    return false;
}

static const char *boot_state_to_str(uint32_t st)
{
    switch (st)
    {
    case BOOT_STATE_NORMAL:
        return "NORMAL";
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    case BOOT_STATE_PROV_START:
        return "PROV_START";
    case BOOT_STATE_PROV_END:
        return "PROV_END";
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    case BOOT_STATE_OTA:
        return "OTA";
#endif
    default:
        return "INVALID";
    }
}

static void boot_fsm_init_on_reset_reason(void)
{
    esp_reset_reason_t rr = esp_reset_reason();
    uint32_t st = BOOT_STATE_NORMAL;

    if (rr != ESP_RST_SW)
    {
        st = BOOT_STATE_NORMAL;
        ESP_LOGI(TAG, "boot_fsm: reset_reason=%d -> force boot_state=%s",
                 (int)rr, boot_state_to_str(st));
    }
    else if (!boot_state_is_valid(boot_state_noinit))
    {
        st = BOOT_STATE_NORMAL;
        ESP_LOGW(TAG, "boot_fsm: invalid boot_state after SW reset, forcing %s",
                 boot_state_to_str(st));
    }
    else
    {
        st = boot_state_noinit;
        ESP_LOGI(TAG, "boot_fsm: SW reset, keeping boot_state=%s",
                 boot_state_to_str(st));
    }

    /* FSM uses normal RAM copy. */
    boot_state = st;

    /* One-shot NOINIT ticket: always clear after consuming. */
    boot_state_noinit = BOOT_STATE_NORMAL;
}

#define BLE_DEVICE_NAME_PREFIX "POR-"
static char ble_device_name[20];

#define NETVARS_CMD_RESPONSE_LEN 249

bool main_req_parse_callback(const char *data, int len, char *response);
bool main_parse_callback(const char *data, int len, char *response);

#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 */
esp_err_t process_received_command(const uint8_t *inbuf, ssize_t inlen)
{
    char response_buf[NETVARS_CMD_RESPONSE_LEN];

    ESP_LOGI(TAG, "Process command %.*s", inlen, (char *)inbuf);
    bool ret = false;
    if ((inlen > 1) && (inbuf[0] == 'c'))
    {
        ESP_LOGI(TAG, "This is a configuration command");
        ret = main_parse_callback((char *)&(inbuf[1]), inlen - 1, response_buf);
    }
    else
    {
        ESP_LOGI(TAG, "This is a request command");
        ret = main_req_parse_callback((char *)inbuf, inlen, response_buf);
    }
    ESP_LOGI(TAG, "END Process command %.*s", inlen, (char *)inbuf);
    if (ret)
    {
        ESP_LOGI(TAG, "Process response %d %s", strlen(response_buf), response_buf);
        bleprph_gatt_svr_callback((uint8_t *)response_buf, strlen(response_buf));
    }
    else
    {
        ESP_LOGI(TAG, "No response");
    }
    return ESP_OK;
}
#endif

app_main_return_code init_components(void)
{
    app_main_return_code ret = app_main_ret_ok;
    bool error_occurred = false;
    bool error_accumulator = false;

#ifdef CONFIG_PORIS_ENABLE_PRJCFG
    error_occurred = (PrjCfg_setup() != PrjCfg_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    if (provisioning_enabled || s_network_started)
    {
        error_occurred = (Provisioning_setup() != Provisioning_ret_ok);
        error_accumulator |= error_occurred;
    }
    else
    {
        ESP_LOGI(TAG, "boot_fsm: provisioning setup skipped (boot_state=%s)",
                 boot_state_to_str(boot_state));
    }
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    if (s_network_started)
    {
        error_occurred = (Wifi_setup() != Wifi_ret_ok);
        error_accumulator |= error_occurred;
    }
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    error_occurred = (OTA_setup() != OTA_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
    error_occurred = (Measurement_setup() != Measurement_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
    error_occurred = (DualLED_setup() != DualLED_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
    error_occurred = (DualLedTester_setup() != DualLedTester_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
    error_occurred = (Relays_setup() != Relays_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
    error_occurred = (RelaysTest_setup() != RelaysTest_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_UIDEMO
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
    error_occurred = (BlePeripheral_setup() != BlePeripheral_ret_ok);
    if (!error_occurred)
    {
        ble_peripheral_set_process_command_cb(process_received_command);
    }
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_I2CMANAGER
    error_occurred = (i2cManager_setup() != i2cManager_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_MCP3426
    error_occurred = (MCP3426_setup() != ESP_OK);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
    error_occurred = (ADCProbes_setup() != ADCProbes_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_SECUREBOX
    error_occurred = (SecureBox_setup() != SecureBox_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    // Delayed UI phase (after OTA check)
#endif
// [PORIS_INTEGRATION_INIT]
    if (error_accumulator)
    {
        ret = app_main_ret_error;
    }
    return ret;
}

app_main_return_code start_components(void)
{
    app_main_return_code ret = app_main_ret_ok;
    bool error_occurred = false;
    bool error_accumulator = false;

#ifdef CONFIG_PORIS_ENABLE_PRJCFG
    error_occurred = (PrjCfg_enable() != PrjCfg_ret_ok);
#ifdef CONFIG_PRJCFG_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (PrjCfg_start() != PrjCfg_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    if (provisioning_enabled || s_network_started)
    {
        error_occurred = (Provisioning_enable() != Provisioning_ret_ok);
#ifdef CONFIG_PROVISIONING_USE_THREAD
        if (!error_occurred)
        {
            error_occurred |= (Provisioning_start() != Provisioning_ret_ok);
        }
#endif
        error_accumulator |= error_occurred;
    }
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    // Wifi does not need starting, setup already did it
    // only on-demand
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    // OTA Should not be started with the rest of the components
    // only on-demand
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
    error_occurred = (MQTTComm_enable() != MQTTComm_ret_ok);
#ifdef CONFIG_MQTTCOMM_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (MQTTComm_start() != MQTTComm_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
    error_occurred = (Measurement_enable() != Measurement_ret_ok);
#ifdef CONFIG_MEASUREMENT_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (Measurement_start() != Measurement_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
    error_occurred = (DualLED_enable() != DualLED_ret_ok);
#ifdef CONFIG_DUALLED_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (DualLED_start() != DualLED_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
    error_occurred = (DualLedTester_enable() != DualLedTester_ret_ok);
#ifdef CONFIG_DUALLEDTESTER_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (DualLedTester_start() != DualLedTester_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
    error_occurred = (Relays_enable() != Relays_ret_ok);
#ifdef CONFIG_RELAYS_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (Relays_start() != Relays_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
    error_occurred = (RelaysTest_enable() != RelaysTest_ret_ok);
#ifdef CONFIG_RELAYSTEST_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (RelaysTest_start() != RelaysTest_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_UIDEMO
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
    error_occurred = (BlePeripheral_enable() != BlePeripheral_ret_ok);
    if (!error_occurred)
    {
        ESP_LOGI(TAG, "************* Peripheral Role *****************************");
        ESP_LOGW(TAG, "Setting process_received_command");
        ble_peripheral_set_process_command_cb(process_received_command);
        sprintf(ble_device_name, "%s%s", BLE_DEVICE_NAME_PREFIX, PrjCfg_dre.unique_id);
        ble_peripheral_app(ble_device_name);
    }
#ifdef CONFIG_BLEPERIPHERAL_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (BlePeripheral_start() != BlePeripheral_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
    error_occurred = (ADCProbes_enable() != ADCProbes_ret_ok);
#ifdef CONFIG_ADCPROBES_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (ADCProbes_start() != ADCProbes_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    // Delayed UI phase (after OTA check)
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    // Delayed UI phase (after OTA check)
#endif
// [PORIS_INTEGRATION_START]
    if (error_accumulator)
    {
        ret = app_main_ret_error;
    }
    return ret;
}

#define MEASUREMENT_CYCLE_LIMIT ((MEASUREMENT_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define MQTTCOMM_CYCLE_LIMIT ((MQTTCOMM_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define DUALLED_CYCLE_PERIOD_MS 100
#define DUALLED_CYCLE_LIMIT ((DUALLED_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define DUALLEDTESTER_CYCLE_PERIOD_MS 100
#define DUALLEDTESTER_CYCLE_LIMIT ((DUALLEDTESTER_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define RELAYS_CYCLE_PERIOD_MS 100
#define RELAYS_CYCLE_LIMIT ((RELAYS_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define RELAYSTEST_CYCLE_PERIOD_MS 100
#define RELAYSTEST_CYCLE_LIMIT ((RELAYSTEST_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define TOUCHSCREEN_CYCLE_PERIOD_MS 50
#define TOUCHSCREEN_CYCLE_LIMIT_MS ((TOUCHSCREEN_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define BLEPERIPHERAL_CYCLE_PERIOD_MS 100
#define BLEPERIPHERAL_CYCLE_LIMIT_MS ((BLEPERIPHERAL_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#define ADCPROBES_CYCLE_PERIOD_MS 100
#define ADCPROBES_CYCLE_LIMIT_MS ((ADCPROBES_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#ifndef CONFIG_LCDFLAG_USE_THREAD
#define LCDFLAG_CYCLE_PERIOD_MS 100
#define LCDFLAG_CYCLE_LIMIT_MS ((LCDFLAG_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#endif
#ifndef CONFIG_PPINJECTORUI_USE_THREAD
#define PPINJECTORUI_CYCLE_PERIOD_MS 100
#define PPINJECTORUI_CYCLE_LIMIT_MS ((PPINJECTORUI_CYCLE_PERIOD_MS / MAIN_CYCLE_PERIOD_MS) - 1)
#endif
// [PORIS_INTEGRATION_DEFINES]

static uint16_t mqttcomm_cycle_counter = 0;
static uint16_t measurement_cycle_counter = 0;
static uint16_t dualled_cycle_counter = 0;
static uint16_t dualledtester_cycle_counter = 0;
static uint16_t relays_cycle_counter = 0;
static uint16_t relaystest_cycle_counter = 0;
static uint8_t touchscreen_cycle_counter = 0;
static uint8_t bleperipheral_cycle_counter = 0;
static uint8_t adcprobes_cycle_counter = 0;
#ifndef CONFIG_LCDFLAG_USE_THREAD
static uint8_t lcdflag_cycle_counter = 0;
#endif
#ifndef CONFIG_PPINJECTORUI_USE_THREAD
static uint8_t ppinjectorui_cycle_counter = 0;
#endif
// [PORIS_INTEGRATION_COUNTERS]

#ifdef CONFIG_PORIS_ENABLE_OTA
static bool ota_checked = false;
static bool ota_enabled = false;
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
static bool mqttcomm_started = false;
#endif
static bool ui_started = false;

#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN) && !defined(CONFIG_TOUCHSCREEN_RGB_PANEL_PROFILE_ELECROW)
static const char *s_prov_qr_payload_ptr = NULL;
static bool s_prov_qr_payload_ready = false;
static bool s_prov_qr_painted = false;
static bool s_prov_qr_render_task_running = false;
static int64_t s_prov_qr_payload_ts_us = 0;

static void main_try_render_provisioning_qr(void);

static void main_provisioning_qr_render_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "boot_qr: deferred render task started");
    vTaskDelay(pdMS_TO_TICKS(1000));
    main_try_render_provisioning_qr();
    s_prov_qr_render_task_running = false;
    ESP_LOGI(TAG, "boot_qr: deferred render task finished");
    vTaskDelete(NULL);
}

static void main_provisioning_qr_payload_cb(const char *payload)
{
    if (!payload)
    {
        return;
    }
    s_prov_qr_payload_ptr = payload;
    s_prov_qr_payload_ready = true;
    s_prov_qr_painted = false;
    s_prov_qr_payload_ts_us = esp_timer_get_time();
    ESP_LOGI(TAG, "boot_qr: payload captured from provisioning callback");
    if (!s_prov_qr_render_task_running)
    {
        s_prov_qr_render_task_running = true;
        BaseType_t ok = xTaskCreate(main_provisioning_qr_render_task, "boot_qr", 4096, NULL, 4, NULL);
        if (ok != pdPASS)
        {
            s_prov_qr_render_task_running = false;
            ESP_LOGW(TAG, "boot_qr: could not create deferred render task");
        }
    }
}

static void main_boot_qr_display_cb(esp_qrcode_handle_t qrcode)
{
    int w = TouchScreen_boot_display_width();
    int h = TouchScreen_boot_display_height();
    if (w <= 0 || h <= 0)
    {
        esp_qrcode_print_console(qrcode);
        return;
    }
    TouchScreen_boot_display_clear(0xFFFF);
    int size = esp_qrcode_get_size(qrcode);
    int max_side = (w < h ? w : h) - 80;
    if (max_side < size)
    {
        max_side = size;
    }
    int scale = max_side / size;
    if (scale < 1)
    {
        scale = 1;
    }
    int qr_w = size * scale;
    int x0 = (w - qr_w) / 2;
    int y0 = (h - qr_w) / 2;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (esp_qrcode_get_module(qrcode, x, y))
            {
                TouchScreen_boot_display_fill_rect(x0 + x * scale, y0 + y * scale, scale, scale, 0x0000);
            }
        }
    }
}

static void main_try_render_provisioning_qr(void)
{
    if (!s_prov_qr_payload_ready || s_prov_qr_painted)
    {
        return;
    }
    if ((esp_timer_get_time() - s_prov_qr_payload_ts_us) < 1000000)
    {
        return;
    }
    if (!TouchScreen_boot_display_ready())
    {
        esp_err_t d_err = TouchScreen_boot_display_init();
        if (d_err != ESP_OK)
        {
            ESP_LOGW(TAG, "boot_qr: display init failed: %s", esp_err_to_name(d_err));
            return;
        }
    }
    if (!TouchScreen_boot_display_ready())
    {
        return;
    }

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func = main_boot_qr_display_cb;
    ESP_LOGI(TAG, "boot_qr: painting QR on boot display now");
    if (!s_prov_qr_payload_ptr || s_prov_qr_payload_ptr[0] == '\0')
    {
        ESP_LOGW(TAG, "boot_qr: payload pointer invalid");
        return;
    }
    ESP_LOGI(TAG, "boot_qr: calling esp_qrcode_generate()");
    esp_qrcode_generate(&cfg, s_prov_qr_payload_ptr);
    s_prov_qr_painted = true;
    ESP_LOGI(TAG, "boot_qr: QR rendered on boot display");
}
#endif

#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
static char s_prov_status_pending[192];
static volatile bool s_prov_status_dirty = false;
static int s_prov_status_level = 0;

static int main_prov_status_level(const char *status)
{
    if (!status)
    {
        return 0;
    }
    if (strstr(status, "PROVISIONED OK"))
    {
        return 5;
    }
    if (strstr(status, "WIFI CONNECT ERROR") || strstr(status, "WIFI DISCONNECTED"))
    {
        return 4;
    }
    if (strstr(status, "CREDENTIALS RECEIVED") || strstr(status, "CONNECTING TO WIFI"))
    {
        return 3;
    }
    if (strstr(status, "PROVISIONING STARTED"))
    {
        return 2;
    }
    if (strstr(status, "PROVISIONING IN PROGRESS"))
    {
        return 1;
    }
    return 2;
}

static void main_provisioning_status_cb(const char *status)
{
    if (!status || ui_enabled)
    {
        return;
    }
    int new_level = main_prov_status_level(status);
    if (new_level < s_prov_status_level)
    {
        ESP_LOGI(TAG, "boot status ignored (downgrade): %s", status);
        return;
    }
    s_prov_status_level = new_level;
    snprintf(s_prov_status_pending, sizeof(s_prov_status_pending), "%s", status);
    s_prov_status_dirty = true;
    ESP_LOGI(TAG, "boot status queued: %s", s_prov_status_pending);
}
#endif

app_main_return_code init_ui_components(void)
{
    app_main_return_code ret = app_main_ret_ok;
    bool error_occurred = false;
    bool error_accumulator = false;

#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    error_occurred = (TouchScreen_setup() != TouchScreen_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_UIDEMO
    error_occurred = (UIDemo_setup() != UIDemo_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    error_occurred = (LCDFlag_setup() != LCDFlag_ret_ok);
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    error_occurred = (PPInjectorUI_setup() != PPInjectorUI_ret_ok);
    error_accumulator |= error_occurred;
#endif

    if (error_accumulator)
    {
        ret = app_main_ret_error;
    }
    return ret;
}

app_main_return_code start_ui_components(void)
{
    app_main_return_code ret = app_main_ret_ok;
    bool error_occurred = false;
    bool error_accumulator = false;

#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    error_occurred = (TouchScreen_enable() != TouchScreen_ret_ok);
#ifdef CONFIG_TOUCHSCREEN_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (TouchScreen_start() != TouchScreen_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_UIDEMO
    error_occurred = (UIDemo_enable() != UIDemo_ret_ok);
#ifdef CONFIG_UIDEMO_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (UIDemo_start() != UIDemo_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    error_occurred = (LCDFlag_enable() != LCDFlag_ret_ok);
#ifdef CONFIG_LCDFLAG_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (LCDFlag_start() != LCDFlag_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    error_occurred = (PPInjectorUI_enable() != PPInjectorUI_ret_ok);
#ifdef CONFIG_PPINJECTORUI_USE_THREAD
    if (!error_occurred)
    {
        error_occurred |= (PPInjectorUI_start() != PPInjectorUI_ret_ok);
    }
#endif
    error_accumulator |= error_occurred;
#endif

    if (error_accumulator)
    {
        ret = app_main_ret_error;
    }
    return ret;
}

app_main_return_code run_components(void)
{
    app_main_return_code ret = app_main_ret_ok;
    bool error_accumulator = false;

#ifdef CONFIG_PORIS_ENABLE_PRJCFG
#ifndef CONFIG_PRJCFG_USE_THREAD
    error_accumulator |= (PrjCfg_spin() != PrjCfg_ret_ok);
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    error_accumulator |= (Wifi_spin() != Wifi_ret_ok);
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    // OTA does not use spin
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
#ifndef CONFIG_MQTTCOMM_USE_THREAD
    if (ota_checked)
    {
        if (mqttcomm_cycle_counter <= 0)
        {
            error_accumulator |= (MQTTComm_spin() != MQTTComm_ret_ok);
            mqttcomm_cycle_counter = MQTTCOMM_CYCLE_LIMIT;
        }
        else
        {
            mqttcomm_cycle_counter--;
        }
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
#ifndef CONFIG_MEASUREMENT_USE_THREAD

    if (measurement_cycle_counter <= 0)
    {
        error_accumulator |= (Measurement_spin() != Measurement_ret_ok);
        measurement_cycle_counter = MEASUREMENT_CYCLE_LIMIT;
    }
    else
    {
        measurement_cycle_counter--;
    }

#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
#ifndef CONFIG_DUALLED_USE_THREAD
    if (dualled_cycle_counter <= 0)
    {
        error_accumulator |= (DualLED_spin() != DualLED_ret_ok);
        dualled_cycle_counter = DUALLED_CYCLE_LIMIT;
    }
    else
    {
        dualled_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
#ifndef CONFIG_RELAYS_USE_THREAD
    if (relays_cycle_counter <= 0)
    {
        error_accumulator |= (Relays_spin() != Relays_ret_ok);
        relays_cycle_counter = RELAYS_CYCLE_LIMIT;
    }
    else
    {
        relays_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
#ifndef CONFIG_RELAYSTEST_USE_THREAD
    if (relaystest_cycle_counter <= 0)
    {
        error_accumulator |= (RelaysTest_spin() != RelaysTest_ret_ok);
        relaystest_cycle_counter = RELAYSTEST_CYCLE_LIMIT;
    }
    else
    {
        relaystest_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
#ifndef CONFIG_DUALLEDTESTER_USE_THREAD
    if (dualledtester_cycle_counter <= 0)
    {
        error_accumulator |= (DualLedTester_spin() != DualLedTester_ret_ok);
        dualledtester_cycle_counter = DUALLEDTESTER_CYCLE_LIMIT;
    }
    else
    {
        dualledtester_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
#ifndef CONFIG_TOUCHSCREEN_USE_THREAD
    if (ui_started)
    {
        if (touchscreen_cycle_counter <= 0)
        {
            error_accumulator |= (TouchScreen_spin() != TouchScreen_ret_ok);
            touchscreen_cycle_counter = TOUCHSCREEN_CYCLE_LIMIT_MS;
        }
        else
        {
            touchscreen_cycle_counter--;
        }
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
#ifndef CONFIG_BLEPERIPHERAL_USE_THREAD
    if (bleperipheral_cycle_counter <= 0)
    {
        error_accumulator |= (BlePeripheral_spin() != BlePeripheral_ret_ok);
        bleperipheral_cycle_counter = BLEPERIPHERAL_CYCLE_LIMIT_MS;
    }
    else
    {
        bleperipheral_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
#ifndef CONFIG_ADCPROBES_USE_THREAD
    if (adcprobes_cycle_counter <= 0)
    {
        error_accumulator |= (ADCProbes_spin() != ADCProbes_ret_ok);
        adcprobes_cycle_counter = ADCPROBES_CYCLE_LIMIT_MS;
    }
    else
    {
        adcprobes_cycle_counter--;
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
#ifndef CONFIG_LCDFLAG_USE_THREAD
    if (ui_started)
    {
        if (lcdflag_cycle_counter <= 0)
        {
            error_accumulator |= (LCDFlag_spin() != LCDFlag_ret_ok);
            lcdflag_cycle_counter = LCDFLAG_CYCLE_LIMIT_MS;
        }
        else
        {
            lcdflag_cycle_counter--;
        }
    }
#endif
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
#ifndef CONFIG_PPINJECTORUI_USE_THREAD
    if (ui_started)
    {
        if (ppinjectorui_cycle_counter <= 0)
        {
            error_accumulator |= (PPInjectorUI_spin() != PPInjectorUI_ret_ok);
            ppinjectorui_cycle_counter = PPINJECTORUI_CYCLE_LIMIT_MS;
        }
        else
        {
            ppinjectorui_cycle_counter--;
        }
    }
#endif
#endif
// [PORIS_INTEGRATION_RUN]
    if (error_accumulator)
    {
        ret = app_main_ret_error;
    }
    return ret;
}

bool main_parse_callback(const char *data, int len, char *response)
{
    ESP_LOGI(TAG, "Parsing the CFG payload %d %.*s", len, len, data);
#ifdef CONFIG_PORIS_ENABLE_PRJCFG
    PrjCfg_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
    Measurement_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
    MQTTComm_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    OTA_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
    DualLED_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
    DualLedTester_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
    Relays_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
    RelaysTest_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_UDPCOMM
    UDPComm_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    Wifi_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    TouchScreen_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
    BlePeripheral_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
    ADCProbes_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    LCDFlag_config_parse_json(data);
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    PPInjectorUI_config_parse_json(data);
#endif
// [PORIS_INTEGRATION_NETVARS_PARSE]
    sprintf(response, "c ok");
    return true;
}

static uint32_t msg_counter = 0;
void main_compose_callback(char *data, int *len)
{
    sprintf((char *)data, "this is a payload (%lu)", msg_counter++);
    *len = strlen((char *)data);
    ESP_LOGI(TAG, "Composing the DATA payload %d %.*s", *len, *len, data);

    cJSON *root = cJSON_CreateObject();
#ifdef CONFIG_PORIS_ENABLE_PRJCFG
    PrjCfg_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
    Measurement_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
    MQTTComm_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    OTA_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLED
    DualLED_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_DUALLEDTESTER
    DualLedTester_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYS
    Relays_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_RELAYSTEST
    RelaysTest_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_UDPCOMM
    UDPComm_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    Wifi_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_TOUCHSCREEN
    TouchScreen_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_BLEPERIPHERAL
    BlePeripheral_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_ADCPROBES
    ADCProbes_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_LCDFLAG
    LCDFlag_netvars_append_json(root);
#endif
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
    PPInjectorUI_netvars_append_json(root);
#endif
// [PORIS_INTEGRATION_NETVARS_APPEND]

    char *cPayload = cJSON_PrintUnformatted(root);
    if (cPayload != NULL)
    {
        size_t length = strlen(cPayload);
        *len = (int)length;
        ESP_LOGI(TAG, "LEN %d", length);
        ESP_LOGI(TAG, "Payload %s", cPayload);
        memcpy(data, cPayload, length);
        data[length] = '\0';
        free(cPayload);
    }
    else
    {
        data[0] = 'a';
        data[1] = '\0';
        *len = 1;
    }
    cJSON_Delete(root);
}

void i_compose_callback(char *data, int *len)
{
    sprintf((char *)data, "this is a payload (%lu)", msg_counter++);
    *len = strlen((char *)data);
    ESP_LOGI(TAG, "Composing the DATA payload %d %.*s", *len, *len, data);

    cJSON *root = cJSON_CreateObject();
#ifdef CONFIG_PORIS_ENABLE_MEASUREMENT
    Measurement_netvars_append_json(root);
#endif
    char *cPayload = cJSON_PrintUnformatted(root);
    if (cPayload != NULL)
    {
        size_t length = strlen(cPayload);
        *len = (int)length;
        ESP_LOGI(TAG, "LEN %d", length);
        ESP_LOGI(TAG, "Payload %s", cPayload);
        memcpy(data, cPayload, length);
        data[length] = '\0';
        free(cPayload);
    }
    else
    {
        data[0] = 'a';
        data[1] = '\0';
        *len = 1;
    }
    cJSON_Delete(root);
}

bool main_req_parse_callback(const char *data, int len, char *response)
{
    bool ret = false;
    ESP_LOGI(TAG, "Parsing theCMD payload %d %.*s", len, len, data);
    if (len == 1)
    {
        if (data[0] == 'r')
        {
            ret = true;
            ESP_LOGW(TAG, "Restaring!!!");
            esp_restart();
        }
#ifdef CONFIG_PORIS_ENABLE_OTA
        if (data[0] == 'u')
        {
            ret = true;
            ESP_LOGW(TAG, "Going OTA!!!");
            OTA_enable();
            OTA_start();
        }
#endif
        if (data[0] == 'i')
        {
            i_compose_callback(response, &len);
            ret = true;
        }
    }
    return ret;
}

bool check_ip_valid(void)
{
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    return Provisioning_dre.ip_valid;
#endif
#ifdef CONFIG_PORIS_ENABLE_WIFI
    return Wifi_dre.ip_valid;
#endif
    return false;
}

void app_main(void)
{
    boot_fsm_init_on_reset_reason();

    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    bool shall_execute = true;

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN) && !defined(CONFIG_TOUCHSCREEN_RGB_PANEL_PROFILE_ELECROW)
    Provisioning_set_qr_payload_callback(main_provisioning_qr_payload_cb);
#endif
#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
    Provisioning_set_status_callback(main_provisioning_status_cb);
#endif

#if defined(CONFIG_PORIS_ENABLE_PPINJECTORUI)
    bool has_wifi_credentials = false;
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    has_wifi_credentials = Provisioning_is_provisioned();
#elif defined(CONFIG_PORIS_ENABLE_WIFI)
    has_wifi_credentials = true;
#endif
    PPInjectorUI_set_wifi_credentials_available(has_wifi_credentials);
    ESP_LOGI(TAG, "boot_fsm: wifi credentials available=%d", has_wifi_credentials);
#endif

#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) || defined(CONFIG_PORIS_ENABLE_OTA)
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    provisioning_enabled = false;
#endif
    ui_enabled = true;
    s_network_started = false;
#ifdef CONFIG_PORIS_ENABLE_OTA
    ota_enabled = false;
#endif
    switch (boot_state)
    {
    case BOOT_STATE_NORMAL:
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
        provisioning_enabled = false;
#endif
        ui_enabled = true;
        s_network_started = false;
        break;
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    case BOOT_STATE_PROV_START:
        ESP_LOGW(TAG, "boot_fsm: entering PROV_START");
        /* Keep one more boot in provisioning flow so mobile app can close session. */
        boot_state_noinit = BOOT_STATE_PROV_END;
#ifdef CONFIG_PORIS_ENABLE_OTA
        ota_enabled = false;
#endif
        provisioning_enabled = true;
        ui_enabled = false;
        break;
    case BOOT_STATE_PROV_END:
        ESP_LOGW(TAG, "boot_fsm: entering PROV_END");
#ifdef CONFIG_PORIS_ENABLE_OTA
        ota_enabled = true;
#endif
        provisioning_enabled = true;
        ui_enabled = false;
        break;
#endif
#ifdef CONFIG_PORIS_ENABLE_OTA
    case BOOT_STATE_OTA:
        ESP_LOGW(TAG, "boot_fsm: entering OTA");
        ota_enabled = true;
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
        provisioning_enabled = true;
#endif
        ui_enabled = false;
        s_network_started = true;
        break;
#endif
    default:
        ESP_LOGW(TAG, "boot_fsm: unexpected state %u", (unsigned)boot_state);
#ifdef CONFIG_PORIS_ENABLE_OTA
        ota_enabled = false;
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
        provisioning_enabled = false;
#endif
        ui_enabled = true;
        s_network_started = false;
        break;
    }
#ifdef CONFIG_PORIS_ENABLE_OTA
    ESP_LOGI(TAG, "boot_fsm: ota_enabled=%d", ota_enabled);
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    ESP_LOGI(TAG, "boot_fsm: provisioning_enabled=%d ui_enabled=%d (boot_state=%s)",
             provisioning_enabled, ui_enabled, boot_state_to_str(boot_state));
#else
    ESP_LOGI(TAG, "boot_fsm: ui_enabled=%d network_started=%d (boot_state=%s)",
             ui_enabled, s_network_started, boot_state_to_str(boot_state));
#endif
#endif

#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
    if (!ui_enabled)
    {
        const char *boot_msg = "BOOT IN PROGRESS";
        char boot_msg_buf[192];
        bool allow_boot_display = true;
#ifdef CONFIG_PORIS_ENABLE_OTA
        if (boot_state == BOOT_STATE_OTA)
        {
            boot_msg = "OTA IN PROGRESS";
        }
#endif
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
        if (boot_state == BOOT_STATE_PROV_START || boot_state == BOOT_STATE_PROV_END)
        {
            if (boot_state == BOOT_STATE_PROV_END)
            {            
                boot_msg = "PROVISIONING SUCCESSFUL";
            } 
            else 
            {
                char prov_name[24];
                main_get_provisioning_service_name(prov_name, sizeof(prov_name));
                snprintf(boot_msg_buf, sizeof(boot_msg_buf),
                         "PROVISIONING IN PROGRESS\nBLE %s\nUSE MOBILE APP", prov_name);
                boot_msg = boot_msg_buf;
            }
#if defined(CONFIG_PROV_TRANSPORT_BLE)
#if !defined(CONFIG_TOUCHSCREEN_RGB_PANEL_PROFILE_ELECROW)
            /* BLE provisioning is sensitive to internal RAM pressure. Skip RGB boot display here. */
            allow_boot_display = false;
#else
            ESP_LOGW(TAG, "boot display enabled during BLE provisioning (Elecrow best-effort)");
#endif
#endif
        }
#endif
        if (allow_boot_display)
        {
            esp_err_t d_err = TouchScreen_boot_display_init();
            if (d_err == ESP_OK)
            {
                TouchScreen_boot_display_draw_center_text(boot_msg, 0xFFFF, 0x0000);
            }
            else
            {
                ESP_LOGW(TAG, "boot display init failed: %s", esp_err_to_name(d_err));
            }
        }
        else
        {
            ESP_LOGW(TAG, "boot display skipped during BLE provisioning state to preserve RAM");
        }
    }
#endif

    if (init_components() != app_main_ret_ok)
    {
        ESP_LOGE(TAG, "Cannot init components!!!");
        shall_execute = false;
    }
    if (shall_execute)
    {
        if (start_components() != app_main_ret_ok)
        {
            ESP_LOGE(TAG, "Cannot start components!!!");
            shall_execute = false;
        }
        else
        {
            ESP_LOGI(TAG, "Core components started. UI can start without waiting for OTA/network.");
        }
#ifdef CONFIG_PORIS_ENABLE_SDMMCFS
        const char *sd_mount_point = "/sdcard";
        SdMmcFS_mount_config_t sd_cfg = {
            .mount_point = sd_mount_point,
            .format_if_mount_failed = false,
            .max_files = 8,
            .allocation_unit_size = 16 * 1024,
        };
        esp_err_t sd_err = SdMmcFS_mount(&sd_cfg);
        if (sd_err != ESP_OK)
        {
            ESP_LOGW(TAG, "No se pudo montar SD: %s", esp_err_to_name(sd_err));
        }
        else
        {
            ESP_ERROR_CHECK(SdMmcFS_log_dir(sd_mount_point));
#ifdef CONFIG_PORIS_ENABLE_SECUREBOX
            esp_err_t sec_err = SecureBox_run_demo(sd_mount_point, "index.htm");
            if (sec_err != ESP_OK)
            {
                ESP_LOGW(TAG, "Demo cifrado fallo: %s", esp_err_to_name(sec_err));
            }
#endif
        }
#endif
    }
    while (true)
    {
        // ESP_LOGI(TAG, "app_main spinning");
        vTaskDelay(MAIN_CYCLE_PERIOD_MS / portTICK_PERIOD_MS);
        if (shall_execute)
        {
#ifdef CONFIG_PORIS_ENABLE_OTA
#ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
            PPInjectorUI_system_action_t pending_action = PPInjectorUI_take_system_action();
            if (pending_action == PPInjectorUI_system_action_ota)
            {
                ESP_LOGW(TAG, "UI requested OTA");
                boot_state_noinit = BOOT_STATE_OTA;
                ESP_LOGW(TAG, "Rebooting into BOOT_STATE_OTA");
                esp_restart();
            }
            else if (pending_action == PPInjectorUI_system_action_reprovision)
            {
                ESP_LOGW(TAG, "UI requested forced re-provisioning");
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
                boot_state_noinit = BOOT_STATE_PROV_START;
                ESP_LOGW(TAG, "Forgetting credentials and rebooting into BOOT_STATE_PROV_START");
                Provisioning_forget();
#else
                ESP_LOGW(TAG, "Re-provision request ignored: provisioning component disabled");
#endif
            }
#endif

            if (ota_enabled && !ota_checked)
            {
                if (check_ip_valid())
                {
                    char ota_running_ver[32] = "unknown";
                    char ota_target_ver[32] = "unknown";
                    char ota_status[192];
#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
                    if (!ui_enabled)
                    {
                        if (!TouchScreen_boot_display_ready())
                        {
                            esp_err_t d_err = TouchScreen_boot_display_init();
                            if (d_err != ESP_OK)
                            {
                                ESP_LOGW(TAG, "boot display init failed before OTA: %s", esp_err_to_name(d_err));
                            }
                        }
                        if (TouchScreen_boot_display_ready())
                        {
                            OTA_get_running_version(ota_running_ver, sizeof(ota_running_ver));
                            OTA_get_target_version(ota_target_ver, sizeof(ota_target_ver));
                            snprintf(ota_status, sizeof(ota_status),
                                     "OTA IN PROGRESS\nCUR %s\nNEW %s\nSTARTING",
                                     ota_running_ver, ota_target_ver);
                            TouchScreen_boot_display_draw_center_text(ota_status, 0xFFFF, 0x0000);
                        }
                    }
#endif
                    // Boot-time actions
                    // Check OTA
                    OTA_enable();
                    OTA_start();
                    OTA_disable();
                    ota_checked = true;

                    /*
                     * From this point on, this branch owns the whole flow.
                     * It must exit only via esp_restart(): either triggered by OTA library
                     * on success, or by this branch after OTA task has ended.
                     */
                    int last_pct = -1;
                    while (OTA_is_running())
                    {
#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
                        if (!ui_enabled && TouchScreen_boot_display_ready())
                        {
                            int read_len = OTA_get_image_len_read();
                            int total_len = OTA_get_image_total_len();
                            int pct = 0;
                            if (total_len > 0)
                            {
                                pct = (int)((100LL * read_len) / total_len);
                                if (pct > 100)
                                {
                                    pct = 100;
                                }
                            }
                            if (pct != last_pct)
                            {
                                OTA_get_running_version(ota_running_ver, sizeof(ota_running_ver));
                                OTA_get_target_version(ota_target_ver, sizeof(ota_target_ver));
                                snprintf(ota_status, sizeof(ota_status),
                                         "OTA IN PROGRESS\nCUR %s\nNEW %s\nPROG %d",
                                         ota_running_ver, ota_target_ver, pct);
                                TouchScreen_boot_display_draw_center_text(ota_status, 0xFFFF, 0x0000);
                                last_pct = pct;
                            }
                        }
#endif
                        vTaskDelay(pdMS_TO_TICKS(200));
                    }

#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
                    if (!ui_enabled && TouchScreen_boot_display_ready())
                    {
                        esp_err_t ota_err = OTA_get_last_error();
                        OTA_get_running_version(ota_running_ver, sizeof(ota_running_ver));
                        OTA_get_target_version(ota_target_ver, sizeof(ota_target_ver));
                        snprintf(ota_status, sizeof(ota_status),
                                 "OTA FAILED\nCUR %s\nNEW %s\nERR 0x%04x\nREBOOT IN 10 S",
                                 ota_running_ver, ota_target_ver, (unsigned)(ota_err & 0xFFFF));
                        TouchScreen_boot_display_draw_center_text(ota_status, 0xFFFF, 0x0000);
                    }
#endif
                    ESP_LOGW(TAG, "OTA task finished without reboot. Rebooting in 10 seconds.");
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    esp_restart();
                }
            }
#endif
#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN) || defined(CONFIG_PORIS_ENABLE_UIDEMO) || defined(CONFIG_PORIS_ENABLE_LCDFLAG) || defined(CONFIG_PORIS_ENABLE_PPINJECTORUI)
            if (!ui_started && ui_enabled)
            {
                /* UI must not block on network/provisioning availability. */
                if (init_ui_components() != app_main_ret_ok)
                {
                    ESP_LOGE(TAG, "Cannot init UI components!!!");
                    shall_execute = false;
                }
                else if (start_ui_components() != app_main_ret_ok)
                {
                    ESP_LOGE(TAG, "Cannot start UI components!!!");
                    shall_execute = false;
                }
                else
                {
                    ui_started = true;
                    ESP_LOGI(TAG, "UI components started.");
                }
            }
#endif
#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN) && !defined(CONFIG_TOUCHSCREEN_RGB_PANEL_PROFILE_ELECROW)
            if (!ui_enabled)
            {
                main_try_render_provisioning_qr();
            }
#endif
#if defined(CONFIG_PORIS_ENABLE_PROVISIONING) && defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
            if (!ui_enabled && s_prov_status_dirty)
            {
                if (!TouchScreen_boot_display_ready())
                {
                    esp_err_t d_err = TouchScreen_boot_display_init();
                    if (d_err != ESP_OK)
                    {
                        ESP_LOGW(TAG, "boot status display init failed: %s", esp_err_to_name(d_err));
                    }
                }
                if (TouchScreen_boot_display_ready())
                {
                    esp_err_t err = TouchScreen_boot_display_draw_center_text(s_prov_status_pending, 0xFFFF, 0x0000);
                    if (err != ESP_OK)
                    {
                        ESP_LOGW(TAG, "boot status draw failed: %s", esp_err_to_name(err));
                    }
                    else
                    {
                        ESP_LOGI(TAG, "boot status painted");
                        s_prov_status_dirty = false;
                    }
                }
            }
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
            if (!mqttcomm_started)
            {
                if (check_ip_valid())
                {
                    // Now let's setup the MQTT topics
                    mqtt_comm_cfg_t cfg = {0};
                    cfg.f_cfg_cb = main_parse_callback;
                    cfg.f_req_cb = main_req_parse_callback;
                    cfg.f_data_cb = main_compose_callback;

                    MQTTComm_setup(&cfg);
                    ESP_LOGI(TAG, "---> MQTT topics for device %s", MQTTComm_dre.device);
                    ESP_LOGI(TAG, "     cfg : %s", MQTTComm_dre.cfg_topic);
                    ESP_LOGI(TAG, "     cmd : %s", MQTTComm_dre.req_topic);
                    ESP_LOGI(TAG, "     data: %s", MQTTComm_dre.data_topic);
#ifndef CONFIG_MQTTCOMM_USE_THREAD
                    MQTTComm_enable();
#endif
                    mqttcomm_started = true;
                }
            }
#endif
            if (run_components() != app_main_ret_ok)
            {
                ESP_LOGW(TAG, "Could not run all  components!!!");
            }
        }
    }
}
