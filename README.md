# pp-injector-ui

Touchscreen HMI firmware for PP Injector, based on **ESP-IDF** (ESP32-S3).

This repository is the evolution of `pp-motorized-injector-ui` (PlatformIO/Arduino + LVGL), migrated and adapted to a production-oriented firmware setup with variants, provisioning, OTA, and release tooling.

## Purpose
- Display machine state, plunger position, and temperature.
- Provide local touchscreen UI interaction.
- Integrate network provisioning (BLE/Wi-Fi) and OTA updates.
- Support PPInjector hardware variants.

## Variants
Variants are defined in `esp_idf_project_configuration.json`:
- `PPInjectorElecrow`
- `PPInjectorWave`

For day-to-day work in this repository, the preferred/default variant is **PPInjectorElecrow**.

## Relevant Structure
- `main/`: startup flow and main FSM (`app_main.c`).
- `components/`: firmware components (UI, TouchScreen, OTA, Provisioning, etc.).
- `sdkcfg/`: per-variant feature/default overlays.
- `buildcfg/`: generated combined sdkconfig defaults per variant.
- `scripts/`: development, release, and OTA publish helpers.
- `releases/`: generated distributable artifacts.

## Quick Build
Typical Elecrow build:

```bash
idf.py -DVARIANT=PPInjectorElecrow build
```

Flash/monitor (adjust port):

```bash
idf.py -p /dev/ttyACM1 flash monitor
```

## Release Flow
### 1) Package local release
Script:
- `scripts/make_release_ppinjectorelecrow.py`

Generates a folder like:
- `releases/ppinjectorelecrow_<version>/`

It reads `version.txt`, copies `bin/elf/map`, `flash_args` and flashing dependencies, and also generates:
- `ppinjectorelecrow_<version>.bin`

### 2) Publish OTA binary
Script:
- `scripts/publish_ota_ppinjectorelecrow.py`

Publishes `pp-injector-ui.bin` from a release folder to the public OTA firmware repository (path configured in `scripts/config_secrets.py`).

## Local Non-Versioned Config
- Template: `scripts/config_secrets.py.example`
- Local copy: `scripts/config_secrets.py` (git-ignored)

Currently used for:
- `PUBLIC_REPO`: local path to the public OTA firmware repository.

## Design Notes
- The project prioritizes firmware stability and variant traceability.
- Any change in boot/provisioning/OTA behavior must be validated on real hardware.

## Historical Antecedent
Reference (historical) project:
- `/home/txinto/repos/cbots/pp-motorized-injector-ui`

This repository (`pp-injector-ui`) is the active ESP-IDF line.
