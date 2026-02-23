# pp-injector-ui

Firmware HMI (pantalla táctil) para PP Injector basado en **ESP-IDF** (ESP32-S3).

Este proyecto es la evolución de `pp-motorized-injector-ui` (PlatformIO/Arduino + LVGL), migrado y adaptado a un entorno de firmware de producción con variantes, provisioning, OTA y utilidades de release.

## Objetivo
- Mostrar estado de máquina, posición del plunger y temperatura.
- Permitir interacción local de UI (pantalla táctil).
- Integrar conectividad para provisioning BLE/Wi-Fi y OTA.
- Mantener variantes de hardware del producto PPInjector.

## Variantes
Las variantes están definidas en `esp_idf_project_configuration.json`:
- `PPInjectorElecrow`
- `PPInjectorWave`

En el flujo diario de este repo, la variante preferida es **PPInjectorElecrow**.

## Estructura relevante
- `main/`: arranque y FSM principal (`app_main.c`).
- `components/`: componentes del firmware (UI, TouchScreen, OTA, Provisioning, etc.).
- `sdkcfg/`: overlays/feature defaults por variante.
- `buildcfg/`: sdkconfig defaults combinados por variante.
- `scripts/`: utilidades de desarrollo/release/publicación OTA.
- `releases/`: artefactos generados para distribución.

## Build rápido
Ejemplo típico para Elecrow:

```bash
idf.py -DVARIANT=PPInjectorElecrow build
```

Flasheo/monitor (ajusta puerto):

```bash
idf.py -p /dev/ttyACM1 flash monitor
```

## Flujo de release
### 1) Empaquetar release local
Script:
- `scripts/make_release_ppinjectorelecrow.py`

Genera un directorio como:
- `releases/ppinjectorelecrow_<version>/`

Usa `version.txt`, copia `bin/elf/map`, `flash_args` y dependencias de flasheo, y además genera:
- `ppinjectorelecrow_<version>.bin`

### 2) Publicar binario OTA
Script:
- `scripts/publish_ota_ppinjectorelecrow.py`

Publica `pp-injector-ui.bin` desde una release hacia el repo público OTA (ruta configurable en `scripts/config_secrets.py`).

## Configuración local (no versionada)
- Plantilla: `scripts/config_secrets.py.example`
- Copia local: `scripts/config_secrets.py` (ignorada por git)

Actualmente se usa para definir, al menos:
- `PUBLIC_REPO`: ruta local al repo público de firmware OTA.

## Notas de diseño
- El proyecto prioriza estabilidad de firmware y trazabilidad de variantes.
- Cambios de comportamiento en boot/provisioning/OTA deben validarse siempre en hardware real.

## Antecedente
Proyecto de referencia (histórico):
- `/home/txinto/repos/cbots/pp-motorized-injector-ui`

Este repo (`pp-injector-ui`) es la línea activa en ESP-IDF.
