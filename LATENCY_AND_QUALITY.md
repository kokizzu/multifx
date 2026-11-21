# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 20 effects plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0060 | 0.0064 | 0.0064 | 0.0116 | 1.1509 | 40 |
| Output gain | 0.0064 | 0.0109 | 0.0116 | 0.0169 | 0.6690 | 40 |
| Volume pedal | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.2216 | 56 |
| High-pass | 0.0375 | 0.0446 | 0.0465 | 0.2532 | 0.9829 | 64 |
| Low-pass | 0.0326 | 0.0375 | 0.0431 | 0.1568 | 0.7635 | 64 |
| Noise gate | 0.0229 | 0.0379 | 0.0409 | 0.2010 | 0.5783 | 64 |
| Pedal compressor | 0.0960 | 0.1193 | 0.1384 | 0.3743 | 0.4354 | 72 |
| Studio compressor | 0.0885 | 0.0889 | 0.1144 | 0.3386 | 0.4185 | 64 |
| Wah | 0.0548 | 0.0630 | 0.0675 | 0.3574 | 1.2203 | 104 |
| Envelope wah | 0.1316 | 0.1478 | 0.1598 | 1.6422 | 2.5909 | 616 |
| Clean boost | 0.0053 | 0.0056 | 0.0060 | 0.0113 | 0.1935 | 48 |
| Soft drive | 0.1448 | 0.1455 | 0.2370 | 0.4320 | 0.7995 | 16 |
| Tremolo | 0.0743 | 0.0750 | 0.0757 | 0.2876 | 1.6549 | 96 |
| Chorus | 0.0578 | 0.0581 | 0.0634 | 0.3034 | 1.5634 | 12,448 |
| Flanger | 0.0679 | 0.0686 | 0.1020 | 0.3600 | 1.7393 | 4,792 |
| Phaser | 0.0829 | 0.0836 | 0.1215 | 0.3769 | 1.7266 | 280 |
| Three-band parametric EQ | 0.0533 | 0.0540 | 0.0784 | 0.3060 | 0.4838 | 160 |
| Delay (2 s stereo allocation) | 0.0240 | 0.0270 | 0.0330 | 0.5396 | 1.2713 | 768,080 |
| Poly pitch shifter, mono | 5.2667 | 6.1955 | 6.7513 | 9.5283 | 11.7127 | 311,056 |
| Poly pitch shifter, stereo | 13.1797 | 13.7081 | 15.3147 | 19.2797 | 20.0597 | 494,016 |
| Peak limiter | 0.0161 | 0.0165 | 0.0274 | 0.2194 | 0.2737 | 32 |
| Signal controls (input + dry/wet) | 0.0019 | 0.0019 | 0.0019 | 0.0026 | 1.5045 | 80 |
| Input + output meters | 0.0180 | 0.0244 | 0.0323 | 0.0427 | 0.3518 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0038 | 0.0045 | 0.0053 | 0.0143 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0 |
| Mono → stereo | 0.0038 | 0.0038 | 0.0038 | 0.0038 | 0.2130 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0041 | 0.0045 | 0.0053 | 0.2063 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0079 | 0.0094 | 0.0109 | 1.5615 | 112 |
| Chromatic tuner monitor | 0.0105 | 0.5370 | 0.7917 | 2.0378 | 2.5088 | 10,400 |
| Full 20-effect chain + controls + meters | 14.9921 | 16.5473 | 19.8733 | 23.2424 | 25.1332 | 1,281,304 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. On this Linux build, mono uses 5.2667% p50 / 6.1955% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 13.1797% / 13.7081% and about 482 KiB. The full 20-effect stereo chain uses 14.9921% p50 / 16.5473% p95, with a 25.1332% maximum in this run. The chorus uses 0.0578% p50 / 0.0581% p95 and 12,448 B; the flanger uses 0.0679% / 0.0686% and 4,792 B; the phaser uses 0.0829% / 0.0836% and 280 B. Each modulation effect adds zero fixed dry-path latency. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 20-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed native/WASM comparison with negative flanger feedback and six-stage phaser enabled: 0.00029360 maximum absolute error and 0.00002968 RMS error (0.8321% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 842,980 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse; this run reported a 9.5% pitch-stage estimate and an 11.0% whole-callback estimate. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
