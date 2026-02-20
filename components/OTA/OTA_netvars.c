#include <string.h>
#include <OTA.h>
#include "OTA_netvars.h"

static netvars_nvs_mgr_t OTA_nvs_mgr = {0};

const NetVars_desc_t OTA_netvars_desc[] = {
#include "OTA_netvars_fragment.c_"
};

const size_t OTA_netvars_count = sizeof(OTA_netvars_desc) / sizeof(OTA_netvars_desc[0]);

void OTA_netvars_append_json(cJSON *root)
{
    if (OTA_netvars_count > 0)
    {
        NetVars_append_json_component("OTA", OTA_netvars_desc, OTA_netvars_count, root);
    }
}

bool OTA_netvars_parse_json_dict(cJSON *root)
{
    if (OTA_netvars_count > 0)
    {
        return NetVars_parse_json_dict(OTA_netvars_desc, OTA_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void OTA_netvars_nvs_load(void)
{
    if (OTA_netvars_count > 0)
    {
        NetVars_nvs_load_component("OTA", OTA_netvars_desc, OTA_netvars_count);
    }
}

void OTA_netvars_nvs_save(void)
{
    if (OTA_netvars_count > 0)
    {
        NetVars_nvs_save_component("OTA", OTA_netvars_desc, OTA_netvars_count);
    }
}

void OTA_config_parse_json(const char *data)
{
    if (OTA_netvars_count > 0)
    {
        bool nvs_cfg_changed = NetVars_parse_json_component_data("OTA", OTA_netvars_desc, OTA_netvars_count, data);
        if (nvs_cfg_changed)
        {
            OTA_nvs_set_dirty();
        }
    }
}

void OTA_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&OTA_nvs_mgr);
}

void OTA_nvs_spin(void)
{
    if (NetVars_nvs_spin(&OTA_nvs_mgr))
    {
        OTA_netvars_nvs_save();
    }
}
