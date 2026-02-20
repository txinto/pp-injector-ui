#pragma once
#ifndef PRJCFG_NETVARS_H
#define PRJCFG_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void PrjCfg_netvars_nvs_load(void);
void PrjCfg_netvars_nvs_save(void);

void PrjCfg_netvars_append_json(cJSON *root);
bool PrjCfg_netvars_parse_json_dict(cJSON *root);

void PrjCfg_config_parse_json(const char *data);

void PrjCfg_nvs_set_dirty(void);
void PrjCfg_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // PRJCFG_NETVARS_H
