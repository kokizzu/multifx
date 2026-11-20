# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0064 | 0.0068 | 0.0068 | 0.0079 | 0.0101 | 40 |
| Volume pedal | 0.0060 | 0.0064 | 0.0113 | 0.0120 | 0.2794 | 56 |
| High-pass | 0.0371 | 0.0413 | 0.0473 | 0.2224 | 0.8187 | 64 |
| Low-pass | 0.0368 | 0.0371 | 0.0372 | 0.2367 | 1.3714 | 64 |
| Noise gate | 0.0214 | 0.0217 | 0.0360 | 0.0439 | 0.5951 | 40 |
| Pedal compressor | 0.1024 | 0.1376 | 0.1646 | 0.5809 | 1.2664 | 72 |
| Studio compressor | 0.1080 | 0.1391 | 0.1448 | 0.4286 | 1.1295 | 64 |
| Soft drive | 0.1414 | 0.2164 | 0.2337 | 0.6465 | 1.2192 | 16 |
| Three-band parametric EQ | 0.0656 | 0.0664 | 0.0986 | 0.2809 | 0.3079 | 160 |
| Delay (2 s stereo allocation) | 0.0236 | 0.0244 | 0.0255 | 0.2516 | 3.2904 | 768,080 |
| Output gain | 0.0049 | 0.0056 | 0.0056 | 0.0056 | 0.2869 | 40 |
| Peak limiter | 0.0203 | 0.0203 | 0.0349 | 0.0386 | 0.4740 | 32 |
| Full twelve-effect chain | 0.5333 | 0.6867 | 0.7894 | 1.1258 | 4.5781 | 768,728 |

The full chain is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Soft Drive → Parametric EQ → Delay → Output Gain → Peak Limiter. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls only; buffer copy/setup are outside the timed region. Memory is processor objects plus directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
