# CMakeLists.txt template for OTA

# Señales de activación:
# - CMake var: PORIS_ENABLE_OTA (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_OTA (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_OTA OR DEFINED ENV{PORIS_ENABLE_OTA}))
  message(STATUS "[ OTA ] disabled (no PORIS_ENABLE_OTA)")
  return()
endif()

idf_component_register(
  SRCS "OTA_netvars.c" "OTA.c" "ble_helper/bluedroid_gatts.c" "ble_helper/nimble_gatts.c" "ble_helper/ble_api.c"
  INCLUDE_DIRS "include" "./ble_helper/include/"
  EMBED_TXTFILES ./server_certs/ca_cert.pem
  PRIV_REQUIRES PrjCfg esp_event app_update esp_http_client esp_https_ota nvs_flash esp_netif esp_wifi bt nimble_utils NetVars json
)

target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_OTA)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_OTA_ENABLED=1")
endif()
