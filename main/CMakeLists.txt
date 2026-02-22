# --- Reading the list from env variables and converting it on an actual list -------------
set(_raw "$ENV{PORIS_COMPONENTS_LIST}")         # f.i. "PrjCfg;...;..."
message(STATUS "[main] PORIS_COMPONENTS_LIST='${_raw}'")
string(REPLACE "," ";" _raw "${_raw}")          # If using colons
string(REPLACE " " ";" _raw "${_raw}")          # If using spaces
separate_arguments(_PORIS_LIST UNIX_COMMAND "${_raw}")  # => actual list

# --- Building REQUIRES in a safe way  -------------------------------------
set(_reqs spi_flash)
list(APPEND _reqs ${_PORIS_LIST})
list(REMOVE_DUPLICATES _reqs)

message(STATUS "[main] REQUIRES(final)='${_reqs}'")

idf_component_register(
  SRCS "app_main.c"
  PRIV_REQUIRES spi_flash mqtt json nvs_flash espressif__qrcode
  REQUIRES ${_reqs}
  INCLUDE_DIRS "."
)
