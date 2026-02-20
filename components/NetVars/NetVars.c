
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cJSON.h"
#include <nvs.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/base64.h>

#include <NetVars.h>

static const char *TAG = "NetVars";

static void u8_buf_to_hex(const uint8_t *src, size_t len, char *dst, size_t dst_len);
static bool hex_to_u8_buf(const char *src, uint8_t *dst, size_t len, bool *out_changed);
static bool base64_encode_buf(const uint8_t *src, size_t len, char **out, size_t *out_len);
static bool base64_decode_buf(const char *src, uint8_t *dst, size_t len, bool *out_changed);
static void append_json_u8_u16_vec(const NetVars_desc_t *d, cJSON *root, size_t elem_size);
static void parse_json_u8_u16_vec(const NetVars_desc_t *d, NetVars_json_repr_t jm, cJSON *nvi,
                                  bool *out_nvs_changed, size_t elem_size);

static inline bool should_mark_nvs(const NetVars_desc_t *d)
{
    return d && d->nvs_key &&
           (d->nvs_mode == PRJCFG_NVS_SAVE || d->nvs_mode == PRJCFG_NVS_LOADSAVE);
}

static void uxx_to_hex(uint64_t val, size_t bytes, char *buf, size_t buf_len)
{
    size_t width = bytes * 2;
    if (!buf || buf_len < width + 1) return;
    (void)snprintf(buf, buf_len, "%0*" PRIX64, (int)width, (uint64_t)val);
}

static void scalar_to_hex_string(const void *ptr, size_t bytes, bool is_signed, char *buf, size_t buf_len)
{
    if (!ptr || bytes == 0 || bytes > sizeof(uint64_t)) return;
    uint64_t val = 0;
    if (is_signed) {
        int64_t tmp = 0;
        memcpy(&tmp, ptr, bytes);
        val = (uint64_t)tmp;
    } else {
        memcpy(&val, ptr, bytes);
    }
    uxx_to_hex(val, bytes, buf, buf_len);
}

static void scalar_to_invhex_string(const void *ptr, size_t bytes, char *buf, size_t buf_len)
{
    if (!ptr || bytes == 0 || bytes > sizeof(uint64_t)) return;
    if (!buf || buf_len < bytes * 2 + 1) return;
    uint8_t tmp[8] = {0};
    memcpy(tmp, ptr, bytes);
    for (size_t i = 0; i < bytes / 2; ++i) {
        uint8_t t = tmp[i];
        tmp[i] = tmp[bytes - 1 - i];
        tmp[bytes - 1 - i] = t;
    }
    uint64_t val = 0;
    memcpy(&val, tmp, bytes);
    uxx_to_hex(val, bytes, buf, buf_len); // uppercase, width = bytes*2
}

static bool hex_to_uint64_rev(const char *src, size_t bytes, bool reverse, uint64_t *out)
{
    if (!src || !out) return false;
    if (bytes == 0 || bytes > sizeof(uint64_t)) return false;
    uint8_t tmp[8] = {0};
    if (!hex_to_u8_buf(src, tmp, bytes, NULL)) return false;
    if (reverse) {
        for (size_t i = 0; i < bytes / 2; ++i) {
            uint8_t t = tmp[i];
            tmp[i] = tmp[bytes - 1 - i];
            tmp[bytes - 1 - i] = t;
        }
    }
    uint64_t val = 0;
    memcpy(&val, tmp, bytes);
    *out = val;
    return true;
}

