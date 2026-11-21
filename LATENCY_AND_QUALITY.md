# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row measures 18 effects plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0064 | 0.0094 | 0.0116 | 0.2134 | 40 |
| Output gain | 0.0060 | 0.0064 | 0.0068 | 0.0068 | 0.2108 | 40 |
| Volume pedal | 0.0045 | 0.0101 | 0.0109 | 0.0120 | 0.1766 | 56 |
| High-pass | 0.0326 | 0.0364 | 0.0394 | 0.1868 | 0.4148 | 64 |
| Low-pass | 0.0326 | 0.0326 | 0.0330 | 0.1515 | 0.1811 | 64 |
| Noise gate | 0.0217 | 0.0319 | 0.0349 | 0.0386 | 0.3506 | 64 |
| Pedal compressor | 0.0926 | 0.0930 | 0.1358 | 0.3480 | 0.4215 | 72 |
| Studio compressor | 0.1133 | 0.1174 | 0.1646 | 0.3188 | 6.2863 | 64 |
| Wah | 0.0427 | 0.0473 | 0.0492 | 0.2861 | 0.3319 | 104 |
| Envelope wah | 0.1020 | 0.1028 | 0.1155 | 0.3529 | 0.7050 | 616 |
| Clean boost | 0.0049 | 0.0049 | 0.0049 | 0.0090 | 0.2352 | 48 |
| Soft drive | 0.1391 | 0.2209 | 0.2262 | 0.4500 | 0.7973 | 16 |
| Tremolo | 0.0585 | 0.0589 | 0.0596 | 0.3038 | 0.3304 | 96 |
| Chorus | 0.0427 | 0.0432 | 0.0709 | 0.2865 | 0.3510 | 12,448 |
| Three-band parametric EQ | 0.0521 | 0.0529 | 0.0743 | 0.2839 | 1.0084 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0229 | 0.0244 | 0.0311 | 0.2340 | 768,080 |
| Poly pitch shifter, mono | 6.4941 | 7.7072 | 9.4960 | 12.9315 | 26.1337 | 311,056 |
| Poly pitch shifter, stereo | 12.9266 | 15.3244 | 18.6196 | 22.3360 | 25.7505 | 494,016 |
| Peak limiter | 0.0161 | 0.0229 | 0.0266 | 0.0293 | 0.2929 | 32 |
| Signal controls (input + dry/wet) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 80 |
| Input + output meters | 0.0139 | 0.0143 | 0.0229 | 0.0356 | 0.2430 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0026 | 0.2171 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Mono → stereo | 0.0030 | 0.0045 | 0.0060 | 0.0068 | 0.0150 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0023 | 0.0045 | 0.0079 | 0.0124 | 0 |
| Parallel mix (dry + two branches) | 0.0049 | 0.0068 | 0.0075 | 0.0094 | 0.0281 | 112 |
| Chromatic tuner monitor | 0.0083 | 0.3923 | 0.4178 | 0.6387 | 0.6739 | 10,400 |
| Full 18-effect chain + controls + meters | 12.3933 | 15.6267 | 18.4704 | 23.5368 | 41.2500 | 1,276,232 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Chorus → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. On this Linux build, mono uses 6.4941% p50 / 7.7072% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 12.9266% / 15.3244% and about 482 KiB. The full 18-effect stereo chain uses 12.3933% p50 / 15.6267% p95. The chorus alone uses 0.0427% p50 / 0.0432% p95 and 12,448 B in stereo; its modulated wet delay is 1–30 ms, with zero fixed dry-path latency. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the chain with the pitch effect enabled at +5 semitones. HeadlessChrome 151 passed native/WASM full-chain output comparison with 0.0000513 maximum absolute error and 0.00000660 RMS error (0.096% reference RMS); its 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
