# CMakeLists.txt template for NetVars

# Señales de activación:
# - CMake var: PORIS_ENABLE_NETVARS (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_NETVARS (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_NETVARS OR DEFINED ENV{PORIS_ENABLE_NETVARS}))
  message(STATUS "[ NetVars ] disabled (no PORIS_ENABLE_NETVARS)")
  return()
endif()

idf_component_register(
  SRCS "NetVars.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES PrjCfg json nvs_flash mbedtls
)

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_NETVARS)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_NETVARS_ENABLED=1")
endif()
