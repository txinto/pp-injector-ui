#pragma once
#ifndef PROVISIONING_NETVARS_H
#define PROVISIONING_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void Provisioning_netvars_nvs_load(void);
void Provisioning_netvars_nvs_save(void);

void Provisioning_netvars_append_json(cJSON *root);
bool Provisioning_netvars_parse_json_dict(cJSON *root);

void Provisioning_config_parse_json(const char *data);

void Provisioning_nvs_set_dirty(void);
void Provisioning_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // PROVISIONING_NETVARS_H
