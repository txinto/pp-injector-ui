# Instrucciones para IAs colaboradoras

Este repositorio es `pp-injector-ui`.

## Política de variantes
Regla obligatoria por defecto:
- **Salvo instrucción expresa del usuario, compila siempre con la variante `PPInjectorElecrow`** definida en `esp_idf_project_configuration.json`.

Implicaciones prácticas:
- Directorio de build por defecto: `build_ppinjectorelecrow`.
- Defaults de sdkconfig por defecto: `buildcfg/sdkconfig.PPInjectorElecrow.defaults`.
- No cambies de variante automáticamente aunque exista `PPInjectorWave`.
- Solo cambia a otra variante si el usuario lo pide explícitamente en ese turno.

## Guardarraíles de repositorio
Antes de ejecutar comandos o editar ficheros:
- Confirma ubicación con `pwd`.
- Confirma raíz git con `git rev-parse --show-toplevel`.
- Si el repositorio activo no coincide con el proyecto solicitado, detén la ejecución y pregunta al usuario.

Seguridad entre repositorios:
- No edites repositorios hermanos (por ejemplo `poris_base` cuando se trabaja en `pp-injector-ui`) salvo petición explícita del usuario y ruta de destino explícita.
- Si un comando debe ejecutarse en otro repositorio, indica la ruta de destino antes de ejecutarlo.

## Comandos canónicos de build y release
Usa estos comandos como flujo de referencia por defecto para `PPInjectorElecrow`:

1. Build
```bash
source "$IDF_PATH/export.sh" >/dev/null 2>&1 && \
idf.py -B build_ppinjectorelecrow -DVARIANT=PPInjectorElecrow -DSDKCONFIG=build_ppinjectorelecrow/sdkconfig build
```

2. Crear paquete de release
```bash
python3 scripts/make_release_ppinjectorelecrow.py
```

3. Publicar binario OTA
```bash
python3 scripts/publish_ota_ppinjectorelecrow.py releases/ppinjectorelecrow_<version>
```

Notas:
- Si la generación de release falla por artefactos de build faltantes o incompletos, ejecuta el comando de build anterior y reintenta.
- Sigue literalmente los mensajes de error y pistas de los scripts salvo que el usuario pida otro flujo.

## Higiene de rutas y comandos
- Nunca expongas rutas absolutas específicas de un desarrollador (por ejemplo `/home/<user>/...`) en documentación visible para el usuario, instrucciones o comandos sugeridos.
- Prefiere rutas relativas a la raíz del proyecto (por ejemplo `scripts/...`, `docs/...`) siempre que sea posible.
- Para configuración de ESP-IDF, prefiere variables de entorno (por ejemplo `$IDF_PATH`) en lugar de rutas locales hardcodeadas.
- Si detectas documentación existente con rutas absolutas de desarrollador, propone o realiza una limpieza para eliminarlas.

## Política de idioma en documentación
- La documentación y los comentarios en el código deben escribirse en **inglés**.
- En algunos casos pueden existir traducciones con sufijo `.<lang>.md` (por ejemplo, `.es.md`).
- Si existen traducciones, deben mantenerse sincronizadas con sus homólogos en inglés.
- Si se pide traducir o crear una versión traducida de un documento, por defecto se debe preguntar al usuario si debe crearse en un fichero nuevo siguiendo esta convención de nombres.

## Defaults de colaboración de sesión
- Antes de aplicar defaults, identifica al usuario activo usando la identidad local del repositorio (`git config user.name` / `git config user.email`).
- La IA no puede basarse en la identidad de la cuenta de ChatGPT; si la identidad local del repositorio falta o es ambigua, pregunta explícitamente antes de asumir defaults.

### If the user is `txinto`
- No ejecutes build/test automáticamente tras cada cambio relevante salvo que te lo pidan.
- No crees commits automáticamente salvo que te lo pidan.

## Flujo de sincronización de traducciones
- Tras confirmar el usuario que el delta principal (normalmente en ficheros en inglés) es correcto, propone de forma proactiva actualizar las copias traducidas existentes (por ejemplo `*.es.md`).
- Prefiere tratar la actualización de traducciones como un paso posterior a validar el conjunto principal de cambios.
- Si el usuario parece satisfecho con una edición de documentación y empieza otra tarea, emite un recordatorio preguntando si desea actualizar los ficheros de traducción.
- Si el usuario indica intención de crear un commit que incluya cambios en documentación sin actualizar traducciones correspondientes, emite el mismo recordatorio antes de seguir.

## Preguntas de onboarding en nueva sesión
- Durante el establecimiento de una nueva sesión de trabajo, si el usuario comienza pidiendo leer README/README_IA/PRD, haz las preguntas de colaboración inmediatamente después de leerlos.
- Incluye, como mínimo, estas preguntas solo cuando no estén ya resueltas por `Session Collaboration Defaults` o respondidas explícitamente antes en la misma sesión:
  1. Si build/test debe ejecutarse automáticamente tras cambios relevantes, o solo bajo petición.
  2. Si los commits deben crearse por defecto, o solo bajo petición.
  3. Si debe proponerse la sincronización de traducciones (`*.es.md`, etc.) tras confirmar el delta principal en inglés.
  4. Estilo de respuesta preferido (conciso o detallado con alternativas).
- Si la identidad local del repositorio falta o es ambigua, o no aplica ninguna sección de defaults específica del usuario, formula la pregunta explícita de política por defecto definida en `Session Collaboration Defaults`.

## Guardarraíl de datos sensibles
- Trata los ficheros que contengan `secret`/`secrets` en su ruta o nombre, y los ficheros ignorados por git, como potencialmente sensibles.
- Nunca propongas pushear ese contenido salvo petición explícita del usuario tras confirmación.

## Preferencia de estilo de respuesta
- Por defecto, usa explicaciones técnicas concisas con alternativas claras.
- Evita listas largas de "ejecuta estos muchos pasos" de entrada; prefiere instrucciones cortas y progresivas que puedan ampliarse si hace falta.
