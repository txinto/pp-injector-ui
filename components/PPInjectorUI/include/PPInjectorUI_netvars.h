#pragma once
#ifndef PPINJECTORUI_NETVARS_H
#define PPINJECTORUI_NETVARS_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <NetVars.h>

void PPInjectorUI_netvars_nvs_load(void);
void PPInjectorUI_netvars_nvs_save(void);

void PPInjectorUI_netvars_append_json(cJSON *root);
bool PPInjectorUI_netvars_parse_json_dict(cJSON *root);

void PPInjectorUI_config_parse_json(const char *data);
void PPInjectorUI_nvs_set_dirty(void);
void PPInjectorUI_nvs_spin(void);

#ifdef __cplusplus
}
#endif

#endif // PPINJECTORUI_NETVARS_H
