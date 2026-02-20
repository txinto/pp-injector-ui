#pragma once
#ifndef OTA_NETVARS_H
#define OTA_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void OTA_netvars_nvs_load(void);
void OTA_netvars_nvs_save(void);

void OTA_netvars_append_json(cJSON *root);
bool OTA_netvars_parse_json_dict(cJSON *root);

void OTA_config_parse_json(const char *data);

void OTA_nvs_set_dirty(void);
void OTA_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_NETVARS_H
