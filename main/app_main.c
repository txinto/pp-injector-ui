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
#include <esp_log.h>
#include <esp_attr.h>

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
#define FORCED_PROVISIONING_MAGIC 0x50525631u
RTC_NOINIT_ATTR static uint32_t s_forced_provisioning_magic;

static bool s_network_started = false;

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
        ESP_LOGI(TAG,"This is a configuration command");
        ret = main_parse_callback((char *)&(inbuf[1]), inlen-1, response_buf);
    }
    else
    {
        ESP_LOGI(TAG, "This is a request command");
        ret = main_req_parse_callback((char *)inbuf, inlen, response_buf);
    }
    ESP_LOGI(TAG,"END Process command %.*s", inlen, (char *)inbuf);
    if (ret)
    {
        ESP_LOGI(TAG, "Process response %d %s",strlen(response_buf), response_buf);
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
    if (s_network_started)
    {
        error_occurred = (Provisioning_setup() != Provisioning_ret_ok);
        error_accumulator |= error_occurred;
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
    if (s_network_started)
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
        sprintf(ble_device_name,"%s%s", BLE_DEVICE_NAME_PREFIX, PrjCfg_dre.unique_id);
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
#endif
#ifdef CONFIG_PORIS_ENABLE_MQTTCOMM
static bool mqttcomm_started = false;
#endif
static bool ui_started = false;

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
            ESP_LOGW(TAG,"Restaring!!!");
            esp_restart();
        }
#ifdef CONFIG_PORIS_ENABLE_OTA
        if (data[0] == 'u')
        {
            ret = true;
            ESP_LOGW(TAG,"Going OTA!!!");
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

    bool forced_provisioning_mode = (s_forced_provisioning_magic == FORCED_PROVISIONING_MAGIC);
    s_forced_provisioning_magic = 0;

#ifdef CONFIG_PORIS_ENABLE_OTA
    ota_checked = true; // Disable implicit boot OTA.
#endif

#if defined(CONFIG_PORIS_ENABLE_PPINJECTORUI)
    // Keep UI offline-first: credentials state will be updated when networking is
    // intentionally started by user action.
    PPInjectorUI_set_wifi_credentials_available(false);
#endif

#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
    if (forced_provisioning_mode)
    {
        ESP_LOGW(TAG, "Forced provisioning mode: starting before UI/components");
        if (Provisioning_start_on_demand(true, true, true) == Provisioning_ret_ok)
        {
            ESP_LOGI(TAG, "Forced provisioning completed, rebooting");
            esp_restart();
        }
        ESP_LOGE(TAG, "Forced provisioning failed, rebooting");
        esp_restart();
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
        //ESP_LOGI(TAG, "app_main spinning");
        vTaskDelay(MAIN_CYCLE_PERIOD_MS / portTICK_PERIOD_MS);
        if (shall_execute)
        {
#ifdef CONFIG_PORIS_ENABLE_OTA
            #ifdef CONFIG_PORIS_ENABLE_PPINJECTORUI
            PPInjectorUI_system_action_t pending_action = PPInjectorUI_take_system_action();
            if (pending_action == PPInjectorUI_system_action_ota)
            {
                ESP_LOGW(TAG, "UI requested OTA on-demand");
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
                if (Provisioning_start_on_demand(false, true, false) == Provisioning_ret_ok)
                {
                    s_network_started = true;
                    OTA_enable();
                    OTA_start();
                    OTA_disable();
                }
                else
                {
                    PPInjectorUI_set_wifi_credentials_available(false);
                    ESP_LOGW(TAG, "OTA request ignored: no stored Wi-Fi credentials");
                }
#else
                ESP_LOGW(TAG, "OTA request ignored: provisioning component disabled");
#endif
            }
            else if (pending_action == PPInjectorUI_system_action_reprovision)
            {
                ESP_LOGW(TAG, "UI requested forced re-provisioning");
#ifdef CONFIG_PORIS_ENABLE_PROVISIONING
                s_forced_provisioning_magic = FORCED_PROVISIONING_MAGIC;
                ESP_LOGW(TAG, "Rebooting into forced provisioning mode");
                esp_restart();
#else
                ESP_LOGW(TAG, "Re-provision request ignored: provisioning component disabled");
#endif
            }
            #endif

            if (!ota_checked)
            {
                if (check_ip_valid())
                {
                    // Boot-time actions
                    // Check OTA
                    OTA_enable();
                    OTA_start();
                    // If OTA has not rebooted, we should continue
                    OTA_disable();
                    ota_checked = true;
                }
            }
#endif
#if defined(CONFIG_PORIS_ENABLE_TOUCHSCREEN) || defined(CONFIG_PORIS_ENABLE_UIDEMO) || defined(CONFIG_PORIS_ENABLE_LCDFLAG) || defined(CONFIG_PORIS_ENABLE_PPINJECTORUI)
            if (!ui_started)
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
