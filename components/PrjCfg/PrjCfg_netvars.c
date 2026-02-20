#include <string.h>
#include <PrjCfg.h>
#include "PrjCfg_netvars.h"

static netvars_nvs_mgr_t PrjCfg_nvs_mgr = {0};

const NetVars_desc_t PrjCfg_netvars_desc[] = {
#include "PrjCfg_netvars_fragment.c_"
};

const size_t PrjCfg_netvars_count = sizeof(PrjCfg_netvars_desc) / sizeof(PrjCfg_netvars_desc[0]);

void PrjCfg_netvars_append_json(cJSON *root)
{
    if (PrjCfg_netvars_count > 0)
    {
        NetVars_append_json_component("PrjCfg", PrjCfg_netvars_desc, PrjCfg_netvars_count, root);
    }
}

bool PrjCfg_netvars_parse_json_dict(cJSON *root)
{
    if (PrjCfg_netvars_count > 0)
    {
        return NetVars_parse_json_dict(PrjCfg_netvars_desc, PrjCfg_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void PrjCfg_netvars_nvs_load(void)
{
    if (PrjCfg_netvars_count > 0)
    {    
        NetVars_nvs_load_component("PrjCfg", PrjCfg_netvars_desc, PrjCfg_netvars_count);
    }
}

void PrjCfg_netvars_nvs_save(void)
{
    if (PrjCfg_netvars_count > 0)
    {  
        NetVars_nvs_save_component("PrjCfg", PrjCfg_netvars_desc, PrjCfg_netvars_count);
    }
}

void PrjCfg_config_parse_json(const char *data)
{
    if (PrjCfg_netvars_count > 0)
    {    
        bool nvs_cfg_changed = NetVars_parse_json_component_data("PrjCfg", PrjCfg_netvars_desc, PrjCfg_netvars_count, data);
        if (nvs_cfg_changed)
        {
            PrjCfg_nvs_set_dirty();
        }
    }
}

void PrjCfg_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&PrjCfg_nvs_mgr);
}

void PrjCfg_nvs_spin(void)
{
    if (NetVars_nvs_spin(&PrjCfg_nvs_mgr))
    {
        PrjCfg_netvars_nvs_save();
    }
}
