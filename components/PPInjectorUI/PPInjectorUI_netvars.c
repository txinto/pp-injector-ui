#include <string.h>
#include <PPInjectorUI.h>
#include "PPInjectorUI_netvars.h"

static netvars_nvs_mgr_t PPInjectorUI_nvs_mgr = {0};

const NetVars_desc_t PPInjectorUI_netvars_desc[] = {
#include "PPInjectorUI_netvars_fragment.c_"
};

const size_t PPInjectorUI_netvars_count = sizeof(PPInjectorUI_netvars_desc) / sizeof(PPInjectorUI_netvars_desc[0]);

void PPInjectorUI_netvars_append_json(cJSON *root)
{
    if (PPInjectorUI_netvars_count > 0)
    {
        NetVars_append_json_component("PPInjectorUI", PPInjectorUI_netvars_desc, PPInjectorUI_netvars_count, root);
    }
}

bool PPInjectorUI_netvars_parse_json_dict(cJSON *root)
{
    if (PPInjectorUI_netvars_count > 0)
    {
        return NetVars_parse_json_dict(PPInjectorUI_netvars_desc, PPInjectorUI_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void PPInjectorUI_netvars_nvs_load(void)
{
    if (PPInjectorUI_netvars_count > 0)
    {
        NetVars_nvs_load_component("PPInjectorUI", PPInjectorUI_netvars_desc, PPInjectorUI_netvars_count);
    }
}

void PPInjectorUI_netvars_nvs_save(void)
{
    if (PPInjectorUI_netvars_count > 0)
    {
        NetVars_nvs_save_component("PPInjectorUI", PPInjectorUI_netvars_desc, PPInjectorUI_netvars_count);
    }
}

void PPInjectorUI_config_parse_json(const char *data)
{
    if (PPInjectorUI_netvars_count > 0)
    {
        bool nvs_cfg_changed = NetVars_parse_json_component_data("PPInjectorUI", PPInjectorUI_netvars_desc, PPInjectorUI_netvars_count, data);
        if (nvs_cfg_changed)
        {
            PPInjectorUI_nvs_set_dirty();
        }
    }
}

void PPInjectorUI_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&PPInjectorUI_nvs_mgr);
}

void PPInjectorUI_nvs_spin(void)
{
    if (NetVars_nvs_spin(&PPInjectorUI_nvs_mgr))
    {
        PPInjectorUI_netvars_nvs_save();
    }
}
