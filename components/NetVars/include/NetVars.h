#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

// ------------------ BEGIN Return code ------------------
typedef enum {
    NetVars_ret_error = -1,
    NetVars_ret_ok    = 0
} NetVars_return_code_t;
// ------------------ END   Return code ------------------

// ------------------ BEGIN Datatypes ------------------
typedef enum {
    NETVARS_TYPE_BOOL,
    NETVARS_TYPE_U8,
    NETVARS_TYPE_I8,
    NETVARS_TYPE_U16,
    NETVARS_TYPE_I16,
    NETVARS_TYPE_I32,
    NETVARS_TYPE_U32,
    NETVARS_TYPE_FLOAT,
    NETVARS_TYPE_FLOATINT,
    NETVARS_TYPE_STRING,
} NetVars_type_t;

typedef enum {
    PRJCFG_NVS_NONE,
    PRJCFG_NVS_LOAD,
    PRJCFG_NVS_SAVE,
    PRJCFG_NVS_LOADSAVE,
} NetVars_nvs_mode_t;

typedef enum {
    NETVARS_JSON_MODE_NONE = 0,
    NETVARS_JSON_MODE_OUT,    // append only
    NETVARS_JSON_MODE_IN,     // parse only
    NETVARS_JSON_MODE_INOUT,  // append + parse
} NetVars_json_mode_t;

typedef enum {
    NETVARS_JSON_REPR_AUTO = 0,
    NETVARS_JSON_REPR_ARRAY,
    NETVARS_JSON_REPR_HEX,
    NETVARS_JSON_REPR_INVHEX,
    NETVARS_JSON_REPR_DEC,
    NETVARS_JSON_REPR_BASE64,
} NetVars_json_repr_t;

typedef struct {
    const char           *name;
    const char           *nvs_key;
    const char           *json_key;
    const char           *group;
    const char           *module;
    NetVars_type_t        type;
    NetVars_nvs_mode_t    nvs_mode;
    bool                  json;      // se expone en JSON
    uint8_t               json_mode; // NetVars_json_mode_t
    uint8_t               json_repr; // NetVars_json_repr_t
    size_t                len;       // longitud para arrays; 0 o 1 para escalares
    void                 *ptr;       // &prjcfg.campo o prjcfg.array
    int32_t               scale;     // scale for NETVARS_TYPE_FLOATINT (>=1)
} NetVars_desc_t;

// ------------------ END   Datatypes ------------------

typedef struct {
    SemaphoreHandle_t mutex;
    bool dirty;
    TickType_t dirty_since;
} netvars_nvs_mgr_t;

void NetVars_nvs_load(const NetVars_desc_t netvars_desc[], const size_t netvars_count, nvs_handle_t h);
void NetVars_nvs_save(const NetVars_desc_t netvars_desc[], const size_t netvars_count, nvs_handle_t h);
void NetVars_append_json(const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root);
bool NetVars_parse_json_dict(const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root);

void NetVars_nvs_load_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count);
void NetVars_nvs_save_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count);
void NetVars_append_json_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root);
bool NetVars_parse_json_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root);
bool NetVars_parse_json_component_data(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, const char *data);

void NetVars_nvs_set_dirty(netvars_nvs_mgr_t *mngr);
bool NetVars_nvs_spin(netvars_nvs_mgr_t *mngr);

#ifdef __cplusplus
}
#endif
