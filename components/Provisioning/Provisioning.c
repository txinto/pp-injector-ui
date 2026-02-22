// BEGIN --- Standard C headers section ---
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// END   --- Standard C headers section ---

// BEGIN --- SDK config section---
#include <sdkconfig.h>
// END   --- SDK config section---

// BEGIN --- FreeRTOS headers section ---
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if CONFIG_PROVISIONING_USE_THREAD
  #include <freertos/semphr.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

// END   --- FreeRTOS headers section ---


// BEGIN --- ESP-IDF headers section ---
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
// END   --- ESP-IDF headers section ---

#include <wifi_provisioning/manager.h>
#include "qrcode.h"

// BEGIN --- Project configuration section ---
#include <PrjCfg.h> // Including project configuration module 
// END   --- Project configuration section ---

// BEGIN --- Self-includes section ---
#include "Provisioning.h"
#include "Provisioning_netvars.h"

// END --- Self-includes section ---

// BEGIN --- Logging related variables
static const char *TAG = "Provisioning";
// END --- Logging related variables

static char s_prov_qr_payload[180];
static bool s_prov_qr_payload_ready = false;
static Provisioning_qr_payload_callback_t s_qr_payload_callback = NULL;
static Provisioning_status_callback_t s_status_callback = NULL;
static char s_status_text[192];
static char s_service_name[12];

// BEGIN --- Internal variables (DRE)
Provisioning_dre_t Provisioning_dre = {
    .enabled = true,
    .ip_valid = false,
    .last_return_code = Provisioning_ret_ok
};
// END   --- Internal variables (DRE)

// BEGIN --- Multitasking variables and handlers


#ifdef CONFIG_PROV_TRANSPORT_BLE
#include <wifi_provisioning/scheme_ble.h>
#endif /* CONFIG_PROV_TRANSPORT_BLE */

#ifdef CONFIG_PROV_TRANSPORT_SOFTAP
#include <wifi_provisioning/scheme_softap.h>
#endif /* CONFIG_PROV_TRANSPORT_SOFTAP */


#if CONFIG_PROV_SECURITY_VERSION_2
#if CONFIG_PROV_PROV_SEC2_DEV_MODE
#define PROV_PROV_SEC2_USERNAME "wifiprov"
#define PROV_PROV_SEC2_PWD "abcd1234"

/* This salt,verifier has been generated for username = "wifiprov" and password = "abcd1234"
 * IMPORTANT NOTE: For production cases, this must be unique to every device
 * and should come from device manufacturing partition.*/
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba};
#endif

static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len)
{
#if CONFIG_PROV_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_PROV_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}

static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
#if CONFIG_PROV_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_PROV_PROV_SEC2_PROD_MODE
    /* This code needs to be updated with appropriate implementation to provide verifier */
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}
#endif

/* Signal Wi-Fi events on this event-group */
const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_SOFTAP "softap"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

static void provisioning_publish_statusf(const char *fmt, ...)
{
    if (!s_status_callback || !fmt)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_status_text, sizeof(s_status_text), fmt, args);
    va_end(args);
    s_status_callback(s_status_text);
}

static const char *wifi_reason_to_text(int reason)
{
    switch (reason)
    {
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY: return "INACTIVITY";
    default: return "UNKNOWN";
    }
}

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
#ifdef CONFIG_PROV_RESET_PROV_MGR_ON_FAILURE
    static int retries;
