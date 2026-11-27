"""Fail-closed exact changed-line selectors for narrowly scoped checkpoints."""

from __future__ import annotations

import re
from collections import Counter
import hashlib


LineSelectorOperation = str | dict[str, object]

PARTIAL_CHECKPOINT_LINE_SELECTORS: dict[
    str, dict[str, tuple[LineSelectorOperation, ...]]
] = {}

PARTIAL_CHECKPOINT_LINE_SELECTORS["fractal-resonator-effect"] = {
    "CMakeLists.txt": (
        "+    src/effects/resonator.cpp",
        "+add_executable(tabletfx_resonator_bench apps/resonator_bench.cpp)",
        "+target_link_libraries(tabletfx_resonator_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_resonator_tests tests/test_resonator.cpp)",
        "+    target_link_libraries(tabletfx_resonator_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_resonator_tests COMMAND tabletfx_resonator_tests)",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator block (2026-11-27)",
                "line_count": 6,
            }
        },
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator (2026-11-27)",
                "end": "+- [ ] Confirm the target device/installed firmware and measure sustained CPU on the intended runtime; v12.0 lists Original/Turbo but does not name Mark II Turbo.",
                "expected_line_count": 11,
            }
        },
    ),
    "LATENCY_AND_QUALITY.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator processor-call benchmark (2026-11-27)",
                "line_count": 5,
            }
        },
    ),
    "include/tabletfx/effects.hpp": (
        '+#include "tabletfx/effects/resonator.hpp"',
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/resonator.hpp"',
        {
            "span": {
                "start": "+          resonator_(std::make_unique<tabletfx::Resonator>(",
                "line_count": 2,
            }
        },
        {"replace": {
            "before": "-        return effects_.size() + 27;",
            "current": "+    return effects_.size() + 33;",
            "selected": "+    return effects_.size() + 28;",
            "hunk_header": "@@ -460 +474,3 @@ public:",
            "allow_intervening_added_lines": True,
            "allow_cross_hunk_staged": True,
        }},
        {
            "span": {
                "hunk_headers": [
                    "@@ -621,0 +638,64 @@ public:",
                    "@@ -621,0 +625,4 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 4,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -979,0 +1060,39 @@ public:",
                    "@@ -979,0 +987,9 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 9,
            }
        },
        "+        resonator_->reset();",
        {
            "span": {
                "start": "+    void configure_resonator(const float* values,",
                "line_count": 48,
            }
        },
        "+        bytes += resonator_->memory_bytes();",
        {
            "span": {
                "hunk_headers": [
                    "@@ -4278,0 +4573,32 @@ public:",
                    "@@ -4278,0 +4345,6 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 6,
            }
        },
        "+        case 122U: return resonator_enabled_;",
        "+    std::unique_ptr<tabletfx::Resonator> resonator_;",
        "+    bool resonator_enabled_ = false;",
        {
            "span": {
                "start": "+TABLETFX_WASM_EXPORT void tf_chain_set_resonator(",
                "line_count": 6,
            }
        },
    ),
    "web/app.js": (
        '+  "Fractal Resonator · project-authored, not a pitch-tracked synth",',
        {
            "span": {
                "start": "+  // Serialized as the ResonatorParameters fields in the stable worklet ABI order.",
                "end": "+  ],",
                "end_after_start": True,
                "expected_line_count": 10,
            }
        },
        {
            "span": {
                "start": "+const resonatorControls = Array.from(",
                "end": "+);",
                "end_after_start": True,
                "expected_line_count": 6,
            }
        },
        {
            "span": {
                "start": "+function updateResonator() {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 26,
            }
        },
        {
            "span": {
                "start": "+for (const control of resonatorControls) {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        "+let browserSmokeBarrierSequence = 0;",
        {
            "span": {
                "start": "+function waitForBrowserSmokeWorkletBarrier(expectedActive) {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 28,
            }
        },
        {
            "span": {
                "start": "+async function stopBrowserSmokePlaybackAndWait() {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        {
            "line": {
                "operation": "+    await stopBrowserSmokePlaybackAndWait();",
                "hunk_context": "async function runMercatorDistortionBrowserSmokeTest",
                "expected_count": 1,
            }
        },
        {
            "line": {
                "operation": "+      await stopBrowserSmokePlaybackAndWait();",
                "hunk_context": "async function runMercatorDistortionBrowserSmokeTest",
                "expected_count": 1,
            }
        },
        "-    const expectedEffectIds = Array.from({ length: 117 }, (_, index) => index);",
        {
            "replace": {
                "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "selected": "+    const expectedEffectIds = Array.from({ length: 123 }, (_, index) => index);",
            }
        },
        "-    if (EFFECT_NAMES.length !== 117 || effectRows.length !== 117 ||",
        {
            "replace": {
                "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "selected": "+    if (EFFECT_NAMES.length !== 123 || effectRows.length !== 123 ||",
            }
        },
        {
            "span": {
                "start": '+                  !EFFECT_NAMES[122]?.startsWith("Fractal Resonator · project-authored, not a pitch-tracked synth") ||',
                "end": '+                  DEFAULT_PARAMETERS.resonator.join(",") !== "0,0,1,110,164.81,220,329.63,110,0.55,0.55,0.55,0.55,2,2,2,2,0.4,0.4,0.4,0.4,-0.75,-0.25,0.25,0.75,1,1,1,1,0,0.8,1,1,1,0.35,0,0,0,0,0" ||',
                "expected_line_count": 4,
            }
        },
        "-                  initialEffectState[118] || initialEffectState[119] ||",
        "+                   initialEffectState[118] || initialEffectState[119] || initialEffectState[120] ||",
        {
            "replace": {
                "current": "+                      initialEffectState[121] || initialEffectState[122] || initialEffectState[123] ||",
                "selected": "+                      initialEffectState[121] || initialEffectState[122] ||",
            }
        },
        {
            "replace": {
                "before": '-                   throw new Error(`The effect picker must list 122 ordered, categorized effects (IDs 0–121), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, Memory Delay, both Tone Master halls, and Pitch Ring Mod; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[121]}, row121=${effectRows[121]?.dataset.category}/${effectRows[121]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[121]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "current": '+                     throw new Error(`The effect picker must list 126 ordered, categorized effects (IDs 0–125), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, both Tone Master halls, Pitch Ring Mod, Fractal Resonator, Studio Pan Delay, Essex Vibe, and Euclidean Delay; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[125]}, row125=${effectRows[125]?.dataset.category}/${effectRows[125]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[125]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "selected": '+                     throw new Error(`The effect picker must list 123 ordered, categorized effects (IDs 0–122), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, Memory Delay, both Tone Master halls, Pitch Ring Mod, and Fractal Resonator; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[122]}, row122=${effectRows[122]?.dataset.category}/${effectRows[122]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[122]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
            }
        },
        "+    // Fractal Resonator has a focused local guitar Play smoke and no offline CLI stage.",
        "+    parityEnabled[122] = false;",
        {
            "span": {
                "start": '+        if (new URLSearchParams(location.search).get("selftest") === "fractal-resonator") {',
                "end": "+        }",
                "end_after_start": True,
                "expected_line_count": 18,
            }
        },
        {
            "span": {
                "start": '+      testResult.textContent = "Checking Fractal Resonator with local guitar playback…";',
                "end": "+      );",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        "+      resonator: resonatorSmoke,",
    ),
}


class LineSelectionError(ValueError):
    """Raised when an exact checkpoint line operation is missing or ambiguous."""


_HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$")


def select_exact_line_operations(
    diff: str, operations: tuple[LineSelectorOperation, ...]
) -> str:
    """Select exact +/- lines and bounded marker-to-marker spans.

    A string operation is the complete diff line, including ``+`` or ``-``.
    A marker span is ``{"span": {"start": "+...", "end": "+..."}}``;
    both markers must occur exactly once in one zero-context hunk. A span can
    instead use ``line_count`` to select a fixed number of diff operations
    beginning with its unique start marker. A bounded span can also name an
    exact ``hunk_header`` and a 1-based ``end_occurrence`` for a repeated
    closing line. Every selected line must be an added/deleted operation;
    ``end_exclusive`` may leave the end marker out of the selected range. An
    optional ``expected_line_count`` makes marker boundaries fail closed if
    the block changes. Missing, ambiguous, overlapping, or stale selectors
    fail closed.
    """
    if not operations:
        raise LineSelectionError("line selector has no operations")
    exact_operations = tuple(operation for operation in operations if isinstance(operation, str))
    replacements = tuple(
        operation["replace"] for operation in operations
        if isinstance(operation, dict) and "replace" in operation
    )
    scoped_lines = tuple(
        operation["line"] for operation in operations
        if isinstance(operation, dict) and "line" in operation
    )
    spans = tuple(
        operation["span"] for operation in operations
        if isinstance(operation, dict) and "span" in operation
    )
    ranges = tuple(
        operation["range"] for operation in operations
        if isinstance(operation, dict) and "range" in operation
    )
    if (
        len(exact_operations) + len(spans) + len(ranges) + len(replacements)
        + len(scoped_lines) != len(operations)
    ):
        raise LineSelectionError("unsupported selector operation")
    if any(not operation.startswith(("+", "-")) or operation.startswith(("+++", "---"))
           for operation in exact_operations):
        raise LineSelectionError("operations must be complete added/deleted diff lines")
    for span in spans:
        if not isinstance(span, dict):
            raise LineSelectionError("span selector must be an object")
        if not isinstance(span.get("start"), str) or not span["start"].startswith(("+", "-")):
            raise LineSelectionError("span selectors require an exact added/deleted start line")
        line_count = span.get("line_count")
        if line_count is not None:
            if not isinstance(line_count, int) or line_count < 1:
                raise LineSelectionError("span line_count must be a positive integer")
            if any(key in span for key in (
                "end", "end_occurrence", "end_after_start", "end_exclusive",
                "expected_line_count",
            )):
                raise LineSelectionError("line-count spans cannot use end-marker options")
        elif (
            not isinstance(span.get("end"), str)
            or not span["end"].startswith(("+", "-"))
        ):
            raise LineSelectionError("marker spans require an exact added/deleted end line")
        occurrence = span.get("end_occurrence")
        if occurrence is None and line_count is None:
            occurrence = 1
        if occurrence is not None and (not isinstance(occurrence, int) or occurrence < 1):
            raise LineSelectionError("span end_occurrence must be a positive integer")
        end_after_start = span.get("end_after_start", False)
        if not isinstance(end_after_start, bool):
            raise LineSelectionError("span end_after_start must be boolean")
        if end_after_start and "end_occurrence" in span:
            raise LineSelectionError(
                "span cannot combine end_after_start with end_occurrence"
            )
        expected_count = span.get("expected_line_count")
        if expected_count is not None and (
            not isinstance(expected_count, int) or expected_count < 1
        ):
            raise LineSelectionError("span expected_line_count must be a positive integer")
        if "end_exclusive" in span and not isinstance(span["end_exclusive"], bool):
            raise LineSelectionError("span end_exclusive must be boolean")
        if span.get("hunk_header") is not None and not isinstance(span["hunk_header"], str):
            raise LineSelectionError("span hunk_header must be a string")
        hunk_headers = span.get("hunk_headers")
        if hunk_headers is not None and (
            not isinstance(hunk_headers, (tuple, list))
            or not hunk_headers
            or any(not isinstance(header, str) for header in hunk_headers)
            or len(set(hunk_headers)) != len(hunk_headers)
            or span.get("hunk_header") is not None
        ):
            raise LineSelectionError("span hunk_headers must be a unique non-empty list")
    for selected_range in ranges:
        if (
            not isinstance(selected_range, dict)
            or not isinstance(selected_range.get("hunk_header"), str)
            or not isinstance(selected_range.get("start_index"), int)
            or not isinstance(selected_range.get("end_index"), int)
            or selected_range["start_index"] < 0
            or selected_range["end_index"] <= selected_range["start_index"]
            or not isinstance(selected_range.get("sha256"), str)
            or len(selected_range["sha256"]) != 64
            or any(ch not in "0123456789abcdef" for ch in selected_range["sha256"])
        ):
            raise LineSelectionError("range selectors require a hunk, indices, and SHA-256")
    for replacement in replacements:
        if (
            not isinstance(replacement, dict)
            or not isinstance(replacement.get("current"), str)
            or not isinstance(replacement.get("selected"), str)
            or not replacement["current"].startswith("+")
            or not replacement["selected"].startswith("+")
            or replacement["current"].startswith("+++")
            or replacement["selected"].startswith("+++")
            or (
                "before" in replacement
                and (
                    not isinstance(replacement["before"], str)
                    or not replacement["before"].startswith("-")
                    or replacement["before"].startswith("---")
                )
            )
            or (
                "hunk_header" in replacement
                and not isinstance(replacement["hunk_header"], str)
            )
            or (
                "current_occurrence" in replacement
                and (
                    not isinstance(replacement["current_occurrence"], int)
                    or replacement["current_occurrence"] < 1
                )
            )
            or (
                "expected_current_count" in replacement
                and (
                    not isinstance(replacement["expected_current_count"], int)
                    or replacement["expected_current_count"] < 1
                )
            )
            or (
                "allow_intervening_added_lines" in replacement
                and not isinstance(replacement["allow_intervening_added_lines"], bool)
            )
            or (
                "allow_cross_hunk_staged" in replacement
                and not isinstance(replacement["allow_cross_hunk_staged"], bool)
            )
            or (
                replacement.get("allow_cross_hunk_staged", False)
                and ("before" not in replacement or "hunk_header" not in replacement)
            )
            or (
                replacement.get("allow_intervening_added_lines", False)
                and ("before" not in replacement or "hunk_header" not in replacement)
            )
        ):
            raise LineSelectionError(
                "replacement selectors require an exact added line and optional deletion"
            )
    for scoped_line in scoped_lines:
        if (
            not isinstance(scoped_line, dict)
            or not isinstance(scoped_line.get("operation"), str)
            or not scoped_line["operation"].startswith(("+", "-"))
            or scoped_line["operation"].startswith(("+++", "---"))
            or not isinstance(scoped_line.get("hunk_context"), str)
            or not scoped_line["hunk_context"]
            or not isinstance(scoped_line.get("expected_count"), int)
            or scoped_line["expected_count"] < 1
        ):
            raise LineSelectionError(
                "scoped line selectors require an exact operation, hunk context, and positive count"
            )

    header: list[str] = []
    hunks: list[tuple[str, list[str]]] = []
    current_header: str | None = None
    current_body: list[str] = []
    for line in diff.splitlines(keepends=True):
        if line.startswith("@@"):
            if current_header is not None:
                hunks.append((current_header, current_body))
            current_header = line
            current_body = []
        elif current_header is None:
            header.append(line)
        else:
            current_body.append(line)
    if current_header is not None:
        hunks.append((current_header, current_body))
    if not hunks:
        raise LineSelectionError("diff contains no change hunks")

    wanted = Counter(exact_operations)
    observed = Counter()
    selected_coordinates: set[tuple[int, int]] = set()
    for hunk_index, (_, body) in enumerate(hunks):
        for line_index, line in enumerate(body):
            operation = line.rstrip("\n")
            if operation in wanted:
                observed[operation] += 1
                selected_coordinates.add((hunk_index, line_index))
    invalid = {
        operation: {"expected": count, "found": observed.get(operation, 0)}
        for operation, count in wanted.items()
        if observed.get(operation, 0) != count
    }
    if invalid:
        raise LineSelectionError(
            "each exact line operation must occur at its listed multiplicity; counts=" + repr(invalid)
        )
    exact_selected_coordinates = set(selected_coordinates)

    for scoped_line in scoped_lines:
        matches = [
            (hunk_index, line_index)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if scoped_line["hunk_context"] in header_line
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == scoped_line["operation"]
        ]
        if len(matches) != scoped_line["expected_count"]:
            raise LineSelectionError(
                "scoped line multiplicity changed; "
                f"context={scoped_line['hunk_context']!r}, "
                f"expected={scoped_line['expected_count']}, found={len(matches)}"
            )
        selected_coordinates.update(matches)

    replacement_by_coordinate: dict[tuple[int, int], str] = {}
    replacement_coordinates: set[tuple[int, int]] = set()
    for replacement in replacements:
        scoped_header = replacement.get("hunk_header")
        current_matches = [
            (hunk_index, line_index)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if scoped_header is None or header_line.rstrip("\n") == scoped_header
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == replacement["current"]
        ]
        if not current_matches:
            # Verification receives the staged, already-selected value. Accept
            # that exact value idempotently so plan, stage, and verify share
            # one selector without broadening its whitelist.
            allow_cross_hunk_staged = replacement.get(
                "allow_cross_hunk_staged", False
            )
            selected_matches = [
                (hunk_index, line_index)
                for hunk_index, (header_line, body) in enumerate(hunks)
                if allow_cross_hunk_staged
                or scoped_header is None
                or header_line.rstrip("\n") == scoped_header
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == replacement["selected"]
            ]
            if allow_cross_hunk_staged and len(selected_matches) != 1:
                raise LineSelectionError(
                    "staged replacement value must occur exactly once; "
                    f"found={len(selected_matches)}"
                )
            if len(selected_matches) == 1:
                selected_coordinate = selected_matches[0]
                if "before" in replacement:
                    before_matches = [
                        (hunk_index, line_index)
                        for hunk_index, (header_line, body) in enumerate(hunks)
                        if allow_cross_hunk_staged
                        or scoped_header is None
                        or header_line.rstrip("\n") == scoped_header
                        for line_index, line in enumerate(body)
                        if line.rstrip("\n") == replacement["before"]
                    ]
                    if len(before_matches) != 1:
                        raise LineSelectionError(
                            "replacement deleted line must occur exactly once"
                        )
                    before_coordinate = before_matches[0]
                    if before_coordinate[0] != selected_coordinate[0]:
                        if not allow_cross_hunk_staged:
                            raise LineSelectionError(
                                "replacement lines must be in the same diff hunk"
                            )
                        if selected_coordinate[0] != before_coordinate[0] + 1:
                            raise LineSelectionError(
                                "cross-hunk staged replacement must use adjacent hunks"
                            )
                    elif abs(before_coordinate[1] - selected_coordinate[1]) != 1:
                        raise LineSelectionError(
                            "staged replacement lines must be adjacent in the same diff hunk"
                        )
                    replacement_coordinates.add(before_coordinate)
                replacement_by_coordinate[selected_coordinate] = replacement["selected"]
                replacement_coordinates.add(selected_coordinate)
                continue
        expected_current_count = replacement.get("expected_current_count")
        if expected_current_count is not None and len(current_matches) != expected_current_count:
            raise LineSelectionError(
                "replacement current-line multiplicity changed; "
                f"expected={expected_current_count}, found={len(current_matches)}"
            )
        current_occurrence = replacement.get("current_occurrence", 1)
        if len(current_matches) < current_occurrence:
            raise LineSelectionError(
                "replacement current line occurrence is missing"
            )
        if (
            len(current_matches) > 1
            and "current_occurrence" not in replacement
            and expected_current_count is None
        ):
            raise LineSelectionError(
                "replacement current line must occur exactly once unless occurrence is scoped"
            )
        if current_occurrence > 1 and expected_current_count is None and scoped_header is None:
            raise LineSelectionError(
                "repeated replacement current lines require a hunk scope or expected count"
            )
        current_coordinate = current_matches[current_occurrence - 1]
        if "before" in replacement:
            before_matches = [
                (hunk_index, line_index)
                for hunk_index, (header_line, body) in enumerate(hunks)
                if scoped_header is None or header_line.rstrip("\n") == scoped_header
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == replacement["before"]
            ]
            if len(before_matches) != 1:
                raise LineSelectionError(
                    "replacement deleted line must occur exactly once"
                )
            before_coordinate = before_matches[0]
            same_hunk = before_coordinate[0] == current_coordinate[0]
            adjacent = abs(before_coordinate[1] - current_coordinate[1]) == 1
            if not same_hunk:
                raise LineSelectionError(
                    "replacement lines must be in the same diff hunk"
                )
            if not adjacent:
                if not replacement.get("allow_intervening_added_lines", False):
                    raise LineSelectionError(
                        "replacement lines must be adjacent in the same diff hunk"
                    )
                first = min(before_coordinate[1], current_coordinate[1])
                last = max(before_coordinate[1], current_coordinate[1])
                intervening = hunks[before_coordinate[0]][1][first + 1:last]
                if not intervening or any(
                    not line.startswith("+") or line.startswith("+++")
                    for line in intervening
                ):
                    raise LineSelectionError(
                        "intervening replacement lines must be additions"
                    )
            replacement_coordinates.add(before_coordinate)
        if current_coordinate in replacement_by_coordinate:
            raise LineSelectionError("replacement targets overlap")
        replacement_by_coordinate[current_coordinate] = replacement["selected"]
        replacement_coordinates.add(current_coordinate)

    for span in spans:
        start_marker = span["start"]
        eligible_hunks = [
            (hunk_index, header_line, body)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if (
                span.get("hunk_header") is None
                and span.get("hunk_headers") is None
            )
            or header_line.rstrip("\n") == span.get("hunk_header")
            or header_line.rstrip("\n") in span.get("hunk_headers", ())
        ]
        if (
            span.get("hunk_header") is not None
            or span.get("hunk_headers") is not None
        ) and len(eligible_hunks) != 1:
            raise LineSelectionError(
                f"span hunk header must occur once; found={len(eligible_hunks)}"
            )
        start_matches = [
            (hunk_index, line_index)
            for hunk_index, _, body in eligible_hunks
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == start_marker
        ]
        if len(start_matches) != 1:
            raise LineSelectionError(
                f"span start marker must occur once; found={len(start_matches)}"
            )
        start_hunk, start_index = start_matches[0]
        if "line_count" in span:
            end_hunk = start_hunk
            end_index = start_index + span["line_count"] - 1
            if end_index >= len(hunks[start_hunk][1]):
                raise LineSelectionError(
                    "line-count span extends beyond its zero-context hunk"
                )
        else:
            end_marker = span["end"]
            end_matches = [
                (hunk_index, line_index)
                for hunk_index, _, body in eligible_hunks
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == end_marker
            ]
            if span.get("end_after_start", False):
                following_ends = [
                    (hunk_index, line_index)
                    for hunk_index, line_index in end_matches
                    if hunk_index == start_hunk and line_index > start_index
                ]
                if not following_ends:
                    raise LineSelectionError(
                        "span end marker missing after its start marker in the same hunk"
                    )
                end_hunk, end_index = following_ends[0]
            else:
                has_explicit_end_occurrence = "end_occurrence" in span
                end_occurrence = span.get("end_occurrence", 1)
                if not has_explicit_end_occurrence and len(end_matches) != 1:
                    raise LineSelectionError(
                        "span markers must each occur once; "
                        f"start=1, end={len(end_matches)}"
                    )
                if len(end_matches) < end_occurrence:
                    raise LineSelectionError(
                        f"span end marker missing; found={len(end_matches)}, "
                        f"requested occurrence={end_occurrence}"
                    )
                end_hunk, end_index = end_matches[end_occurrence - 1]
        if start_hunk != end_hunk or end_index < start_index:
            raise LineSelectionError("span markers must be ordered within the same hunk")
        end_exclusive = span.get("end_exclusive", False)
        selected_end = end_index if end_exclusive else end_index + 1
        if selected_end <= start_index:
            raise LineSelectionError("span contains no selected change lines")
        expected_count = span.get("expected_line_count")
        if expected_count is not None and selected_end - start_index != expected_count:
            raise LineSelectionError(
                f"span length changed; expected={expected_count}, found={selected_end - start_index}"
            )
        body = hunks[start_hunk][1]
        for line_index in range(start_index, selected_end):
            line = body[line_index]
            if not line.startswith(("+", "-")) or line.startswith(("+++", "---")):
                raise LineSelectionError("span includes a non-change diff line")
            coordinate = (start_hunk, line_index)
            if coordinate in selected_coordinates:
                raise LineSelectionError("selector operations overlap")
            selected_coordinates.add(coordinate)

    for selected_range in ranges:
        matching_hunks = [
            (hunk_index, body)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if header_line.rstrip("\n") == selected_range["hunk_header"]
        ]
        if len(matching_hunks) != 1:
            raise LineSelectionError(
                f"range hunk header must occur once; found={len(matching_hunks)}"
            )
        hunk_index, body = matching_hunks[0]
        start_index = selected_range["start_index"]
        end_index = selected_range["end_index"]
        if end_index > len(body):
            raise LineSelectionError("range extends beyond its zero-context hunk")
        selected_lines = body[start_index:end_index]
        if any(
            not line.startswith(("+", "-")) or line.startswith(("+++", "---"))
            for line in selected_lines
        ):
            raise LineSelectionError("range includes a non-change diff line")
        actual_hash = hashlib.sha256("".join(selected_lines).encode("utf-8")).hexdigest()
        if actual_hash != selected_range["sha256"]:
            raise LineSelectionError("range contents do not match their SHA-256 whitelist")
        for line_index in range(start_index, end_index):
            coordinate = (hunk_index, line_index)
            if coordinate in selected_coordinates:
                raise LineSelectionError("selector operations overlap")
            selected_coordinates.add(coordinate)

    if replacement_coordinates & exact_selected_coordinates:
        raise LineSelectionError("replacement overlaps an exact line operation")
    selected_coordinates.update(replacement_coordinates)

    selected_hunks: list[str] = []
    selected_line_delta = 0

    def format_range(start: int, count: int) -> str:
        return str(start) if count == 1 else f"{start},{count}"

    for hunk_index, (hunk_header, body) in enumerate(hunks):
        match = _HUNK_RE.match(hunk_header.rstrip("\n"))
        if match is None:
            raise LineSelectionError("cannot parse unified diff hunk header")
        old_cursor = int(match.group(1))
        new_cursor = int(match.group(3))
        suffix = match.group(5)
        groups: list[tuple[int, int, list[str], int, int]] = []
        group_lines: list[str] = []
        group_old_start = 0
        group_delta_before = 0
        group_first_sign = ""
        group_deleted = 0
        group_added = 0

        def flush_group() -> None:
            nonlocal group_lines, group_deleted, group_added, group_first_sign
            if group_lines:
                if group_first_sign == "+":
                    group_new_start = group_old_start + group_delta_before + 1
                else:
                    group_new_start = (
                        group_old_start + group_delta_before - (0 if group_added else 1)
                    )
                groups.append((group_old_start, group_new_start, group_lines,
                               group_deleted, group_added))
            group_lines = []
            group_deleted = 0
            group_added = 0
            group_first_sign = ""

        for line_index, line in enumerate(body):
            if line.startswith("\\"):
                if group_lines:
                    group_lines.append(line)
                continue
            is_added = line.startswith("+") and not line.startswith("+++")
            is_deleted = line.startswith("-") and not line.startswith("---")
            if is_added or is_deleted:
                selected = (hunk_index, line_index) in selected_coordinates
                if selected:
                    if not group_lines:
                        group_old_start = old_cursor
                        group_delta_before = selected_line_delta
                        group_first_sign = "+" if is_added else "-"
                    selected_line = replacement_by_coordinate.get(
                        (hunk_index, line_index), line
                    )
                    if line.endswith("\n") and not selected_line.endswith("\n"):
                        selected_line += "\n"
                    group_lines.append(selected_line)
                    if is_added:
                        group_added += 1
                        selected_line_delta += 1
                    else:
                        group_deleted += 1
                        selected_line_delta -= 1
                else:
                    flush_group()
                if is_added:
                    new_cursor += 1
                else:
                    old_cursor += 1
                continue
            flush_group()
            if line.startswith(" "):
                old_cursor += 1
                new_cursor += 1

        flush_group()
        for old_start, new_start, lines, deleted, added in groups:
            selected_hunks.append(
                f"@@ -{format_range(old_start, deleted)} "
                f"+{format_range(new_start, added)} @@{suffix}\n"
                + "".join(lines)
            )

    if not selected_hunks:
        raise LineSelectionError("no exact selector operations matched")
    return "".join(header + selected_hunks)


PARTIAL_CHECKPOINT_LINE_SELECTORS["fractal-resonator-effect"].update({
    "web/audio-worklet.js": (
        '+EFFECTS.push("resonator");',
        {"span": {
            "start": "+      this.configureResonator(",
            "line_count": 11,
        }},
        {"span": {
            "start": "+  configureResonator(values) {",
            "line_count": 16,
        }},
        '+    } else if (message.type === "resonator" && this.ready) {',
        "+      this.configureResonator(message.value);",
        {
            "span": {
                "start": '-      this.port.postMessage({ type: "test-barrier-ack", token: message.token });',
                "line_count": 7,
            }
        },
    ),
    "web/index.html": (
        '+        <label class="effect-row" data-category="filter"><input type="checkbox" data-effect="122"><span class="effect-number">123</span><span class="effect-name">Fractal Resonator · project-authored, not a pitch-tracked synth</span><span class="effect-default">Four feedback-fed band-pass voices · disabled by default</span><span class="effect-category-tag">Filters &amp; wah</span></label>',
        {"span": {
            "start": "+      <div class=\"effect-parameter-group\" aria-label=\"Fractal Resonator controls\">",
            "line_count": 60,
        }},
    ),
    "research-state.json": (
        {
            "span": {
                "start": '+      "name": "Fractal FM9-family Resonator block — project-authored four-voice filter",',
                "line_count": 23,
            }
        },
    ),
    "make/publish.mk": (
        "+.PHONY: publish-fractal-resonator-effect-plan stage-fractal-resonator-effect publish-fractal-resonator-effect",
        "+publish-fractal-resonator-effect-plan:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "fractal-resonator-effect"',
        "+stage-fractal-resonator-effect:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py stage --checkpoint "fractal-resonator-effect"',
        "+publish-fractal-resonator-effect:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "fractal-resonator-effect"',
    ),
    "scripts/publish_checkpoint.py": (
        {
            "span": {
                "start": "+FRACTAL_RESONATOR_EFFECT_CHECKPOINT_PATHS = (",
                "end": "+)",
                "end_occurrence": 1,
                "expected_line_count": 21,
            }
        },
        {
            "span": {
                "start": '+CHECKPOINTS["fractal-resonator-effect"] = (',
                "end": "+)  # publish this feature as one bounded primary-branch checkpoint",
                "expected_line_count": 4,
            }
        },
    ),
})


def select_checkpoint_line_patch(checkpoint: str, path: str, diff: str) -> str | None:
    """Apply a checkpoint/path exact-line selector, or return None if absent."""
    operations = PARTIAL_CHECKPOINT_LINE_SELECTORS.get(checkpoint, {}).get(path)
    if operations is None:
        return None
    return select_exact_line_operations(diff, operations)
