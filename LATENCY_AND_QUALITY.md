# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 22 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0083 | 0.0094 | 0.0097 | 0.0101 | 0.2599 | 40 |
| Output gain | 0.0079 | 0.0094 | 0.0097 | 0.0097 | 2.4953 | 40 |
| Volume pedal | 0.0105 | 0.0124 | 0.0124 | 0.0173 | 0.3885 | 56 |
| High-pass | 0.0379 | 0.0405 | 0.0409 | 0.2010 | 1.0118 | 64 |
| Low-pass | 0.0382 | 0.0405 | 0.0409 | 0.2648 | 0.3615 | 64 |
| Noise gate | 0.0311 | 0.0341 | 0.0345 | 0.2550 | 6.3830 | 64 |
| Pedal compressor | 0.1365 | 0.1452 | 0.1601 | 0.4706 | 1.2218 | 72 |
| Studio compressor | 0.0915 | 0.1189 | 0.1245 | 0.4234 | 0.5112 | 64 |
| Wah | 0.0476 | 0.0514 | 0.0574 | 0.3188 | 0.8393 | 104 |
| Envelope wah | 0.1091 | 0.1241 | 0.1335 | 0.5389 | 0.5783 | 616 |
| Clean boost | 0.0045 | 0.0086 | 0.0097 | 0.0161 | 0.3732 | 48 |
| Transparent overdrive | 0.0180 | 0.0323 | 0.0334 | 0.1016 | 0.4467 | 64 |
| Soft drive | 0.1935 | 0.1969 | 0.2010 | 0.4883 | 1.4573 | 16 |
| Tremolo | 0.0619 | 0.0623 | 0.0660 | 0.3334 | 0.8160 | 96 |
| Chorus | 0.0458 | 0.0713 | 0.0754 | 0.2336 | 0.3927 | 12,448 |
| Flanger | 0.0540 | 0.0791 | 0.0840 | 0.3334 | 1.1697 | 4,792 |
| Phaser | 0.0690 | 0.1028 | 0.1151 | 0.4260 | 0.5509 | 280 |
| Vibrato | 0.0443 | 0.0750 | 0.0814 | 0.2940 | 0.4286 | 1,696 |
| Three-band parametric EQ | 0.0551 | 0.0829 | 0.0870 | 0.3555 | 0.4226 | 160 |
| Delay (2 s stereo allocation) | 0.0206 | 0.0210 | 0.0221 | 0.0360 | 0.3319 | 768,080 |
| Poly pitch shifter, mono | 6.1783 | 8.9455 | 11.2046 | 17.3592 | 34.3641 | 311,056 |
| Poly pitch shifter, stereo | 12.2801 | 34.1331 | 59.2647 | 75.9703 | 87.0005 | 494,016 |
| Peak limiter | 0.0173 | 0.0229 | 0.0236 | 0.0405 | 0.3139 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 80 |
| Input + output meters | 0.0199 | 0.0278 | 0.0289 | 0.0589 | 0.4373 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0023 | 0.0023 | 0.0034 | 0.0116 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Mono → stereo | 0.0030 | 0.0064 | 0.0068 | 0.0075 | 0.0146 | 0 |
| Parallel split (copy to two branches) | 0.0034 | 0.0045 | 0.0053 | 0.0083 | 0.4890 | 0 |
| Parallel mix (dry + two branches) | 0.0053 | 0.0083 | 0.0090 | 0.0116 | 0.3274 | 112 |
| Chromatic tuner monitor | 0.0139 | 0.5906 | 0.6791 | 1.1558 | 1.8394 | 10,400 |
| Full 22-effect chain + controls + meters | 15.0360 | 23.4850 | 59.7137 | 80.4014 | 86.3592 | 1,283,064 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 6.1783% p50 / 8.9455% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 12.2801% / 34.1331% and about 482 KiB. The full 22-effect stereo chain uses 15.0360% p50 / 23.4850% p95, with high tail values up to 86.3592% maximum in this run; this spread indicates host scheduling noise, not a per-effect increase. Transparent Overdrive uses 0.0180% p50 / 0.0323% p95 and 64 B; it adds zero fixed latency and no process-time allocations. Chorus uses 0.0458% / 0.0713% and 12,448 B; flanger uses 0.0540% / 0.0791% and 4,792 B; phaser uses 0.0690% / 0.1028% and 280 B; vibrato uses 0.0443% / 0.0750% and 1,696 B. These modulation effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 22-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed native/WASM comparison with transparent overdrive, negative flanger feedback, six-stage phaser, and vibrato enabled: 0.0000103191 maximum absolute error and 0.00000105125 RMS error (0.04906% reference RMS), within the test's 6% peak and 7.5% RMS bounds. Its chain state uses 843,940 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse; the latest browser run reported an average 18.9% pitch-stage estimate and 22.7% whole-callback estimate. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
