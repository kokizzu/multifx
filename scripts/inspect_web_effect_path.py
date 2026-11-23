#!/usr/bin/env python3
"""Print bounded UI and Worklet examples for effect-slot integration."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SECTIONS = (
    ("web/app.js", (r"Peak limiter", r"output-level", r"volume-pan-position", r"EFFECT_NAMES =", r"EFFECT_CATEGORIES", r"effectCategories", r"dataset.category", r"effect-category-tag", r"categoryFor", r'category: "filter"', r'category: "utility"', r"updateVolumePanOptions", r"runVolumePanBrowserSmokeTest"), 4, 18),
    ("web/audio-worklet.js", (r"const EFFECTS", r"output-level", r"peak-limiter", r"configureVolumePan", r'message.type === "volume-pan"'), 5, 20),
    ("web/index.html", (r'data-effect="93"', r'data-effect="95"', r"Peak limiter", r"Volume/Pan controls", r"volume-pan-position"), 5, 24),
    ("scripts/web.py", (r"stereo-crossover", r"volume-pan", r"EFFECT_NAMES"), 5, 20),
    ("scripts/inspect_effect_picker.py", (r"category", r"effect-row", r"data-effect", r"EFFECT_NAMES"), 4, 12),
)


def print_section(relative: str, patterns: tuple[str, ...], before: int, after: int) -> None:
    path = ROOT / relative
    lines = path.read_text(encoding="utf-8").splitlines()
    matches = [
        index
        for index, line in enumerate(lines)
        if any(re.search(pattern, line, re.IGNORECASE) for pattern in patterns)
    ]
    windows: list[tuple[int, int]] = []
    for index in matches:
        start = max(0, index - before)
        end = min(len(lines), index + after + 1)
        if windows and start <= windows[-1][1]:
            windows[-1] = (windows[-1][0], max(windows[-1][1], end))
        else:
            windows.append((start, end))

    print(f"===== {relative} ({len(matches)} matching lines) =====")
    if not windows:
        print("(no matching lines)")
        return
    total = 0
    for start, end in windows:
        for index in range(start, end):
            print(f"{index + 1:5}: {lines[index]}")
            total += 1
            if total >= 260:
                print("... truncated after 260 context lines")
                return
        print("...")


def main() -> None:
    for relative, patterns, before, after in SECTIONS:
        print_section(relative, patterns, before, after)


if __name__ == "__main__":
    main()
