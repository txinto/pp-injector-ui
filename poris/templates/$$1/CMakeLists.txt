# CMakeLists.txt template for $$1

# Señales de activación:
# - CMake var: PORIS_ENABLE_$#1 (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_$#1 (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_$#1 OR DEFINED ENV{PORIS_ENABLE_$#1}))
  message(STATUS "[ $$1 ] disabled (no PORIS_ENABLE_$#1)")
  return()
endif()

idf_component_register(
  SRCS "$$1_netvars.c" "$$1.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES PrjCfg NetVars json nvs_flash
)

if(CONFIG_$#1_USE_THREAD)
  target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)
endif()

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_$#1)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_$#1_ENABLED=1")
endif()
