# Instrucciones Para IAs Colaboradoras

Este repositorio es `pp-injector-ui`.

Regla obligatoria por defecto:
- **Salvo instrucción expresa del usuario, compila siempre con la variante `PPInjectorElecrow`** definida en `esp_idf_project_configuration.json`.

Implicaciones prácticas:
- Build directory por defecto: `build_ppinjectorelecrow`.
- Defaults de sdkconfig por defecto: `buildcfg/sdkconfig.PPInjectorElecrow.defaults`.
- No cambies de variante automáticamente aunque exista `PPInjectorWave`.

Solo cambia a otra variante si el usuario lo pide explícitamente en ese turno.
