# Latency, quality, and DSP-load protocol

## Latest DLY-05 verification (2026-11-21)

This post-multi-tap run supersedes the earlier pre-DLY-05 delay, full-chain, browser-parity, and memory values retained in the historical note below. The 2 s / 48 kHz instance owns 384,580 B in mono and 768,584 B in stereo; the mode reuses the existing delay ring and fixed-size tap state, adds no stage or process-time allocation, and the dry path has no fixed algorithmic delay. On a Ryzen 9 5950X / GCC 13.3.0 / Release / 48 kHz / 128-frame configuration, four-tap processing measured 0.1077% p50 / 0.1084% p95 mono and 0.1380% / 0.1451% stereo of the 2.667 ms callback deadline. The same run measured normal delay at 0.0589% / 0.0596%, ping-pong at 0.0897% / 0.1560%, dual mono at 0.0743% / 0.0750%, and dual stereo at 0.0896% / 0.0908%. Full 25-effect chain load was 13.2758% p50 / 16.8192% p95, 29.7128% maximum, and 1,283,872 B owned state.

The current HeadlessChrome 151 mono Play/native comparison uses all four taps on a generated guitar-like local fixture. Maximum absolute error was 0.01019327, RMS error 0.00165978 (1.9760% of reference RMS), and chain-owned state was 1,228,628 B plus 10,364 B tuner state inside fixed 16 MiB WASM linear memory. This passed the existing 6% peak and 7.5% RMS bounds. Mono playback centers the echoes; core and offline stereo impulse tests verify per-tap equal-power pan. The browser's 1 ms timer fallback makes its live CPU percentage coarse. The older paragraph below records the earlier dual-delay browser run and previous benchmark baseline.

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 25 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0064 | 0.0079 | 0.0083 | 0.8059 | 40 |
| Output gain | 0.0049 | 0.0053 | 0.0053 | 0.0116 | 0.2614 | 40 |
| Volume pedal | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.3927 | 56 |
| High-pass | 0.0300 | 0.0304 | 0.0304 | 0.0360 | 0.2914 | 64 |
| Low-pass | 0.0300 | 0.0330 | 0.0368 | 0.0518 | 0.2441 | 64 |
| Noise gate | 0.0184 | 0.0188 | 0.0188 | 0.0356 | 0.2531 | 64 |
| Pedal compressor | 0.0960 | 0.1313 | 0.1380 | 0.3267 | 0.5258 | 72 |
| Studio compressor | 0.0881 | 0.0889 | 0.1129 | 0.2925 | 0.4118 | 64 |
| Wah | 0.0469 | 0.0469 | 0.0473 | 0.2078 | 0.2250 | 104 |
| Envelope wah | 0.1065 | 0.1324 | 0.1489 | 0.6176 | 1.3590 | 616 |
| Clean boost | 0.0053 | 0.0064 | 0.0109 | 0.0120 | 0.1935 | 48 |
| Transparent overdrive | 0.0176 | 0.0300 | 0.0311 | 0.0465 | 0.2468 | 64 |
| Mid-focused overdrive | 0.0300 | 0.0458 | 0.0473 | 0.1976 | 0.2655 | 104 |
| High-gain distortion | 0.0311 | 0.0315 | 0.0525 | 0.1988 | 0.3169 | 136 |
| Round fuzz | 0.0259 | 0.0259 | 0.0454 | 0.0529 | 0.3064 | 64 |
| Soft drive | 0.1429 | 0.1448 | 0.2348 | 0.5247 | 0.9259 | 16 |
| Tremolo | 0.0581 | 0.0585 | 0.0604 | 0.2396 | 0.2659 | 96 |
| Chorus | 0.0439 | 0.0713 | 0.0731 | 0.2677 | 0.3791 | 12,448 |
| Flanger | 0.0518 | 0.0525 | 0.0529 | 0.4376 | 0.6255 | 4,792 |
| Phaser | 0.1219 | 0.1223 | 0.1223 | 0.3199 | 0.4474 | 280 |
| Vibrato | 0.0420 | 0.0439 | 0.0443 | 0.2175 | 0.9447 | 1,696 |
| Three-band parametric EQ | 0.0540 | 0.0829 | 0.0889 | 0.2914 | 1.1325 | 160 |
| Delay, normal (2 s stereo allocation) | 0.0589 | 0.0596 | 0.1170 | 0.2528 | 0.3698 | 768,584 |
| Delay, ping-pong (2 s stereo allocation) | 0.0897 | 0.1560 | 0.1909 | 1.0583 | 1.5589 | 768,584 |
| Delay, independent dual mono (two 2 s lines) | 0.0743 | 0.0750 | 0.0765 | 0.3188 | 1.5330 | 768,584 |
| Delay, independent dual stereo (two 2 s lines) | 0.0896 | 0.0908 | 0.0930 | 0.3484 | 1.5484 | 768,584 |
| Delay, four-tap mono (2 s ring) | 0.1077 | 0.1084 | 0.1091 | 0.5141 | 1.5514 | 384,580 |
| Delay, four-tap stereo (2 s ring) | 0.1380 | 0.1451 | 0.2486 | 0.3548 | 0.4755 | 768,584 |
| Poly pitch shifter, mono | 5.2254 | 6.5379 | 7.7608 | 9.8624 | 11.4611 | 311,056 |
| Poly pitch shifter, stereo | 10.7193 | 13.7239 | 15.8258 | 20.1283 | 23.4123 | 494,016 |
| Peak limiter | 0.0176 | 0.0180 | 0.0304 | 0.0345 | 0.2689 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0019 | 0.0023 | 0.0034 | 0.0041 | 80 |
| Input + output meters | 0.0143 | 0.0146 | 0.0229 | 0.0270 | 0.2475 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0023 | 0 |
| Mono → stereo | 0.0019 | 0.0023 | 0.0023 | 0.0023 | 0.0023 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0026 | 0.0026 | 0.0026 | 0.1781 | 0 |
| Parallel mix (dry + two branches) | 0.0053 | 0.0071 | 0.0079 | 0.0139 | 0.2182 | 112 |
| Chromatic tuner monitor | 0.0083 | 0.4290 | 0.6143 | 0.6941 | 0.8760 | 10,400 |
| Full 25-effect chain + controls + meters | 13.2758 | 16.8192 | 18.8867 | 22.4478 | 29.7128 | 1,283,872 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.3222% p50 / 6.8218% p95 of the callback deadline with 311,056 B estimated state; stereo uses 12.7721% / 14.3385% and 494,016 B. High-gain Distortion uses 0.0315% p50 / 0.0401% p95 and 136 B; Round Fuzz uses 0.0323% / 0.0356% and 64 B. Both add zero fixed latency and allocate nothing in `process()`. The full 25-effect stereo chain uses 13.1153% p50 / 16.9568% p95; the maximum was 45.5634% in this run, reflecting host scheduling outliers. Mid-focused Overdrive uses 0.0300% / 0.0300% and 104 B. Transparent Overdrive uses 0.0176% / 0.0176% and 64 B. Chorus uses 0.0551% / 0.0559% and 12,448 B; flanger uses 0.0656% / 0.0664% and 4,792 B; phaser uses 0.0833% / 0.0836% and 280 B; vibrato uses 0.0525% / 0.0529% and 1,696 B. The 2 s / 48 kHz delay uses 768,304 B in stereo and measures 0.0341% p50 / 0.0345% p95 normally, 0.0533% / 0.0536% in ping-pong mode, and 0.0529% / 0.0533% for independent dual stereo. Independent dual mono uses two preallocated lines, 768,304 B, and 0.0420% / 0.0424%; no processing-mode switch allocates memory. These modulation and delay effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 25-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with the independent Dual routing selector active, comparing the WASM chain with the native renderer at 0.00607224 maximum absolute error and 0.000697286 RMS error (1.0219% reference RMS), within the test's 6% peak and 7.5% RMS bounds. The fixture is mono and verifies the independent timing/mix of both delayed taps; stereo CLI/core tests verify separate left/right times and feedback. The browser chain state uses 1,228,404 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
