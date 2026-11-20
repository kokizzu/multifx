#!/usr/bin/env python3
"""Create the initial tracked requirements and evidence documents once."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path("/tmp/TabletFX_Full_Goal_Prompt.md")

MAINTAINER_ADDENDUM = """

## Maintainer-approved additions (2026-11-20)

- Windows is a required implementation target alongside Android and Linux. Provide a Windows x64 build, a native WASAPI audio backend, the shared editor and portable preset format, and platform-specific tests. Mark Windows hardware audio verification incomplete until tested on Windows with a real audio interface.
- Prefer low latency and bounded CPU/memory use. For each processor, record measured callback cost as a percentage of the callback deadline, with sample rate, block size, CPU, build mode, warmup, percentile, and test command. Keep per-processor benchmark results distinct from whole-chain DSP load.
- Test every implementation automatically before marking its implementation/test checkboxes complete. Keep hardware and listening checks separate. Commit and push finished checkpoints directly to the existing primary branch; do not create branches or GitHub Actions workflows.
- Prioritize reproducible local Linux tests that process prerecorded guitar audio. Keep file-based renders and offline algorithm latency distinct from live hardware round-trip measurements; do not upload private recordings or commit them without explicit permission.
- Keep large prerecorded fixtures outside the Git worktree and expose them through a local-only, Git-ignored symlink. Verify dataset checksums, archive paths, audio formats, and attribution before use.
- For clean electric-guitar coverage, use EGFxSet Clean for its five documented pickup configurations, noting that it contains isolated monophonic notes rather than full performances. Use the CC BY 4.0 Guitar-TECHS DI music excerpt for performance/chord tests; its pickup-selector position is not documented as a per-take label.
- G.A.D.A. appears to match clean DI chords across pickup settings, but its paper says access is for academic and research purposes and the dataset page does not provide a clear license. Do not download or redistribute it unless its terms are clarified.

### Browser/WASM addition (2026-11-21)

