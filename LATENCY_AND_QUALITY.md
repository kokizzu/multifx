# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. Each effect row resets the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. Utility rows time their respective conversion, split, or mix operation, including the operation's validation and sample work. The full-chain row measures the integrated 16-effect path plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0049 | 0.0049 | 0.0049 | 0.0094 | 0.0097 | 40 |
| Output gain | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.1804 | 40 |
| Volume pedal | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 1.6651 | 56 |
| High-pass | 0.0326 | 0.0330 | 0.0375 | 0.1920 | 1.5683 | 64 |
| Low-pass | 0.0379 | 0.0461 | 0.0488 | 0.4016 | 3.9226 | 64 |
| Noise gate | 0.0199 | 0.0338 | 0.0427 | 0.2573 | 1.2604 | 64 |
| Pedal compressor | 0.1159 | 0.1170 | 0.1628 | 0.4346 | 1.0785 | 72 |
| Studio compressor | 0.1084 | 0.1087 | 0.1403 | 0.2730 | 0.3300 | 64 |
| Wah | 0.0536 | 0.0600 | 0.0623 | 0.2782 | 0.9668 | 104 |
| Envelope wah | 0.1283 | 0.1290 | 0.1440 | 0.3926 | 0.5862 | 616 |
| Clean boost | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.1782 | 48 |
| Soft drive | 0.1444 | 0.2303 | 0.2412 | 0.7519 | 1.9801 | 16 |
| Tremolo | 0.0724 | 0.0731 | 0.0735 | 0.2674 | 0.8216 | 96 |
| Three-band parametric EQ | 0.0578 | 0.0660 | 0.0979 | 0.2325 | 0.8108 | 160 |
| Delay (2 s stereo allocation) | 0.0240 | 0.0248 | 0.0255 | 0.1384 | 0.2737 | 768,080 |
| Peak limiter | 0.0177 | 0.0203 | 0.0304 | 0.0353 | 0.2186 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0030 | 80 |
| Input + output meters | 0.0173 | 0.0176 | 0.0323 | 0.2010 | 0.2741 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0015 | 0.0019 | 0.0019 | 0.0023 | 0 |
| Mono → stereo | 0.0023 | 0.0026 | 0.0026 | 0.0026 | 0.2250 | 0 |
| Parallel split (copy to two branches) | 0.0026 | 0.0030 | 0.0030 | 0.0030 | 0.0038 | 0 |
| Parallel mix (dry + two branches) | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.1710 | 112 |
| Full sixteen-effect chain + controls + meters | 0.8025 | 0.9941 | 1.2233 | 1.5443 | 6.3740 | 769,768 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Effect timing includes processor calls, both level-meter passes and their status reads, and signal-control/copy work; buffer setup is outside the timed region. The separate converter and parallel-routing measurements include validation plus sample copy/mix work. The full-chain row does not include an active parallel graph. Memory is processor objects plus signal controls, two meter objects, and directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, backend/runtime overhead, and caller-provided utility buffers. Signal controls use 80 B and measured 0.0015% p50 / 0.0019% p95 of the block deadline. Both level meters use 72 B and measured 0.0173% p50 / 0.0176% p95 together. The envelope wah uses 616 B and measured 0.1283% p50 / 0.1290% p95. The converter has no persistent storage; average downmix measured 0.0015% p50 / 0.0015% p95, equal-power downmix 0.0015% / 0.0015%, and mono upmix 0.0023% / 0.0026%. Parallel split measured 0.0026% / 0.0030%; the mixer uses 112 B and measured 0.0060% / 0.0064%. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
