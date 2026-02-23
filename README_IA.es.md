# Instrucciones Para IAs Colaboradoras

Este repositorio es `pp-injector-ui`.

## Política de Variantes
Regla obligatoria por defecto:
- **Salvo instrucción expresa del usuario, compila siempre con la variante `PPInjectorElecrow`** definida en `esp_idf_project_configuration.json`.

Implicaciones prácticas:
- Build directory por defecto: `build_ppinjectorelecrow`.
- Defaults de sdkconfig por defecto: `buildcfg/sdkconfig.PPInjectorElecrow.defaults`.
- No cambies de variante automáticamente aunque exista `PPInjectorWave`.
- Solo cambia a otra variante si el usuario lo pide explícitamente en ese turno.

## Política de Idioma en Documentación
- La documentación y los comentarios en el código deben escribirse en **inglés**.
- En algunos casos pueden existir traducciones con sufijo `.<lang>.md` (por ejemplo, `.es.md`).
- Si existen traducciones, deben mantenerse sincronizadas con sus homónimos en inglés.
- Si se pide traducir o crear una versión traducida de un documento, por defecto se debe preguntar al usuario si debe crearse en un fichero nuevo siguiendo esta convención de nombres.
