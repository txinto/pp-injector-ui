# PRD — pp-injector-ui (ESP-IDF)

**Versión:** 1.0  
**Estado:** Activo  
**Proyecto:** `pp-injector-ui`

## 1. Propósito
Desarrollar y mantener el firmware de interfaz de usuario (HMI) para PP Injector en ESP32-S3, con soporte de:
- Visualización de estado y magnitudes clave (posición, temperatura, estado de máquina).
- Interacción táctil.
- Provisioning de red (BLE/Wi-Fi).
- Actualización OTA.
- Variantes de hardware PPInjector.

## 2. Alcance
Incluye:
- Firmware ESP-IDF del HMI.
- UI y lógica de representación (plunger/barrel y pantallas de estado).
- Integración con componentes de comunicaciones y configuración.
- Scripts de release y publicación OTA.

No incluye:
- Firmware de control de motor/actuador externo al HMI.
- Backend cloud o app móvil de provisioning (solo integración del lado dispositivo).

## 3. Plataforma objetivo
- MCU: ESP32-S3
- Framework: ESP-IDF
- Variantes soportadas:
  - `PPInjectorElecrow`
  - `PPInjectorWave`

## 4. Requisitos funcionales
### 4.1 UI operativa
- Mostrar estado de sistema de forma continua.
- Mostrar posición/plunger y temperatura.
- Permitir interacción táctil estable.

### 4.2 Provisioning
- Entrar en modo provisioning cuando corresponda por estado de arranque.
- Publicar identificador BLE de provisioning (prefijo PPI_* según configuración).
- Informar al usuario del estado de provisioning en pantalla/log.

### 4.3 OTA
- Soportar arranque en modo OTA según boot state.
- Mostrar progreso OTA en modo boot-display (sin LVGL completo).
- Reinicio controlado al finalizar o fallar OTA según política de aplicación.

### 4.4 Persistencia y configuración
- Mantener configuración/credenciales y estado de boot según flujo definido.
- Soportar particionado y artefactos de flash por variante.

## 5. Requisitos no funcionales
- Estabilidad en memoria (especialmente en modos provisioning/OTA sin UI completa).
- Trazas suficientes para diagnóstico de boot/provisioning/OTA.
- Reproducibilidad de builds por variante.
- Scripts de release/publish simples y auditables.

## 6. Arquitectura (alto nivel)
- `main/app_main.c`:
  - FSM de arranque.
  - Orquestación de provisioning, OTA y arranque UI.
- `components/PPInjectorUI`:
  - UI y lógica visual principal.
- `components/TouchScreen`:
  - Backend display/touch (incluyendo modo boot-display ligero).
- `components/Provisioning`:
  - Provisioning BLE/Wi-Fi y estado asociado.
- `components/OTA`:
  - Flujo OTA y estado de progreso.

## 7. Variantes y build
Fuente de verdad de variantes:
- `esp_idf_project_configuration.json`

Regla operativa por defecto:
- Compilar y validar con `PPInjectorElecrow`, salvo instrucción explícita para otra variante.

## 8. Release y distribución
### 8.1 Empaquetado
- Script: `scripts/make_release_ppinjectorelecrow.py`
- Entrada: `build_ppinjectorelecrow`
- Salida: `releases/ppinjectorelecrow_<version>/`

### 8.2 Publicación OTA
- Script: `scripts/publish_ota_ppinjectorelecrow.py`
- Copia binario a repo público de firmware OTA.
- Requiere repo destino limpio (`pristine`) antes de commit/pull/push.

## 9. Criterios de aceptación
- Build exitoso en `PPInjectorElecrow`.
- Flujo provisioning funcional en hardware objetivo.
- Flujo OTA funcional y con feedback visual/log.
- Scripts de release/publish operativos extremo a extremo.

## 10. Trazabilidad con antecedente
Este PRD adapta el antecedente de `pp-motorized-injector-ui` a la realidad actual de `pp-injector-ui` en ESP-IDF, manteniendo intención funcional pero ajustando arquitectura, tooling y flujo de entrega.
