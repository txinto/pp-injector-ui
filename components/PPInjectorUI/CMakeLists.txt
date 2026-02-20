# CMakeLists.txt template for PPInjectorUI

# Señales de activación:
# - CMake var: PORIS_ENABLE_PPINJECTORUI (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_PPINJECTORUI (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_PPINJECTORUI OR DEFINED ENV{PORIS_ENABLE_PPINJECTORUI}))
  message(STATUS "[ PPInjectorUI ] disabled (no PORIS_ENABLE_PPINJECTORUI)")
  return()
endif()

idf_component_register(
  SRCS
    "PPInjectorUI_netvars.c"
    "PPInjectorUI.c"
    "PPInjectorUI_ui_bridge.cpp"
    "PPInjectorUI_display_comms.cpp"
    "PPInjectorUI_prd_ui.cpp"
    "ui/actions_impl.cpp"
    "ui/eez-flow.cpp"
    "ui/images.c"
    "ui/screens.c"
    "ui/styles.c"
    "ui/ui.c"
  INCLUDE_DIRS "include" "ui"
  PRIV_REQUIRES PrjCfg NetVars json nvs_flash TouchScreen esp_timer esp_driver_uart spiffs
)

if(CONFIG_PPINJECTORUI_USE_THREAD)
  target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)
endif()

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_PPINJECTORUI)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_PPINJECTORUI_ENABLED=1")
endif()
