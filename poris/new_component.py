#!/usr/bin/env python3
# poris/new_component.py
import argparse, os, pathlib, re, shutil, sys

PH_LOWER = "$$1"  # Normal module name (MyModule)
PH_UPPER = "$#1"  # upper+sanitized module name (MYMODULE)

def upper_sanitized(name: str) -> str:
    u = re.sub(r'[^A-Za-z0-9]', '_', name).upper()
    u = re.sub(r'_+', '_', u).strip('_')
    return u or "COMPONENT"

def copy_with_placeholders(tmpl_dir: pathlib.Path, dest_dir: pathlib.Path, name: str):
    up = upper_sanitized(name)
    if dest_dir.exists():
        raise FileExistsError(f"El destino ya existe: {dest_dir}")
    for root, _, files in os.walk(tmpl_dir):
        rel = pathlib.Path(root).relative_to(tmpl_dir)
        rel_str = str(rel)
        rel_str = rel_str.replace(PH_LOWER, name).replace(PH_UPPER, up)
        out_dir = dest_dir / rel_str if rel_str != '.' else dest_dir
        out_dir.mkdir(parents=True, exist_ok=True)

        for f in files:
            src = pathlib.Path(root) / f
            new_name = f.replace(PH_LOWER, name).replace(PH_UPPER, up)
            dst = out_dir / new_name
            data = src.read_bytes()
            try:
                s = data.decode('utf-8')
                s = s.replace(PH_LOWER, name).replace(PH_UPPER, up)
                dst.write_text(s, encoding='utf-8')
            except UnicodeDecodeError:
                shutil.copy2(src, dst)

def ensure_guard(comp_dir: pathlib.Path, name: str):
    up = upper_sanitized(name)
    cmake = comp_dir / "CMakeLists.txt"
    if not cmake.exists():
        cmake.write_text(f"""# Auto-generated CMakeLists for {name}
if(NOT DEFINED ENV{{PORIS_ENABLE_{up}}})
  message(STATUS "[ {name} ] disabled by variant (PORIS_ENABLE_{up} not set)")
  return()
endif()

idf_component_register(
  SRCS "{name}.c"
  INCLUDE_DIRS "include"
)

if(CONFIG_PORIS_ENABLE_{up})
  target_compile_definitions({{COMPONENT_LIB}} PRIVATE "PORIS_{up}_ENABLED=1")
endif()
""", encoding="utf-8")
        return
    t = cmake.read_text(encoding="utf-8")
    if f"PORIS_ENABLE_{up}" not in t:
        t = f"""# Guard por variante (no compilar si no está habilitado)
if(NOT DEFINED ENV{{PORIS_ENABLE_{up}}})
  message(STATUS "[ {name} ] disabled by variant (PORIS_ENABLE_{up} not set)")
  return()
endif()

""" + t
        cmake.write_text(t, encoding="utf-8")

def main():
    ap = argparse.ArgumentParser(description="Creates a new component from poris/templates/$$1 with placeholders $$1 and $#1")
    ap.add_argument("--name", required=True, help="Component name (f.i. MyComponent)")
    ap.add_argument("--type-library", action="store_true", help="Use the library template (no threads/netvars/spin)")
    ap.add_argument("--template", default=None, help="Template path (overrides --type-library)")
    ap.add_argument("--dest", default="components", help="Target folder (components)")
    args = ap.parse_args()

    root = pathlib.Path(__file__).resolve().parent.parent
    template = args.template
    if not template:
        template = "poris/templates/$$1lib" if args.type_library else "poris/templates/$$1"
    tmpl_dir = (root / template).resolve()
    dest_parent = (root / args.dest).resolve()

    if not tmpl_dir.exists():
        print(f"No encuentro la plantilla: {tmpl_dir}", file=sys.stderr); sys.exit(2)
    if str(tmpl_dir).startswith(str(dest_parent)):
        print("La plantilla no debe estar dentro de 'components/'. Usa poris/templates/$$1.", file=sys.stderr); sys.exit(3)

    name = args.name  # se mantiene tal cual para $$1
    dest_dir = dest_parent / name

    copy_with_placeholders(tmpl_dir, dest_dir, name)

    # Renombrar include/$$1.h si quedase
    inc = dest_dir / "include"
    if inc.exists():
        tmp = inc / (PH_LOWER + ".h")
        if tmp.exists():
            tmp.rename(inc / f"{name}.h")

    ensure_guard(dest_dir, name)

    print(f"Componente creado en: {dest_dir}")
    print(f"Variable para habilitarlo en variantes: PORIS_ENABLE_{upper_sanitized(name)}=1")

if __name__ == "__main__":
    main()