#endif
    if (event_base == WIFI_PROV_EVENT)
    {
        switch (event_id)
        {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            provisioning_publish_statusf("PROVISIONING STARTED\nBLE %s\nUSE MOBILE APP",
                                         s_service_name[0] ? s_service_name : "PPI_?");
            break;
        case WIFI_PROV_CRED_RECV:
        {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials"
                          "\n\tSSID     : %s\n\tPassword : %s",
                     (const char *)wifi_sta_cfg->ssid,
                     (const char *)wifi_sta_cfg->password);
            provisioning_publish_statusf("CREDENTIALS RECEIVED\nCONNECTING TO WIFI...");
            break;
        }
        case WIFI_PROV_CRED_FAIL:
        {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                          "\n\tPlease reset to factory and retry provisioning",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            provisioning_publish_statusf("WIFI CONNECT ERROR\n%d (%s)",
                                         (int)*reason,
                                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "AUTH_FAIL" : "AP_NOT_FOUND");
#ifdef CONFIG_PROV_RESET_PROV_MGR_ON_FAILURE
            retries++;
            if (retries >= CONFIG_PROV_MGR_MAX_RETRY_CNT)
            {
                ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
                wifi_prov_mgr_reset_sm_state_on_failure();
                retries = 0;
            }
#endif
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
#ifdef CONFIG_PROV_RESET_PROV_MGR_ON_FAILURE
            retries = 0;
#endif
            break;
        case WIFI_PROV_END:
            /* De-initialize manager once provisioning is finished */
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
        }
    }
    else if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
            if (event_data)
            {
                wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
                provisioning_publish_statusf("WIFI DISCONNECTED\nreason=%d (%s)",
                                             (int)disc->reason,
                                             wifi_reason_to_text((int)disc->reason));
            }
            esp_wifi_connect();
            break;
#ifdef CONFIG_PROV_TRANSPORT_SOFTAP
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SoftAP transport: Connected!");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
            break;
#endif
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_ap_record_t wifi_data;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sprintf(Provisioning_dre.ip_str, IPSTR, IP2STR(&event->ip_info.ip));
        Provisioning_dre.ip.addr = event->ip_info.ip.addr; 
        strcpy(Provisioning_dre.ssid, (char *)wifi_data.ssid);
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        provisioning_publish_statusf("PROVISIONED OK\nIP " IPSTR "\nWAITING NEXT STEP", IP2STR(&event->ip_info.ip));
        Provisioning_dre.ip_valid = true;
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
#ifdef CONFIG_PROV_TRANSPORT_BLE
    }
    else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT)
    {
        switch (event_id)
        {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "BLE transport: Connected!");
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "BLE transport: Disconnected!");
            break;
        default:
            break;
        }
#endif
    }
    else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT)
    {
        switch (event_id)
        {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Secured session established!");
            break;
        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
            break;
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!");
            break;
        default:
            break;
        }
    }
}

static void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = CONFIG_PROVISIONING_SERVICE_NAME_PREFIX;
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 */
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                   uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf)
    {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL)
    {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

static void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport)
{
    if (!name || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = {0};
    if (pop)
    {
#if CONFIG_PROV_SECURITY_VERSION_1
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                           ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, pop, transport);
#elif CONFIG_PROV_SECURITY_VERSION_2
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                           ",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, username, pop, transport);
#endif
    }
    else
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                           ",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, transport);
    }
    strncpy(s_prov_qr_payload, payload, sizeof(s_prov_qr_payload) - 1);
    s_prov_qr_payload[sizeof(s_prov_qr_payload) - 1] = '\0';
    s_prov_qr_payload_ready = true;
    if (s_qr_payload_callback)
    {
        s_qr_payload_callback(s_prov_qr_payload);
    }
    provisioning_publish_statusf("PROVISIONING IN PROGRESS\nBLE: %s\nUSE MOBILE APP", name ? name : "N/A");
#ifdef CONFIG_PROV_SHOW_QR
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
#endif /* CONFIG_APP_WIFI_PROV_SHOW_QR */
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload);
}


void provision_app(void)
{

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop if not present */
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(err);
    }

    wifi_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
#ifdef CONFIG_PROV_TRANSPORT_BLE
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
#endif
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
#ifdef CONFIG_PROV_TRANSPORT_SOFTAP
    esp_netif_create_default_wifi_ap();
#endif /* CONFIG_PROV_TRANSPORT_SOFTAP */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    bool provisioned = false;
#ifdef CONFIG_PROV_RESET_PROVISIONED
    wifi_prov_mgr_reset_provisioning();
#else
    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
