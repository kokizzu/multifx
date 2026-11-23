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
if __name__ == "__main__":
    from pathlib import Path as _Path

    _app_path = _Path(__file__).resolve().parents[1] / "web" / "app.js"
    _app_lines = _app_path.read_text(encoding="utf-8").splitlines()
    _needle = "The effect picker must list 96 ordered, categorized effects"
    _matches = [index for index, line in enumerate(_app_lines) if _needle in line]
    print(f"===== web/app.js picker-count assertion ({len(_matches)} matches) =====")
    for _match in _matches:
        _start = max(0, _match - 120)
        _end = min(len(_app_lines), _match + 3)
        for _index in range(_start, _end):
            print(f"{_index + 1:4}: {_app_lines[_index]}")
    _vocal_needle = "Vocal FX picker mismatch"
    _vocal_matches = [index for index, line in enumerate(_app_lines)
                      if _vocal_needle in line]
    print(f"===== web/app.js Vocal FX count assertion ({len(_vocal_matches)} matches) =====")
    for _match in _vocal_matches:
        _start = max(0, _match - 20)
        _end = min(len(_app_lines), _match + 3)
        for _index in range(_start, _end):
            print(f"{_index + 1:4}: {_app_lines[_index]}")
    _category_needle = "Filtering by effect category changed chain state"
    _category_matches = [index for index, line in enumerate(_app_lines)
                         if _category_needle in line]
    print(f"===== web/app.js category-filter assertion ({len(_category_matches)} matches) =====")
    for _match in _category_matches:
        _start = max(0, _match - 28)
        _end = min(len(_app_lines), _match + 3)
        for _index in range(_start, _end):
            print(f"{_index + 1:4}: {_app_lines[_index]}")
    _dynamics_needle = "Dynamics filter must include Auto Swell"
    _dynamics_matches = [index for index, line in enumerate(_app_lines)
                         if _dynamics_needle in line]
    print(f"===== web/app.js Dynamics filter assertion ({len(_dynamics_matches)} matches) =====")
    for _match in _dynamics_matches:
        _start = max(0, _match - 24)
        _end = min(len(_app_lines), _match + 3)
        for _index in range(_start, _end):
            print(f"{_index + 1:4}: {_app_lines[_index]}")
    _drive_needle = "Drive & fuzz filter must include"
    _drive_matches = [index for index, line in enumerate(_app_lines)
                      if _drive_needle in line]
    print(f"===== web/app.js Drive & fuzz filter assertion ({len(_drive_matches)} matches) =====")
    for _match in _drive_matches:
        _start = max(0, _match - 24)
        _end = min(len(_app_lines), _match + 3)
        for _index in range(_start, _end):
            print(f"{_index + 1:4}: {_app_lines[_index]}")
    _count_matches = [index for index, line in enumerate(_app_lines)
                      if any(f" of {count}" in line for count in (96, 97, 98))]
    print(f"===== web/app.js hard-coded picker totals ({len(_count_matches)} matches) =====")
    for _index in _count_matches:
        print(f"{_index + 1:4}: {_app_lines[_index]}")
from pathlib import Path as _EffectPath

_effect_app = _EffectPath(__file__).resolve().parents[1] / "web" / "app.js"
_effect_lines = _effect_app.read_text(encoding="utf-8").splitlines()
for _effect_index, _effect_line in enumerate(_effect_lines):
    if "The all-effect chain must contain" in _effect_line:
        print("===== all-effect chain assertion context =====")
        for _effect_context_index in range(max(0, _effect_index - 40), min(len(_effect_lines), _effect_index + 26)):
            print(f"{_effect_context_index + 1}: {_effect_lines[_effect_context_index]}")
