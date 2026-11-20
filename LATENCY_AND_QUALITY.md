# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0064 | 0.0075 | 0.0146 | 0.1759 | 0.2603 | 40 |
| Volume pedal | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2216 | 56 |
| High-pass | 0.0371 | 0.0375 | 0.0379 | 0.2153 | 0.3278 | 64 |
| Low-pass | 0.0326 | 0.0409 | 0.0420 | 0.4399 | 4.8106 | 64 |
| Noise gate | 0.0199 | 0.0206 | 0.0364 | 0.1481 | 0.2764 | 64 |
| Pedal compressor | 0.1189 | 0.1751 | 0.2888 | 1.1052 | 1.9805 | 72 |
| Studio compressor | 0.1099 | 0.1455 | 0.1552 | 0.4774 | 1.0433 | 64 |
| Wah | 0.0540 | 0.0600 | 0.0626 | 0.2603 | 0.3570 | 104 |
| Envelope wah | 0.1309 | 0.1463 | 0.1526 | 0.4961 | 1.0088 | 616 |
| Clean boost | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 0.2756 | 48 |
| Soft drive | 0.1440 | 0.2306 | 0.2547 | 0.8295 | 1.3350 | 16 |
| Tremolo | 0.0746 | 0.0750 | 0.0754 | 0.2408 | 0.5093 | 96 |
| Three-band parametric EQ | 0.0664 | 0.0671 | 0.0675 | 0.3338 | 0.4301 | 160 |
| Delay (2 s stereo allocation) | 0.0240 | 0.0248 | 0.0251 | 0.0364 | 0.3367 | 768,080 |
| Output gain | 0.0064 | 0.0064 | 0.0064 | 0.0150 | 0.4905 | 40 |
| Peak limiter | 0.0203 | 0.0206 | 0.0206 | 0.0364 | 0.2288 | 32 |
| Signal controls (input + dry/wet) | 0.0019 | 0.0026 | 0.0026 | 0.0038 | 0.1647 | 80 |
| Input + output meters | 0.0180 | 0.0180 | 0.0326 | 0.0398 | 0.3379 | 72 |
| Full sixteen-effect chain + controls + meters | 0.8438 | 1.0054 | 1.2064 | 3.4692 | 11.0388 | 769,768 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls, both level-meter passes and their status reads, and signal-control/copy work; buffer setup is outside the timed region. Memory is processor objects plus signal controls, two meter objects, and directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. Signal controls use 80 B and measured 0.0019% p50 / 0.0026% p95 of the block deadline. Both level meters use 72 B and measured 0.0180% p50 / 0.0180% p95 together. The envelope wah uses 616 B and measured 0.1309% p50 / 0.1463% p95. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
