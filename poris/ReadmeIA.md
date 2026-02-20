# ReadmeIA

Guia operativa para asistentes IA en este repositorio.

## Que es PORIS en este proyecto

- `poris` es un conjunto de herramientas para crear componentes, activarlos y configurarlos en proyectos ESP-IDF.
- Es especialmente potente para el manejo de variantes del proyecto.
- El punto de partida es `variants/variants.yml`.
- Flujo base:
  - Ejecutar `poris/gen_configs.py`.
  - Esto genera `esp_idf_project_configuration.json`.
  - Tambien genera variables de entorno para encender y apagar componentes.
  - Despues, la integracion de componentes se hace de forma condicionada por variantes y KConfig con `poris/add_component_integration.py`.

## Regla obligatoria sobre documentacion PORIS

- Antes de tocar variantes o integracion de componentes, leer los manuales de las herramientas PORIS.
- Los manuales estan en el cajon `poris` enlazado como submodulo del proyecto.

## Memoria entre sesiones

- La IA no garantiza memoria persistente entre sesiones.
- Todo lo importante debe quedar documentado aqui para reutilizarse despues.

## Flujo obligatorio de integracion de componentes PORIS

- No integrar componentes editando `main/app_main.c` a mano.
- Usar siempre:

```bash
python3 poris/add_component_integration.py --help
```

- Para integrar un componente, usar el script correspondiente y revisar el diff generado.
- Si una integracion previa manual contradice el resultado del script, prevalece el script.

## Flujo de build correcto (variante IceC6)

- Preparar variante antes de compilar:

```bash
bash poris/prebuild.sh IceC6
```

- Cargar entorno ESP-IDF y compilar:

```bash
source /home/txinto/esp/v5.5.2/esp-idf/export.sh
export VARIANT=IceC6
cmake --build build_icec6 -j4
```

## Reglas de trabajo acordadas

- Si hay duda de arquitectura, comparar con `wireless_module` antes de portar.
- Priorizar cambios pequenos, verificables y con build local.
- No asumir que una tarea corre desde `main` si el componente tiene modo thread propio.

## Pendientes / notas del usuario

- [ ] Completar aqui nuevas pautas y convenciones del proyecto.
