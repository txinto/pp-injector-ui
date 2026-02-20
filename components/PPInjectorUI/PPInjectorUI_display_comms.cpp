#include "PPInjectorUI_display_comms.h"

#include "ui/eez-flow.h"
#include "ui/screens.h"
#include "ui/structs.h"
#include "ui/ui.h"
#include "ui/vars.h"

#include <esp_log.h>

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace DisplayComms {

static const char *TAG = "PPInjectorComms";

static Status status = {};
static MouldParams mould = {};
static CommonParams common = {};

static bool s_mock_enabled = false;
static bool s_mock_has_pos = false;
static bool s_mock_has_temp = false;
static float s_mock_pos = 0.0f;
static float s_mock_temp = 0.0f;
static char s_mock_state[24] = {0};
static Status s_status_view = {};

static tx_callback_t s_tx_cb = nullptr;
static void *s_tx_ctx = nullptr;

static float turnsToCm3(float turns)
{
    static const float TURNS_PER_CM3 = 0.99925f;
    if (TURNS_PER_CM3 == 0.0f) {
        return 0.0f;
    }
    return turns / TURNS_PER_CM3;
}

static const char *nextToken(const char *str, char *out, size_t outLen, char delim)
{
    if (!str || !out || outLen == 0) {
        return nullptr;
    }
    const char *p = strchr(str, delim);
    if (p) {
        size_t len = (size_t)(p - str);
        if (len >= outLen) {
            len = outLen - 1;
        }
        memcpy(out, str, len);
        out[len] = '\0';
        return p + 1;
    }
    strncpy(out, str, outLen - 1);
    out[outLen - 1] = '\0';
    return nullptr;
}

static void trimInPlace(char *text)
{
    if (!text) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start++;
    }

    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static const Status &effectiveStatus(void)
{
    if (!s_mock_enabled) {
        return status;
    }

    s_status_view = status;
    if (s_mock_has_pos) {
        s_status_view.encoderTurns = s_mock_pos;
    }
    if (s_mock_has_temp) {
        s_status_view.tempC = s_mock_temp;
    }
    if (s_mock_state[0] != '\0') {
        strncpy(s_status_view.state, s_mock_state, sizeof(s_status_view.state) - 1);
        s_status_view.state[sizeof(s_status_view.state) - 1] = '\0';
    }
    return s_status_view;
}

static void parseMessage(const char *msg)
{
    char cmd[24] = {0};
    const char *rest = nextToken(msg, cmd, sizeof(cmd), '|');
    trimInPlace(cmd);

    if (strcasecmp(cmd, "ENC") == 0) {
        if (rest) {
            status.encoderTurns = (float)atof(rest);
        }
        return;
    }

    if (strcasecmp(cmd, "TEMP") == 0) {
        if (rest) {
            status.tempC = (float)atof(rest);
        }
        return;
    }

    if (strcasecmp(cmd, "STATE") == 0) {
        char field[32] = {0};
        if (rest) {
            rest = nextToken(rest, field, sizeof(field), '|');
            trimInPlace(field);
            strncpy(status.state, field, sizeof(status.state) - 1);
            status.state[sizeof(status.state) - 1] = '\0';
        }
        return;
    }

    if (strcasecmp(cmd, "ERROR") == 0) {
        char field[64] = {0};
        if (rest) {
            rest = nextToken(rest, field, sizeof(field), '|');
            trimInPlace(field);
            status.errorCode = (uint16_t)strtoul(field, nullptr, 16);
            if (rest) {
                strncpy(status.errorMsg, rest, sizeof(status.errorMsg) - 1);
                status.errorMsg[sizeof(status.errorMsg) - 1] = '\0';
                trimInPlace(status.errorMsg);
            }
        }
        return;
    }

    if (strcasecmp(cmd, "MOULD_OK") == 0) {
        char field[64] = {0};
        int idx = 0;
        while (rest) {
            rest = nextToken(rest, field, sizeof(field), '|');
            trimInPlace(field);
            switch (idx) {
                case 0: strncpy(mould.name, field, sizeof(mould.name) - 1); mould.name[sizeof(mould.name) - 1] = '\0'; break;
                case 1: mould.fillVolume = atof(field); break;
                case 2: mould.fillSpeed = atof(field); break;
                case 3: mould.fillPressure = atof(field); break;
                case 4: mould.packVolume = atof(field); break;
                case 5: mould.packSpeed = atof(field); break;
                case 6: mould.packPressure = atof(field); break;
                case 7: mould.packTime = atof(field); break;
                case 8: mould.coolingTime = atof(field); break;
                case 9: mould.fillAccel = atof(field); break;
                case 10: mould.fillDecel = atof(field); break;
                case 11: mould.packAccel = atof(field); break;
                case 12: mould.packDecel = atof(field); break;
                case 13: strncpy(mould.mode, field, sizeof(mould.mode) - 1); mould.mode[sizeof(mould.mode) - 1] = '\0'; break;
                case 14: mould.injectTorque = atof(field); break;
                default: break;
            }
            idx++;
        }
        return;
    }

    if (strcasecmp(cmd, "COMMON_OK") == 0) {
        char field[64] = {0};
        int idx = 0;
        while (rest) {
            rest = nextToken(rest, field, sizeof(field), '|');
            trimInPlace(field);
            switch (idx) {
                case 0: common.trapAccel = atof(field); break;
                case 1: common.compressTorque = atof(field); break;
                case 2: common.microIntervalMs = (uint32_t)atol(field); break;
                case 3: common.microDurationMs = (uint32_t)atol(field); break;
                case 4: common.purgeUp = atof(field); break;
                case 5: common.purgeDown = atof(field); break;
                case 6: common.purgeCurrent = atof(field); break;
                case 7: common.antidripVel = atof(field); break;
                case 8: common.antidripCurrent = atof(field); break;
                case 9: common.releaseDist = atof(field); break;
                case 10: common.releaseTrapVel = atof(field); break;
                case 11: common.releaseCurrent = atof(field); break;
                case 12: common.contactorCycles = (uint32_t)atol(field); break;
                case 13: common.contactorLimit = (uint32_t)atol(field); break;
                default: break;
            }
            idx++;
        }
        return;
    }

    if (strcasecmp(cmd, "MOCK") == 0) {
        char action[16] = {0};
        char field[32] = {0};
        if (!rest) {
            return;
        }

        rest = nextToken(rest, action, sizeof(action), '|');
        trimInPlace(action);

        if (strcasecmp(action, "OFF") == 0) {
            s_mock_enabled = false;
            s_mock_has_pos = false;
            s_mock_has_temp = false;
            s_mock_pos = 0.0f;
            s_mock_temp = 0.0f;
            s_mock_state[0] = '\0';
            ESP_LOGI(TAG, "MOCK mode disabled");
            return;
        }

        if (!rest) {
            return;
        }

        s_mock_enabled = true;
        nextToken(rest, field, sizeof(field), '|');
        trimInPlace(field);

        if (strcasecmp(action, "STATE") == 0) {
            strncpy(s_mock_state, field, sizeof(s_mock_state) - 1);
            s_mock_state[sizeof(s_mock_state) - 1] = '\0';
            ESP_LOGI(TAG, "MOCK state=%s", s_mock_state);
            return;
        }

        if (strcasecmp(action, "POS") == 0) {
            s_mock_pos = (float)atof(field);
            s_mock_has_pos = true;
            ESP_LOGI(TAG, "MOCK pos=%.3f", (double)s_mock_pos);
            return;
        }

        if (strcasecmp(action, "TEMP") == 0) {
            s_mock_temp = (float)atof(field);
            s_mock_has_temp = true;
            ESP_LOGI(TAG, "MOCK temp=%.3f", (double)s_mock_temp);
            return;
        }

        ESP_LOGW(TAG, "Unknown MOCK action: %s", action);
        return;
    }

    ESP_LOGW(TAG, "Unknown RX message: %s", msg ? msg : "(null)");
}

static void txLine(const char *line)
{
    if (!line) {
        return;
    }
    ESP_LOGD(TAG, "TX: %s", line);
    if (s_tx_cb) {
        s_tx_cb(line, s_tx_ctx);
    }
}

void init(void)
{
    memset(&status, 0, sizeof(status));
    memset(&mould, 0, sizeof(mould));
    memset(&common, 0, sizeof(common));
    memset(&s_status_view, 0, sizeof(s_status_view));
    s_mock_enabled = false;
    s_mock_has_pos = false;
    s_mock_has_temp = false;
    s_mock_pos = 0.0f;
    s_mock_temp = 0.0f;
    s_mock_state[0] = '\0';
}

void update(void)
{
    // UART transport is intentionally deferred to next iteration.
}

void injectRxLine(const char *line)
{
    if (!line || !line[0]) {
        return;
    }

    char local[256] = {0};
    strncpy(local, line, sizeof(local) - 1);
    trimInPlace(local);
    if (!local[0]) {
        return;
    }

    ESP_LOGD(TAG, "RX: %s", local);
    parseMessage(local);
}

static void setLabelText(lv_obj_t *label, const char *text)
{
    if (!label || !text) {
        return;
    }
    lv_label_set_text(label, text);
}

static void setLabelFloat(lv_obj_t *label, float value, const char *suffix = nullptr)
{
    if (!label) {
        return;
    }
    char buf[32] = {0};
    if (suffix) {
        snprintf(buf, sizeof(buf), "%.2f%s", value, suffix);
    } else {
        snprintf(buf, sizeof(buf), "%.2f", value);
    }
    lv_label_set_text(label, buf);
}

void applyUiUpdates(void)
{
    const Status &uiStatus = effectiveStatus();

    eez::flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_PLUNGER_TIP_POSITION, FloatValue(turnsToCm3(uiStatus.encoderTurns)));

    plunger_stateValue plungerStateValue(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_PLUNGER_STATE));
    if (plungerStateValue) {
        plungerStateValue.temperature(uiStatus.tempC);
        plungerStateValue.current_barrel_capacity(turnsToCm3(uiStatus.encoderTurns));
    }

    const char *stateText = (uiStatus.state[0] != '\0') ? uiStatus.state : "--";
    setLabelText(objects.obj1__machine_state_text, stateText);
    setLabelText(objects.obj3__machine_state_text, stateText);
    setLabelText(objects.obj6__machine_state_text, stateText);

    setLabelText(objects.obj4__mould_name_value, mould.name);
    setLabelFloat(objects.obj4__mould_fill_speed_value, mould.fillSpeed);
    setLabelFloat(objects.obj4__mould_fill_dist_value, mould.fillVolume);
    setLabelFloat(objects.obj4__mould_fill_accel_value, mould.fillAccel);
    setLabelFloat(objects.obj4__mould_hold_speed_value, mould.packSpeed);
    setLabelFloat(objects.obj4__mould_hold_dist_value, mould.packVolume);
    setLabelFloat(objects.obj4__mould_hold_accel_value, mould.packAccel);
}

