#!/usr/bin/env python3
"""Verify the tracked goal, requirement register, and evidence documents."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    required = (
        "README.md", "GOAL.md", "RESEARCH.md", "ARCHITECTURE.md",
        "IMPLEMENTATION_CHECKLIST.md", "EFFECTS_MATRIX.md", "PRESET_FORMAT.md",
        "AUDIO_COMPATIBILITY.md", "LATENCY_AND_QUALITY.md", "THIRD_PARTY_NOTICES.md",
    )
    missing = [name for name in required if not (ROOT / name).is_file()]
    if missing:
        raise SystemExit("Missing required documents: " + ", ".join(missing))

    state_text = ""
    try:
        state_text = (ROOT / "research-state.json").read_text(encoding="utf-8")
        research_state = json.loads(state_text)
    except OSError as error:
        raise SystemExit(f"research-state.json cannot be read: {error}") from error
    except json.JSONDecodeError as error:
        start = max(0, error.pos - 80)
        stop = min(len(state_text), error.pos + 80)
        excerpt = state_text[start:error.pos] + "<<<HERE>>>" + state_text[error.pos:stop]
        raise SystemExit(
            f"research-state.json is not valid JSON: {error} near {excerpt!r}"
        ) from error
    if (not isinstance(research_state, dict) or
            research_state.get("schema_version") != 1 or
            not isinstance(research_state.get("current_checkpoint"), dict) or
            not isinstance(research_state.get("next_research"), list)):
        raise SystemExit("research-state.json is missing its required workflow state")

    goal = (ROOT / "GOAL.md").read_text(encoding="utf-8")
    checklist = (ROOT / "IMPLEMENTATION_CHECKLIST.md").read_text(encoding="utf-8")
    if "Maintainer-approved additions" not in goal or "Windows is a required implementation target" not in goal:
        raise SystemExit("GOAL.md is missing the approved Windows scope")
    if "percentage of the callback deadline" not in goal:
        raise SystemExit("GOAL.md is missing per-processor DSP-cost reporting")
    if "WebAssembly" not in goal or "AudioWorklet" not in goal or "Pressing Play" not in goal:
        raise SystemExit("GOAL.md is missing the browser/WASM guitar-audio playback requirement")
    bootstrap = (ROOT / "scripts/bootstrap_docs.py").read_text(encoding="utf-8")
    if '"WEB-01"' not in bootstrap or "WebAssembly" not in bootstrap:
        raise SystemExit("The initial documentation generator is missing the browser/WASM requirement")

    id_pattern = re.compile(r"^\s*- \[ \] \*\*([A-Z][A-Z0-9]*-\d+)(?: · P[012])?\*\*", re.MULTILINE)
    source_ids = id_pattern.findall(goal.split("## Maintainer-approved additions", 1)[0])
    row_pattern = re.compile(r"^\| \[[ x]\] \| `([A-Z][A-Z0-9]*-\d+)` \|", re.MULTILINE)
    tracked_ids = row_pattern.findall(checklist)
    if len(source_ids) != len(set(source_ids)):
        raise SystemExit("GOAL.md contains duplicate requirement IDs")
    if len(tracked_ids) != len(set(tracked_ids)):
        raise SystemExit("IMPLEMENTATION_CHECKLIST.md contains duplicate IDs")
    if set(source_ids) - set(tracked_ids):
        raise SystemExit("Checklist is missing goal IDs: " + ", ".join(sorted(set(source_ids) - set(tracked_ids))))
    expected_additions = {
        "WIN-01", "WIN-02", "WIN-03", "WIN-04", "WIN-05", "DSP-01", "LIN-01", "LIN-02",
        "DATA-01", "DATA-02", "WEB-01",
    }
    if not expected_additions.issubset(set(tracked_ids)):
        raise SystemExit("Checklist is missing approved platform/DSP requirements")
    print(f"Documentation verified: {len(source_ids)} source requirements and {len(tracked_ids)} tracked rows.")


if __name__ == "__main__":
    main()
