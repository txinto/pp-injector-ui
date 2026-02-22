#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


def read_version(version_file: Path) -> str:
    version = version_file.read_text(encoding="utf-8").strip()
    if not version:
        raise ValueError(f"Version file is empty: {version_file}")
    if not re.fullmatch(r"[A-Za-z0-9._-]+", version):
        raise ValueError(f"Invalid version '{version}' in {version_file}")
    return version


def copy_file_preserve_rel(src: Path, rel_from: Path, dst_root: Path) -> None:
    rel = src.relative_to(rel_from)
    dst = dst_root / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def should_skip_artifact(src: Path, build_dir: Path) -> bool:
    rel = src.relative_to(build_dir)
    rel_str = str(rel)
    if "CMakeFiles/" in rel_str and "CMakeDetermineCompilerABI_" in rel.name:
        return True
    return False


def parse_flash_args_paths(args_file: Path) -> list[Path]:
    paths: list[Path] = []
    if not args_file.exists():
        return paths

    text = args_file.read_text(encoding="utf-8")
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        tokens = line.split()
        for tok in tokens:
            if tok.startswith("-"):
                continue
            if tok.startswith("0x"):
                continue
            if "$<" in tok:
                continue
            p = (args_file.parent / tok).resolve()
            if p.exists() and p.is_file():
                paths.append(p)
    return paths


def find_main_app_bin(build_dir: Path, flash_args: Path) -> Path:
    if flash_args.exists():
        lines = flash_args.read_text(encoding="utf-8").splitlines()
        for line in lines:
            m = re.match(r"^\s*0x10000\s+(.+\.bin)\s*$", line)
            if m:
                candidate = (build_dir / m.group(1)).resolve()
                if candidate.exists():
                    return candidate

    bins = sorted(build_dir.glob("*.bin"))
    if not bins:
        raise FileNotFoundError(f"No top-level .bin found in {build_dir}")
    return bins[0]


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Create PPInjector Elecrow release bundle from build artifacts"
    )
    ap.add_argument(
        "--build-dir",
        default="build_ppinjectorelecrow",
        help="Build folder with ESP-IDF artifacts",
    )
    ap.add_argument(
        "--releases-dir",
        default="releases",
        help="Base releases directory",
    )
    ap.add_argument(
        "--version-file",
        default="version.txt",
        help="File containing the firmware version",
    )
    ap.add_argument(
        "--product-name",
        default="ppinjectorelecrow",
        help="Release folder/artifact prefix",
    )
    args = ap.parse_args()

    repo_root = Path.cwd()
    build_dir = (repo_root / args.build_dir).resolve()
    releases_dir = (repo_root / args.releases_dir).resolve()
    version_file = (repo_root / args.version_file).resolve()

    if not build_dir.exists():
        raise FileNotFoundError(f"Build directory not found: {build_dir}")
    if not version_file.exists():
        raise FileNotFoundError(f"Version file not found: {version_file}")

    version = read_version(version_file)
    release_dir = releases_dir / f"{args.product_name}_{version}"
    if release_dir.exists():
        shutil.rmtree(release_dir)
    release_dir.mkdir(parents=True, exist_ok=True)

    copied: set[Path] = set()

    # 1) Copy all .bin/.elf/.map artifacts from build directory (recursive)
    for pattern in ("*.bin", "*.elf", "*.map"):
        for src in build_dir.rglob(pattern):
            if src.is_file() and not should_skip_artifact(src, build_dir):
                copy_file_preserve_rel(src, build_dir, release_dir)
                copied.add(src.resolve())

    # 2) Copy flash args files and referenced files
    args_candidates = [
        build_dir / "flash_args",
        build_dir / "encrypted_flash_args",
    ]
    args_candidates.extend(build_dir.glob("*encrypted*flash_args*"))

    unique_args_files = []
    seen = set()
    for p in args_candidates:
        rp = p.resolve()
        if p.exists() and p.is_file() and rp not in seen:
            unique_args_files.append(p)
            seen.add(rp)

    for args_file in unique_args_files:
        copy_file_preserve_rel(args_file, build_dir, release_dir)
        copied.add(args_file.resolve())

        for dep in parse_flash_args_paths(args_file):
            if dep not in copied:
                copy_file_preserve_rel(dep, build_dir, release_dir)
                copied.add(dep)

    # 3) Copy/rename application bin to ppinjectorelecrow_<version>.bin
    flash_args = build_dir / "flash_args"
    app_bin = find_main_app_bin(build_dir, flash_args)
    renamed_bin = release_dir / f"{args.product_name}_{version}.bin"
    shutil.copy2(app_bin, renamed_bin)

    print(f"Release dir: {release_dir}")
    print(f"Version: {version}")
    print(f"Main app bin: {app_bin.relative_to(build_dir)}")
    print(f"Renamed app bin: {renamed_bin.name}")
    print(f"Files copied: {len(copied) + 1}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
