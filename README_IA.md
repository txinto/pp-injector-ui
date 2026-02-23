# Instructions for AI Collaborators

This repository is `pp-injector-ui`.

Default mandatory rule:
- **Unless explicitly instructed otherwise by the user, always build using the `PPInjectorElecrow` variant** defined in `esp_idf_project_configuration.json`.

Practical implications:
- Default build directory: `build_ppinjectorelecrow`.
- Default sdkconfig defaults: `buildcfg/sdkconfig.PPInjectorElecrow.defaults`.
- Do not switch variants automatically, even if `PPInjectorWave` exists.

Only switch to another variant when the user explicitly asks for it in that turn.
