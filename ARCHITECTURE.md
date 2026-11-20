# Architecture decision record 0001 — shared native application

**Status:** proposed; platform/API and licensing validation required before the first shipping build.

## Decision direction

Use a headless C++20 DSP/audio graph as the shared product core, CMake for reproducible builds, and Qt 6 Quick/QML for the editor. Implement Android audio with Oboe, Linux audio with a JACK-compatible backend (including PipeWire JACK), and Windows audio with WASAPI. Keep platform audio adapters behind one backend interface; keep preset and processor formats platform-independent. The Linux, Windows, and Android builds must share the same effect IDs and serialized parameter meanings.

The Windows backend is an explicit user requirement added to the Android/Linux scope in `GOAL.md`; it is not optional or inferred from cross-platform C++ alone.

## Channel-layout conversion

The shared DSP core provides a stateless mono/stereo converter for 1- and 2-channel interleaved float audio. Mono-to-stereo duplicates each sample; stereo-to-mono defaults to arithmetic average `(L + R) / 2`, with an explicit equal-power option `(L + R) / sqrt(2)`. Equal-power can exceed full scale and does not clip or normalize. Same-layout conversion preserves samples. The converter does not resample, spatialize, or accept layouts above stereo. It performs no allocation or persistent-state work, permits exact-start in-place conversion, and rejects partial buffer overlap. The offline WAV renderer exposes output layout and summing policy; the browser/WASM module compiles and self-checks the same code.

## Parallel routing utility (UTL-05)

The shared core provides `split_to_parallel` and `ParallelMixer` primitives for mono/stereo float blocks. Split copies the source into two preallocated caller buffers; the mixer sums dry audio and two branches with independent gains, stereo balance, and per-branch phase inversion. The default is dry-only. Gain/balance/polarity changes ramp over 10 ms unless configured otherwise. Processing is stateless, allocation-free, and adds no algorithmic latency; the mixer object occupies 112 B. Buffer/layout errors are rejected and documented exact-start aliasing is supported. Native CTest, the WASM startup self-check, and a Windows cross-build exercise it. This does not yet mean the preset graph/editor or live scheduler can create parallel routes; those remain separate integration work. Linux 48 kHz/128-frame stereo p50/p95 is 0.0030/0.0030% of the block deadline for splitting and 0.0064/0.0094% for mixing.

## Browser/WASM test target

Add a browser test application that reuses the C++ DSP processors compiled to WebAssembly, rather than maintaining a second set of effect algorithms. Use a Web Audio `AudioWorklet` to feed fixed-size blocks into the selected effect chain and send its processed output to the browser's audio destination. Load a user's local guitar audio file in the page; do not upload it. The Play button is a user gesture that starts/resumes audio and must play the processed chain result, not the dry input. Provide stop/rewind and dry/wet comparison controls.

Prepare the WASM module, effect state, and bounded buffers before playback. The render callback must not allocate, block, log, access files, or rebuild the graph. Add browser automation that loads a local prerecorded test WAV, activates the chain, presses Play, captures the output, verifies non-silent processed audio, and compares deterministic WASM output with the native offline C++ renderer within a documented numeric tolerance. Report the browser/version, sample rate, render block/callback deadline, CPU percentage for each processor and the whole chain, and memory use. Browser playback and benchmarks do not prove native Oboe, JACK/PipeWire, or WASAPI device support or live round-trip latency.

The browser/WASM test page uses the same C++ DSP chain in an AudioWorklet; its build, headless audio comparison, memory figures, and timer-resolution limitation are recorded in `AUDIO_COMPATIBILITY.md` and `IMPLEMENTATION_CHECKLIST.md`. The implementation is an audition/test page, not a full graph editor. Manual audible-output verification remains open.

## Windows WASAPI backend status

An initial event-driven shared-mode WASAPI backend now lives in `include/tabletfx/audio/wasapi_backend.hpp` and `src/audio/wasapi_backend.cpp`. It opens the default capture/render endpoints, accepts supported PCM 8/16/24/32-bit or float32 mix formats, converts into bounded float32 scratch buffers, splits endpoint events into prepared blocks, and exposes startup errors and capture underrun/overrun counters. The processing callback performs no repository-owned allocation or lock operations.

`make windows-cross-build` cross-compiles the shared DSP/offline CLI and a WASAPI object-lifecycle smoke executable with MinGW-w64. This does not run the executable or establish Windows runtime support. The initial backend requires equal capture/render sample rates and currently has no device selection, resampling, exclusive mode, GUI integration, or hardware latency measurement; keep those checks open in `AUDIO_COMPATIBILITY.md`.

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
