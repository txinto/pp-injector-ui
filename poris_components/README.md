This folder is an optional override for PORIS components.

Build precedence for ESP-IDF components:
- `components/` (project-local)
- `poris_components/` (this directory)
- `poris/components/` (the PORIS submodule)

Use `python3 poris/poriscomp2project.py` to copy all components from the submodule (`poris/components`) into `poris_components` so you can build without the submodule or tweak a local copy. If you remove a component from `poris_components`, ESP-IDF will fall back to the version in `poris/components`.
