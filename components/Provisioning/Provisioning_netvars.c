#include <string.h>
#include <Provisioning.h>
#include "Provisioning_netvars.h"

static netvars_nvs_mgr_t Provisioning_nvs_mgr = {0};

const NetVars_desc_t Provisioning_netvars_desc[] = {
#include "Provisioning_netvars_fragment.c_"
};

const size_t Provisioning_netvars_count = sizeof(Provisioning_netvars_desc) / sizeof(Provisioning_netvars_desc[0]);

void Provisioning_netvars_append_json(cJSON *root)
{
    if (Provisioning_netvars_count > 0)
    {
        NetVars_append_json_component("Provisioning", Provisioning_netvars_desc, Provisioning_netvars_count, root);
    }
}

bool Provisioning_netvars_parse_json_dict(cJSON *root)
{
    if (Provisioning_netvars_count > 0)
    {
        return NetVars_parse_json_dict(Provisioning_netvars_desc, Provisioning_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void Provisioning_netvars_nvs_load(void)
{
    if (Provisioning_netvars_count > 0)
    {
        NetVars_nvs_load_component("Provisioning", Provisioning_netvars_desc, Provisioning_netvars_count);
    }
}

void Provisioning_netvars_nvs_save(void)
{
    if (Provisioning_netvars_count > 0)
    {
        NetVars_nvs_save_component("Provisioning", Provisioning_netvars_desc, Provisioning_netvars_count);
    }
}

void Provisioning_config_parse_json(const char *data)
{
    if (Provisioning_netvars_count > 0)
    {
        bool nvs_cfg_changed = NetVars_parse_json_component_data("Provisioning", Provisioning_netvars_desc, Provisioning_netvars_count, data);
        if (nvs_cfg_changed)
        {
            Provisioning_nvs_set_dirty();
        }
    }
}

void Provisioning_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&Provisioning_nvs_mgr);
}

void Provisioning_nvs_spin(void)
{
    if (NetVars_nvs_spin(&Provisioning_nvs_mgr))
    {
        Provisioning_netvars_nvs_save();
    }
}