#endif

    ESP_LOGI(TAG, "Provisioning check: provisioned=%d", provisioned ? 1 : 0);

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned)
    {
        ESP_LOGI(TAG, "Provisioning flow: entering initial provisioning mode");
        /* Configuration for the provisioning manager */
        wifi_prov_mgr_config_t config = {
        /* What is the Provisioning Scheme that we want ?
         * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
#ifdef CONFIG_PROV_TRANSPORT_BLE
            .scheme = wifi_prov_scheme_ble,
#endif /* CONFIG_PROV_TRANSPORT_BLE */
#ifdef CONFIG_PROV_TRANSPORT_SOFTAP
            .scheme = wifi_prov_scheme_softap,
#endif /* CONFIG_PROV_TRANSPORT_SOFTAP */

        /* Any default scheme specific event handler that you would
         * like to choose. Since our example application requires
         * neither BT nor BLE, we can choose to release the associated
         * memory once provisioning is complete, or not needed
         * (in case when device is already provisioned). Choosing
         * appropriate scheme specific event handler allows the manager
         * to take care of this automatically. This can be set to
         * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
#ifdef CONFIG_PROV_TRANSPORT_BLE
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
#endif /* CONFIG_PROV_TRANSPORT_BLE */
#ifdef CONFIG_PROV_TRANSPORT_SOFTAP
                                        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
#endif /* CONFIG_PROV_TRANSPORT_SOFTAP */
        };

        /* Initialize provisioning manager with the
         * configuration parameters set above */
        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

        ESP_LOGI(TAG, "Starting provisioning");

        /* What is the Device Service Name that we want
         * This translates to :
         *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
         *     - device name when scheme is wifi_prov_scheme_ble
         */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        snprintf(s_service_name, sizeof(s_service_name), "%s", service_name);
#ifdef CONFIG_PROV_SECURITY_VERSION_1
        /* What is the security level that we want (0, 1, 2):
         *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
         *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
         *          using X25519 key exchange and proof of possession (pop) and AES-CTR
         *          for encryption/decryption of messages.
         *      - WIFI_PROV_SECURITY_2 SRP6a based authentication and key exchange
         *        + AES-GCM encryption/decryption of messages
         */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        /* Do we want a proof-of-possession (ignored if Security 0 is selected):
         *      - this should be a string with length > 0
         *      - NULL if not used
         */
        const char *pop = "abcd1234";

        /* This is the structure for passing security parameters
         * for the protocomm security 1.
         */
        wifi_prov_security1_params_t *sec_params = pop;

        const char *username = NULL;

#elif CONFIG_PROV_SECURITY_VERSION_2
        wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
        /* The username must be the same one, which has been used in the generation of salt and verifier */
#if CONFIG_PROV_PROV_SEC2_DEV_MODE
        /* This pop field represents the password that will be used to generate salt and verifier.
         * The field is present here in order to generate the QR code containing password.
         * In production this password field shall not be stored on the device */
        const char *username = PROV_PROV_SEC2_USERNAME;
        const char *pop = PROV_PROV_SEC2_PWD;
#elif CONFIG_PROV_PROV_SEC2_PROD_MODE
        /* The username and password shall not be embedded in the firmware,
         * they should be provided to the user by other means.
         * e.g. QR code sticker */
        const char *username = NULL;
        const char *pop = NULL;
#endif
        /* This is the structure for passing security parameters
         * for the protocomm security 2.
         * If dynamically allocated, sec2_params pointer and its content
         * must be valid till WIFI_PROV_END event is triggered.
         */
        wifi_prov_security2_params_t sec2_params = {};
        ESP_ERROR_CHECK(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len));
        ESP_ERROR_CHECK(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len));
        wifi_prov_security2_params_t *sec_params = &sec2_params;
#endif
        /* What is the service key (could be NULL)
         * This translates to :
         *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
         *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
         *     - simply ignored when scheme is wifi_prov_scheme_ble
         */
        const char *service_key = NULL;
