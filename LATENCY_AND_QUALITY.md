# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0053 | 0.0060 | 0.0064 | 0.0064 | 0.1545 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.1613 | 56 |
| High-pass | 0.0326 | 0.0409 | 0.0409 | 0.3038 | 1.2458 | 64 |
| Low-pass | 0.0323 | 0.0326 | 0.0330 | 0.0394 | 0.3188 | 64 |
| Noise gate | 0.0214 | 0.0217 | 0.0289 | 0.0401 | 0.2997 | 64 |
| Pedal compressor | 0.1163 | 0.1669 | 0.1770 | 0.4624 | 0.6282 | 72 |
| Studio compressor | 0.1084 | 0.1500 | 0.1598 | 0.3274 | 0.4279 | 64 |
| Wah | 0.0469 | 0.0525 | 0.0585 | 0.1976 | 0.2640 | 104 |
| Soft drive | 0.1433 | 0.1444 | 0.2292 | 0.4463 | 1.4738 | 16 |
| Tremolo | 0.0728 | 0.0735 | 0.0743 | 0.2929 | 0.4065 | 96 |
| Three-band parametric EQ | 0.0570 | 0.0716 | 0.0979 | 0.4016 | 0.7965 | 160 |
| Delay (2 s stereo allocation) | 0.0240 | 0.0248 | 0.0251 | 0.0296 | 0.2925 | 768,080 |
| Output gain | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.3330 | 40 |
| Peak limiter | 0.0195 | 0.0199 | 0.0311 | 0.0349 | 0.2396 | 32 |
| Full fourteen-effect chain | 0.6758 | 0.8082 | 0.9623 | 1.9752 | 20.7707 | 768,952 |

The full chain is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls only; buffer copy/setup are outside the timed region. Memory is processor objects plus directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise; the 20.7707% full-chain maximum is a clear outlier and does not represent typical load. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
