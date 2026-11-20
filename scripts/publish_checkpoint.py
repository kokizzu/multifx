#!/usr/bin/env python3
"""Safely publish the reviewed DSP and Windows-backend checkpoint to upstream main."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
COMMIT_MESSAGE = "feat: add volume pedal and peak limiter"
CHECKPOINT_PATHS = (
    ".gitignore",
    "ARCHITECTURE.md",
    "AUDIO_COMPATIBILITY.md",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "GOAL.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "Makefile",
    "PRESET_FORMAT.md",
    "README.md",
    "RESEARCH.md",
    "THIRD_PARTY_NOTICES.md",
    "apps/offline_render.cpp",
    "cmake/toolchains/mingw-w64.cmake",
    "include/tabletfx/effect.hpp",
    "include/tabletfx/effects.hpp",
    "tests/test_gain.cpp",
    "tests/test_peak_limiter.cpp",
    "tests/test_volume_pedal.cpp",
    "include/tabletfx/effects/filters.hpp",
    "include/tabletfx/effects/pedal_compressor.hpp",
    "include/tabletfx/audio/wasapi_backend.hpp",
    "include/tabletfx/effects/compressor.hpp",
    "include/tabletfx/effects/three_band_eq.hpp",
    "include/tabletfx/effects/peak_limiter.hpp",
    "include/tabletfx/effects/volume_pedal.hpp",
    "include/tabletfx/wav.hpp",
    "scripts/audio_fixtures.py",
    "scripts/audio_regression.py",
    "scripts/bootstrap_docs.py",
    "scripts/check_repo_diff.py",
    "scripts/doctor.py",
    "scripts/publish_checkpoint.py",
    "scripts/repo_status.py",
    "scripts/run_cmake.py",
    "scripts/test_audio_fixtures.py",
    "scripts/test_offline_cli.py",
    "scripts/validate_docs.py",
    "scripts/web.py",
    "src/effects.cpp",
    "src/effects/filters.cpp",
    "src/effects/pedal_compressor.cpp",
    "src/effects/peak_limiter.cpp",
    "src/effects/volume_pedal.cpp",
    "src/audio/wasapi_backend.cpp",
    "src/web/wasm_chain.cpp",
    "src/wav.cpp",
    "tests/test_dsp.cpp",
    "tests/test_compressor.cpp",
    "tests/test_three_band_eq.cpp",
    "tests/test_filters.cpp",
    "tests/test_pedal_compressor.cpp",
    "tests/wasapi_compile.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "web/wav_decoder.js",
    "web/worklet-smoke.js",
)


def git(*args: str, capture: bool = True) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=capture, text=True, check=False
    )
    if result.returncode != 0:
        detail = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        raise SystemExit(detail or f"git {' '.join(args)} failed with status {result.returncode}")
    return result.stdout if capture else ""


def inspect() -> tuple[list[str], str, str]:
    branch = git("branch", "--show-current").strip()
    remote_url = git("remote", "get-url", "origin").strip()
    normalized = remote_url.lower().removesuffix(".git")
    if "github.com/kokizzu/multifx" not in normalized:
        raise SystemExit(f"origin does not identify kokizzu/multifx: {remote_url}")
    default_info = git("ls-remote", "--symref", "origin", "HEAD")
    default_ref = next((line.split()[1] for line in default_info.splitlines() if line.startswith("ref:")), "")
    if default_ref != "refs/heads/main":
        raise SystemExit(f"Upstream default branch is {default_ref or 'unknown'}, expected refs/heads/main")
    if branch != "main":
        raise SystemExit(f"Current branch is {branch or 'detached HEAD'}, expected main")
    upstream = git("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}").strip()
    if upstream != "origin/main":
        raise SystemExit(f"Tracked upstream is {upstream or 'none'}, expected origin/main")

    remote_main = next(
        (line.split()[0] for line in git("ls-remote", "origin", "refs/heads/main").splitlines() if line.strip()),
        "",
    )
    head = git("rev-parse", "HEAD").strip()
    if not remote_main or head != remote_main:
        raise SystemExit("Local main is not exactly at the current upstream main; synchronize safely before publishing")

    status = git("status", "--porcelain=v1", "--untracked-files=all")
    paths: list[str] = []
    staged: list[str] = []
    for line in status.splitlines():
        code = line[:2]
        path = line[3:]
        if path.startswith('"'):
            raise SystemExit(f"Unusual quoted path needs manual review: {line}")
        paths.append(path)
        if code[0] not in (" ", "?"):
            staged.append(path)
    unexpected_staged = sorted(set(staged) - set(CHECKPOINT_PATHS))
    if unexpected_staged:
        raise SystemExit(
            "Refusing to publish with staged changes outside the reviewed checkpoint: "
            + ", ".join(unexpected_staged)
        )
    unexpected = sorted(set(paths) - set(CHECKPOINT_PATHS))
    if unexpected:
        raise SystemExit("Dirty files outside the reviewed checkpoint: " + ", ".join(unexpected))
    workflows = [path for path in paths if path.startswith(".github/workflows/")]
    if workflows:
        raise SystemExit("Refusing to publish workflow changes: " + ", ".join(workflows))
    if not paths:
        raise SystemExit("No changed checkpoint files to publish")
    return sorted(set(paths)), branch, remote_url


def plan() -> list[str]:
    paths, branch, remote_url = inspect()
    git("diff", "--check")
    git("diff", "--cached", "--check")
    print(f"Repository: {remote_url}")
    print(f"Branch: {branch} (verified upstream default; local HEAD matches origin/main)")
    print("Commit message: " + COMMIT_MESSAGE)
    print("Exact changed paths:")
    for path in paths:
        print("  " + path)
    stat = git("diff", "--stat")
    print(stat.rstrip() or "Untracked files are listed above; no tracked diff stat yet.")
    print("No workflow files are included. Only the paths above will be staged.")
    return paths


def publish() -> None:
    paths, _, _ = inspect()
    git("add", "--", *paths, capture=False)
    planned_paths = plan()
    if planned_paths != paths:
        raise SystemExit("Checkpoint paths changed while preparing publication; stopping before commit")
    git("diff", "--cached", "--check")
    print("Staged diff:")
    print(git("diff", "--cached", "--stat").rstrip())
    staged = set(git("diff", "--cached", "--name-only").splitlines())
    if staged != set(paths):
        raise SystemExit("Staged paths differ from reviewed checkpoint; stopping before commit")
    git("commit", "-m", COMMIT_MESSAGE, capture=False)
    commit = git("rev-parse", "HEAD").strip()
    git("push", "origin", "main", capture=False)
    remote_main = next(
        (line.split()[0] for line in git("ls-remote", "origin", "refs/heads/main").splitlines() if line.strip()),
        "",
    )
    if remote_main != commit:
        raise SystemExit(f"Push verification failed: local {commit}, remote {remote_main or 'missing'}")
    print(f"Published and verified commit {commit} on origin/main")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "publish"))
    mode = parser.parse_args().mode
    if mode == "plan":
        plan()
    else:
        publish()


if __name__ == "__main__":
    main()