- **WEB-01 (P0):** Provide a browser-run test build that compiles the shared C++ DSP processors to WebAssembly (or an equivalent browser-portable module without duplicating DSP) and runs them in block-based Web Audio `AudioWorklet` processing. The page must load a user-selected local guitar audio file without uploading it. Pressing Play must run the selected effect chain and play the processed chain output through the browser's audio output; provide stop/rewind and a dry/wet comparison. Prepare processor state and memory before playback; do not allocate, block, log, or perform file/network I/O in the audio render callback. Add automated browser tests for local file loading, chain processing, and Play output that capture the processed signal and compare it with the native offline C++ renderer within a defined numeric tolerance. Report browser/version, sample rate, block size and callback deadline, per-processor and whole-chain CPU percentage, and memory use. Keep browser test results separate from native audio hardware and round-trip latency claims. User audio must remain local and never be uploaded.
""".strip()


def write_new(relative: str, content: str) -> None:
    path = ROOT / relative
    if path.exists():
        raise SystemExit(f"Refusing to overwrite existing file: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content.rstrip() + "\n", encoding="utf-8")


def write_empty_readme(content: str) -> None:
    path = ROOT / "README.md"
    if path.exists() and path.read_text(encoding="utf-8").strip():
        raise SystemExit(f"Refusing to replace non-empty README: {path}")
    path.write_text(content.rstrip() + "\n", encoding="utf-8")


def build_checklist(spec: str) -> str:
    pattern = re.compile(r"^\s*- \[ \] \*\*([A-Z][A-Z0-9]*-\d+)(?: · (P[012]))?\*\* (.+)$")
    rows: list[tuple[str, str, str]] = []
    for line in spec.splitlines():
        match = pattern.match(line)
        if match:
            requirement_id, priority, requirement = match.groups()
            rows.append((requirement_id, priority or "—", requirement.replace("|", "\\|")))
    if not rows:
        raise SystemExit("No requirement rows found in the supplied goal spec")

    output = [
        "# Implementation checklist",
        "",
        "Generated once from `GOAL.md`. The requirement text is preserved from the source prompt.",
        "",
        "Mark an implementation and automated-test box only after the code and relevant automated checks pass. Mark device/listening only with direct evidence from the named hardware and route. The requirement box is complete only when all required evidence exists.",
        "",
        "| Done | ID | Priority | Requirement | Implementation | Automated | Device/listening | Evidence or remaining limit |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for requirement_id, priority, requirement in rows:
        output.append(f"| [ ] | `{requirement_id}` | {priority} | {requirement} | [ ] | [ ] | [ ] | Not started |")
    additions = [
        ("WIN-01", "P0", "Build the shared application for Windows x64 with a native WASAPI backend."),
        ("WIN-02", "P0", "Verify portable preset save/load and matching DSP state across Windows, Linux, and Android builds."),
        ("WIN-03", "P0", "Run automated Windows compile, DSP, preset, and audio-backend tests; retain exact toolchain and results."),
        ("WIN-04", "P0", "Verify live full-duplex guitar input/output on Windows with an actual supported interface; keep this manual check open until performed."),
        ("DSP-01", "P0", "Measure each processor's CPU use as a percentage of the callback deadline and report the complete benchmark configuration and percentiles."),
        ("LIN-01", "P0", "Provide a reproducible Linux WAV ingest/render/regression path using prerecorded audio and deterministic settings."),
        ("LIN-02", "P0", "Exercise the local prerecorded guitar corpus through implemented DSP and retain output metrics without publishing private audio."),
        ("DATA-01", "P0", "Keep licensed prerecorded guitar fixtures outside Git, verify their source checksums, and expose them through a local-only ignored symlink."),
        ("DATA-02", "P0", "Run deterministic offline DSP regressions on documented clean pickup-position samples and a clean DI musical excerpt."),
        ("WEB-01", "P0", "Provide a browser/WASM guitar-audio test build whose Play control processes and plays the selected effect-chain output, with local-only files and automated audio-output comparison against native C++."),
    ]
    for requirement_id, priority, requirement in additions:
        output.append(f"| [ ] | `{requirement_id}` | {priority} | {requirement} | [ ] | [ ] | [ ] | Not started |")
    output.extend(["", f"Total source requirements: {len(rows)}; approved additions: {len(additions)}.", ""])
    return "\n".join(output)


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Required source spec not found: {SOURCE}")
    planned = (
        "GOAL.md", "IMPLEMENTATION_CHECKLIST.md", "EFFECTS_MATRIX.md", "RESEARCH.md",
        "ARCHITECTURE.md", "PRESET_FORMAT.md", "AUDIO_COMPATIBILITY.md",
        "LATENCY_AND_QUALITY.md", "THIRD_PARTY_NOTICES.md",
    )
    existing = [str(ROOT / relative) for relative in planned if (ROOT / relative).exists()]
    if existing:
        raise SystemExit("Refusing to overwrite existing project documents: " + ", ".join(existing))
    readme = ROOT / "README.md"
    if readme.exists() and readme.read_text(encoding="utf-8").strip():
        raise SystemExit(f"Refusing to replace non-empty README: {readme}")
    spec = SOURCE.read_text(encoding="utf-8")
    write_new("GOAL.md", spec.rstrip() + "\n\n" + MAINTAINER_ADDENDUM)
    write_empty_readme(
        """# TabletFX

An offline guitar multi-effects application for Android, Linux, and Windows. The shared C++20 DSP core is intended to pair with a touch-friendly Qt Quick editor and native full-duplex audio backends (Oboe, JACK/PipeWire, and WASAPI).

Project status: requirements and architecture are being established; no live audio path or supported hardware configuration is claimed yet.

Start with [`GOAL.md`](GOAL.md), [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md), and [`ARCHITECTURE.md`](ARCHITECTURE.md). Run `make doctor` to inspect locally available toolchains. Build and test instructions will be added as the first vertical slice becomes reproducible.
""",
    )
    write_new("IMPLEMENTATION_CHECKLIST.md", build_checklist(spec))
    write_new(
        "EFFECTS_MATRIX.md",
        """# Effects matrix

