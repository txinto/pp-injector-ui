#!/usr/bin/env python3
# poris/ensure_variant.py
import argparse, pathlib, re, sys, shutil

def slug_lower(s: str) -> str:
    return re.sub(r'[^a-z0-9]+', '', s.lower())

def main():
    ap = argparse.ArgumentParser(description="Copia buildcfg/<Var>.env a build_<var>/config.env")
    ap.add_argument("--variant", required=True)
    args = ap.parse_args()

    root = pathlib.Path(__file__).resolve().parent.parent
    var = args.variant
    suffix = slug_lower(var)
    src = root / "buildcfg" / f"{var}.env"
    dst_dir = root / f"build_{suffix}"
    dst_dir.mkdir(parents=True, exist_ok=True)
    dst = dst_dir / "config.env"

    if not src.exists():
        # si no hay env, deja un config.env vacío (no pasa nada)
        dst.write_text("", encoding="utf-8")
        print(f"(info) No hay {src.name}; creado vacío: {dst}")
        return

    shutil.copyfile(src, dst)
    print(f"ENV aplicado: {src} -> {dst}")

if __name__ == "__main__":
    main()
