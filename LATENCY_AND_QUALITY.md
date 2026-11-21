# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 23 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0097 | 0.0105 | 0.0165 | 0.2667 | 40 |
| Output gain | 0.0053 | 0.0090 | 0.0101 | 0.0109 | 0.0184 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0090 | 0.0101 | 0.1946 | 56 |
| High-pass | 0.0323 | 0.0326 | 0.0382 | 0.1489 | 0.3173 | 64 |
| Low-pass | 0.0326 | 0.0409 | 0.0473 | 0.2318 | 0.6851 | 64 |
| Noise gate | 0.0195 | 0.0203 | 0.0289 | 0.0604 | 0.2666 | 64 |
| Pedal compressor | 0.1166 | 0.1178 | 0.1189 | 0.2929 | 0.3338 | 72 |
| Studio compressor | 0.1087 | 0.1099 | 0.1414 | 0.4148 | 1.3350 | 64 |
| Wah | 0.0544 | 0.0596 | 0.0630 | 0.3765 | 1.6392 | 104 |
| Envelope wah | 0.1316 | 0.1455 | 0.1530 | 0.5310 | 1.7907 | 616 |
| Clean boost | 0.0064 | 0.0113 | 0.0120 | 0.0139 | 0.2468 | 48 |
| Transparent overdrive | 0.0217 | 0.0379 | 0.0394 | 0.0454 | 0.2678 | 64 |
| Mid-focused overdrive | 0.0375 | 0.0379 | 0.0585 | 0.1748 | 0.2704 | 104 |
| Soft drive | 0.1249 | 0.1253 | 0.2078 | 0.3791 | 0.7887 | 16 |
| Tremolo | 0.0743 | 0.0750 | 0.0757 | 0.2963 | 0.3551 | 96 |
| Chorus | 0.0548 | 0.0578 | 0.0585 | 0.2434 | 0.3593 | 12,448 |
| Flanger | 0.0649 | 0.0656 | 0.0986 | 0.2734 | 0.3146 | 4,792 |
| Phaser | 0.0829 | 0.0836 | 0.1016 | 0.2933 | 0.6147 | 280 |
| Vibrato | 0.0529 | 0.0533 | 0.0675 | 0.2546 | 0.8678 | 1,696 |
| Three-band parametric EQ | 0.0649 | 0.0724 | 0.1001 | 0.2914 | 0.7568 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0210 | 0.0217 | 0.1208 | 0.7080 | 768,080 |
| Poly pitch shifter, mono | 5.2603 | 6.9122 | 7.8471 | 9.7394 | 12.3045 | 311,056 |
| Poly pitch shifter, stereo | 11.5736 | 14.4645 | 16.8774 | 21.0013 | 21.5286 | 494,016 |
| Peak limiter | 0.0199 | 0.0203 | 0.0323 | 0.1991 | 0.6772 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0023 | 0.0026 | 0.0041 | 0.0053 | 80 |
| Input + output meters | 0.0180 | 0.0195 | 0.0195 | 0.2276 | 0.7485 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0026 | 0.0030 | 0.0045 | 0.3214 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0019 | 0.0030 | 0.0038 | 0.3015 | 0 |
| Mono → stereo | 0.0038 | 0.0038 | 0.0075 | 0.0086 | 0.2498 | 0 |
| Parallel split (copy to two branches) | 0.0030 | 0.0030 | 0.0030 | 0.0030 | 0.2779 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0064 | 0.0097 | 0.0158 | 0.2629 | 112 |
| Chromatic tuner monitor | 0.0105 | 0.5318 | 0.5438 | 0.8126 | 0.9139 | 10,400 |
| Full 23-effect chain + controls + meters | 12.7807 | 15.8055 | 16.4525 | 19.5729 | 23.0860 | 1,283,168 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.2603% p50 / 6.9122% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 11.5736% / 14.4645% and about 482 KiB. The full 23-effect stereo chain uses 12.7807% p50 / 15.8055% p95; the maximum was 23.0860% in this run. Mid-focused Overdrive uses 0.0375% p50 / 0.0379% p95 and 104 B; about 1.00/1.01 μs per 128-frame block. It adds zero fixed latency and `process()` allocates nothing. Transparent Overdrive uses 0.0217% / 0.0379% and 64 B. Chorus uses 0.0548% / 0.0578% and 12,448 B; flanger uses 0.0649% / 0.0656% and 4,792 B; phaser uses 0.0829% / 0.0836% and 280 B; vibrato uses 0.0529% / 0.0533% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 23-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed native/WASM comparison with both overdrives, negative flanger feedback, six-stage phaser, and vibrato enabled: 0.0000245946 maximum absolute error and 0.00000336178 RMS error (0.14281% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 844,044 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
