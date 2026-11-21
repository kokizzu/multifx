# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 24 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0053 | 0.0056 | 0.0056 | 0.0135 | 0.4290 | 40 |
| Output gain | 0.0053 | 0.0071 | 0.0116 | 0.0135 | 0.5025 | 40 |
| Volume pedal | 0.0049 | 0.0060 | 0.0086 | 0.0097 | 0.2355 | 56 |
| High-pass | 0.0323 | 0.0330 | 0.0382 | 0.0465 | 0.2318 | 64 |
| Low-pass | 0.0326 | 0.0364 | 0.0390 | 0.2490 | 0.6026 | 64 |
| Noise gate | 0.0203 | 0.0251 | 0.0300 | 0.1046 | 0.5753 | 64 |
| Pedal compressor | 0.1170 | 0.1174 | 0.1635 | 0.3116 | 0.4616 | 72 |
| Studio compressor | 0.1087 | 0.1099 | 0.1399 | 0.2963 | 0.3124 | 64 |
| Wah | 0.0533 | 0.0540 | 0.0604 | 0.2243 | 0.3019 | 104 |
| Envelope wah | 0.1294 | 0.1298 | 0.1410 | 0.3735 | 0.8985 | 616 |
| Clean boost | 0.0068 | 0.0071 | 0.0131 | 0.0150 | 0.3012 | 48 |
| Transparent overdrive | 0.0214 | 0.0214 | 0.0360 | 0.0405 | 0.2456 | 64 |
| Mid-focused overdrive | 0.0368 | 0.0371 | 0.0559 | 0.0656 | 0.2456 | 104 |
| High-gain distortion | 0.0382 | 0.0386 | 0.0623 | 0.2067 | 0.2558 | 136 |
| Soft drive | 0.1249 | 0.1256 | 0.2063 | 0.5468 | 1.1198 | 16 |
| Tremolo | 0.0739 | 0.0743 | 0.0750 | 0.2651 | 0.3068 | 96 |
| Chorus | 0.0555 | 0.0563 | 0.0893 | 0.2903 | 0.6255 | 12,448 |
| Flanger | 0.0649 | 0.0660 | 0.0844 | 0.2771 | 0.6465 | 4,792 |
| Phaser | 0.0829 | 0.0833 | 0.1200 | 0.3019 | 0.3341 | 280 |
| Vibrato | 0.0518 | 0.0525 | 0.0529 | 0.2040 | 0.2400 | 1,696 |
| Three-band parametric EQ | 0.0649 | 0.0656 | 0.0983 | 0.2737 | 1.1025 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0210 | 0.0221 | 0.0232 | 0.6555 | 768,080 |
| Poly pitch shifter, mono | 5.6192 | 7.1766 | 9.3704 | 12.9521 | 30.8472 | 311,056 |
| Poly pitch shifter, stereo | 11.8908 | 14.3243 | 15.9154 | 20.7824 | 47.9192 | 494,016 |
| Peak limiter | 0.0199 | 0.0203 | 0.0203 | 0.0353 | 0.3480 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0030 | 80 |
| Input + output meters | 0.0176 | 0.0176 | 0.0304 | 0.0330 | 0.2355 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0015 | 0.0019 | 0.0019 | 0.1661 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0019 | 0.0019 | 0.0019 | 0.0019 | 0 |
| Mono → stereo | 0.0023 | 0.0026 | 0.0041 | 0.0060 | 0.0161 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0034 | 0.0038 | 0.0049 | 0.2348 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0090 | 0.0101 | 0.1684 | 0.2749 | 112 |
| Chromatic tuner monitor | 0.0097 | 0.5044 | 0.6829 | 0.8269 | 1.0242 | 10,400 |
| Full 24-effect chain + controls + meters | 13.0248 | 16.5601 | 20.3312 | 24.7248 | 29.0325 | 1,283,304 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.6192% p50 / 7.1766% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 11.8908% / 14.3243% and about 482 KiB. High-gain Distortion uses 0.0382% p50 / 0.0386% p95 and 136 B; about 1.02/1.03 μs per 128-frame stereo block. It adds zero fixed latency and `process()` allocates nothing. The full 24-effect stereo chain uses 13.0248% p50 / 16.5601% p95; the maximum was 29.0325% in this run. Mid-focused Overdrive uses 0.0368% / 0.0371% and 104 B. Transparent Overdrive uses 0.0214% / 0.0214% and 64 B. Chorus uses 0.0555% / 0.0563% and 12,448 B; flanger uses 0.0649% / 0.0660% and 4,792 B; phaser uses 0.0829% / 0.0833% and 280 B; vibrato uses 0.0518% / 0.0525% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 24-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with the high-gain block enabled, comparing the WASM chain with the native renderer at 0.000244505 maximum absolute error and 0.0000318502 RMS error (0.14237% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 844,176 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
