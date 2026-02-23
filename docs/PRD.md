# PRD — pp-injector-ui (ESP-IDF)

**Version:** 1.0  
**Status:** Active  
**Project:** `pp-injector-ui`

## 1. Purpose
Develop and maintain the PP Injector HMI firmware on ESP32-S3 with support for:
- Live display of state and key magnitudes (position, temperature, machine state).
- Touchscreen interaction.
- Network provisioning (BLE/Wi-Fi).
- OTA firmware updates.
- PPInjector hardware variants.

## 2. Scope
Includes:
- ESP-IDF HMI firmware.
- UI and visual logic (plunger/barrel and status screens).
- Integration with communication and configuration components.
- Release and OTA publish scripts.

Excludes:
- Motor/actuator control firmware external to the HMI.
- Cloud backend or mobile app implementation (device-side integration only).

## 3. Target Platform
- MCU: ESP32-S3
- Framework: ESP-IDF
- Supported variants:
  - `PPInjectorElecrow`
  - `PPInjectorWave`

## 4. Functional Requirements
### 4.1 Operational UI
- Continuously display system status.
- Display position/plunger and temperature.
- Provide stable touch interaction.

### 4.2 Provisioning
- Enter provisioning mode according to boot state.
- Expose BLE provisioning identifier (PPI_* prefix, per configuration).
- Provide clear provisioning status to users via screen/logs.

### 4.3 OTA
- Support OTA mode based on boot state.
- Show OTA progress in lightweight boot-display mode (without full LVGL UI).
- Perform controlled reboot after OTA success/failure according to application policy.

### 4.4 Persistence and Configuration
- Preserve configuration/credentials and boot state according to defined flow.
- Support variant-specific partitioning and flash artifacts.

## 5. Non-Functional Requirements
- Memory stability, especially in provisioning/OTA modes without full UI.
- Sufficient logs for boot/provisioning/OTA diagnostics.
- Reproducible variant-specific builds.
- Simple, auditable release/publish scripts.

## 6. Architecture (High-Level)
- `main/app_main.c`:
  - Boot FSM.
  - Provisioning, OTA, and UI startup orchestration.
- `components/PPInjectorUI`:
  - Main UI and visual logic.
- `components/TouchScreen`:
  - Display/touch backend, including lightweight boot-display mode.
- `components/Provisioning`:
  - BLE/Wi-Fi provisioning and related state.
- `components/OTA`:
  - OTA flow and progress state.

## 7. Variants and Build
Source of truth for variants:
- `esp_idf_project_configuration.json`

Operational default rule:
- Build and validate with `PPInjectorElecrow` unless explicitly instructed otherwise.

## 8. Release and Distribution
### 8.1 Packaging
- Script: `scripts/make_release_ppinjectorelecrow.py`
- Input: `build_ppinjectorelecrow`
- Output: `releases/ppinjectorelecrow_<version>/`

### 8.2 OTA Publishing
- Script: `scripts/publish_ota_ppinjectorelecrow.py`
- Copies firmware binary to public OTA repository.
- Requires destination repo to be pristine before commit/pull/push.

## 9. Acceptance Criteria
- Successful build on `PPInjectorElecrow`.
- Functional provisioning flow on target hardware.
- Functional OTA flow with visual/log feedback.
- Release/publish scripts working end-to-end.

## 10. Traceability to Antecedent
This PRD adapts the antecedent from `pp-motorized-injector-ui` to the current ESP-IDF reality of `pp-injector-ui`, preserving functional intent while updating architecture, tooling, and delivery workflow.
