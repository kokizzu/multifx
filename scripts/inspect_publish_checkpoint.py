#!/usr/bin/env python3
"""Print bounded publisher checkpoint registration and target excerpts."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAKE_PATH = ROOT / "make" / "publish.mk"
PUBLISHER_PATH = ROOT / "scripts" / "publish_checkpoint.py"


def print_block(path: Path, label: str, lines: list[str], start: int, end: int) -> None:
    print(f"===== {path.relative_to(ROOT)} · {label} · lines {start + 1}-{end} =====")
    for index in range(start, end):
        print(f"{index + 1:4}: {lines[index]}")


def find_line(lines: list[str], needle: str) -> int:
    for index, line in enumerate(lines):
        if needle in line:
            return index
    raise SystemExit(f"Missing expected publisher anchor: {needle}")


def balanced_block_end(lines: list[str], start: int, opening: str, closing: str) -> int:
    depth = 0
    for index in range(start, len(lines)):
        depth += lines[index].count(opening) - lines[index].count(closing)
        if depth == 0:
            return index + 1
    raise SystemExit(f"Unclosed publisher block at {start + 1}")


def main() -> None:
    make_lines = MAKE_PATH.read_text(encoding="utf-8").splitlines()
    publisher_lines = PUBLISHER_PATH.read_text(encoding="utf-8").splitlines()

    start = find_line(make_lines, "PUBLISH_CHECKPOINTS :=")
    end = start + 1
    while end < len(make_lines) and make_lines[end - 1].rstrip().endswith("\\"):
        end += 1
    print_block(MAKE_PATH, "checkpoint list", make_lines, start, end)
    for index, line in enumerate(make_lines):
        if line.startswith("PUBLISH_CHECKPOINTS +="):
            print_block(MAKE_PATH, "checkpoint registration", make_lines, index, index + 1)

    start = find_line(make_lines, "define publish-checkpoint-rules")
    end = find_line(make_lines, "$(foreach checkpoint,$(PUBLISH_CHECKPOINTS),$(eval $(call publish-checkpoint-rules,$(checkpoint))))") + 1
    print_block(MAKE_PATH, "dynamic plan and publish targets", make_lines, start, end)

    start = find_line(publisher_lines, "VOLUME_PAN_CHECKPOINT_PATHS = (")
    end = balanced_block_end(publisher_lines, start, "(", ")")
    print_block(PUBLISHER_PATH, "volume-pan path tuple", publisher_lines, start, end)

    start = find_line(publisher_lines, '"volume-pan": (')
    end = balanced_block_end(publisher_lines, start, "(", ")")
    print_block(PUBLISHER_PATH, "volume-pan checkpoint registration", publisher_lines, start, end)

    start = find_line(publisher_lines, "STEREO_FILTER_CHECKPOINT_PATHS = (")
    end = balanced_block_end(publisher_lines, start, "(", ")")
    print_block(PUBLISHER_PATH, "stereo-filter path tuple", publisher_lines, start, end)

    start = find_line(publisher_lines, '"stereo-filter": (')
    end = balanced_block_end(publisher_lines, start, "(", ")")
    print_block(PUBLISHER_PATH, "stereo-filter checkpoint registration", publisher_lines, start, end)


if __name__ == "__main__":
    main()
