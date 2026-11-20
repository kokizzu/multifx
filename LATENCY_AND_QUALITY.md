# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0060 | 0.0105 | 0.0116 | 0.1800 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.1856 | 56 |
| High-pass | 0.0326 | 0.0326 | 0.0330 | 0.0330 | 0.2280 | 64 |
| Low-pass | 0.0326 | 0.0327 | 0.0364 | 0.1384 | 0.4632 | 64 |
| Noise gate | 0.0195 | 0.0199 | 0.0199 | 0.0341 | 0.2179 | 64 |
| Pedal compressor | 0.1159 | 0.1167 | 0.1609 | 0.3338 | 0.5996 | 72 |
| Studio compressor | 0.1087 | 0.1099 | 0.1407 | 0.3510 | 0.4091 | 64 |
| Wah | 0.0529 | 0.0600 | 0.0668 | 0.3139 | 1.3654 | 104 |
| Envelope wah | 0.1020 | 0.1279 | 0.1463 | 0.4260 | 0.8318 | 616 |
| Clean boost | 0.0049 | 0.0049 | 0.0049 | 0.0086 | 0.2190 | 48 |
| Soft drive | 0.1406 | 0.2205 | 0.2299 | 0.3837 | 0.6248 | 16 |
| Tremolo | 0.0589 | 0.0645 | 0.0671 | 0.3053 | 1.2701 | 96 |
| Three-band parametric EQ | 0.0638 | 0.0649 | 0.0986 | 0.2797 | 0.4114 | 160 |
| Delay (2 s stereo allocation) | 0.0240 | 0.0248 | 0.0255 | 0.2591 | 5.4714 | 768,080 |
| Output gain | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.1511 | 40 |
| Peak limiter | 0.0195 | 0.0195 | 0.0308 | 0.0330 | 0.2772 | 32 |
| Signal controls (input + dry/wet) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0038 | 80 |
| Full sixteen-effect chain + controls | 0.6394 | 0.6540 | 0.8565 | 4.8767 | 10.0953 | 769,696 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. Input mute runs before that sequence and the global dry/wet crossfade runs after it. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls and the chain's signal-control/copy work; buffer setup is outside the timed region. Memory is processor objects plus the signal-control object and directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. Signal controls use 80 B and measured 0.0011% p50 / 0.0015% p95 of the block deadline. The envelope wah uses 616 B and measured 0.1020% p50 / 0.1279% p95. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
