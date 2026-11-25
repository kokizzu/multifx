#!/usr/bin/env python3
"""Safely fast-forward local main after a checkpoint was published by API."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HTTPS_REMOTE = "https://github.com/kokizzu/multifx.git"
EXPECTED_ORIGIN = "github.com/kokizzu/multifx"
SYNC_MAKE_BLOCK = (
    "\n.PHONY: sync-primary-main-plan sync-primary-main-apply\n"
    "sync-primary-main-plan:\n"
    "\t$(PYTHON) scripts/sync_published_main.py plan\n\n"
    "sync-primary-main-apply:\n"
    "\t$(PYTHON) scripts/sync_published_main.py apply\n"
)
OVERLAY_PATHS = {"make/publish.mk", "scripts/sync_published_main.py"}


def git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True, check=False
    )
    if check and result.returncode:
        detail = "\n".join(
            part.strip() for part in (result.stdout, result.stderr) if part.strip()
        )
        raise SystemExit(detail or f"git {' '.join(args)} failed ({result.returncode})")
    return result


def verify_origin_and_branch() -> tuple[str, str]:
    origin = git("remote", "get-url", "origin").stdout.strip()
    normalized = origin.lower().removesuffix(".git")
    if EXPECTED_ORIGIN not in normalized:
        raise SystemExit(f"origin does not identify kokizzu/multifx: {origin}")
    branch = git("branch", "--show-current").stdout.strip()
    if branch != "main":
        raise SystemExit(f"current branch is {branch or 'detached HEAD'}, expected main")
    upstream = git(
        "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"
    ).stdout.strip()
    if upstream != "origin/main":
        raise SystemExit(f"tracked upstream is {upstream or 'none'}, expected origin/main")
    symref = git("ls-remote", "--symref", HTTPS_REMOTE, "HEAD").stdout
    default_ref = next(
        (line.split()[1] for line in symref.splitlines() if line.startswith("ref:")), ""
    )
    if default_ref != "refs/heads/main":
        raise SystemExit(f"verified HTTPS default branch is {default_ref or 'unknown'}")
    return origin, branch


def fetch_main() -> str:
    git("fetch", "--no-tags", HTTPS_REMOTE, "main")
    return git("rev-parse", "FETCH_HEAD").stdout.strip()


def changed_paths(remote: str) -> list[str]:
    head = git("rev-parse", "HEAD").stdout.strip()
    if git("merge-base", "--is-ancestor", head, remote, check=False).returncode:
        raise SystemExit("remote main is not a fast-forward from local HEAD")
    paths = git("diff", "--name-only", head, remote).stdout.splitlines()
    if not paths:
        return []
    workflow_paths = [path for path in paths if path.startswith(".github/workflows/")]
    if workflow_paths:
        raise SystemExit("refusing to synchronize workflow changes: " + ", ".join(workflow_paths))
    return paths


def validate_overlay(remote: str, paths: list[str]) -> set[str]:
    if "make/publish.mk" not in paths:
        raise SystemExit("remote checkpoint does not include the expected publish makefile")
    published_make = git("show", f"{remote}:make/publish.mk").stdout
    local_make = (ROOT / "make/publish.mk").read_text(encoding="utf-8")
    overlay: set[str] = set()
    if local_make == published_make:
        pass
    elif local_make == published_make + SYNC_MAKE_BLOCK:
        overlay.add("make/publish.mk")
    else:
        raise SystemExit("make/publish.mk has extra or conflicting edits; preserving checkout")
    local_helper = ROOT / "scripts/sync_published_main.py"
    if not local_helper.is_file():
        raise SystemExit("sync helper file is missing")
    if git("ls-tree", remote, "--", "scripts/sync_published_main.py").stdout:
        published_helper = git("show", f"{remote}:scripts/sync_published_main.py").stdout
        if local_helper.read_text(encoding="utf-8") != published_helper:
            raise SystemExit("sync helper differs from fetched main; preserving checkout")
    else:
        overlay.add("scripts/sync_published_main.py")

    other_paths = [path for path in paths if path != "make/publish.mk"]
    if other_paths:
        # For all published checkpoint files, the working tree must exactly
        # match the remote commit before advancing the local branch pointer.
        diff = git("diff", "--quiet", remote, "--", *other_paths, check=False)
        if diff.returncode:
            raise SystemExit(
                "working-tree content differs from published main in: "
                + ", ".join(
                    git("diff", "--name-only", remote, "--", *other_paths).stdout.splitlines()
                )
            )
    return overlay


def staged_paths() -> set[str]:
    return set(git("diff", "--cached", "--name-only").stdout.splitlines())


def report(remote: str, paths: list[str]) -> None:
    head = git("rev-parse", "HEAD").stdout.strip()
    print(f"Repository: {EXPECTED_ORIGIN}")
    print(f"Branch: main; local HEAD: {head}")
    print(f"Fetched main: {remote}")
    print("Fast-forward paths:")
    for path in paths:
        print("  " + path)
    status = git("status", "--porcelain=v1", "--untracked-files=all").stdout
    deferred = [
        line[3:]
        for line in status.splitlines()
        if line[3:] not in set(paths) | OVERLAY_PATHS
    ]
    if deferred:
        print("Preserved outside paths:")
        for path in deferred:
            print("  " + path)
    print("The plan stages only fetched main paths and the verified helper overlay.")


def synchronize() -> None:
    origin, _ = verify_origin_and_branch()
    remote = fetch_main()
    paths = changed_paths(remote)
    if not paths:
        raise SystemExit("local main already matches fetched main")
    overlay = validate_overlay(remote, paths)

    unexpected = sorted(staged_paths() - set(paths) - OVERLAY_PATHS)
    if unexpected:
        raise SystemExit(
            "refusing to move main with staged work outside the fetched checkpoint: "
            + ", ".join(unexpected)
        )
    if any(path.startswith(".github/workflows/") for path in paths):
        raise SystemExit("workflow files are never synchronized by this helper")

    head = git("rev-parse", "HEAD").stdout.strip()
    print(f"Verified origin: {origin}")
    report(remote, paths)
    if args.mode == "plan":
        return

    # The fetched checkpoint bytes are already present locally. Stage only
    # those exact paths plus this reviewed helper, then prove the only delta
    # from fetched main is the helper overlay before moving refs/heads/main.
    git("add", "--", *paths, *sorted(overlay))
    staged_diff = set(git("diff", "--cached", "--name-only", remote).stdout.splitlines())
    if staged_diff != overlay:
        raise SystemExit(
            "staged index differs from fetched main outside the helper overlay: "
            + ", ".join(sorted(staged_diff ^ overlay))
        )

    git(
        "fetch",
        "--no-tags",
        HTTPS_REMOTE,
        "main:refs/remotes/origin/main",
    )
    tracking = git("rev-parse", "refs/remotes/origin/main").stdout.strip()
    if tracking != remote:
        raise SystemExit("origin/main changed while synchronizing; refusing local ref update")
    # Compare-and-swap only advances main from its verified ancestor. The
    # index/worktree stay intact and contain just the reviewed helper edits.
    git("update-ref", "refs/heads/main", remote, head)
    if git("rev-parse", "HEAD").stdout.strip() != remote:
        raise SystemExit("local main fast-forward verification failed")
    print(f"Fast-forwarded local main to {remote}; unrelated work remains untouched.")


def main() -> None:
    global args
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "apply"))
    args = parser.parse_args()
    synchronize()


if __name__ == "__main__":
    main()
