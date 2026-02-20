#pragma once
#ifndef TOUCHSCREEN_NETVARS_H
#define TOUCHSCREEN_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void TouchScreen_netvars_nvs_load(void);
void TouchScreen_netvars_nvs_save(void);

void TouchScreen_netvars_append_json(cJSON *root);
bool TouchScreen_netvars_parse_json_dict(cJSON *root);

void TouchScreen_config_parse_json(const char *data);
void TouchScreen_nvs_set_dirty(void);
void TouchScreen_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // TOUCHSCREEN_NETVARS_H
