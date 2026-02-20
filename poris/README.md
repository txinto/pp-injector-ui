# PORIS build workflow

This project extends ESP-IDF with a simple variant/component system. The flow is:

1) Declare variants in `variants/variants.yml`.
2) Generate `esp_idf_project_configuration.json` (VS Code uses it to list variants and run pre-build hooks).
3) Pre-build scripts materialize per-variant defaults/env and pass them to `idf.py`.

Recommended layout: add this repo as a Git submodule under `poris/` in the root of your ESP-IDF project (next to `components/` and `esp_idf_project_configuration.json`).

Utilities in `poris/`:
- `gen_configs.py`: renders `esp_idf_project_configuration.json` from `variants.yml`.
- `prebuild.sh`: invoked by VS Code (or manually) before build; calls `gen_variant.py` and `ensure_variant.py`.
- `gen_variant.py`: merges overlays into `buildcfg/sdkconfig.<Var>.defaults` and writes env/CMake helpers.
- `ensure_variant.py`: copies `buildcfg/<Var>.env` into `build_<var>/config.env` so `idf.py` picks extra env.
- `new_component.py`: scaffolds a component from the template in `poris/templates/$$1`.
- `gen_netvars.py`: generates netvars fragments from `components/<Module>/netvars.csv`.
- `add_component_integration.py`: inserts component hooks into `main/app_main.c` via markers.
- `build.sh`: wraps `idf.py` with the right defaults/env for a variant.
- `clean_variant.sh`: backs up `sdkconfig` and deletes the build directory of a variant (see troubleshooting). Use `--complete` to skip restoring the backed-up `sdkconfig`.
- `poriscomp2project.py`: copies `poris/components` into `poris_components` in the project root (overwrite by default). Build precedence is `components/` > `poris_components/` > `poris/components/`; delete entries from `poris_components` to fall back to the submodule.

## Netvars (shared vars for JSON/NVS)
Netvars are component attributes prepared to be shared over JSON payloads and persisted in NVS. A common NetVars component supplies the enums/helpers; each component declares its own netvars in `components/<Module>/netvars.csv` and we generate only the per-module fragments.

CSV schema:

| column      | meaning |
|-------------|---------|
| `name`      | Field name in the component’s DRE struct. |
| `c_type`    | C type (declare buffers for strings, e.g. `char[LEN]`). |
| `storage_type` | `BOOL`, `U8`, `I8`, `I32`, `U32`, `FLOAT`, `FLOATINT`, `STRING` (maps to NetVars enums). |
| `scale`     | Scale factor for `FLOATINT` (integer >= 1). Empty for other types. |
| `nvs_key`   | Key in NVS (empty = not persisted). |
| `json_key`  | Key in JSON (empty = not exposed). |
| `group`/`module` | Optional tags for future grouping/filters (currently unused). |
| `nvs_mode`  | `NONE`, `LOAD`, `SAVE`, `LOADSAVE`. |
| `json`      | `1` to include in JSON; `0` to skip JSON exposure. |
| `json_mode` | JSON encoding for `U8` entries: `AUTO` (default), `ARRAY`, `HEX`, `DEC`, `BASE64`. |
| `enabler`   | Optional Kconfig macro to wrap the entry in `#ifdef` (empty = always included). Entries with the same enabler are grouped under a single guard. |

Example (`components/PrjCfg/netvars.csv`):

| name       | c_type                         | storage_type | scale | nvs_key | json_key | group  | module | nvs_mode | json | json_mode | enabler |
|------------|--------------------------------|--------------|-------|---------|----------|--------|--------|----------|------|-----------|---------|
| skip_ota   | bool                           | BOOL         |       | otaskip | otaskip  | config | ota    | LOADSAVE | 1    |           |         |
| ip_address | char[PRJCFG_IPADDRESS_MAX_LEN] | STRING       |       |         | ip       | config | net    | NONE     | 1    |           |         |
| wifi_key   | uint8_t[16]                    | U8           |       |         | wkey     | config | net    | NONE     | 1    | HEX       | PORIS_ENABLE_WIFI |

