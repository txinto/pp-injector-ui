#include "ui.h"
#include "structs.h"
#include "vars.h"
#include "eez-flow.h"
#include "PPInjectorUI_display_comms.h"
#include "PPInjectorUI_prd_ui.h"
#include <sdkconfig.h>

extern const float BARREL_CAPACITY_MM;

extern "C" void PPInjectorUI_ui_init_bridge(void)
{
    ui_init();
    DisplayComms::init();

    plunger_stateValue plunger_state(eez::flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_PLUNGER_STATE));
    if (plunger_state) {
        plunger_state.max_barrel_capacity(BARREL_CAPACITY_MM);
    }

#if CONFIG_PPINJECTORUI_ENABLE_PRD_UI
    PrdUi::init();
#endif
}

extern "C" void PPInjectorUI_ui_tick_bridge(void)
{
    ui_tick();
    DisplayComms::update();
    DisplayComms::applyUiUpdates();
#if CONFIG_PPINJECTORUI_ENABLE_PRD_UI
    PrdUi::tick();
#endif
}

extern "C" void PPInjectorUI_comms_inject_line_bridge(const char *line)
{
    DisplayComms::injectRxLine(line);
}

extern "C" void PPInjectorUI_comms_set_tx_callback_bridge(void (*cb)(const char *, void *), void *ctx)
{
    DisplayComms::setTxCallback(cb, ctx);
}

extern "C" void PPInjectorUI_storage_selftest_bridge(void)
{
    PrdUi::storageSelfTest();
}

extern "C" void PPInjectorUI_storage_read_dump_bridge(void)
{
    PrdUi::storageReadDump();
}