void setTxCallback(tx_callback_t cb, void *ctx)
{
    s_tx_cb = cb;
    s_tx_ctx = ctx;
}

void sendQueryMould(void) { txLine("QUERY_MOULD"); }
void sendQueryCommon(void) { txLine("QUERY_COMMON"); }
void sendQueryState(void) { txLine("QUERY_STATE"); }
void sendQueryError(void) { txLine("QUERY_ERROR"); }

bool sendMould(const MouldParams &params)
{
    if (!isSafeForUpdate()) {
        return false;
    }

    char message[320] = {0};
    snprintf(
        message,
        sizeof(message),
        "MOULD|%s|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%s|%.3f",
        params.name,
        params.fillVolume,
        params.fillSpeed,
        params.fillPressure,
        params.packVolume,
        params.packSpeed,
        params.packPressure,
        params.packTime,
        params.coolingTime,
        params.fillAccel,
        params.fillDecel,
        params.packAccel,
        params.packDecel,
        params.mode,
        params.injectTorque
    );
    txLine(message);
    return true;
}

bool sendCommon(const CommonParams &params)
{
    if (!isSafeForUpdate()) {
        return false;
    }

    char message[320] = {0};
    snprintf(
        message,
        sizeof(message),
        "COMMON|%.3f|%.3f|%lu|%lu|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%lu|%lu",
        params.trapAccel,
        params.compressTorque,
        (unsigned long)params.microIntervalMs,
        (unsigned long)params.microDurationMs,
        params.purgeUp,
        params.purgeDown,
        params.purgeCurrent,
        params.antidripVel,
        params.antidripCurrent,
        params.releaseDist,
        params.releaseTrapVel,
        params.releaseCurrent,
        (unsigned long)params.contactorCycles,
        (unsigned long)params.contactorLimit
    );
    txLine(message);
    return true;
}

const Status &getStatus(void) { return effectiveStatus(); }
const MouldParams &getMould(void) { return mould; }
const CommonParams &getCommon(void) { return common; }

static bool stateEquals(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }
    return strcasecmp(a, b) == 0;
}

bool isSafeForUpdate(void)
{
    return stateEquals(status.state, "INIT_HEATING") ||
           stateEquals(status.state, "INIT_HOT_WAIT") ||
           stateEquals(status.state, "REFILL") ||
           stateEquals(status.state, "READY_TO_INJECT") ||
           stateEquals(status.state, "PURGE_ZERO") ||
           stateEquals(status.state, "CONFIRM_REMOVAL");
}

} // namespace DisplayComms
