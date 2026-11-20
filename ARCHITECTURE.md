# Architecture decision record 0001 — shared native application

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
