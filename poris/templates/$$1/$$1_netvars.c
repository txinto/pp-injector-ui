#include <string.h>
#include <$$1.h>
#include "$$1_netvars.h"

static netvars_nvs_mgr_t $$1_nvs_mgr = {0};

const NetVars_desc_t $$1_netvars_desc[] = {
#include "$$1_netvars_fragment.c_"
};

const size_t $$1_netvars_count = sizeof($$1_netvars_desc) / sizeof($$1_netvars_desc[0]);

void $$1_netvars_append_json(cJSON *root)
{
    if ($$1_netvars_count > 0)
    {
        NetVars_append_json_component("$$1", $$1_netvars_desc, $$1_netvars_count, root);
    }
}

bool $$1_netvars_parse_json_dict(cJSON *root)
{
    if ($$1_netvars_count > 0)
    {
        return NetVars_parse_json_dict($$1_netvars_desc, $$1_netvars_count, root);
    }
    else
    {
        return false;
    }
}

void $$1_netvars_nvs_load(void)
{
    if ($$1_netvars_count > 0)
    {
        NetVars_nvs_load_component("$$1", $$1_netvars_desc, $$1_netvars_count);
    }
}

void $$1_netvars_nvs_save(void)
{
    if ($$1_netvars_count > 0)
    {
        NetVars_nvs_save_component("$$1", $$1_netvars_desc, $$1_netvars_count);
    }
}

void $$1_config_parse_json(const char *data)
{
    if ($$1_netvars_count > 0)
    {
        bool nvs_cfg_changed = NetVars_parse_json_component_data("$$1", $$1_netvars_desc, $$1_netvars_count, data);
        if (nvs_cfg_changed)
        {
            $$1_nvs_set_dirty();
        }
    }
}

void $$1_nvs_set_dirty(void)
{
    NetVars_nvs_set_dirty(&$$1_nvs_mgr);
}

void $$1_nvs_spin(void)
{
    if (NetVars_nvs_spin(&$$1_nvs_mgr))
    {
        $$1_netvars_nvs_save();
    }
}