`json_mode` only affects `U8` types:
- `AUTO` (or empty): scalar `U8` is a decimal number; `U8` arrays become hex strings by default.
- `ARRAY`: always emit/accept arrays of numbers for `U8` arrays.
- `HEX`: emit/accept hex strings (good for binary blobs).
- `DEC`: force decimal numbers (scalar) or arrays of decimals.
- `BASE64`: emit/accept base64 strings.

Generation flow:
1) Edit `components/<Module>/netvars.csv` as above.
2) Run `python3 poris/gen_netvars.py <Module>` (supports both `components/<Module>` and `poris/components/<Module>`). This only touches two fragment files:
   - `components/<Module>/include/<Module>_netvar_types_fragment.h_` (struct members).
   - `components/<Module>/<Module>_netvars_fragment.c_` (descriptor array entries).
3) In `<Module>.h`, include `include/<Module>_netvar_types_fragment.h_` inside your `<Module>_dre_t` struct.
4) The component source should include `<Module>_netvars.h` and `#include "<Module>_netvars_fragment.c_"` inside the descriptor array (template/new_component already does this). Functions available:
   - `<Module>_netvars_append_json(cJSON *root)` / `<Module>_netvars_parse_json_dict(cJSON *root)` for JSON export/import.
   - `<Module>_netvars_nvs_load/save()` to persist according to `nvs_mode` (no-op if the descriptor array is empty).
   - `<Module>_config_parse_json(const char *data)` convenience wrapper used by `app_main` callbacks.

Only the fragment files are regenerated; the rest of the component code can be customized safely.

## Add a new variant
- Edit `variants/variants.yml` and add an entry under `variants`:
  - `id`: the variant name (used everywhere).
  - `target`: ESP-IDF target (`esp32s3`, `esp32c6`, etc.).
  - `overlays`: files under `sdkcfg/` to layer on top of `sdkconfig.defaults`. Missing files are auto-created empty.
  - `components`: list of component names to enable.
  - `extra_env`: optional `KEY: VAL` pairs exported to the build env for that variant.
- If no variants file exists, `gen_configs.py` will auto-create `variants/variants.yml` with a single `main` variant listing all directories under `components/` as components.
- Regenerate VS Code configs (mandatory for the extension to see the variant):
  - `python3 poris/gen_configs.py`
- Build:
  - VS Code ESP-IDF extension: select the variant and hit Build. The extension will run `poris/prebuild.sh <VariantId>` automatically.
  - CLI alternative: `bash poris/build.sh <VariantId> build` (wraps `idf.py` with the right defaults/env).

## Add a new component
- Scaffold from the template:
  - `python3 poris/new_component.py --name MyFeature`
  - Creates `components/MyFeature/` with a CMake guard that checks `PORIS_ENABLE_MYFEATURE`.
  - Drops `Kconfig.projbuild` with thread/spin/jitter options and `PORIS_ENABLE_MYFEATURE`.
- Implement your logic in `MyFeature.c` and headers in `include/`. Kconfig is picked up automatically when the component is enabled.

## Component integration in app_main (add_component_integration.py)
This script generates the usual integration blocks in `main/app_main.c` for a component, using markers:

- `// [PORIS_INTEGRATION_INCLUDE]`
- `// [PORIS_INTEGRATION_INIT]`
- `// [PORIS_INTEGRATION_START]`
- `// [PORIS_INTEGRATION_DEFINES]`
- `// [PORIS_INTEGRATION_COUNTERS]`
- `// [PORIS_INTEGRATION_RUN]`
- `// [PORIS_INTEGRATION_NETVARS_INCLUDE]`
- `// [PORIS_INTEGRATION_NETVARS_PARSE]`
- `// [PORIS_INTEGRATION_NETVARS_APPEND]`

Canonical command (uses defaults, does not override headers, and applies directly to `main/app_main.c`):

```
python3 poris/add_component_integration.py --name MyComponent --apply
```

Notes:
- The script only generates the spin/counters/run snippets when `--spin-period-ms` is greater than 0 (defaults to 100).
- If you omit `--apply`, it prints the snippets to stdout for manual copy/paste.

