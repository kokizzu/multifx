#!/usr/bin/env python3
"""Run non-mutating Git whitespace and scope checks for the current checkpoint."""

from __future__ import annotations

from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        detail = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        raise SystemExit(detail or f"git {' '.join(args)} failed")
    return result.stdout


def main() -> None:
    git("diff", "--check")
    git("diff", "--cached", "--check")
    status = git("status", "--short", "--untracked-files=all")
    changed = [line[3:] for line in status.splitlines() if len(line) >= 4]
    workflows = [path for path in changed if path.startswith(".github/workflows/")]
    if workflows:
        raise SystemExit("Workflow files changed: " + ", ".join(workflows))
    print(status.rstrip() or "Worktree is clean.")
    print(git("diff", "--stat").rstrip() or "No tracked unstaged diff.")
    print("Working and staged diffs pass git diff --check; no workflow changes detected.")


if __name__ == "__main__":
    main()
