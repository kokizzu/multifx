# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.6308 | 40 |
| High-pass | 0.0326 | 0.0371 | 0.0394 | 0.1519 | 0.2741 | 64 |
| Low-pass | 0.0326 | 0.0405 | 0.0458 | 0.1095 | 0.7264 | 64 |
| Noise gate | 0.0188 | 0.0191 | 0.0248 | 0.0300 | 0.2044 | 40 |
| Pedal compressor | 0.1028 | 0.1459 | 0.1515 | 0.3881 | 1.1948 | 72 |
| Studio compressor | 0.0941 | 0.1260 | 0.1316 | 0.2760 | 0.3657 | 64 |
| Soft drive | 0.1253 | 0.2007 | 0.2078 | 0.4549 | 1.3699 | 16 |
| Three-band parametric EQ | 0.0570 | 0.0574 | 0.0866 | 0.2231 | 0.3056 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0210 | 0.0214 | 0.0232 | 0.2460 | 768,080 |
| Output gain | 0.0053 | 0.0056 | 0.0101 | 0.0109 | 0.2569 | 40 |
| Full ten-effect chain | 0.5445 | 0.7789 | 0.8659 | 1.1115 | 1.7877 | 768,640 |

The full chain is Input Gain → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Soft Drive → Parametric EQ → Delay → Output Gain. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Timing includes processor calls only; buffer copy/setup are outside the timed region. Memory is processor objects plus directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, and backend/runtime overhead. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
