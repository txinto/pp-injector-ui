#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#ifdef __cplusplus
}

#include <stdint.h>

namespace DisplayComms {

struct MouldParams {
    char name[32];
    float fillVolume;
    float fillSpeed;
    float fillPressure;
    float packVolume;
    float packSpeed;
    float packPressure;
    float packTime;
    float coolingTime;
    float fillAccel;
    float fillDecel;
    float packAccel;
    float packDecel;
    char mode[3];
    float injectTorque;
};

struct CommonParams {
    float trapAccel;
    float compressTorque;
    uint32_t microIntervalMs;
    uint32_t microDurationMs;
    float purgeUp;
    float purgeDown;
    float purgeCurrent;
    float antidripVel;
    float antidripCurrent;
    float releaseDist;
    float releaseTrapVel;
    float releaseCurrent;
    uint32_t contactorCycles;
    uint32_t contactorLimit;
};

struct Status {
    float encoderTurns;
    float tempC;
    char state[24];
    uint16_t errorCode;
    char errorMsg[64];
};

typedef void (*tx_callback_t)(const char *line, void *ctx);

void init(void);
void update(void);
void injectRxLine(const char *line);
void applyUiUpdates(void);

void setTxCallback(tx_callback_t cb, void *ctx);

void sendQueryMould(void);
void sendQueryCommon(void);
void sendQueryState(void);
void sendQueryError(void);
bool sendMould(const MouldParams &params);
bool sendCommon(const CommonParams &params);

const Status &getStatus(void);
const MouldParams &getMould(void);
const CommonParams &getCommon(void);
bool isSafeForUpdate(void);

} // namespace DisplayComms

#endif
