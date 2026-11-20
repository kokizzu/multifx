# Latency, quality, and DSP-load protocol

Status: protocol imported from `GOAL.md`; no project measurements exist yet.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Initial per-processor CPU baseline (Linux, 2026-11-20)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. The benchmark resets the same deterministic buffer before each timed call and measures only `Effect::process()`; source-buffer copy and setup are outside the timed interval.

| Processor | Median % | p95 % | p99 % | Maximum % |
|---|---:|---:|---:|---:|
| Gain | 0.0015 | 0.0015 | 0.0015 | 0.0049 |
| Noise gate | 0.0191 | 0.0217 | 0.0278 | 0.4954 |
| Soft drive | 0.1245 | 0.1403 | 0.2175 | 0.5006 |
| Delay | 0.0210 | 0.0232 | 0.0236 | 0.3004 |

This is a processor-call baseline on the current desktop CPU, not a live callback or whole-chain measurement. Maximum values include host scheduling noise. Re-run on target devices and after each DSP change; keep Android tablet, Linux interface, and Windows results separate.