static bool parse_json_scalar_int(const NetVars_desc_t *d, NetVars_json_repr_t jm, cJSON *nvi, bool *out_nvs_changed)
{
    if (!d || !nvi || !out_nvs_changed) return false;

    size_t bytes = 0;
    bool is_signed = false;
    switch (d->type) {
    case NETVARS_TYPE_U8:  bytes = sizeof(uint8_t);  is_signed = false; break;
    case NETVARS_TYPE_I8:  bytes = sizeof(int8_t);   is_signed = true;  break;
    case NETVARS_TYPE_U16: bytes = sizeof(uint16_t); is_signed = false; break;
    case NETVARS_TYPE_I16: bytes = sizeof(int16_t);  is_signed = true;  break;
    case NETVARS_TYPE_I32: bytes = sizeof(int32_t);  is_signed = true;  break;
    case NETVARS_TYPE_U32: bytes = sizeof(uint32_t); is_signed = false; break;
    default: return false;
    }

    NetVars_json_repr_t mode = jm;
    if (mode == NETVARS_JSON_REPR_AUTO) {
        mode = NETVARS_JSON_REPR_DEC;
    }

    uint64_t new_val_u = 0;
    bool have_val = false;

    if (mode == NETVARS_JSON_REPR_HEX || mode == NETVARS_JSON_REPR_INVHEX) {
        if (cJSON_IsString(nvi) && nvi->valuestring) {
            bool reverse = (mode == NETVARS_JSON_REPR_INVHEX);
            if (hex_to_uint64_rev(nvi->valuestring, bytes, reverse, &new_val_u)) {
                have_val = true;
            }
        }
    } else if (mode == NETVARS_JSON_REPR_BASE64 && d->type == NETVARS_TYPE_U8) {
        if (cJSON_IsString(nvi) && nvi->valuestring) {
            uint8_t tmp = 0;
            if (base64_decode_buf(nvi->valuestring, &tmp, 1, out_nvs_changed)) {
                new_val_u = tmp;
                have_val = true;
            }
        }
    }

    if (!have_val) {
        if (cJSON_IsNumber(nvi)) {
            new_val_u = (uint64_t)nvi->valueint;
            have_val = true;
        } else if (cJSON_IsString(nvi) && nvi->valuestring) {
            // Accept decimal strings as fallback
            new_val_u = strtoull(nvi->valuestring, NULL, 10);
            have_val = true;
        }
    }

    if (!have_val) return false;

    bool changed = false;
    switch (d->type) {
    case NETVARS_TYPE_U8: {
        uint8_t old = *(uint8_t *)d->ptr;
        uint8_t new_v = (uint8_t)new_val_u;
        if (new_v != old) {
            *(uint8_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    case NETVARS_TYPE_I8: {
        int8_t old = *(int8_t *)d->ptr;
        int8_t new_v = (int8_t)new_val_u;
        if (new_v != old) {
            *(int8_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    case NETVARS_TYPE_I16: {
        int32_t old = *(int16_t *)d->ptr;
        int32_t new_v = (int16_t)new_val_u;
        if (new_v != old) {
            *(int16_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    case NETVARS_TYPE_U16: {
        uint32_t old = *(uint16_t *)d->ptr;
        uint32_t new_v = (uint16_t)new_val_u;
        if (new_v != old) {
            *(uint16_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    case NETVARS_TYPE_I32: {
        int32_t old = *(int32_t *)d->ptr;
        int32_t new_v = (int32_t)new_val_u;
        if (new_v != old) {
            *(int32_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    case NETVARS_TYPE_U32: {
        uint32_t old = *(uint32_t *)d->ptr;
        uint32_t new_v = (uint32_t)new_val_u;
        if (new_v != old) {
            *(uint32_t *)d->ptr = new_v;
            changed = true;
        }
        break;
    }
    default:
        break;
    }

    if (changed && should_mark_nvs(d)) {
        *out_nvs_changed = true;
    }
    return true;
}

static void add_json_scalar_int(const NetVars_desc_t *d, NetVars_json_repr_t jm, cJSON *root)
{
    if (!d || !root || !d->json_key) return;
    NetVars_json_repr_t mode = jm;
    if (mode == NETVARS_JSON_REPR_AUTO) {
        mode = NETVARS_JSON_REPR_DEC;
    }

    size_t bytes = 0;
    bool is_signed = false;
    switch (d->type) {
    case NETVARS_TYPE_U8:  bytes = sizeof(uint8_t);  is_signed = false; break;
    case NETVARS_TYPE_I8:  bytes = sizeof(int8_t);   is_signed = true;  break;
    case NETVARS_TYPE_U16: bytes = sizeof(uint16_t); is_signed = false; break;
    case NETVARS_TYPE_I16: bytes = sizeof(int16_t);  is_signed = true;  break;
    case NETVARS_TYPE_I32: bytes = sizeof(int32_t);  is_signed = true;  break;
    case NETVARS_TYPE_U32: bytes = sizeof(uint32_t); is_signed = false; break;
    default: return;
    }

    if (mode == NETVARS_JSON_REPR_HEX || mode == NETVARS_JSON_REPR_INVHEX) {
        char buf[2 * sizeof(uint64_t) + 1] = {0};
        if (mode == NETVARS_JSON_REPR_INVHEX) {
            scalar_to_invhex_string(d->ptr, bytes, buf, sizeof(buf));
        } else {
            scalar_to_hex_string(d->ptr, bytes, is_signed, buf, sizeof(buf));
        }
        cJSON_AddStringToObject(root, d->json_key, buf);
        return;
    }

    switch (d->type) {
    case NETVARS_TYPE_U8:
        cJSON_AddNumberToObject(root, d->json_key, *(uint8_t *)d->ptr);
        break;
    case NETVARS_TYPE_I8:
        cJSON_AddNumberToObject(root, d->json_key, *(int8_t *)d->ptr);
        break;
    case NETVARS_TYPE_I16:
        cJSON_AddNumberToObject(root, d->json_key, *(int16_t *)d->ptr);
        break;
    case NETVARS_TYPE_U16:
        cJSON_AddNumberToObject(root, d->json_key, *(uint16_t *)d->ptr);
        break;
    case NETVARS_TYPE_I32:
        cJSON_AddNumberToObject(root, d->json_key, *(int32_t *)d->ptr);
        break;
    case NETVARS_TYPE_U32:
        cJSON_AddNumberToObject(root, d->json_key, *(uint32_t *)d->ptr);
        break;
    default:
        break;
    }
}

static inline bool set_u8_u16_elem(void *ptr, size_t idx, size_t elem_size, uint16_t value)
{
    if (!ptr) return false;
    if (elem_size == sizeof(uint8_t)) {
        uint8_t *p = (uint8_t *)ptr;
        uint8_t v = (uint8_t)value;
        if (p[idx] != v) {
            p[idx] = v;
            return true;
        }
        return false;
    }
    if (elem_size == sizeof(uint16_t)) {
        uint16_t *p = (uint16_t *)ptr;
        if (p[idx] != value) {
            p[idx] = value;
            return true;
        }
        return false;
    }
    return false;
}

static void append_json_u8_u16_vec(const NetVars_desc_t *d, cJSON *root, size_t elem_size)
{
    if (!d || !root || !d->json_key || (elem_size != sizeof(uint8_t) && elem_size != sizeof(uint16_t))) return;
    size_t len = d->len ? d->len : 1;
    NetVars_json_repr_t repr = (NetVars_json_repr_t)d->json_repr;
    if (repr == NETVARS_JSON_REPR_AUTO) {
        repr = (len > 1) ? NETVARS_JSON_REPR_HEX : NETVARS_JSON_REPR_DEC;
    }
    if (len > 1) {
        size_t byte_len = len * elem_size;
        switch (repr) {
        case NETVARS_JSON_REPR_ARRAY: {
            cJSON *arr = cJSON_CreateArray();
            if (arr) {
                for (size_t j = 0; j < len; ++j) {
                    uint32_t value = (elem_size == sizeof(uint8_t)) ?
                        ((uint8_t *)d->ptr)[j] : ((uint16_t *)d->ptr)[j];
                    cJSON_AddItemToArray(arr, cJSON_CreateNumber(value));
                }
                cJSON_AddItemToObject(root, d->json_key, arr);
            }
            break;
        }
        case NETVARS_JSON_REPR_HEX: {
            char *buf = (char *)malloc(2 * byte_len + 1);
            if (buf) {
                u8_buf_to_hex((const uint8_t *)d->ptr, byte_len, buf, 2 * byte_len + 1);
                cJSON_AddStringToObject(root, d->json_key, buf);
                free(buf);
            }
            break;
        }
        case NETVARS_JSON_REPR_BASE64: {
            char *out = NULL; size_t out_len = 0;
            if (base64_encode_buf((const uint8_t *)d->ptr, byte_len, &out, &out_len) && out) {
                cJSON_AddStringToObject(root, d->json_key, out);
                free(out);
            }
            break;
        }
        case NETVARS_JSON_REPR_DEC:
        default:
            cJSON_AddNumberToObject(root, d->json_key,
                                    (elem_size == sizeof(uint8_t)) ?
                                    *(uint8_t *)d->ptr : *(uint16_t *)d->ptr);
            break;
        }
    } else {
        add_json_scalar_int(d, repr, root);
    }
}

static void parse_json_u8_u16_vec(const NetVars_desc_t *d, NetVars_json_repr_t jm, cJSON *nvi,
                                  bool *out_nvs_changed, size_t elem_size)
{
    if (!d || !nvi || !out_nvs_changed || (elem_size != sizeof(uint8_t) && elem_size != sizeof(uint16_t))) return;
    size_t len = d->len ? d->len : 1;
    NetVars_json_repr_t repr = jm;
    if (repr == NETVARS_JSON_REPR_AUTO) {
        repr = (len > 1) ? NETVARS_JSON_REPR_HEX : NETVARS_JSON_REPR_DEC;
    }

    if (len > 1) {
        size_t byte_len = len * elem_size;
        if (repr == NETVARS_JSON_REPR_BASE64) {
            if (cJSON_IsString(nvi) && nvi->valuestring) {
                if (base64_decode_buf(nvi->valuestring, (uint8_t *)d->ptr, byte_len, out_nvs_changed)) {
                    // handled
                }
            }
        } else if (repr == NETVARS_JSON_REPR_HEX) {
            if (cJSON_IsString(nvi) && nvi->valuestring) {
                bool changed = false;
                if (hex_to_u8_buf(nvi->valuestring, (uint8_t *)d->ptr, byte_len, &changed)) {
                    if (changed && should_mark_nvs(d)) *out_nvs_changed = true;
                    // handled
                }
            } else if (cJSON_IsArray(nvi)) {
                size_t idx = 0;
                cJSON *el = NULL;
                cJSON_ArrayForEach(el, nvi) {
                    if (idx >= len) break;
                    uint16_t value = (uint16_t)el->valueint;
                    if (set_u8_u16_elem(d->ptr, idx, elem_size, value)) {
                        if (should_mark_nvs(d)) *out_nvs_changed = true;
                    }
                    ++idx;
                }
            }
        } else { // ARRAY or DEC default fallback uses array form
            if (cJSON_IsArray(nvi)) {
                size_t idx = 0;
                cJSON *el = NULL;
                cJSON_ArrayForEach(el, nvi) {
                    if (idx >= len) break;
                    uint16_t value = (uint16_t)el->valueint;
                    if (set_u8_u16_elem(d->ptr, idx, elem_size, value)) {
                        if (should_mark_nvs(d)) *out_nvs_changed = true;
                    }
                    ++idx;
                }
            } else if (cJSON_IsString(nvi) && nvi->valuestring) {
                // accept hex as fallback for array/dec
                bool changed = false;
                hex_to_u8_buf(nvi->valuestring, (uint8_t *)d->ptr, byte_len, &changed);
                if (changed && should_mark_nvs(d)) *out_nvs_changed = true;
            }
        }
    } else {
        parse_json_scalar_int(d, repr, nvi, out_nvs_changed);
    }
}

void NetVars_nvs_load(const NetVars_desc_t netvars_desc[], const size_t netvars_count, nvs_handle_t h)
{
    esp_err_t err;

    for (size_t i = 0; i < netvars_count; ++i) {
        const NetVars_desc_t *d = &netvars_desc[i];
        if (!d->nvs_key) continue;
        if (d->nvs_mode != PRJCFG_NVS_LOAD &&
            d->nvs_mode != PRJCFG_NVS_LOADSAVE) continue;

        switch (d->type) {
        case NETVARS_TYPE_BOOL: {
            uint8_t v;
            err = nvs_get_u8(h, d->nvs_key, &v);
            if (err == ESP_OK) *(bool *)d->ptr = (bool)v;
            break;
        }
        case NETVARS_TYPE_U8: {
            size_t len = d->len ? d->len : 1;
            if (len > 1) {
                size_t out_len = len;
                err = nvs_get_blob(h, d->nvs_key, d->ptr, &out_len);
                if (err == ESP_OK && out_len == len) {
                    // copied blob
                }
            } else {
                uint8_t v;
                err = nvs_get_u8(h, d->nvs_key, &v);
                if (err == ESP_OK) *(uint8_t *)d->ptr = v;
            }
            break;
        }
        case NETVARS_TYPE_I8: {
            int8_t v;
            err = nvs_get_i8(h, d->nvs_key, &v);
            if (err == ESP_OK) *(int8_t *)d->ptr = v;
            break;
        }
        case NETVARS_TYPE_I16: {
            int16_t v;
            err = nvs_get_i16(h, d->nvs_key, &v);
            if (err == ESP_OK) *(int16_t *)d->ptr = v;
            break;
        }
        case NETVARS_TYPE_U16: {
            uint16_t v;
            err = nvs_get_u16(h, d->nvs_key, &v);
            if (err == ESP_OK) *(uint16_t *)d->ptr = v;
            break;
        }
        case NETVARS_TYPE_I32: {
            int32_t v;
            err = nvs_get_i32(h, d->nvs_key, &v);
            if (err == ESP_OK) *(int32_t *)d->ptr = v;
            break;
        }
        case NETVARS_TYPE_U32: {
            uint32_t v;
            err = nvs_get_u32(h, d->nvs_key, &v);
            if (err == ESP_OK) *(uint32_t *)d->ptr = v;
            break;
        }
        case NETVARS_TYPE_FLOAT:
        case NETVARS_TYPE_FLOATINT: {
            size_t len = sizeof(float);
            err = nvs_get_blob(h, d->nvs_key, d->ptr, &len);
            if (err == ESP_OK && len == sizeof(float)) {
                // stored directly into ptr
            }
            break;
        }
        case NETVARS_TYPE_STRING:
            break;
        }
    }
}

void NetVars_nvs_save(const NetVars_desc_t netvars_desc[], const size_t netvars_count, nvs_handle_t h)
{
    esp_err_t err;

    for (size_t i = 0; i < netvars_count; ++i) {
        const NetVars_desc_t *d = &netvars_desc[i];
        if (!d->nvs_key) continue;
        if (d->nvs_mode != PRJCFG_NVS_SAVE &&
            d->nvs_mode != PRJCFG_NVS_LOADSAVE) continue;

        switch (d->type) {
        case NETVARS_TYPE_BOOL:
            err = nvs_set_u8(h, d->nvs_key, *(bool *)d->ptr ? 1 : 0);
            break;
        case NETVARS_TYPE_U8: {
            size_t len = d->len ? d->len : 1;
            if (len > 1) {
                err = nvs_set_blob(h, d->nvs_key, d->ptr, len);
            } else {
                err = nvs_set_u8(h, d->nvs_key, *(uint8_t *)d->ptr);
            }
            break;
        }
        case NETVARS_TYPE_I8:
            err = nvs_set_i8(h, d->nvs_key, *(int8_t *)d->ptr);
            break;
        case NETVARS_TYPE_I16:
            err = nvs_set_i16(h, d->nvs_key, *(int16_t *)d->ptr);
            break;
        case NETVARS_TYPE_U16:
            err = nvs_set_u16(h, d->nvs_key, *(uint16_t *)d->ptr);
            break;
        case NETVARS_TYPE_I32:
            err = nvs_set_i32(h, d->nvs_key, *(int32_t *)d->ptr);
            break;
        case NETVARS_TYPE_U32:
            err = nvs_set_u32(h, d->nvs_key, *(uint32_t *)d->ptr);
            break;
        case NETVARS_TYPE_FLOAT:
        case NETVARS_TYPE_FLOATINT:
            err = nvs_set_blob(h, d->nvs_key, d->ptr, sizeof(float));
            break;
        case NETVARS_TYPE_STRING:
            err = ESP_OK;
            break;
        }

        if (err != ESP_OK) {
            // opcional: logs
        }
    }

    (void)nvs_commit(h);
}

void NetVars_append_json(const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root)
{
    for (size_t i = 0; i < netvars_count; ++i) {
        const NetVars_desc_t *d = &netvars_desc[i];
        if (!d->json || !d->json_key) continue;
        NetVars_json_mode_t mode_dir = (NetVars_json_mode_t)d->json_mode;
        if (mode_dir == NETVARS_JSON_MODE_NONE || mode_dir == NETVARS_JSON_MODE_IN) continue;
        NetVars_json_repr_t jm = (NetVars_json_repr_t)d->json_repr;

        switch (d->type) {
        case NETVARS_TYPE_BOOL:
            cJSON_AddBoolToObject(root, d->json_key, *(bool *)d->ptr);
            break;
        case NETVARS_TYPE_U8:
            append_json_u8_u16_vec(d, root, sizeof(uint8_t));
            break;
        case NETVARS_TYPE_I8:
        case NETVARS_TYPE_I16:
        case NETVARS_TYPE_I32:
        case NETVARS_TYPE_U32:
            add_json_scalar_int(d, jm, root);
            break;
        case NETVARS_TYPE_U16:
            append_json_u8_u16_vec(d, root, sizeof(uint16_t));
            break;
        case NETVARS_TYPE_FLOAT:
            cJSON_AddNumberToObject(root, d->json_key, *(float *)d->ptr);
            break;
        case NETVARS_TYPE_FLOATINT: {
            int32_t scale = d->scale > 0 ? d->scale : 1;
            float v = *(float *)d->ptr;
            int64_t scaled = llroundf(v * (float)scale);
            cJSON_AddNumberToObject(root, d->json_key, scaled);
            break;
        }
        case NETVARS_TYPE_STRING:
            cJSON_AddStringToObject(root, d->json_key, (const char *)d->ptr);
            break;
        default:
            break;
        }
    }
}

static const NetVars_desc_t *
NetVars_find_by_json_key(const NetVars_desc_t netvars_desc[], const size_t netvars_count, const char *key)
{
    for (size_t i = 0; i < netvars_count; ++i) {
        const NetVars_desc_t *d = &netvars_desc[i];
        if (d->json_key && strcmp(d->json_key, key) == 0)
            return d;
    }
    return NULL;
}

bool NetVars_parse_json_dict(const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root)
{
    bool out_nvs_changed = false;

    cJSON *nvi = NULL;
    cJSON_ArrayForEach(nvi, root) {
        const NetVars_desc_t *d =
            NetVars_find_by_json_key(netvars_desc, netvars_count, nvi->string);
        if (!d) continue;
        NetVars_json_mode_t mode_dir = (NetVars_json_mode_t)d->json_mode;
        if (mode_dir == NETVARS_JSON_MODE_NONE || mode_dir == NETVARS_JSON_MODE_OUT) continue;
        NetVars_json_repr_t jm = (NetVars_json_repr_t)d->json_repr;

        switch (d->type) {
        case NETVARS_TYPE_BOOL: {
            bool value = cJSON_IsTrue(nvi);
            if (value != *(bool *)d->ptr) {
                *(bool *)d->ptr = value;
                if (should_mark_nvs(d)) out_nvs_changed = true;
            }
            break;
        }
        case NETVARS_TYPE_U8:
            parse_json_u8_u16_vec(d, jm, nvi, &out_nvs_changed, sizeof(uint8_t));
            break;
        case NETVARS_TYPE_I8:
        case NETVARS_TYPE_I16:
        case NETVARS_TYPE_I32: {
            parse_json_scalar_int(d, jm, nvi, &out_nvs_changed);
            break;
        }
        case NETVARS_TYPE_U16:
            parse_json_u8_u16_vec(d, jm, nvi, &out_nvs_changed, sizeof(uint16_t));
            break;
        case NETVARS_TYPE_U32:
            parse_json_scalar_int(d, jm, nvi, &out_nvs_changed);
            break;
        case NETVARS_TYPE_FLOAT: {
            float value = (float)nvi->valuedouble;
            if (value != *(float *)d->ptr) {
                *(float *)d->ptr = value;
                if (should_mark_nvs(d)) out_nvs_changed = true;
            }
            break;
        }
        case NETVARS_TYPE_FLOATINT: {
            if (cJSON_IsNumber(nvi)) {
                int64_t scaled = (int64_t)llround(nvi->valuedouble);
                int32_t scale = d->scale > 0 ? d->scale : 1;
                float value = (float)scaled / (float)scale;
                if (value != *(float *)d->ptr) {
                    *(float *)d->ptr = value;
                    if (should_mark_nvs(d)) out_nvs_changed = true;
                }
            }
            break;
        }
        case NETVARS_TYPE_STRING:
            if (cJSON_IsString(nvi) && nvi->valuestring) {
                size_t max_len = d->len ? d->len : strlen(nvi->valuestring) + 1;
                char *dst = (char *)d->ptr;
                if (!dst) break;
                size_t prev_len = strnlen(dst, max_len);
                strlcpy(dst, nvi->valuestring, max_len);
                size_t new_len = strnlen(dst, max_len);
                if (new_len != prev_len || strncmp(dst, nvi->valuestring, max_len) != 0) {
                    if (should_mark_nvs(d)) out_nvs_changed = true;
                }
            }
            break;
        }
    }
    return out_nvs_changed;
}

void NetVars_nvs_load_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count)
{
    if (!ident) return;
    nvs_handle_t h;
    esp_err_t err = nvs_open(ident, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_open(%s) failed: %s", ident, esp_err_to_name(err));
        return;
    }
    NetVars_nvs_load(netvars_desc, netvars_count, h);
    nvs_close(h);
}

void NetVars_nvs_save_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count)
{
    if (!ident) return;
    nvs_handle_t h;
    ESP_LOGW(TAG, "Saving component %s", ident);
    esp_err_t err = nvs_open(ident, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_open(%s) failed: %s", ident, esp_err_to_name(err));
        return;
    }
    NetVars_nvs_save(netvars_desc, netvars_count, h);
    nvs_close(h);
}

void NetVars_append_json_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root)
{
    if (!ident || !root) return;
    if (netvars_count == 0) return;

    cJSON *sub = cJSON_GetObjectItemCaseSensitive(root, ident);
    if (!sub)
    {
        sub = cJSON_AddObjectToObject(root, ident);
    }
    if (sub)
    {
        NetVars_append_json(netvars_desc, netvars_count, sub);
    }
}

bool NetVars_parse_json_component(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, cJSON *root)
{
    if (!ident || !root) return false;
    if (netvars_count == 0) return false;

    bool changed = false;

    if (cJSON_IsArray(root))
    {
        cJSON *elem = NULL;
        cJSON_ArrayForEach(elem, root)
        {
            cJSON *sub = cJSON_GetObjectItemCaseSensitive(elem, ident);
            if (sub)
            {
                if (NetVars_parse_json_dict(netvars_desc, netvars_count, sub))
                    changed = true;
            }
        }
    }
    else
    {
        cJSON *sub = cJSON_GetObjectItemCaseSensitive(root, ident);
        if (sub)
        {
            changed = NetVars_parse_json_dict(netvars_desc, netvars_count, sub);
        }
    }

    return changed;
}

bool NetVars_parse_json_component_data(const char *ident, const NetVars_desc_t netvars_desc[], const size_t netvars_count, const char *data)
{
    if (!data) return false;
    cJSON *root = cJSON_Parse(data);
    if (!root) return false;
    bool changed = NetVars_parse_json_component(ident, netvars_desc, netvars_count, root);
    cJSON_Delete(root);
    return changed;
}

static inline BaseType_t _create_nvs_mutex_once(netvars_nvs_mgr_t *mngr)
{
    if (!mngr) return pdFAIL;
    if (!mngr->mutex)
    {
        mngr->mutex = xSemaphoreCreateMutex();
        if (!mngr->mutex) return pdFAIL;
    }
    return pdPASS;
}

void NetVars_nvs_set_dirty(netvars_nvs_mgr_t *mngr)
{
    if (_create_nvs_mutex_once(mngr) != pdPASS) return;
    xSemaphoreTake(mngr->mutex, portMAX_DELAY);
    mngr->dirty = true;
        mngr->dirty_since = xTaskGetTickCount();
        xSemaphoreGive(mngr->mutex);
}

static void u8_buf_to_hex(const uint8_t *src, size_t len, char *dst, size_t dst_len)
{
    static const char hex[] = "0123456789abcdef";
    size_t needed = len * 2 + 1;
    if (!src || !dst || dst_len < needed) return;
    for (size_t i = 0; i < len; ++i) {
        dst[2 * i]     = hex[(src[i] >> 4) & 0xF];
        dst[2 * i + 1] = hex[src[i] & 0xF];
    }
    dst[needed - 1] = '\0';
}

static bool hex_to_u8_buf(const char *src, uint8_t *dst, size_t len, bool *out_changed)
{
    if (!src || !dst) return false;
    size_t slen = strlen(src);
    if (slen != len * 2) return false;
    bool changed = false;
    for (size_t idx = 0; idx < len; ++idx) {
        char hi = src[2 * idx];
        char lo = src[2 * idx + 1];
        int hv = (hi >= '0' && hi <= '9') ? (hi - '0') :
                 (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                 (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : -1;
        int lv = (lo >= '0' && lo <= '9') ? (lo - '0') :
                 (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                 (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : -1;
        if (hv < 0 || lv < 0) return false;
        uint8_t value = (uint8_t)((hv << 4) | lv);
        if (value != dst[idx]) {
            dst[idx] = value;
            changed = true;
        }
    }
    if (out_changed && changed) *out_changed = true;
    return true;
}

static bool base64_encode_buf(const uint8_t *src, size_t len, char **out, size_t *out_len)
{
    if (!src || !out) return false;
    size_t needed = 4 * ((len + 2) / 3) + 1;
    char *buf = (char *)malloc(needed);
    if (!buf) return false;
    size_t olen = 0;
    int rc = mbedtls_base64_encode((unsigned char *)buf, needed, &olen, src, len);
    if (rc != 0) {
        free(buf);
        return false;
    }
    buf[olen] = '\0';
    *out = buf;
    if (out_len) *out_len = olen;
    return true;
}

static bool base64_decode_buf(const char *src, uint8_t *dst, size_t len, bool *out_changed)
{
    if (!src || !dst) return false;
    size_t slen = strlen(src);
    size_t olen = 0;
    // decode to temp
    uint8_t *tmp = (uint8_t *)malloc(len);
    if (!tmp) return false;
    int rc = mbedtls_base64_decode(tmp, len, &olen, (const unsigned char *)src, slen);
    if (rc != 0 || olen != len) {
        free(tmp);
        return false;
    }
    bool changed = false;
    for (size_t i = 0; i < len; ++i) {
        if (dst[i] != tmp[i]) {
            dst[i] = tmp[i];
            changed = true;
        }
    }
    free(tmp);
    if (out_changed && changed) *out_changed = true;
    return true;
}

bool NetVars_nvs_spin(netvars_nvs_mgr_t *mngr)
{
    if (_create_nvs_mutex_once(mngr) != pdPASS) return false;
    TickType_t now_ticks = xTaskGetTickCount();
    bool should_save = false;

    xSemaphoreTake(mngr->mutex, portMAX_DELAY);
    if (mngr->dirty &&
        (TickType_t)(now_ticks - mngr->dirty_since) >= pdMS_TO_TICKS(5000))
    {
        mngr->dirty = false;
        should_save = true;
    }
    xSemaphoreGive(mngr->mutex);

    return should_save;
}
