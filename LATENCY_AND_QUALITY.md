# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 21 effects plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0049 | 0.0060 | 0.0094 | 0.0105 | 0.0188 | 40 |
| Output gain | 0.0045 | 0.0045 | 0.0045 | 0.0086 | 0.3563 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.1879 | 56 |
| High-pass | 0.0326 | 0.0327 | 0.0364 | 0.2288 | 0.5119 | 64 |
| Low-pass | 0.0330 | 0.0379 | 0.0416 | 0.2674 | 2.8553 | 64 |
| Noise gate | 0.0221 | 0.0225 | 0.0229 | 0.0413 | 0.3784 | 64 |
| Pedal compressor | 0.0975 | 0.0979 | 0.0983 | 0.2782 | 0.3222 | 72 |
| Studio compressor | 0.0900 | 0.0908 | 0.0911 | 0.2880 | 0.3615 | 64 |
| Wah | 0.0446 | 0.0499 | 0.0518 | 0.2888 | 0.4905 | 104 |
| Envelope wah | 0.1069 | 0.1204 | 0.1241 | 0.4227 | 0.5974 | 616 |
| Clean boost | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2269 | 48 |
| Soft drive | 0.1451 | 0.1586 | 0.2374 | 0.3870 | 0.5277 | 16 |
| Tremolo | 0.0604 | 0.0608 | 0.0615 | 0.2959 | 0.5168 | 96 |
| Chorus | 0.0450 | 0.0454 | 0.0690 | 0.2666 | 3.6668 | 12,448 |
| Flanger | 0.0529 | 0.0536 | 0.0829 | 0.2828 | 0.3225 | 4,792 |
| Phaser | 0.0671 | 0.0675 | 0.0679 | 0.2359 | 0.2681 | 280 |
| Vibrato | 0.0427 | 0.0431 | 0.0705 | 0.1744 | 0.3551 | 1,696 |
| Three-band parametric EQ | 0.0548 | 0.0559 | 0.0563 | 0.2603 | 0.3446 | 160 |
| Delay (2 s stereo allocation) | 0.0244 | 0.0244 | 0.0255 | 0.0360 | 0.2888 | 768,080 |
| Poly pitch shifter, mono | 5.3439 | 7.3719 | 8.8514 | 10.5059 | 15.2640 | 311,056 |
| Poly pitch shifter, stereo | 10.9001 | 13.0331 | 15.6578 | 18.0327 | 21.6264 | 494,016 |
| Peak limiter | 0.0165 | 0.0165 | 0.0165 | 0.0173 | 0.3113 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 80 |
| Input + output meters | 0.0146 | 0.0146 | 0.0146 | 0.0147 | 0.4643 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Mono → stereo | 0.0030 | 0.0030 | 0.0030 | 0.0068 | 0.2115 | 0 |
| Parallel split (copy to two branches) | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0.0026 | 0 |
| Parallel mix (dry + two branches) | 0.0053 | 0.0071 | 0.0083 | 0.0094 | 0.2925 | 112 |
| Chromatic tuner monitor | 0.0083 | 0.4358 | 0.6158 | 0.7980 | 0.9405 | 10,400 |
| Full 21-effect chain + controls + meters | 13.0530 | 13.9841 | 17.1207 | 20.6454 | 30.6601 | 1,283,000 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. On this Linux build, mono uses 5.3439% p50 / 7.3719% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 10.9001% / 13.0331% and about 482 KiB. The full 21-effect stereo chain uses 13.0530% p50 / 13.9841% p95, with a 30.6601% maximum in this run. Chorus uses 0.0450% p50 / 0.0454% p95 and 12,448 B; flanger uses 0.0529% / 0.0536% and 4,792 B; phaser uses 0.0671% / 0.0675% and 280 B; vibrato uses 0.0427% / 0.0431% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 21-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed native/WASM comparison with negative flanger feedback, six-stage phaser, and vibrato enabled: 0.0000169900 maximum absolute error and 0.00000183320 RMS error (0.07965% of reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 843,876 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse; the latest browser run reported an average 9.4% pitch-stage estimate and 11.6% whole-callback estimate. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
