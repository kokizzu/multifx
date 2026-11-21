# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Latest measurement on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 25 effects plus signal controls and both meters. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0064 | 0.0101 | 0.0120 | 0.0173 | 0.4155 | 40 |
| Output gain | 0.0068 | 0.0071 | 0.0071 | 0.0120 | 0.2306 | 40 |
| Volume pedal | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.2978 | 56 |
| High-pass | 0.0300 | 0.0345 | 0.0364 | 0.0536 | 0.3004 | 64 |
| Low-pass | 0.0304 | 0.0353 | 0.0368 | 0.0514 | 0.3053 | 64 |
| Noise gate | 0.0184 | 0.0338 | 0.0349 | 0.0409 | 0.2648 | 64 |
| Pedal compressor | 0.1189 | 0.1193 | 0.1196 | 0.3581 | 0.5201 | 72 |
| Studio compressor | 0.1099 | 0.1140 | 0.1148 | 0.3758 | 0.6540 | 64 |
| Wah | 0.0544 | 0.0548 | 0.0551 | 0.2243 | 0.2850 | 104 |
| Envelope wah | 0.1313 | 0.1444 | 0.1654 | 1.0467 | 3.3601 | 616 |
| Clean boost | 0.0053 | 0.0064 | 0.0064 | 0.0064 | 0.1909 | 48 |
| Transparent overdrive | 0.0217 | 0.0221 | 0.0221 | 0.0390 | 0.2066 | 64 |
| Mid-focused overdrive | 0.0371 | 0.0379 | 0.0578 | 0.0653 | 0.3345 | 104 |
| High-gain distortion | 0.0390 | 0.0394 | 0.0638 | 0.2235 | 0.3015 | 136 |
| Round fuzz | 0.0311 | 0.0312 | 0.0315 | 0.2021 | 0.2442 | 64 |
| Soft drive | 0.1148 | 0.1369 | 0.1845 | 0.3795 | 0.5115 | 16 |
| Tremolo | 0.0649 | 0.0806 | 0.0862 | 0.5370 | 1.3617 | 96 |
| Chorus | 0.0473 | 0.0480 | 0.0754 | 0.1826 | 0.3244 | 12,448 |
| Flanger | 0.0653 | 0.0949 | 0.1046 | 0.5299 | 1.2405 | 4,792 |
| Phaser | 0.0833 | 0.0836 | 0.0836 | 0.2711 | 0.3919 | 280 |
| Vibrato | 0.0521 | 0.0862 | 0.0904 | 0.2715 | 0.3417 | 1,696 |
| Three-band parametric EQ | 0.0664 | 0.0668 | 0.0671 | 0.2760 | 0.4174 | 160 |
| Delay, normal (2 s stereo allocation) | 0.0266 | 0.0274 | 0.0555 | 0.0698 | 0.2820 | 768,168 |
| Delay, ping-pong (2 s stereo allocation) | 0.0278 | 0.0285 | 0.0593 | 0.2918 | 0.8074 | 768,168 |
| Poly pitch shifter, mono | 5.2952 | 6.8158 | 7.8070 | 11.0857 | 12.9345 | 311,056 |
| Poly pitch shifter, stereo | 11.1655 | 14.3865 | 16.9043 | 20.6691 | 22.1271 | 494,016 |
| Peak limiter | 0.0203 | 0.0206 | 0.0206 | 0.0214 | 0.2029 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0023 | 0.0026 | 0.0041 | 0.0139 | 80 |
| Input + output meters | 0.0180 | 0.0180 | 0.0180 | 0.0195 | 0.3341 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0019 | 0.0030 | 0.0034 | 0.0146 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0019 | 0.0030 | 0.0034 | 0.2389 | 0 |
| Mono → stereo | 0.0038 | 0.0068 | 0.0079 | 0.0131 | 0.2648 | 0 |
| Parallel split (copy to two branches) | 0.0030 | 0.0030 | 0.0030 | 0.0030 | 0.0038 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 0.1830 | 112 |
| Chromatic tuner monitor | 0.0191 | 0.5261 | 0.5633 | 0.9882 | 2.0457 | 10,400 |
| Full 25-effect chain + controls + meters | 13.4903 | 17.9862 | 21.1281 | 24.3236 | 26.2830 | 1,283,456 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.2952% p50 / 6.8158% p95 of the callback deadline with 311,056 B estimated state; stereo uses 11.1655% / 14.3865% and 494,016 B. High-gain Distortion uses 0.0390% p50 / 0.0394% p95 and 136 B; Round Fuzz uses 0.0311% / 0.0312% and 64 B. Both add zero fixed latency and allocate nothing in `process()`. The full 25-effect stereo chain uses 13.4903% p50 / 17.9862% p95; the maximum was 26.2830% in this run, reflecting host scheduling outliers. Mid-focused Overdrive uses 0.0371% / 0.0379% and 104 B. Transparent Overdrive uses 0.0217% / 0.0221% and 64 B. Chorus uses 0.0473% / 0.0480% and 12,448 B; flanger uses 0.0653% / 0.0949% and 4,792 B; phaser uses 0.0833% / 0.0836% and 280 B; vibrato uses 0.0521% / 0.0862% and 1,696 B. The 2 s / 48 kHz / stereo Digital Delay uses 768,168 B and costs 0.0266% p50 / 0.0274% p95 in normal mode or 0.0278% / 0.0285% in ping-pong mode; the routing mode adds no state. These modulation and delay effects add no fixed dry-path latency. Vibrato's wet tap moves between 0.5 and 3.5 ms at maximum depth; the default is 5 Hz, 0.35 ms peak depth, 100% wet, with a 10 ms parameter ramp. Its ring buffer is allocated at construction and `process()` allocates nothing. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the 25-stage chain with pitch enabled at +5 semitones. HeadlessChrome 151 passed local-file decode and Play-output capture with the Ping-Pong routing selector active, comparing the WASM chain with the native renderer at 0.00694054 maximum absolute error and 0.000207709 RMS error (0.2589% reference RMS), within the test's 6% peak and 7.5% RMS bounds. The fixture is mono and verifies the routing setter/mono fallback; native and stereo CLI tests verify alternating stereo repeats. The chain state uses 844,300 B plus 10,364 B tuner state in 16 MiB WASM memory. The 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
