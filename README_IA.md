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

## Repository Guardrails
Before running commands or editing files:
- Confirm location with `pwd`.
- Confirm git root with `git rev-parse --show-toplevel`.
- If the active repo does not match the requested project, stop and ask the user.

Cross-repo safety:
- Do not edit sibling repositories (for example `poris_base` when working in `pp-injector-ui`) unless the user explicitly requests it and the target path is explicit.
- If a command must run in another repo, state the target path before executing it.

## Canonical Build and Release Commands
Use these commands as the default reference workflow for `PPInjectorElecrow`:

1. Build
```bash
source /home/txinto/esp/v5.5.2/esp-idf/export.sh >/dev/null 2>&1 && \
idf.py -B build_ppinjectorelecrow -DVARIANT=PPInjectorElecrow -DSDKCONFIG=build_ppinjectorelecrow/sdkconfig build
```

2. Create release bundle
```bash
python3 scripts/make_release_ppinjectorelecrow.py
```

3. Publish OTA binary
```bash
python3 scripts/publish_ota_ppinjectorelecrow.py releases/ppinjectorelecrow_<version>
```

Notes:
- If release generation fails due to missing/incomplete build artifacts, run the build command above and retry.
- Follow script-provided error hints verbatim unless the user asks for a different flow.

## Documentation Language Policy
- Documentation and code comments must be written in **English**.
- In some cases, translated copies may exist with suffix `.<lang>.md` (for example, `.es.md`).
- If translated copies exist, they should be kept synchronized with their English counterparts.
- If asked to translate or create a translated version of a document, ask the user by default whether it should be created as a new file following this naming convention.
