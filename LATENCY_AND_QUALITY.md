# Latency, quality, and DSP-load protocol

## Latest DLY-06 verification (2026-11-21)

This run supersedes the earlier delay, full-chain, browser-parity, and memory values retained in the historical note below. The BBD-style mode reuses the existing delay ring, adds 32 B of fixed processor state, no stage or process-time allocation, and no dry-path delay. On an AMD Ryzen 9 5950X / GCC 13.3.0 / Release / 48 kHz / 128-frame configuration, BBD processing measured 0.0776% p50 / 0.0818% p95 mono and 0.1211% / 0.1230% stereo of the 2.667 ms callback deadline. The full 25-effect chain measured 12.8647% p50 / 13.1767% p95, 15.9462% maximum, and 1,283,904 B owned state.

The current HeadlessChrome 151 mono Play/native comparison enables BBD-style coloration on the four-tap Delay and uses a generated guitar-like local fixture. Maximum absolute error was 0.00693906, RMS error 0.00020373 (0.2572% of reference RMS), and chain-owned state was 1,228,656 B plus 10,364 B tuner state inside fixed 16 MiB WASM linear memory. This passed the existing 6% peak and 7.5% RMS bounds. Mono playback centers the echoes; core and offline stereo impulse tests verify per-tap equal-power pan and BBD stereo isolation. The browser's 1 ms timer fallback makes its live CPU percentage coarse. The older paragraph below records earlier browser runs and benchmark baselines.

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 25 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0064 | 0.0064 | 0.0079 | 0.2288 | 40 |
| Output gain | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2097 | 40 |
| Volume pedal | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 56 |
| High-pass | 0.0371 | 0.0375 | 0.0379 | 0.1751 | 0.2010 | 64 |
| Low-pass | 0.0375 | 0.0379 | 0.0450 | 0.2610 | 0.9777 | 64 |
| Noise gate | 0.0229 | 0.0390 | 0.0409 | 0.0604 | 0.4358 | 64 |
| Pedal compressor | 0.1185 | 0.1189 | 0.1658 | 0.3364 | 0.3589 | 72 |
| Studio compressor | 0.1087 | 0.1095 | 0.1444 | 0.3244 | 0.7459 | 64 |
| Wah | 0.0540 | 0.0548 | 0.0630 | 0.2580 | 0.2764 | 104 |
| Envelope wah | 0.1305 | 0.1313 | 0.1459 | 0.3533 | 0.3866 | 616 |
| Clean boost | 0.0056 | 0.0060 | 0.0101 | 0.0116 | 0.0206 | 48 |
| Transparent overdrive | 0.0214 | 0.0217 | 0.0364 | 0.0420 | 0.2677 | 64 |
| Mid-focused overdrive | 0.0371 | 0.0551 | 0.0589 | 0.3154 | 0.8265 | 104 |
| High-gain distortion | 0.0386 | 0.0619 | 0.0641 | 0.2205 | 0.7328 | 136 |
| Round fuzz | 0.0319 | 0.0323 | 0.0323 | 0.2231 | 0.2419 | 64 |
| Soft drive | 0.1444 | 0.1448 | 0.2378 | 0.4905 | 0.8805 | 16 |
| Tremolo | 0.0720 | 0.0724 | 0.0743 | 0.5599 | 0.7947 | 96 |
| Chorus | 0.0544 | 0.0548 | 0.0555 | 0.2449 | 0.3319 | 12,448 |
| Flanger | 0.0638 | 0.0649 | 0.1005 | 0.2592 | 0.3570 | 4,792 |
| Phaser | 0.0821 | 0.0825 | 0.1230 | 0.2816 | 0.3431 | 280 |
| Vibrato | 0.0525 | 0.0529 | 0.0724 | 0.2415 | 0.2704 | 1,696 |
| Three-band parametric EQ | 0.0656 | 0.0926 | 0.1046 | 0.2974 | 0.4084 | 160 |
| Delay, normal (2 s stereo allocation) | 0.0938 | 0.0949 | 0.1980 | 0.4031 | 0.6623 | 768,616 |
| Delay, BBD-style mono | 0.0776 | 0.0818 | 0.1391 | 0.2393 | 0.3019 | 384,612 |
| Delay, BBD-style stereo | 0.1211 | 0.1230 | 0.2153 | 0.3574 | 0.7328 | 768,616 |
| Delay, ping-pong (2 s stereo allocation) | 0.1208 | 0.1219 | 0.2359 | 0.3424 | 0.6825 | 768,616 |
| Delay, independent dual mono (two 2 s lines) | 0.0979 | 0.0986 | 0.1845 | 0.3405 | 0.4170 | 768,616 |
| Delay, independent dual stereo (two 2 s lines) | 0.1211 | 0.1226 | 0.1965 | 0.3735 | 0.4886 | 768,616 |
| Delay, four-tap mono (2 s ring) | 0.1208 | 0.1219 | 0.2427 | 0.3754 | 1.1445 | 384,612 |
| Delay, four-tap stereo (2 s ring) | 0.1853 | 0.1931 | 0.3484 | 0.4557 | 0.7069 | 768,616 |
| Poly pitch shifter, mono | 5.6619 | 6.8222 | 7.5197 | 9.6858 | 13.4340 | 311,056 |
| Poly pitch shifter, stereo | 13.1989 | 13.6684 | 16.0325 | 19.8782 | 22.8201 | 494,016 |
| Peak limiter | 0.0206 | 0.0338 | 0.0375 | 0.0450 | 0.3334 | 32 |
| Signal controls (input + dry/wet) | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 80 |
| Input + output meters | 0.0176 | 0.0180 | 0.0180 | 0.0180 | 0.2179 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0045 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0 |
| Mono → stereo | 0.0023 | 0.0026 | 0.0026 | 0.0045 | 0.2104 | 0 |
| Parallel split (copy to two branches) | 0.0030 | 0.0030 | 0.0060 | 0.0064 | 0.0176 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 0.2216 | 112 |
| Chromatic tuner monitor | 0.0101 | 0.5014 | 0.6893 | 0.7812 | 0.8629 | 10,400 |
| Full 25-effect chain + controls + meters | 12.8647 | 13.1767 | 13.6849 | 13.9815 | 15.9462 | 1,283,904 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.3222% p50 / 6.8218% p95 of the callback deadline with 311,056 B estimated state; stereo uses 12.7721% / 14.3385% and 494,016 B. High-gain Distortion uses 0.0315% p50 / 0.0401% p95 and 136 B; Round Fuzz uses 0.0323% / 0.0356% and 64 B. Both add zero fixed latency and allocate nothing in `process()`. The full 25-effect stereo chain uses 13.1153% p50 / 16.9568% p95; the maximum was 45.5634% in this run, reflecting host scheduling outliers. Mid-focused Overdrive uses 0.0300% / 0.0300% and 104 B. Transparent Overdrive uses 0.0176% / 0.0176% and 64 B. Chorus uses 0.0551% / 0.0559% and 12,448 B; flanger uses 0.0656% / 0.0664% and 4,792 B; phaser uses 0.0833% / 0.0836% and 280 B; vibrato uses 0.0525% / 0.0529% and 1,696 B. The 2 s / 48 kHz delay uses 768,304 B in stereo and measures 0.0341% p50 / 0.0345% p95 normally, 0.0533% / 0.0536% in ping-pong mode, and 0.0529% / 0.0533% for independent dual stereo. Independent dual mono uses two preallocated lines, 768,304 B, and 0.0420% / 0.0424%; no processing-mode switch allocates memory. These modulation and delay effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 25-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with the independent Dual routing selector active, comparing the WASM chain with the native renderer at 0.00607224 maximum absolute error and 0.000697286 RMS error (1.0219% reference RMS), within the test's 6% peak and 7.5% RMS bounds. The fixture is mono and verifies the independent timing/mix of both delayed taps; stereo CLI/core tests verify separate left/right times and feedback. The browser chain state uses 1,228,404 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
