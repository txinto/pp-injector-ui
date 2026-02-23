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
source "$IDF_PATH/export.sh" >/dev/null 2>&1 && \
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

## Build Reporting Requirement
- After each build execution, report the app partition free-space value shown by the build output (both percentage and bytes).
- Treat this value as critical project health information and highlight it in the build summary.

## Path and Command Hygiene
- Never expose developer-specific absolute paths (for example `/home/<user>/...`) in user-facing documentation, instructions, or suggested commands.
- Prefer project-root-relative paths (for example `scripts/...`, `docs/...`) whenever possible.
- For ESP-IDF setup, prefer environment variables (for example `$IDF_PATH`) instead of hardcoded local installation paths.
- If you detect existing docs with developer-specific absolute paths, propose/perform a cleanup to remove them.

## Documentation Language Policy
- Documentation and code comments must be written in **English**.
- In some cases, translated copies may exist with suffix `.<lang>.md` (for example, `.es.md`).
- If translated copies exist, they should be kept synchronized with their English counterparts.
- If asked to translate or create a translated version of a document, ask the user by default whether it should be created as a new file following this naming convention.

## Session Collaboration Defaults
- Before applying defaults, identify the active user using local repository identity (`git config user.name` / `git config user.email`).
- The AI cannot rely on ChatGPT account identity; if repository identity is missing/ambiguous, ask explicitly before assuming defaults.

### If the user is `txinto`
- Do **not** run build/test automatically after each relevant change unless asked.
- Do **not** create commits automatically unless asked.

## Translation Sync Workflow
- After the user confirms that the main delta (normally in English files) is correct, proactively propose updating any existing translated copies (for example `*.es.md`).
- Prefer translation updates as a follow-up step after validating the primary change set.
- If the user appears satisfied with a documentation edit and starts a different task, emit a reminder asking whether translation files should be updated.
- If the user indicates intent to create a commit that includes documentation changes without corresponding translation updates, emit the same reminder before proceeding.

## New Session Onboarding Questions
- During establishment of a new working session, if the user starts by asking to read README/README_IA/PRD docs, ask collaboration questions immediately after reading them.
- Include, at minimum, these questions only when they are not already resolved by `Session Collaboration Defaults` or explicitly answered earlier in the same session:
  1. Whether build/test should run automatically after relevant changes, or only on request.
  2. Whether commits should be created by default, or only on request.
  3. Whether translation sync (`*.es.md` etc.) should be proposed after confirming the main English delta.
  4. Preferred response style (concise vs detailed with alternatives).
- If repository identity is missing/ambiguous or no matching user-specific default section applies, ask the explicit default-policy question defined in `Session Collaboration Defaults`.

## Sensitive Data Guardrail
- Treat files containing `secret`/`secrets` in their path/name and files ignored by git as potentially sensitive.
- Never propose pushing such content unless the user explicitly requests it after confirmation.

## Response Style Preference
- Default to concise technical explanations with clear alternatives.
- Avoid long "execute these many steps" lists up front; prefer short, progressive instructions that can be expanded as needed.
