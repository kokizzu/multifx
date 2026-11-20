# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0049 | 0.0053 | 0.0053 | 0.0086 | 0.8216 | 40 |
| Volume pedal | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.0068 | 56 |
| High-pass | 0.0368 | 0.0409 | 0.0420 | 0.2543 | 1.2960 | 64 |
| Low-pass | 0.0372 | 0.0480 | 0.0514 | 0.2329 | 0.3806 | 64 |
| Noise gate | 0.0229 | 0.0413 | 0.0499 | 0.2479 | 0.7834 | 64 |
| Pedal compressor | 0.1024 | 0.1414 | 0.1463 | 0.3488 | 10.5918 | 72 |
| Studio compressor | 0.1069 | 0.1087 | 0.1391 | 0.3173 | 0.7954 | 64 |
| Wah | 0.0469 | 0.0533 | 0.0585 | 0.2865 | 0.3252 | 104 |
| Envelope wah | 0.1133 | 0.1294 | 0.1459 | 0.3656 | 0.4185 | 616 |
| Clean boost | 0.0045 | 0.0045 | 0.0090 | 0.0097 | 0.0105 | 48 |
| Soft drive | 0.1418 | 0.1433 | 0.2292 | 0.4069 | 0.9747 | 16 |
| Tremolo | 0.0641 | 0.0743 | 0.0765 | 0.2820 | 0.3454 | 96 |
| Three-band parametric EQ | 0.0570 | 0.0641 | 0.0908 | 0.3124 | 0.8561 | 160 |
| Delay (2 s stereo allocation) | 0.0236 | 0.0251 | 0.0281 | 0.1422 | 0.3154 | 768,080 |
| Output gain | 0.0049 | 0.0083 | 0.0113 | 0.0124 | 0.3630 | 40 |
| Peak limiter | 0.0199 | 0.0199 | 0.0330 | 0.0371 | 0.2974 | 32 |
| Full sixteen-effect chain | 0.7043 | 0.9518 | 1.1940 | 1.4314 | 8.0575 | 769,616 |

The full chain is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls only; buffer copy/setup are outside the timed region. Memory is processor objects plus directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. The envelope wah contributes 616 B to this chain and measured 0.1133% p50 / 0.1294% p95 of the block deadline. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
