#!/usr/bin/env python3
"""Print the focused Stadium Delay Trails integration code for review."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
result = subprocess.run(
    ["git", "diff", "--no-ext-diff", "--unified=4", "--", "make/publish.mk"],
    cwd=ROOT,
    check=True,
    capture_output=True,
    text=True,
)
print("===== uncommitted make/publish.mk diff =====")
print(result.stdout or "No unstaged changes.")
TARGETS = {
    "web/app.js": (
        "runStadiumDelayTrailsBrowserSmokeTest", "Delay Trails browser",
        "dryErrorSquares", "delayTrailsSmoke",
    ),
    "scripts/web.py": ("stadium_delay_trails", "delay_trails_dry_reference_path"),
    "web/audio-worklet.js": ("runDelayTrailsWasmSelfTest", "delay-trails-wasm-selftest"),
    "src/web/wasm_chain.cpp": (
        "set_enabled(std::uint32_t index", "process_effect(std::uint32_t index",
        "effects_[index].enabled", "process_bypassed",
    ),
}

for relative_path, markers in TARGETS.items():
    path = ROOT / relative_path
    lines = path.read_text(encoding="utf-8").splitlines()
    selected: set[int] = set()
    for index, line in enumerate(lines):
        if any(marker in line for marker in markers):
            selected.update(range(max(0, index - 8), min(len(lines), index + 100)))
    print(f"===== {relative_path} =====")
    if not selected:
        print("No focused marker found.")
        continue
    previous = -2
    for index in sorted(selected):
        if index > previous + 1:
            print("...")
        print(f"{index + 1:5d}: {lines[index]}")
        previous = index