#ifdef CONFIG_PROV_TRANSPORT_BLE
        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4,
            0xdf,
            0x5a,
            0x1c,
            0x3f,
            0x6b,
            0xf4,
            0xbf,
            0xea,
            0x4a,
            0x82,
            0x03,
            0x04,
            0x90,
            0x1a,
            0x02,
        };

        /* If your build fails with linker errors at this point, then you may have
         * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
         * the sdkconfig.defaults in the example project) */
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
#endif /* CONFIG_PROV_TRANSPORT_BLE */
        /* An optional endpoint that applications can create if they expect to
         * get some additional custom data during provisioning workflow.
         * The endpoint name can be anything of your choice.
         * This call must be made before starting the provisioning.
         */
        wifi_prov_mgr_endpoint_create("custom-data");
        /* Do not stop and de-init provisioning even after success,
         * so that we can restart it later. */
#ifdef CONFIG_PROV_REPROVISIONING
        wifi_prov_mgr_disable_auto_stop(1000);
#endif
        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, service_key));
        /* The handler for the optional endpoint created above.
         * This call must be made after starting the provisioning, and only if the endpoint
         * has already been created above.
         */
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);
        /* Uncomment the following to wait for the provisioning to finish and then release
         * the resources of the manager. Since in this case de-initialization is triggered
         * by the default event loop handler, we don't need to call the following */
        // wifi_prov_mgr_wait();
        // wifi_prov_mgr_deinit();
        /* Print QR code for provisioning */
#ifdef CONFIG_PROV_TRANSPORT_BLE
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);
#else  /* CONFIG_PROV_TRANSPORT_SOFTAP */
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_SOFTAP);
#endif /* CONFIG_PROV_TRANSPORT_BLE */
        /* Wait for Wi-Fi connection */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);

        ESP_LOGI(TAG, "Successfully provisioned, rebooting in 10 s");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else
    {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        /* Start Wi-Fi station */
        wifi_init_sta();
    }

    /* Wait for Wi-Fi connection */
    // xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);

    /* Start main application now */
#if CONFIG_PROV_REPROVISIONING
    while (1)
    {
        for (int i = 0; i < 10; i++)
        {
            ESP_LOGI(TAG, "Hello World!");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        /* Resetting provisioning state machine to enable re-provisioning */
        wifi_prov_mgr_reset_sm_state_for_reprovision();

        /* Wait for Wi-Fi connection */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);
    }
#else
/* Txinto: Removing this in order to fall back to the application */
/*
     while (1) {
         ESP_LOGI(TAG, "Hello World!");
         vTaskDelay(1000 / portTICK_PERIOD_MS);
     }
*/
#endif
}


/* This has to be called to forget credentials*/
void Provisioning_forget(void)
{

    ESP_LOGI(TAG, "Forgetting credentials");

    wifi_config_t wifi_cfg = {0};
    esp_err_t err = esp_wifi_stop();

    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGW(TAG, "Wi-Fi not initialized, initializing STA to clear credentials");

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_netif_create_default_wifi_sta();
        esp_err_t init_err = esp_wifi_init(&cfg);
        if (init_err != ESP_OK && init_err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(init_err));
        }
        else
        {
            esp_wifi_set_mode(WIFI_MODE_STA);
            err = esp_wifi_stop();
        }
    }
    else if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGW(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err == ESP_ERR_WIFI_STATE)
    {
        ESP_LOGW(TAG, "Wi-Fi busy state while clearing config, retrying after stop");
        esp_wifi_stop();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clear wifi config, 0x%x (%s)", err, esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "********* CREDENTIALS FORGOTTEN !!! ******");
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();
}

#if CONFIG_PROVISIONING_USE_THREAD
static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;
static uint32_t s_period_ms =
    #ifdef CONFIG_PROVISIONING_PERIOD_MS
      CONFIG_PROVISIONING_PERIOD_MS
    #else
      1000
    #endif
;
static SemaphoreHandle_t s_mutex = NULL;

static inline void _lock(void)   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
static inline void _unlock(void) { if (s_mutex) xSemaphoreGive(s_mutex); }

#ifdef CONFIG_PROVISIONING_MINIMIZE_JITTER
    static TickType_t xLastWakeTime;
    static TickType_t xFrequency;
#endif

