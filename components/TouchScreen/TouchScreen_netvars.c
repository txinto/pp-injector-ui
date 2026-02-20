#include <string.h>
#include <TouchScreen.h>
#include "TouchScreen_netvars.h"

static netvars_nvs_mgr_t TouchScreen_nvs_mgr = {0};

const NetVars_desc_t TouchScreen_netvars_desc[] = {
#include "TouchScreen_netvars_fragment.c_"
};

const size_t TouchScreen_netvars_count = sizeof(TouchScreen_netvars_desc) / sizeof(TouchScreen_netvars_desc[0]);

void TouchScreen_netvars_append_json(cJSON *root)
{
    if (TouchScreen_netvars_count > 0)
    {
        NetVars_append_json_component("TouchScreen", TouchScreen_netvars_desc, TouchScreen_netvars_count, root);
    }
}

bool TouchScreen_netvars_parse_json_dict(cJSON *root)
{
    if (TouchScreen_netvars_count > 0)
    {
        return NetVars_parse_json_dict(TouchScreen_netvars_desc, TouchScreen_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void TouchScreen_netvars_nvs_load(void)
{
    if (TouchScreen_netvars_count > 0)
    {
        NetVars_nvs_load_component("TouchScreen", TouchScreen_netvars_desc, TouchScreen_netvars_count);
    }
}

void TouchScreen_netvars_nvs_save(void)
{
    if (TouchScreen_netvars_count > 0)
    {
        NetVars_nvs_save_component("TouchScreen", TouchScreen_netvars_desc, TouchScreen_netvars_count);
    }
}

void TouchScreen_config_parse_json(const char *data)
{
    if (TouchScreen_netvars_count > 0)
    {
        bool nvs_cfg_changed = NetVars_parse_json_component_data("TouchScreen", TouchScreen_netvars_desc, TouchScreen_netvars_count, data);
        if (nvs_cfg_changed)
        {
            TouchScreen_nvs_set_dirty();
        }
    }
}

void TouchScreen_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&TouchScreen_nvs_mgr);
}

void TouchScreen_nvs_spin(void)
{
    if (NetVars_nvs_spin(&TouchScreen_nvs_mgr))
    {
        TouchScreen_netvars_nvs_save();
    }
}