### Arguments and types
- `--name` (string, required): component name, e.g. `DualLED`. Used to derive default macros and headers.
- `--enable-macro` (string): Kconfig macro that enables the component. Default: `CONFIG_PORIS_ENABLE_<NAME>`.
- `--include` (string): main header to include. Default: `<Name.h>`.
- `--netvars-include` (string): netvars header. Default: `<Name_netvars.h>`.
- `--use-thread-macro` (string): macro name that indicates threaded mode. Default: `CONFIG_<NAME>_USE_THREAD`.
  - This is not a boolean: it expects a macro name, not `true/false` or `0/1`.
  - If it exists in your `sdkconfig`, the generated code includes a `#ifdef` to call `*_start()` when appropriate.
  - If you want to skip that `#ifdef`, pass an empty string: `--use-thread-macro ""`.
- `--spin-period-ms` (int): period used to generate the spin block and defines/counters. Default: `0` (no spin generated).
- `--period-macro` (string): period macro name. Default: `<NAME>_CYCLE_PERIOD_MS`.
- `--counter-name` (string): counter variable name. Default: `<name>_cycle_counter`.
- `--no-spin` (flag): disables generation of spin/counters/defines.
- `--apply` (flag): inserts snippets into `main/app_main.c` using markers.
- `--app-main` (string): path to `app_main.c` for `--apply`. Default: `main/app_main.c`.

## Enable components in a variant
- Add the component name to the `components` array of the target variant(s) in `variants/variants.yml`.
- Run `python3 poris/gen_configs.py`, then trigger a build:
  - `prebuild.sh` (called by VS Code or manually) runs `gen_variant.py`, which writes:
    - `buildcfg/<Var>.env` with `PORIS_ENABLE_<COMP>=1` for every listed component (plus `extra_env`).
    - `buildcfg/<Var>.components.cmake` exporting the component list to CMake/ENV.
    - `buildcfg/sdkconfig.<Var>.defaults` (merged overlays) and `<Var>.cmakecache.cmake` with `IDF_TARGET`.

## Spin vs. self-threaded components (Measurement example)
All components generated from the template (and Measurement) support two modes via Kconfig:
- `*_USE_THREAD = n` (default): the component exposes `*_spin()`; you call it from your scheduler/loop. No FreeRTOS task is created.
- `*_USE_THREAD = y`: the component spawns its own FreeRTOS task that calls `*_spin()` periodically. Period, stack, priority, and core affinity are configurable via `*_PERIOD_MS`, `*_TASK_STACK`, `*_TASK_PRIO`, and `*_PIN_CORE`.

Measurement specifics:
- APIs: `Measurement_setup()`, `Measurement_enable()/disable()`, `Measurement_spin()` (call yourself if not threaded), `Measurement_start()/stop()` (start/stop its task when threaded), `Measurement_set_period_ms()` to change the period on the fly.
- When threaded, `Measurement_start()` creates the task and loops with `Measurement_spin()`; disable it via `Measurement_stop()`.

## Jitter minimization
- Kconfig option `*_MINIMIZE_JITTER` (enabled by default when threaded) controls the delay strategy:
  - On: uses `vTaskDelayUntil()` to keep a stable period even after long iterations (lower jitter).
  - Off: uses `vTaskDelay()`, so long iterations push the next wakeup (more drift).
- This only applies when `*_USE_THREAD = y`; pick it per component in `menuconfig`.

## Troubleshooting: menuconfig not opening
- Occasionally `menuconfig` may fail to open due to a stale build directory. Cleaning the variant usually fixes it.
- Warning: `sdkconfig` often contains sensitive data (Wi-Fi, URLs) and is not versioned; deleting `build_*` will drop those settings unless you back them up.
- Use the helper to clean safely:
  - `bash poris/clean_variant.sh <VariantId>` — backs up `build_<slug>/sdkconfig` to `buildcfg/sdkconfig_backups/sdkconfig.<VariantId>.<timestamp>.bak`, deletes `build_<slug>`, and restores the backup into a fresh `build_<slug>/sdkconfig` automatically.
  - `bash poris/clean_variant.sh --complete <VariantId>` — same, but does not restore `sdkconfig` (use if you want a fully clean slate).
  - After cleaning, re-run prebuild/build (VS Code build button or `bash poris/prebuild.sh <VariantId>`). If you used `--complete`, revisit `menuconfig` to re-apply settings.
