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

## Guardarraíles de Repositorio
Antes de ejecutar comandos o editar ficheros:
- Confirma ubicación con `pwd`.
- Confirma raíz git con `git rev-parse --show-toplevel`.
- Si el repositorio activo no coincide con el proyecto solicitado, detente y pregunta al usuario.

Seguridad entre repositorios:
- No edites repositorios hermanos (por ejemplo `poris_base` cuando se trabaja en `pp-injector-ui`) salvo petición explícita del usuario y ruta de destino explícita.
- Si un comando debe ejecutarse en otro repositorio, indica la ruta de destino antes de ejecutarlo.

## Comandos Canónicos de Build y Release
Usa estos comandos como flujo de referencia por defecto para `PPInjectorElecrow`:

1. Build
```bash
source /home/txinto/esp/v5.5.2/esp-idf/export.sh >/dev/null 2>&1 && \
idf.py -B build_ppinjectorelecrow -DVARIANT=PPInjectorElecrow -DSDKCONFIG=build_ppinjectorelecrow/sdkconfig build
```

2. Crear paquete release
```bash
python3 scripts/make_release_ppinjectorelecrow.py
```

3. Publicar binario OTA
```bash
python3 scripts/publish_ota_ppinjectorelecrow.py releases/ppinjectorelecrow_<version>
```

Notas:
- Si la generación de release falla por artefactos faltantes/incompletos de build, ejecuta el comando de build anterior y reintenta.
- Sigue literalmente los mensajes de error/pistas de los scripts salvo que el usuario pida otro flujo.

## Política de Idioma en Documentación
- La documentación y los comentarios en el código deben escribirse en **inglés**.
- En algunos casos pueden existir traducciones con sufijo `.<lang>.md` (por ejemplo, `.es.md`).
- Si existen traducciones, deben mantenerse sincronizadas con sus homónimos en inglés.
- Si se pide traducir o crear una versión traducida de un documento, por defecto se debe preguntar al usuario si debe crearse en un fichero nuevo siguiendo esta convención de nombres.
