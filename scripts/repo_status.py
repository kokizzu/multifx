#!/usr/bin/env python3
"""Inspect repository identity, branch, and worktree before edits/publication."""

from __future__ import annotations

import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(*args: str) -> str:
    result = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=False)
    if result.returncode:
        return (result.stderr or result.stdout).strip() or f"exit {result.returncode}"
    return result.stdout.strip()


def main() -> None:
    print(f"Repository root: {ROOT}")
    print(f"Branch: {run('branch', '--show-current')}")
    print(f"Origin: {run('remote', 'get-url', 'origin')}")
    head = run("rev-parse", "--verify", "HEAD")
    print(f"HEAD: {head if len(head) == 40 else 'no commit yet (' + head + ')'}")
    print("Worktree:")
    print(run("status", "--short", "--branch"))
    remote_head = run("ls-remote", "--symref", "origin", "HEAD")
    print("Remote HEAD:")
    print(remote_head or "No advertised refs (empty repository or unavailable remote)")


if __name__ == "__main__":
    main()
