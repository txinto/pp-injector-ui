#!/usr/bin/env python3
# poris/gen_configs.py
import argparse, json, pathlib, sys, re, os
try:
    import yaml
except ModuleNotFoundError:
    sys.stderr.write(
        "ERROR: PyYAML no disponible. Activa el entorno de ESP-IDF "
        "(. $IDF_PATH/export.sh) o instala pyyaml en tu Python.\n"
    )
    sys.exit(2)

def slug_lower(s: str) -> str:
    # "S3WifiLCD" -> "s3wifilcd"  (sin espacios, guiones, etc.)
    return re.sub(r'[^a-z0-9]+', '', s.lower())

def discover_components(root: pathlib.Path):
    comps_dir = root / "components"
    comps = []
    if comps_dir.exists():
        for entry in comps_dir.iterdir():
            if entry.is_dir() and not entry.name.startswith("."):
                comps.append(entry.name)
    return sorted(comps)

def main():
    ap = argparse.ArgumentParser(description="Genera esp_idf_project_configuration.json desde variants.yml")
    ap.add_argument("--variants-yml", default="variants/variants.yml")
    ap.add_argument("--out", default="esp_idf_project_configuration.json",
                    help="Ruta del JSON a generar")
    ap.add_argument("--project", default="poris_base",
                    help="Sólo informativo; no se usa dentro del JSON salvo que quieras extenderlo")
    args = ap.parse_args()

    root = pathlib.Path(__file__).resolve().parent.parent
    variants_yml = (root / args.variants_yml).resolve()
    if not variants_yml.exists():
        comps = discover_components(root)
        auto_cfg = {
            "defaults_dir": "sdkcfg",
            "out_dir": "buildcfg",
            "variants": [
                {
                    "id": "main",
                    "target": "",
                    "overlays": [],
                    "components": comps,
                    "extra_env": {}
                }
            ]
        }
        variants_yml.parent.mkdir(parents=True, exist_ok=True)
        variants_yml.write_text(yaml.safe_dump(auto_cfg, sort_keys=False, allow_unicode=True), encoding="utf-8")
        print(f"Created default variants file at {variants_yml} with variant 'main' and components: {comps}")

    cfg = yaml.safe_load(variants_yml.read_text(encoding="utf-8"))
    variants = cfg.get("variants", [])
    if not variants:
        comps = discover_components(root)
        cfg["variants"] = [
            {
                "id": "main",
                "target": "",
                "overlays": [],
                "components": comps,
                "extra_env": {}
            }
        ]
        variants = cfg["variants"]
        variants_yml.write_text(yaml.safe_dump(cfg, sort_keys=False, allow_unicode=True), encoding="utf-8")
        print(f"No variants found; wrote default 'main' with components: {comps}")

    out_obj = {}
    for v in variants:
        vid = v.get("id")
        if not vid:
            print("Entrada de variante sin 'id' en YAML; la ignoro.", file=sys.stderr)
            continue

        suffix = slug_lower(vid)
        build_dir = f"./build_{suffix}"
        # El defaults combinado que generamos con gen_variant.py:
        combined_defaults = f"buildcfg/sdkconfig.{vid}.defaults"
        # El fichero 'sdkconfig' efectivo (lo forzamos al directorio de build)
        sdkconfig_path = f"{build_dir}/sdkconfig"
        idf_target = v.get("target", "")

        env_map = {
            "VARIANT": vid,
            "VAR_SUFFIX": suffix,
        }
        out_obj[vid] = {
            "build": {
                "compileArgs": [],
                "ninjaArgs": [],
                "buildDirectoryPath": build_dir,
                "sdkconfigDefaults": [ combined_defaults ],
                "sdkconfigFilePath": sdkconfig_path
            },
            "env": env_map,
            "idfTarget": idf_target,
            "flashBaudRate": "",
            "monitorBaudRate": "",
            "openOCD": {
                "debugLevel": 0,
                "configs": [],
                "args": []
            },
            "tasks": {
            "preBuild": f"bash poris/prebuild.sh {vid}",
            "preFlash": "",
            "postBuild": "",
            "postFlash": ""
            }
        }

    out_path = (root / args.out).resolve()
    out_path.write_text(json.dumps(out_obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(str(out_path))

if __name__ == "__main__":
    main()
