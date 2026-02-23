# Instructions for AI Collaborators

This repository is `pp-injector-ui`.

## Variant Policy
Default mandatory rule:
- **Unless explicitly instructed otherwise by the user, always build using the `PPInjectorElecrow` variant** defined in `esp_idf_project_configuration.json`.

Practical implications:
- Default build directory: `build_ppinjectorelecrow`.
- Default sdkconfig defaults: `buildcfg/sdkconfig.PPInjectorElecrow.defaults`.
- Do not switch variants automatically, even if `PPInjectorWave` exists.
- Only switch to another variant when the user explicitly asks for it in that turn.

## Documentation Language Policy
- Documentation and code comments must be written in **English**.
- In some cases, translated copies may exist with suffix `.<lang>.md` (for example, `.es.md`).
- If translated copies exist, they should be kept synchronized with their English counterparts.
- If asked to translate or create a translated version of a document, ask the user by default whether it should be created as a new file following this naming convention.
