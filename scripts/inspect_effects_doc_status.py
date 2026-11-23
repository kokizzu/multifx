#!/usr/bin/env python3
"""Print filter-related checklist and matrix rows for safe doc synchronization."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = (
    "IMPLEMENTATION_CHECKLIST.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "EFFECTS_MATRIX.md",
)
NEEDLES = (
    "| **Filter** |",
    "stereo-filter",
    "Stereo Filter",
    "Fractal FM9 Mark II Turbo — Pitch/Synth/Tremolo",
    "Next implement the project-authored Fractal FM9-family static Volume/Pan",
)


def main() -> int:
    for name in DOCS:
        path = ROOT / name
        print(f"## {name}")
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if any(needle.casefold() in line.casefold() for needle in NEEDLES):
                print(f"{line_number}: {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