static Provisioning_return_code_t Provisioning_spin(void);  // In case we are using a thread, this function should not be part of the public API


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
    #if CONFIG_PROVISIONING_PIN_CORE_ANY
        return tskNO_AFFINITY;
    #elif CONFIG_PROVISIONING_PIN_CORE_0
        return 0;
    #elif CONFIG_PROVISIONING_PIN_CORE_1
        return 1;
    #else
        return tskNO_AFFINITY;
    #endif
}

static void Provisioning_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "task started (period=%u ms)", (unsigned)s_period_ms);
#ifdef CONFIG_PROVISIONING_MINIMIZE_JITTER
    xLastWakeTime = xTaskGetTickCount();
    xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
    while (s_run) {
        Provisioning_return_code_t ret = Provisioning_spin();
        if (ret != Provisioning_ret_ok)
        {
            ESP_LOGW(TAG, "Error in spin");
        }
#ifdef CONFIG_PROVISIONING_MINIMIZE_JITTER
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
#else
        vTaskDelay(pdMS_TO_TICKS(s_period_ms));
#endif
    }
    ESP_LOGI(TAG, "task exit");
    vTaskDelete(NULL);
}

#endif // CONFIG_PROVISIONING_USE_THREAD

// END   --- Multitasking variables and handlers

// BEGIN ------------------ Public API (MULTITASKING)------------------


#if CONFIG_PROVISIONING_USE_THREAD

Provisioning_return_code_t Provisioning_start(void)
{
    if (_create_mutex_once() != pdPASS) {
        ESP_LOGE(TAG, "mutex creation failed");
        return Provisioning_ret_error;
    }
    if (s_task) {
        // idempotente
        return Provisioning_ret_ok;
    }
    s_run = true;

    BaseType_t core = _get_core_affinity();
    BaseType_t ok = xTaskCreatePinnedToCore(
        Provisioning_task,
        "Provisioning",
        CONFIG_PROVISIONING_TASK_STACK,
        NULL,
        CONFIG_PROVISIONING_TASK_PRIO,
        &s_task,
        core
    );
    if (ok != pdPASS) {
        s_task = NULL;
        s_run = false;
        ESP_LOGE(TAG, "xTaskCreatePinnedToCore failed");
        return Provisioning_ret_error;
    }
    return Provisioning_ret_ok;
}

Provisioning_return_code_t Provisioning_stop(void)
{
    if (!s_task) return Provisioning_ret_ok; // idempotente
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
    return Provisioning_ret_ok;
}

Provisioning_return_code_t Provisioning_get_dre_clone(Provisioning_dre_t *dst)
{
    if (!dst) return Provisioning_ret_error;
    _lock();
    memcpy(dst, &Provisioning_dre, sizeof(Provisioning_dre));
    _unlock();
    return Provisioning_ret_ok;
}

Provisioning_return_code_t Provisioning_set_period_ms(uint32_t period_ms)
{
    if (period_ms < 10) period_ms = 10;
    _lock();
    s_period_ms = period_ms;
#ifdef CONFIG_PROVISIONING_MINIMIZE_JITTER    
    xFrequency = (s_period_ms / portTICK_PERIOD_MS);
#endif
    _unlock();
    ESP_LOGI(TAG, "period set to %u ms", (unsigned)period_ms);
    return Provisioning_ret_ok;
}

uint32_t Provisioning_get_period_ms(void)
{
    _lock();
    uint32_t v = s_period_ms;
    _unlock();
    return v;
}



#endif // CONFIG_PROVISIONING_USE_THREAD

bool Provisioning_ip_valid(void)
{
#ifdef CONFIG_PROVISIONING_USE_THREAD
    _lock();
#endif
    bool ret = Provisioning_dre.ip_valid;
#ifdef CONFIG_PROVISIONING_USE_THREAD
    _unlock();
#endif
    return ret;
}

