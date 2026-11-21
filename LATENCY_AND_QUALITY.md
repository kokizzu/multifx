# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 25 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0101 | 0.0120 | 0.0191 | 0.2393 | 40 |
| Output gain | 0.0056 | 0.0101 | 0.0105 | 0.0161 | 0.2175 | 40 |
| Volume pedal | 0.0045 | 0.0094 | 0.0097 | 0.0165 | 0.2006 | 56 |
| High-pass | 0.0326 | 0.0401 | 0.0409 | 0.1950 | 0.2603 | 64 |
| Low-pass | 0.0289 | 0.0405 | 0.0409 | 0.3709 | 0.7943 | 64 |
| Noise gate | 0.0176 | 0.0180 | 0.0274 | 0.1009 | 0.2858 | 64 |
| Pedal compressor | 0.1185 | 0.1196 | 0.1631 | 0.5824 | 1.7415 | 72 |
| Studio compressor | 0.0870 | 0.0874 | 0.0878 | 0.2520 | 0.2869 | 64 |
| Wah | 0.0529 | 0.0570 | 0.0611 | 0.2385 | 0.3195 | 104 |
| Envelope wah | 0.1271 | 0.1279 | 0.1414 | 0.3499 | 0.5321 | 616 |
| Clean boost | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.1812 | 48 |
| Transparent overdrive | 0.0210 | 0.0214 | 0.0375 | 0.0439 | 0.1830 | 64 |
| Mid-focused overdrive | 0.0368 | 0.0566 | 0.0581 | 0.2970 | 0.6762 | 104 |
| High-gain distortion | 0.0379 | 0.0379 | 0.0382 | 0.2325 | 0.6701 | 136 |
| Round fuzz | 0.0304 | 0.0308 | 0.0698 | 0.2074 | 0.3371 | 64 |
| Soft drive | 0.1133 | 0.1815 | 0.2228 | 0.4448 | 1.3242 | 16 |
| Tremolo | 0.0709 | 0.0713 | 0.0720 | 0.3079 | 0.3180 | 96 |
| Chorus | 0.0529 | 0.0716 | 0.0862 | 0.3195 | 1.2855 | 12,448 |
| Flanger | 0.0630 | 0.0638 | 0.0986 | 0.3113 | 0.6495 | 4,792 |
| Phaser | 0.0803 | 0.0807 | 0.0810 | 0.2535 | 0.3368 | 280 |
| Vibrato | 0.0510 | 0.0544 | 0.0795 | 0.2670 | 0.4039 | 1,696 |
| Three-band parametric EQ | 0.0649 | 0.0990 | 0.1024 | 0.3510 | 0.7838 | 160 |
| Delay (2 s stereo allocation) | 0.0252 | 0.0263 | 0.0266 | 0.2869 | 4.5414 | 768,144 |
| Poly pitch shifter, mono | 5.2051 | 6.1037 | 6.4963 | 10.9402 | 16.0557 | 311,056 |
| Poly pitch shifter, stereo | 11.1232 | 14.5346 | 18.8225 | 21.5942 | 26.5489 | 494,016 |
| Peak limiter | 0.0199 | 0.0203 | 0.0353 | 0.0427 | 0.9435 | 32 |
| Signal controls (input + dry/wet) | 0.0023 | 0.0030 | 0.0038 | 0.0045 | 0.0135 | 80 |
| Input + output meters | 0.0173 | 0.0176 | 0.0319 | 0.0341 | 0.2967 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Mono → stereo | 0.0030 | 0.0034 | 0.0034 | 0.0034 | 0.1665 | 0 |
| Parallel split (copy to two branches) | 0.0026 | 0.0026 | 0.0026 | 0.0027 | 0.0053 | 0 |
| Parallel mix (dry + two branches) | 0.0060 | 0.0064 | 0.0068 | 0.0071 | 0.1519 | 112 |
| Chromatic tuner monitor | 0.0113 | 0.4909 | 0.5194 | 0.8115 | 2.1323 | 10,400 |
| Full 25-effect chain + controls + meters | 13.1107 | 17.1113 | 20.1849 | 24.6502 | 27.7721 | 1,283,432 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.2497% p50 / 6.8799% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 13.1801% / 14.5759% and about 482 KiB. High-gain Distortion uses 0.0386% p50 / 0.0390% p95 and 136 B; Round Fuzz uses 0.0308% / 0.0311% and 64 B. Both add zero fixed latency and allocate nothing in `process()`. The full 25-effect stereo chain uses 12.9071% p50 / 16.1685% p95; the maximum was 49.8921% in this run, reflecting host scheduling outliers. Mid-focused Overdrive uses 0.0368% / 0.0375% and 104 B. Transparent Overdrive uses 0.0214% / 0.0214% and 64 B. Chorus uses 0.0540% / 0.0544% and 12,448 B; flanger uses 0.0641% / 0.0656% and 4,792 B; phaser uses 0.0821% / 0.0829% and 280 B; vibrato uses 0.0529% / 0.0818% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 25-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with Round Fuzz enabled, comparing the WASM chain with the native renderer at 0.00693906 maximum absolute error and 0.000207516 RMS error (0.2773% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 844,240 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
