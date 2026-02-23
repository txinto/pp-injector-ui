#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path


def is_likely_artifact_path_token(tok: str) -> bool:
    lower = tok.lower()
    artifact_exts = (".bin", ".elf", ".map", ".csv", ".txt")
    return "/" in tok or lower.endswith(artifact_exts)


def read_version(version_file: Path) -> str:
    version = version_file.read_text(encoding="utf-8").strip()
    if not version:
        raise ValueError(f"Version file is empty: {version_file}")
    if not re.fullmatch(r"[A-Za-z0-9._-]+", version):
        raise ValueError(f"Invalid version '{version}' in {version_file}")
    return version


def read_compiled_version(project_description_file: Path) -> str:
    if not project_description_file.exists():
        raise FileNotFoundError(f"Build metadata file not found: {project_description_file}")

    try:
        data = json.loads(project_description_file.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"Invalid JSON in {project_description_file}: {exc}") from exc

    compiled_version = str(data.get("project_version", "")).strip()
    if not compiled_version:
        raise ValueError(
            f"Missing or empty 'project_version' in {project_description_file}"
        )
    return compiled_version


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
            if not is_likely_artifact_path_token(tok):
                continue
            p = (args_file.parent / tok).resolve()
            if p.exists() and p.is_file():
                paths.append(p)
    return paths


def parse_flash_args_referenced_files(args_file: Path) -> tuple[list[Path], list[Path]]:
    """Return (existing_files, missing_files) referenced from flash args."""
    existing: list[Path] = []
    missing: list[Path] = []
    if not args_file.exists():
        return existing, missing

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
            if not is_likely_artifact_path_token(tok):
                continue
            p = (args_file.parent / tok).resolve()
            if p.exists() and p.is_file():
                existing.append(p)
            else:
                missing.append(p)
    return existing, missing


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


def print_variant_not_built_or_incomplete(build_dir: Path, missing: list[str] | None = None) -> None:
    if missing:
        missing_lines = "\n".join(f"  - {m}" for m in missing)
        state_msg = "ERROR: PPInjectorElecrow build is incomplete."
        missing_msg = f"Missing/invalid artifacts:\n{missing_lines}\n"
    else:
        state_msg = "ERROR: PPInjectorElecrow variant is not compiled yet."
        missing_msg = ""

    msg = (
        f"{state_msg}\n"
        f"Expected build artifacts in: {build_dir}\n"
        f"{missing_msg}"
        "Compile first, then retry release packaging.\n"
        "Suggested command in an ESP-IDF environment:\n"
        "  idf.py -B build_ppinjectorelecrow -DVARIANT=PPInjectorElecrow -DSDKCONFIG=build_ppinjectorelecrow/sdkconfig build\n"
    )
    sys.stderr.write(msg)


def print_version_mismatch(version_file: Path, expected: str, compiled: str, build_dir: Path) -> None:
    msg = (
        "ERROR: Build artifacts version does not match version.txt.\n"
        f"version.txt ({version_file.name}): {expected}\n"
        f"Compiled version (project_description.json): {compiled}\n"
        f"Build directory checked: {build_dir}\n"
        "Rebuild with the target version, then retry release packaging.\n"
        "Suggested command in an ESP-IDF environment:\n"
        "  idf.py -B build_ppinjectorelecrow -DVARIANT=PPInjectorElecrow -DSDKCONFIG=build_ppinjectorelecrow/sdkconfig build\n"
    )
    sys.stderr.write(msg)


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
        print_variant_not_built_or_incomplete(build_dir)
        return 1
    if not version_file.exists():
        raise FileNotFoundError(f"Version file not found: {version_file}")

    missing: list[str] = []

    flash_args = build_dir / "flash_args"
    if not flash_args.exists():
        missing.append("flash_args")
    else:
        _, missing_from_args = parse_flash_args_referenced_files(flash_args)
        for p in missing_from_args:
            try:
                missing.append(str(p.relative_to(build_dir)))
            except ValueError:
                missing.append(str(p))

    # Ensure core artifact kinds exist before creating release directory.
    for pattern in ("*.bin", "*.elf", "*.map"):
        if not any(build_dir.rglob(pattern)):
            missing.append(f"no files matching {pattern}")

    try:
        _ = find_main_app_bin(build_dir, flash_args)
    except FileNotFoundError:
        missing.append("top-level application .bin (from flash_args 0x10000 or fallback)")

    if missing:
        print_variant_not_built_or_incomplete(build_dir, missing)
        return 1

    version = read_version(version_file)
    project_description = build_dir / "project_description.json"
    try:
        compiled_version = read_compiled_version(project_description)
    except (FileNotFoundError, ValueError) as exc:
        print_variant_not_built_or_incomplete(build_dir, [str(exc)])
        return 1

    if compiled_version != version:
        print_version_mismatch(version_file, version, compiled_version, build_dir)
        return 1

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
    try:
        app_bin = find_main_app_bin(build_dir, flash_args)
    except FileNotFoundError:
        print_variant_not_built_or_incomplete(
            build_dir,
            ["top-level application .bin (from flash_args 0x10000 or fallback)"],
        )
        return 1

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
