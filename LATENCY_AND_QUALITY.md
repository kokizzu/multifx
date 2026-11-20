# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0060 | 0.0060 | 0.0124 | 0.2749 | 40 |
| Output gain | 0.0049 | 0.0109 | 0.0120 | 0.0146 | 0.0311 | 40 |
| Volume pedal | 0.0049 | 0.0049 | 0.0113 | 0.0124 | 0.0195 | 56 |
| High-pass | 0.0293 | 0.0296 | 0.0338 | 0.1714 | 0.2501 | 64 |
| Low-pass | 0.0293 | 0.0341 | 0.0375 | 0.0420 | 0.2577 | 64 |
| Noise gate | 0.0180 | 0.0274 | 0.0338 | 0.0364 | 0.4751 | 64 |
| Pedal compressor | 0.1024 | 0.1279 | 0.1339 | 0.3750 | 0.6132 | 72 |
| Studio compressor | 0.0866 | 0.0870 | 0.0874 | 0.2985 | 0.3702 | 64 |
| Wah | 0.0424 | 0.0431 | 0.0488 | 0.2381 | 0.4392 | 104 |
| Envelope wah | 0.1024 | 0.1174 | 0.1275 | 0.3458 | 1.2135 | 616 |
| Clean boost | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.2014 | 48 |
| Soft drive | 0.1133 | 0.1560 | 0.1823 | 0.3720 | 1.5304 | 16 |
| Tremolo | 0.0578 | 0.0581 | 0.0585 | 0.2130 | 0.4718 | 96 |
| Three-band parametric EQ | 0.0514 | 0.0533 | 0.0814 | 0.2411 | 0.2812 | 160 |
| Delay (2 s stereo allocation) | 0.0188 | 0.0191 | 0.0195 | 0.0206 | 0.2179 | 768,080 |
| Peak limiter | 0.0161 | 0.0165 | 0.0210 | 0.0221 | 0.2179 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0023 | 80 |
| Input + output meters | 0.0139 | 0.0143 | 0.0232 | 0.0255 | 0.1819 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.1699 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Mono → stereo | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0.1485 | 0 |
| Full sixteen-effect chain + controls + meters | 0.6630 | 0.8929 | 0.9293 | 1.1674 | 2.1814 | 769,768 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Effect timing includes processor calls, both level-meter passes and their status reads, and signal-control/copy work; buffer setup is outside the timed region. The separate converter measurements include channel-count/buffer validation and sample conversion. Memory is processor objects plus signal controls, two meter objects, and directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, backend/runtime overhead, and caller-provided converter buffers. Signal controls use 80 B and measured 0.0015% p50 / 0.0015% p95 of the block deadline. Both level meters use 72 B and measured 0.0139% p50 / 0.0143% p95 together. The envelope wah uses 616 B and measured 0.1024% p50 / 0.1174% p95. The converter has no persistent storage; average downmix measured 0.0011% p50 / 0.0015% p95, equal-power downmix 0.0011% / 0.0015%, and mono upmix 0.0019% / 0.0019%. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
