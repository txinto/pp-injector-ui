#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))


def run(cmd: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd) if cwd else None, text=True, capture_output=True)


def ensure_ok(result: subprocess.CompletedProcess[str], what: str) -> None:
    if result.returncode != 0:
        sys.stderr.write(f"ERROR: {what} failed (exit {result.returncode})\n")
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise SystemExit(1)


def is_pristine(repo: Path) -> bool:
    r = run(["git", "status", "--porcelain"], cwd=repo)
    ensure_ok(r, "git status")
    return r.stdout.strip() == ""


def repo_root_for_path(path: Path) -> Path:
    r = run(["git", "-C", str(path), "rev-parse", "--show-toplevel"])
    ensure_ok(r, "git rev-parse --show-toplevel")
    return Path(r.stdout.strip())


def load_default_public_repo() -> str:
    try:
        import config_secrets  # type: ignore
    except ModuleNotFoundError:
        sys.stderr.write(
            "ERROR: Missing scripts/config_secrets.py\n"
            "Copy scripts/config_secrets.py.example to scripts/config_secrets.py and set PUBLIC_REPO.\n"
        )
        raise SystemExit(1)

    value = getattr(config_secrets, "PUBLIC_REPO", "").strip()
    if not value:
        sys.stderr.write(
            "ERROR: scripts/config_secrets.py must define PUBLIC_REPO with your local path.\n"
        )
        raise SystemExit(1)
    return value


def main() -> int:
    ap = argparse.ArgumentParser(description="Publish PPInjector OTA binary to public firmware repo")
    ap.add_argument(
        "release_dir",
        nargs="?",
        default="releases/ppinjectorelecrow_0.0.1.3-ota",
        help="Release folder like releases/ppinjectorelecrow_<version>",
    )
    ap.add_argument(
        "--src-bin",
        default="pp-injector-ui.bin",
        help="Binary file name inside release_dir",
    )
    ap.add_argument(
        "--public-repo",
        default=None,
        help="Override public git repository (optional, defaults to scripts/config_secrets.py: PUBLIC_REPO)",
    )
    ap.add_argument(
        "--dest-rel",
        default="firmware/esp32s3_ppinjectorui_elec.bin",
        help="Destination path relative to public repo",
    )
    args = ap.parse_args()

    cwd = Path.cwd()
    release_dir = (cwd / args.release_dir).resolve()
    src_bin = release_dir / args.src_bin

    public_repo_raw = args.public_repo or load_default_public_repo()
    public_repo = Path(public_repo_raw).resolve()
    dest_file = public_repo / args.dest_rel

    if not release_dir.is_dir():
        sys.stderr.write(f"ERROR: release dir not found: {release_dir}\n")
        return 1
    if not src_bin.is_file():
        sys.stderr.write(f"ERROR: source binary not found: {src_bin}\n")
        return 1
    if not public_repo.is_dir():
        sys.stderr.write(f"ERROR: public repo not found: {public_repo}\n")
        return 1

    git_root = repo_root_for_path(public_repo)
    if git_root != public_repo:
        sys.stderr.write(f"ERROR: public repo is not git root ({public_repo} -> {git_root})\n")
        return 1

    if not is_pristine(public_repo):
        sys.stderr.write("ERROR: public repo is not pristine. Aborting publish.\n")
        return 1

    dest_file.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src_bin, dest_file)

    add = run(["git", "add", args.dest_rel], cwd=public_repo)
    ensure_ok(add, "git add")

    staged = run(["git", "diff", "--cached", "--name-only"], cwd=public_repo)
    ensure_ok(staged, "git diff --cached")
    if staged.stdout.strip() == "":
        print("No changes to publish (destination already up to date).")
        return 0

    release_name = release_dir.name
    commit_msg = f"Uploading  {release_name}"

    commit = run(["git", "commit", "-m", commit_msg], cwd=public_repo)
    ensure_ok(commit, "git commit")

    pull = run(["git", "pull", "--rebase"], cwd=public_repo)
    ensure_ok(pull, "git pull --rebase")

    push = run(["git", "push"], cwd=public_repo)
    ensure_ok(push, "git push")

    print(f"Published: {src_bin} -> {dest_file}")
    print(f"Commit: {commit_msg}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