The canonical capability list is in `GOAL.md` section 10. The row-by-row implementation, automated-test, and device/listening status is tracked in [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md). No effect is considered shipped merely because it appears in the catalog or UI.

For each implemented processor, record: stable ID/version; category and exact function; source revision and license; channel layout; parameters/defaults/units; sample rates; latency; tail and bypass policy; quality modes; resource cap; measured per-block CPU percentage; automated evidence; and manual listening/hardware evidence.

Current state: no DSP processor has been integrated or verified.
""",
    )
    write_new(
        "RESEARCH.md",
        """# Research and source audit

Research baseline: `GOAL.md` was supplied with a source bibliography dated 20 November 2026. Its product-reference and component tables are research leads, not evidence that this repository has integrated or revalidated those sources.

## Required audit before adoption

- Reconfirm exact upstream commit/tag, source files, full license text, transitive dependencies, supported architectures, sample-rate behavior, algorithmic delay, CPU/memory requirements, and parameter semantics.
- Preserve GPLv3-compatible distribution; exclude GPL-2-only, unknown-license code, proprietary binaries, and assets without redistribution rights unless a compatible grant is verified.
- Record upstream links and exact versions in `THIRD_PARTY_NOTICES.md`; record integration changes and build recipe here.
- The Amp Rack audit in `GOAL.md` identifies a pinned source snapshot and warns that bundled binary libraries, commented-out source modules, and individual effect licenses must be considered separately.
- Pitch candidates in `GOAL.md` are not yet integrated, built, or measured in this project. Do not treat library buffer/window values as end-to-end latency.

## Evidence ledger

| Component or claim | Revision/version | Primary source | Finding | Status |
|---|---|---|---|---|
| TabletFX requirements | `GOAL.md`, research baseline 2026-11-20 | supplied full specification | Requirements, proposed architecture, test protocol, and references | Imported; source revalidation pending |
| Android target | itel VistaTab 11 | manufacturer specification link in `GOAL.md` [H1] | T615, Android 14, 4 GB physical RAM per supplied research | Recheck actual device ABI/build/memory before support claim |
| Amp Rack | `345b961636d0725bb267fb0012859c7c75ff9d63` | upstream links in `GOAL.md` [R1–R6] | Reference host/effect sources; license varies by file | Audit exact files before reuse |
| Pitch DSP candidates | See `GOAL.md` [D1–D4] | upstream primary links | Candidate APIs, licenses, and delay must be independently rechecked | No candidate selected |
| Qt / Oboe / JACK / WASAPI | Current versions not selected | official links in `GOAL.md` [D12, D15, H3–H7] | Proposed platform stack | Verify release support and licenses before pinning |

## Implementation-start revalidation

Record retrieved date, version/tag/commit, relevant files or manual sections, exact claim supported, and limitations. Keep hardware/editor capability distinct from desktop-editor capability. Keep channel count distinct from musical polyphony.
""",
    )
    write_new(
        "ARCHITECTURE.md",
        """# Architecture decision record 0001 — shared native application

**Status:** proposed; platform/API and licensing validation required before the first shipping build.

## Decision direction

Use a headless C++20 DSP/audio graph as the shared product core, CMake for reproducible builds, and Qt 6 Quick/QML for the editor. Implement Android audio with Oboe, Linux audio with a JACK-compatible backend (including PipeWire JACK), and Windows audio with WASAPI. Keep platform audio adapters behind one backend interface; keep preset and processor formats platform-independent. The Linux, Windows, and Android builds must share the same effect IDs and serialized parameter meanings.

The Windows backend is an explicit user requirement added to the Android/Linux scope in `GOAL.md`; it is not optional or inferred from cross-platform C++ alone.

## Alternatives considered

1. Reuse selected Amp Rack host concepts or effect source. Advantage: existing Android guitar-host work; risk: mixed per-file licenses, prebuilt-only processors, existing host lifecycle choices, and incomplete integration.
2. Adapt its Alana desktop counterpart. Advantage: desktop editor precedent; risk: does not supply one shared portable audio core or Android lifecycle.
3. Build a new shared core and integrate individually audited open-source DSP. Advantage: explicit real-time contract, cross-platform presets, and shared processor identity; cost: more engineering and source/license review.

