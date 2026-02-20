# CMakeLists.txt template for PrjCfg

# Señales de activación:
# - CMake var: PORIS_ENABLE_PRJCFG (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_PRJCFG (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_PRJCFG OR DEFINED ENV{PORIS_ENABLE_PRJCFG}))
  message(STATUS "[ PrjCfg ] disabled (no PORIS_ENABLE_PRJCFG)")
  return()
endif()

idf_component_register(
  SRCS "PrjCfg_netvars.c" "PrjCfg.c" 
  INCLUDE_DIRS "include"
  PRIV_REQUIRES PrjCfg NetVars nvs_flash json
  REQUIRES esp_wifi
)

if(CONFIG_PRJCFG_USE_THREAD)
  target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)
endif()

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_PRJCFG)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_PRJCFG_ENABLED=1")
endif()
