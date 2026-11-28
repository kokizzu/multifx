#!/usr/bin/env python3
"""Record the tested Euclidean Delay checkpoint in the research ledger."""

import copy
import json
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STATE_PATH = ROOT / "research-state.json"
NAME = "Line 6 Helix Stadium XL — Euclidean Delay"
CHECKPOINT = {
    "name": NAME,
    "date": "2026-11-27",
    "category": "Delay",
    "picker_index": 125,
    "official_source": {
        "title": "Helix Stadium model catalogue, Effects manual, Blocks manual, and 1.4 release notes",
        "revision": "Catalogue checked 2026-11-27; live Effects manual selector v1.4 Rev D; hardware 1.4.2 release notes dated 2026-10-27; installed firmware unknown.",
        "urls": [
            "https://line6.com/helix-stadium-models/",
            "https://manuals.line6.com/en/helix-stadium/live/effect-blocks",
            "https://manuals.line6.com/en/helix-stadium/live/the-blocks",
            "https://kb.line6.com/helix-stadium-1-4-release-notes",
        ],
        "qualification": "The catalogue identifies a Mono/Stereo Line 6 Original Euclidean Delay, first appearing in 1.0. The accessible manual provides common Delay controls but no model-specific parameter table, ranges, defaults, topology, CPU cost, or latency. No settings or parity are inferred.",
    },
    "documented_controls": "Mono/Stereo listing and category identity only; exact model-specific controls and numeric values are not published in the checked manual.",
    "project_controls": "Step Time 10–2,000 ms (125 ms default); Steps 1–16 (8); Pulses 0–Steps (3); Rotation 0–Steps−1 (0); Feedback 0–0.85 (0.25); Mix 0–100% (25%); Output Level −24…+12 dB (0 dB). Project-authored bounded delay line and Euclidean pulse gate; no Line 6 algorithm or settings claim.",
    "status": "Implemented as default-disabled Delay picker ID 125/display number 126. Project-authored functional coverage only; no Line 6 settings, DSP, or sound parity.",
    "verification": {
        "native": "make test-euclidean-delay passed focused defaults, bounds, pattern/rotation, pulse density, mono/stereo, reset, partition continuity, invalid samples, and no process allocation. make test passed 13 fixture-safety checks and 128/128 CTest cases.",
        "browser": "make web-test-euclidean-delay passed HeadlessChrome 151 with local prerecorded stereo Guitar-TECHS DI. It checked Delay category, ID 125, defaults/bounds, pulse slots, rotation, silence between pulses, bypass identity, deterministic reset, lazy WASM storage, and the stored bypass tail (first-window RMS 0.0199466; later-window RMS 0.00073736). Active effect state 768,208 B; default disabled state 0 B; active chain 82,495,724 B; fixed WASM heap 100,663,296 B.",
        "benchmark": "AMD Ryzen 9 5950X Release, 48 kHz stereo/128 frames, 3,000 warm-up/20,000 measured; 2.667 ms callback deadline. Default p50/p95/p99/p99.9/max 0.1455/0.1541/0.2404/0.3915/0.9657%; sparse 1/16 0.1451/0.1522/0.1537/0.3626/0.5299%; dense 8/16 0.1451/0.1526/0.1538/0.3600/0.7729%; muted 0/16 0.1455/0.1530/0.2246/0.3649/0.9773%. All profiles 0/20,000 deadline misses and no processing allocations; native state 768,216 B; fixed dry latency 0 frames.",
        "windows": "make windows-cross-build compiled the MinGW shared core and offline renderer; this is not native Windows runtime/UI validation.",
        "diff": "make diff-check passed; no workflow files changed.",
        "full_browser_suite": "Full make web-test passed on 2026-11-28 after worklet synchronization was added to the Essex Vibe browser smoke; Euclidean Delay local-guitar Play and browser/native comparison passed. Earlier Fractal/Synth Global Mix capture failures remain historical repeatability observations; one clean run does not eliminate them.",
        "remaining": [
            "Human listening and comparison against Helix Stadium XL",
            "Installed firmware and exact on-device effect behavior",
            "Target-device CPU and latency measurements",
            "Full browser suite repeatability issue in unrelated Fractal Global Mix captures",
        ],
    },
}


def main() -> int:
    data = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True, capture_output=True, text=True
    ).stdout.strip()
    completed = data.setdefault("completed_checkpoints", [])
    previous = data.get("current_checkpoint")
    if isinstance(previous, dict) and previous.get("name") == NAME:
        previous = data.get("previous_checkpoint")
    if isinstance(previous, dict) and not any(
        entry.get("name") == previous.get("name") and entry.get("date") == previous.get("date")
        for entry in completed
        if isinstance(entry, dict)
    ):
        completed.append(copy.deepcopy(previous))
    matching = next(
        (
            index
            for index, entry in enumerate(completed)
            if isinstance(entry, dict) and entry.get("name") == NAME and entry.get("date") == CHECKPOINT["date"]
        ),
        None,
    )
    if matching is None:
        completed.append(copy.deepcopy(CHECKPOINT))
    else:
        completed[matching] = copy.deepcopy(CHECKPOINT)
    data["updated"] = "2026-11-28"
    data["previous_checkpoint_commit"] = head
    data["previous_checkpoint"] = copy.deepcopy(previous)
    data["current_checkpoint"] = copy.deepcopy(CHECKPOINT)
    data["latest_audit"] = copy.deepcopy(CHECKPOINT)
    data["continuation_cursor"] = {
        "task": "categorized flagship effects",
        "next_device": "HeadRush Prime",
        "next_feature": "Integrate and validate the project-authored Prime Drum Machine prototype under Loop & performance, then continue the ordered source-backed Prime, Fender, Line 6 Stadium, Fractal, Melofty, and XRecer gaps. Complete, test, and publish one feature checkpoint at a time.",
        "qualification": "Maintain exact-model and firmware qualifications, source URLs, categorized placement, project-authored control labels, CPU/memory/latency measurements, and manual-test remainder. The full browser suite has historical intermittent Global Mix capture failures; a successful run does not eliminate them. Keep expression-volume, footswitch-display, and general-research cursors untouched.",
    }
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", dir=ROOT, prefix=".research-state-", suffix=".tmp", delete=False
    ) as temporary:
        json.dump(data, temporary, ensure_ascii=False, indent=2)
        temporary.write("\n")
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, STATE_PATH)
    print(f"Recorded {NAME}; previous published baseline {head}; {len(completed)} completed checkpoints.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
