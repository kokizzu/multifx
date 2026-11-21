# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 25 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0053 | 0.0109 | 0.0128 | 0.0135 | 0.2839 | 40 |
| Output gain | 0.0053 | 0.0056 | 0.0056 | 0.0057 | 0.1796 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 56 |
| High-pass | 0.0326 | 0.0327 | 0.0409 | 0.0454 | 0.2483 | 64 |
| Low-pass | 0.0326 | 0.0327 | 0.0364 | 0.0465 | 0.2111 | 64 |
| Noise gate | 0.0203 | 0.0206 | 0.0206 | 0.0420 | 0.8108 | 64 |
| Pedal compressor | 0.1170 | 0.1635 | 0.1740 | 0.3956 | 1.2075 | 72 |
| Studio compressor | 0.1076 | 0.1099 | 0.1103 | 0.3323 | 0.3634 | 64 |
| Wah | 0.0533 | 0.0540 | 0.0608 | 0.2666 | 0.3292 | 104 |
| Envelope wah | 0.1294 | 0.1301 | 0.1421 | 0.4796 | 1.5923 | 616 |
| Clean boost | 0.0064 | 0.0071 | 0.0071 | 0.0072 | 1.5612 | 48 |
| Transparent overdrive | 0.0214 | 0.0214 | 0.0217 | 0.2333 | 1.4805 | 64 |
| Mid-focused overdrive | 0.0368 | 0.0375 | 0.0563 | 0.2985 | 1.5570 | 104 |
| High-gain distortion | 0.0386 | 0.0390 | 0.0548 | 0.3053 | 1.5285 | 136 |
| Round fuzz | 0.0308 | 0.0311 | 0.0312 | 0.0619 | 1.4423 | 64 |
| Soft drive | 0.1253 | 0.1256 | 0.2010 | 0.3375 | 0.4339 | 16 |
| Tremolo | 0.0739 | 0.0746 | 0.0750 | 0.2318 | 0.4984 | 96 |
| Chorus | 0.0540 | 0.0544 | 0.0548 | 0.2689 | 0.7050 | 12,448 |
| Flanger | 0.0641 | 0.0656 | 0.0998 | 0.3026 | 1.0669 | 4,792 |
| Phaser | 0.0821 | 0.0829 | 0.1238 | 0.2812 | 0.4016 | 280 |
| Vibrato | 0.0529 | 0.0818 | 0.0885 | 0.2955 | 0.3953 | 1,696 |
| Three-band parametric EQ | 0.0649 | 0.0653 | 0.0960 | 0.2929 | 0.3484 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0263 | 0.0266 | 0.2415 | 6.7134 | 768,080 |
| Poly pitch shifter, mono | 5.2497 | 6.8799 | 8.7520 | 11.3973 | 28.5627 | 311,056 |
| Poly pitch shifter, stereo | 13.1801 | 14.5759 | 16.5863 | 23.9253 | 69.4523 | 494,016 |
| Peak limiter | 0.0199 | 0.0199 | 0.0319 | 0.0394 | 0.3637 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0019 | 0.0026 | 0.0041 | 0.2378 | 80 |
| Input + output meters | 0.0176 | 0.0248 | 0.0326 | 0.0398 | 1.4813 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0026 | 0.0030 | 0.0034 | 0.3098 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0 |
| Mono → stereo | 0.0026 | 0.0026 | 0.0026 | 0.0027 | 0.0027 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0023 | 0.0023 | 0.0045 | 0.2456 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0064 | 0.0083 | 0.0105 | 0.2078 | 112 |
| Chromatic tuner monitor | 0.0113 | 0.5299 | 0.7792 | 1.9332 | 11.8717 | 10,400 |
| Full 25-effect chain + controls + meters | 12.9071 | 16.1685 | 19.4818 | 32.7752 | 49.8921 | 1,283,368 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.2497% p50 / 6.8799% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 13.1801% / 14.5759% and about 482 KiB. High-gain Distortion uses 0.0386% p50 / 0.0390% p95 and 136 B; Round Fuzz uses 0.0308% / 0.0311% and 64 B. Both add zero fixed latency and allocate nothing in `process()`. The full 25-effect stereo chain uses 12.9071% p50 / 16.1685% p95; the maximum was 49.8921% in this run, reflecting host scheduling outliers. Mid-focused Overdrive uses 0.0368% / 0.0375% and 104 B. Transparent Overdrive uses 0.0214% / 0.0214% and 64 B. Chorus uses 0.0540% / 0.0544% and 12,448 B; flanger uses 0.0641% / 0.0656% and 4,792 B; phaser uses 0.0821% / 0.0829% and 280 B; vibrato uses 0.0529% / 0.0818% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 25-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with Round Fuzz enabled, comparing the WASM chain with the native renderer at 0.00693906 maximum absolute error and 0.000207516 RMS error (0.2773% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 844,240 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