## Selection criteria and constraints

- Demonstrable full-duplex audio on each supported host.
- No allocation, lock, wait, I/O, logging, GUI work, or graph rebuild in the audio callback.
- Bounded CPU, memory, queues, latency, graph size, and imported data.
- Prepared graph swaps with safe deferred reclamation and a known-good fallback.
- Correct licenses for the actual linked files and transitive dependencies.
- Per-processor callback cost reported as percentage of the callback time budget; whole-chain load measured separately.
- No Android Linux plugin binaries or Windows VST DLLs presented as portable processors.

## Validation gates

Before accepting this ADR: verify Qt release support for all three targets; Android Qt/Oboe lifecycle and 4 GB physical RAM; Linux JACK/PipeWire routing; Windows WASAPI endpoint/format negotiation; package sizes; build baseline; licenses; and whether native UI and backend APIs can be tested in the available environment. Record actual versions in `RESEARCH.md`. Revise this decision only with evidence.
""",
    )
    write_new(
        "PRESET_FORMAT.md",
        """# Preset format

Status: design requirement only; production schema and code are not implemented.

Presets must store schema version, stable preset/node/processor/backend IDs and versions, typed parameter values, bypass, graph connections and channel layouts, routing/mixer state, tempo, quality profile, assets with hashes/provenance, controller assignments, and scene definitions/overrides. They must not rely on absolute platform paths or select a different pitch engine silently between platforms.

Before a format is called production-ready, add a versioned JSON Schema, validation and migration code, atomic save/backup, bounded import, archive path-traversal protection, deterministic missing-asset behavior, round-trip fixtures, and Android/Linux/Windows portability tests.
""",
    )
    write_new(
        "AUDIO_COMPATIBILITY.md",
        """# Audio compatibility and hardware evidence

No audio hardware has been probed by this repository yet. All rows remain unverified until collected directly.

| Host | Required configuration | Observed device/OS/API/route | Build/audio test | Result |
|---|---|---|---|---|
| Android | itel VistaTab 11, Android 14; record actual ABI, build fingerprint and physical RAM; USB full-duplex interface | Not probed | Not run | Untested |
| Linux | Pop!_OS 22.04, x86_64; tested JACK/PipeWire route and interface | Not probed | Not run | Untested |
| Windows | Windows x64; WASAPI shared/exclusive route and interface | Not probed | Not run | Untested |

Record exact app commit, OS build, interface/firmware, USB/power arrangement, ports, sample rate, actual buffer/burst, granted sharing mode, xruns, test duration, and limitations. Never infer support from CPU model or a successful compile.
""",
    )
    write_new(
        "LATENCY_AND_QUALITY.md",
        """# Latency, quality, and DSP-load protocol

Status: protocol imported from `GOAL.md`; no project measurements exist yet.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

No latency, DSP percentage, real-device, or listening result has been measured yet.
""",
    )
    write_new(
        "THIRD_PARTY_NOTICES.md",
        """# Third-party code, assets, and notices

No third-party DSP source, binary, model, impulse response, recording, or artwork has been added to this repository at bootstrap.

Distribution target: GPLv3-compatible application with exact upstream licenses and notices preserved. GPL-2.0-only and unknown-license files are excluded unless a compatible permission is obtained. Source code licenses do not grant rights to presets, IRs, recordings, capture models, or artwork.

| Component | Exact revision/files | License | Changes | Platforms/build | Evidence |
|---|---|---|---|---|---|
| None integrated yet | — | — | — | — | — |

Before integration, update this inventory and preserve corresponding-source obligations. Do not rely on a repository's root license as blanket permission for every file.
""",
    )
    print(f"Created initial TabletFX requirements and evidence documents in {ROOT}")
    print(f"Checklist requirements parsed from source: {sum(1 for line in spec.splitlines() if re.match(r'^\s*- \[ \] \*\*[A-Z][A-Z0-9]*-\d+', line))}")


if __name__ == "__main__":
    main()
