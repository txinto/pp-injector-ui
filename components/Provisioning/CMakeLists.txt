# CMakeLists.txt template for Provisioning

# Señales de activación:
# - CMake var: PORIS_ENABLE_PROVISIONING (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_PROVISIONING (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_PROVISIONING OR DEFINED ENV{PORIS_ENABLE_PROVISIONING}))
  message(STATUS "[ Provisioning ] disabled (no PORIS_ENABLE_PROVISIONING)")
  return()
endif()

idf_component_register(
  SRCS "Provisioning_netvars.c" "Provisioning.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES PrjCfg NetVars json nvs_flash esp_wifi wifi_provisioning
)

if(CONFIG_PROVISIONING_USE_THREAD)
  target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)
endif()

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_PROVISIONING)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_PROVISIONING_ENABLED=1")
endif()
