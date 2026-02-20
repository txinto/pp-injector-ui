#pragma once
#ifndef $#1_NETVARS_H
#define $#1_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void $$1_netvars_nvs_load(void);
void $$1_netvars_nvs_save(void);

void $$1_netvars_append_json(cJSON *root);
bool $$1_netvars_parse_json_dict(cJSON *root);

void $$1_config_parse_json(const char *data);
void $$1_nvs_set_dirty(void);
void $$1_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // $#1_NETVARS_H
