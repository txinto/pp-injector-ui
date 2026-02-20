# CMakeLists.txt template for TouchScreen

# Señales de activación:
# - CMake var: PORIS_ENABLE_TOUCHSCREEN (preferida, desde buildcfg/<Var>.components.cmake)
# - Env var  : PORIS_ENABLE_TOUCHSCREEN (compat si exportas env manualmente)

if(NOT (PORIS_ENABLE_TOUCHSCREEN OR DEFINED ENV{PORIS_ENABLE_TOUCHSCREEN}))
  message(STATUS "[ TouchScreen ] disabled (no PORIS_ENABLE_TOUCHSCREEN)")
  return()
endif()

set(srcs
  "TouchScreen_netvars.c"
  "TouchScreen.c"
  "lvgl_port.c"
)

if(CONFIG_TOUCHSCREEN_DISPLAY_BACKEND_4DS)
  list(APPEND srcs "TouchScreen_display_4ds.c")
elseif(CONFIG_TOUCHSCREEN_DISPLAY_BACKEND_WAVESHARE OR NOT DEFINED CONFIG_TOUCHSCREEN_DISPLAY_BACKEND_WAVESHARE)
  list(APPEND srcs "TouchScreen_display_waveshare.c" "waveshare_rgb_lcd_port.c")
else()
  # If Kconfig wasn't processed yet (or symbols are missing), fall back to the known-good backend.
  message(WARNING "TouchScreen: display backend not selected; defaulting to Waveshare")
  list(APPEND srcs "TouchScreen_display_waveshare.c" "waveshare_rgb_lcd_port.c")
endif()

idf_component_register(
  SRCS ${srcs}
  INCLUDE_DIRS "include"
  PRIV_REQUIRES PrjCfg NetVars json nvs_flash driver esp_lcd esp_timer
)

# Suprime warnings de formato en LVGL
if(TARGET lvgl__lvgl)
  target_compile_options(lvgl__lvgl PRIVATE -Wno-format)
endif()

if(CONFIG_TOUCHSCREEN_USE_THREAD)
  target_link_libraries(${COMPONENT_LIB} PRIVATE idf::freertos)
endif()

# Si Kconfig lo activó, define una macro útil
if(CONFIG_PORIS_ENABLE_TOUCHSCREEN)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE "PORIS_TOUCHSCREEN_ENABLED=1")
endif()
