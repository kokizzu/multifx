# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0045 | 0.0045 | 0.0097 | 0.0105 | 0.0143 | 40 |
| Volume pedal | 0.0053 | 0.0056 | 0.0056 | 0.0060 | 0.6023 | 56 |
| High-pass | 0.0401 | 0.0435 | 0.0480 | 0.2475 | 0.2978 | 64 |
| Low-pass | 0.0398 | 0.0402 | 0.0469 | 0.2528 | 0.6364 | 64 |
| Noise gate | 0.0240 | 0.0244 | 0.0244 | 0.1178 | 0.9383 | 64 |
| Pedal compressor | 0.0971 | 0.1361 | 0.1395 | 0.3897 | 0.4684 | 72 |
| Studio compressor | 0.0911 | 0.1189 | 0.1474 | 0.5468 | 0.9068 | 64 |
| Wah | 0.0435 | 0.0439 | 0.0495 | 0.2584 | 0.3079 | 104 |
| Clean boost | 0.0041 | 0.0041 | 0.0041 | 0.0042 | 0.2029 | 48 |
| Soft drive | 0.1511 | 0.1526 | 0.2505 | 0.7099 | 0.9420 | 16 |
| Tremolo | 0.0593 | 0.0596 | 0.0597 | 0.2554 | 0.3934 | 96 |
| Three-band parametric EQ | 0.0536 | 0.0544 | 0.0735 | 0.2722 | 0.2993 | 160 |
| Delay (2 s stereo allocation) | 0.0251 | 0.0252 | 0.0263 | 0.0375 | 0.3008 | 768,080 |
| Output gain | 0.0056 | 0.0116 | 0.0124 | 0.0135 | 0.0232 | 40 |
| Peak limiter | 0.0165 | 0.0165 | 0.0165 | 0.0289 | 0.2760 | 32 |
| Full fifteen-effect chain | 0.5367 | 0.6420 | 0.8014 | 1.1052 | 4.5770 | 769,000 |

The full chain is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls only; buffer copy/setup are outside the timed region. Memory is processor objects plus directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
