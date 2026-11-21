# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 19 effects plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0064 | 0.0064 | 0.0075 | 0.0154 | 0.3206 | 40 |
| Output gain | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 0.0068 | 40 |
| Volume pedal | 0.0064 | 0.0083 | 0.0146 | 0.0244 | 0.8419 | 56 |
| High-pass | 0.0375 | 0.0379 | 0.0465 | 0.2742 | 0.4061 | 64 |
| Low-pass | 0.0379 | 0.0454 | 0.0476 | 0.2726 | 0.3881 | 64 |
| Noise gate | 0.0232 | 0.0236 | 0.0237 | 0.0416 | 0.2696 | 64 |
| Pedal compressor | 0.0953 | 0.0960 | 0.1365 | 0.3124 | 0.6634 | 72 |
| Studio compressor | 0.0881 | 0.1125 | 0.1193 | 0.3135 | 0.4140 | 64 |
| Wah | 0.0431 | 0.0435 | 0.0439 | 0.2220 | 0.2640 | 104 |
| Envelope wah | 0.1031 | 0.1039 | 0.1151 | 0.3038 | 0.3735 | 616 |
| Clean boost | 0.0049 | 0.0056 | 0.0109 | 0.0116 | 0.2224 | 48 |
| Soft drive | 0.1451 | 0.1463 | 0.2382 | 0.4673 | 0.8644 | 16 |
| Tremolo | 0.0585 | 0.0593 | 0.0615 | 0.2288 | 0.3465 | 96 |
| Chorus | 0.0427 | 0.0431 | 0.0705 | 0.2408 | 1.0395 | 12,448 |
| Flanger | 0.0518 | 0.0795 | 0.0821 | 0.2805 | 0.3825 | 4,792 |
| Three-band parametric EQ | 0.0529 | 0.0533 | 0.0844 | 0.2355 | 0.3863 | 160 |
| Delay (2 s stereo allocation) | 0.0244 | 0.0251 | 0.0263 | 0.0296 | 0.3807 | 768,080 |
| Poly pitch shifter, mono | 5.2385 | 6.4907 | 7.5377 | 9.7409 | 11.4663 | 311,056 |
| Poly pitch shifter, stereo | 10.7729 | 13.3459 | 14.5770 | 18.4021 | 19.7180 | 494,016 |
| Peak limiter | 0.0165 | 0.0165 | 0.0278 | 0.0308 | 0.2397 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0019 | 0.0026 | 0.0034 | 0.2037 | 80 |
| Input + output meters | 0.0139 | 0.0143 | 0.0240 | 0.0263 | 0.2734 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0038 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Mono → stereo | 0.0019 | 0.0023 | 0.0023 | 0.0023 | 0.0038 | 0 |
| Parallel split (copy to two branches) | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0.0023 | 0 |
| Parallel mix (dry + two branches) | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2078 | 112 |
| Chromatic tuner monitor | 0.0083 | 0.4286 | 0.5471 | 0.7598 | 4.0629 | 10,400 |
| Full 19-effect chain + controls + meters | 12.3675 | 14.2346 | 16.8109 | 19.5343 | 21.0576 | 1,281,024 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Chorus → Flanger → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. On this Linux build, mono uses 5.2385% p50 / 6.4907% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 10.7729% / 13.3459% and about 482 KiB. The full 19-effect stereo chain uses 12.3675% p50 / 14.2346% p95. The chorus alone uses 0.0427% p50 / 0.0431% p95 and 12,448 B in stereo; the flanger uses 0.0518% / 0.0795% and 4,792 B. Both have zero fixed dry-path latency. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the chain with the pitch effect enabled at +5 semitones. HeadlessChrome 151 passed native/WASM full-chain output comparison with negative flanger feedback enabled: 0.00001885 maximum absolute error and 0.00000249 RMS error (0.0528% reference RMS), within the test's dynamic 6% peak and 7.5% RMS bounds. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
