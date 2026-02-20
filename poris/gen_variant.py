#!/usr/bin/env python3
# poris/gen_variant.py
import argparse, yaml, pathlib, sys

def find_variants_yml(root: pathlib.Path, arg_path: str | None) -> pathlib.Path:
    candidates = []
    if arg_path:
      p = (root / arg_path).resolve() if not pathlib.Path(arg_path).is_absolute() else pathlib.Path(arg_path)
      candidates.append(p)
    candidates += [
      (root / "variants" / "variants.yml").resolve(),
      (root / "variants.yml").resolve(),
    ]
    for c in candidates:
      if c.exists():
        return c
    raise FileNotFoundError("No encuentro el descriptor YAML de variantes. Busca en: " +
                            ", ".join(str(c) for c in candidates))

def ensure_file(path: pathlib.Path, comment: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        path.write_text(f"# {comment}\n", encoding="utf-8")

def concat_files(paths):
    out = []
    for p in paths:
        pp = pathlib.Path(p)
        if not pp.exists():
            raise FileNotFoundError(str(pp))
        out.append(pp.read_text(encoding="utf-8").rstrip() + "\n")
    return "".join(out)

def main():
    ap = argparse.ArgumentParser(description="Combina overlays en buildcfg/sdkconfig.<variant>.defaults y crea env por variante")
    ap.add_argument("--variant", required=True, help="ID de la variante (definida en variants.yml)")
    ap.add_argument("--variants-yml", help="Ruta al YAML (opcional). Por defecto busca en variants/variants.yml")
    args = ap.parse_args()

    script_dir = pathlib.Path(__file__).resolve().parent
    root = script_dir.parent

    try:
        variants_yml = find_variants_yml(root, args.variants_yml)
    except FileNotFoundError as e:
        print(str(e), file=sys.stderr)
        sys.exit(2)

    cfg = yaml.safe_load(variants_yml.read_text(encoding="utf-8")) or {}
    variants = cfg.get("variants", [])
    var = next((v for v in variants if v.get("id") == args.variant), None)
    if not var:
        print(f"La variante '{args.variant}' no está en {variants_yml}", file=sys.stderr)
        print("Variantes disponibles:", [v.get("id") for v in variants], file=sys.stderr)
        sys.exit(3)

    defaults_dir = (root / cfg.get("defaults_dir", "sdkcfg")).resolve()
    out_dir = (root / cfg.get("out_dir", "buildcfg")).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    # base defaults vacío si no existe
    base_defaults = root / "sdkconfig.defaults"
    ensure_file(base_defaults, "base defaults (auto-created empty)")

    # overlays (crea vacíos si faltan; SIN contenido)
    overlays = list(var.get("overlays", []))
    inputs = [str(base_defaults)]
    for ov in overlays:
        ov_path = defaults_dir / ov
        ensure_file(ov_path, f"auto-created empty overlay: {ov}")
        inputs.append(str(ov_path))

    # combinar
    content = concat_files(inputs)
    out_path = out_dir / f"sdkconfig.{var['id']}.defaults"
    out_path.write_text(content, encoding="utf-8")

    # hint de target (si está en YAML)
    cache = out_dir / f"{var['id']}.cmakecache.cmake"
    target = var.get("target")
    cache.write_text((f"set(IDF_TARGET {target})\n" if target else "# no target in YAML\n"), encoding="utf-8")

    # 3) ENV de componentes (PORIS_ENABLE_<COMP>=1)
    env = out_dir / f"{var['id']}.env"
    lines = []
    for comp in var.get("components", []):
        key = "PORIS_ENABLE_" + "".join(ch if ch.isalnum() else "_" for ch in comp).upper()
        key = "_".join(filter(None, key.split("_")))
        lines.append(f"{key}=1")

    # extra_env opcional del YAML
    for k, v in (var.get("extra_env") or {}).items():
        lines.append(f"{k}={v}")

    env.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")

    # 4) componentes para CMake
    cmk = out_dir / f"{var['id']}.components.cmake"
    comp_list = var.get("components", [])

    cmklines = []
    # 4.1) lista para CMake y espejo en CACHE/ENV
    cmklines.append("set(PORIS_COMPONENTS " + " ".join(comp_list) + ")")
    # NEW: lista cacheada (útil si quieres leerla como var normal en CMake)
    cmklines.append('set(PORIS_COMPONENTS_LIST "' + ";".join(comp_list) + '" CACHE STRING "PORIS components" FORCE)')
    # NEW: ENV con comillas y ';' (evita el warning y mantiene la lista)
    cmklines.append('set(ENV{PORIS_COMPONENTS_LIST} "' + ";".join(comp_list) + '")')

    # 4.2) flags individuales ON en CACHE (clave para CLI sin sourcear env)
    for comp in comp_list:
        u = "".join(ch if ch.isalnum() else "_" for ch in comp).upper()
        cmklines.append(f'set(PORIS_ENABLE_{u} ON CACHE BOOL "Enable {u}" FORCE)')

    # 4.3) extra_env: exporta a entorno y refleja en var CMake (opcional)
    for k, v in (var.get("extra_env") or {}).items():
        cmklines.append(f'set(ENV{{{k}}} "{v}")')
        cmklines.append(f'set(PORIS_EXTRA_{k} "{v}")')

    # 4.4) escribir una sola vez
    cmk.write_text("\n".join(cmklines) + "\n", encoding="utf-8")

    print(str(out_path))

if __name__ == "__main__":
    main()