bool Provisioning_is_provisioned(void)
{
    bool provisioned = false;
    esp_err_t err = wifi_prov_mgr_is_provisioned(&provisioned);
    if (err == ESP_OK)
    {
        return provisioned;
    }

    /* If Wi-Fi driver is not initialized yet, init minimally and retry once. */
    bool wifi_initialized_here = false;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err == ESP_OK)
    {
        wifi_initialized_here = true;
    }
    else if (err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "is_provisioned: esp_wifi_init failed (%s)", esp_err_to_name(err));
        return false;
    }

    err = wifi_prov_mgr_is_provisioned(&provisioned);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "is_provisioned: wifi_prov_mgr_is_provisioned failed (%s)", esp_err_to_name(err));
        provisioned = false;
    }

    if (wifi_initialized_here)
    {
        esp_wifi_deinit();
    }

    return provisioned;
}

void Provisioning_set_qr_payload_callback(Provisioning_qr_payload_callback_t callback)
{
    s_qr_payload_callback = callback;
}

bool Provisioning_get_last_qr_payload(char *dst, size_t dst_len)
{
    if (!dst || dst_len == 0 || !s_prov_qr_payload_ready)
    {
        return false;
    }
    strncpy(dst, s_prov_qr_payload, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return true;
}

void Provisioning_set_status_callback(Provisioning_status_callback_t callback)
{
    s_status_callback = callback;
}


// END   ------------------ Public API (MULTITASKING)------------------

void provision_app(void);

// BEGIN ------------------ Public API (COMMON + SPIN)------------------
/**
 *  Execute a function wrapped with locks so you can access the DRE variables in thread-safe mode
 */
void Provisioning_execute_function_safemode(void (*callback)())
{
#ifdef CONFIG_PROVISIONING_USE_THREAD
    _lock();
#endif
    callback();
#ifdef CONFIG_PROVISIONING_USE_THREAD
    _unlock();
#endif
}


Provisioning_return_code_t Provisioning_setup(void)
{
    // Init liviano; no arranca tarea.
    ESP_LOGD(TAG, "setup()");
#if CONFIG_PROVISIONING_USE_THREAD
    if (_create_mutex_once() != pdPASS) {
        ESP_LOGE(TAG, "mutex creation failed");
        return Provisioning_ret_error;
    }
#endif
    provision_app();
    Provisioning_dre.last_return_code = Provisioning_ret_ok;
    return Provisioning_ret_ok;
}

#if CONFIG_PROVISIONING_USE_THREAD
static  // In case we are using a thread, this function should not be part of the public API
#endif
Provisioning_return_code_t Provisioning_spin(void)
{
#if CONFIG_PROVISIONING_USE_THREAD
    _lock();
#endif
    bool en = Provisioning_dre.enabled;
#if CONFIG_PROVISIONING_USE_THREAD
    _unlock();
#endif

    if (!en)
    {
#if CONFIG_PROVISIONING_USE_THREAD        
        _unlock();
#endif
        return Provisioning_ret_ok;
    }
    else
    {
        // Implement your spin here
        // this area is protected, so concentrate here
        // the stuff which needs protection against
        // concurrency issues

        //ESP_LOGI(TAG, "Doing protected stuff %d", Provisioning_dre.enabled);
        //vTaskDelay(pdMS_TO_TICKS(120));

#if CONFIG_PROVISIONING_USE_THREAD
        // Unlocking after the protected data has been managed for this cycle
        _unlock();
#endif
        Provisioning_nvs_spin();

        // Communicate results, do stuff which 
        // does not need protection
        // ...
        //ESP_LOGI(TAG, "Hello world!");
        return Provisioning_ret_ok;
    }
}

Provisioning_return_code_t Provisioning_enable(void)
{
#if CONFIG_PROVISIONING_USE_THREAD
    _lock();
#endif
    Provisioning_dre.enabled = true;
    Provisioning_dre.last_return_code = Provisioning_ret_ok;
#if CONFIG_PROVISIONING_USE_THREAD
    _unlock();
#endif
    return Provisioning_ret_ok;
}

Provisioning_return_code_t Provisioning_disable(void)
{
#if CONFIG_PROVISIONING_USE_THREAD
    _lock();
#endif
    Provisioning_dre.enabled = false;
    Provisioning_dre.last_return_code = Provisioning_ret_ok;
#if CONFIG_PROVISIONING_USE_THREAD
    _unlock();
#endif
    return Provisioning_ret_ok;
}

// END ------------------ Public API (COMMON)------------------
