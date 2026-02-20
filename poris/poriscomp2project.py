#!/usr/bin/env python3
"""
Copy poris/components into poris_components in the project root.

Use this when you want to vendor the submodule components into the project tree
for distribution or local tweaks without relying on the submodule at build time.

Precedence at build:
  components/ > poris_components/ > poris/components (submodule)
So leaving poris_components empty will use the submodule; running this script
materializes copies into poris_components.
"""

import argparse
import shutil
from pathlib import Path
import sys


def copy_components(src: Path, dst: Path):
    if not src.exists():
        raise FileNotFoundError(f"Source components directory not found: {src}")
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        src_item = item
        dst_item = dst / item.name
        if src_item.is_dir():
            shutil.copytree(src_item, dst_item, dirs_exist_ok=True)
        else:
            shutil.copy2(src_item, dst_item)


def main():
    ap = argparse.ArgumentParser(description="Copy poris/components into poris_components at project root.")
    ap.add_argument("--force", action="store_true", help="Deprecated; kept for compatibility (overwrite is default).")
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    src = repo_root / "poris" / "components"
    dst = repo_root / "poris_components"

    try:
        copy_components(src, dst)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Copied {src} -> {dst}")


if __name__ == "__main__":
    main()
