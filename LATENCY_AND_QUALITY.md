# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. The full-chain row now measures 17 effects plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0053 | 0.0071 | 0.0075 | 0.0075 | 0.2070 | 40 |
| Output gain | 0.0045 | 0.0045 | 0.0079 | 0.0101 | 0.0146 | 40 |
| Volume pedal | 0.0053 | 0.0056 | 0.0128 | 0.0143 | 0.0214 | 56 |
| High-pass | 0.0323 | 0.0326 | 0.0327 | 0.1654 | 0.2989 | 64 |
| Low-pass | 0.0364 | 0.0405 | 0.0443 | 0.3600 | 0.8149 | 64 |
| Noise gate | 0.0195 | 0.0221 | 0.0222 | 0.0458 | 0.1995 | 64 |
| Pedal compressor | 0.0934 | 0.0941 | 0.1365 | 0.2577 | 0.3262 | 72 |
| Studio compressor | 0.1076 | 0.1399 | 0.1485 | 0.3683 | 0.4879 | 64 |
| Wah | 0.0431 | 0.0495 | 0.0506 | 0.2063 | 0.4279 | 104 |
| Envelope wah | 0.1043 | 0.1159 | 0.1196 | 0.3154 | 0.9184 | 616 |
| Clean boost | 0.0045 | 0.0045 | 0.0045 | 0.0045 | 0.1436 | 48 |
| Soft drive | 0.1377 | 0.2018 | 0.2201 | 0.4710 | 8.2161 | 16 |
| Tremolo | 0.0728 | 0.0735 | 0.0746 | 0.2689 | 0.3469 | 96 |
| Three-band parametric EQ | 0.0525 | 0.0529 | 0.0536 | 0.2175 | 0.3232 | 160 |
| Delay (2 s stereo allocation) | 0.0229 | 0.0229 | 0.0236 | 0.0270 | 0.5044 | 768,080 |
| Poly pitch shifter, mono | 6.2882 | 7.1454 | 8.8360 | 10.2010 | 11.9021 | 311,056 |
| Poly pitch shifter, stereo | 10.5381 | 13.4981 | 14.9048 | 18.2030 | 33.9887 | 494,016 |
| Peak limiter | 0.0165 | 0.0165 | 0.0165 | 0.0165 | 0.2291 | 32 |
| Signal controls (input + dry/wet) | 0.0011 | 0.0015 | 0.0019 | 0.0038 | 0.0116 | 80 |
| Input + output meters | 0.0143 | 0.0143 | 0.0143 | 0.0270 | 0.4628 | 72 |
| Stereo → mono (average) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0 |
| Stereo → mono (equal-power) | 0.0011 | 0.0015 | 0.0015 | 0.0015 | 0.1541 | 0 |
| Mono → stereo | 0.0019 | 0.0023 | 0.0023 | 0.0023 | 0.0023 | 0 |
| Parallel split (copy to two branches) | 0.0023 | 0.0023 | 0.0023 | 0.0023 | 0.0030 | 0 |
| Parallel mix (dry + two branches) | 0.0064 | 0.0083 | 0.0097 | 0.0150 | 0.0184 | 112 |
| Chromatic tuner monitor | 0.0109 | 0.4961 | 0.6525 | 1.0110 | 2.4391 | 10,400 |
| Full 17-effect chain + controls + meters | 13.5409 | 15.2880 | 19.2178 | 22.2066 | 49.1961 | 1,263,784 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Poly Pitch Shifter → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. On this Linux build, mono uses 6.2882% p50 / 7.1454% p95 of the callback deadline and about 304 KiB estimated state; stereo uses 10.5381% / 13.4981% and about 482 KiB. The full 17-effect stereo chain uses 13.5409% p50 / 15.2880% p95. These processor-call values are not a live callback or physical round trip. The local prerecorded regression rendered five clean pickup selections and a stereo DI performance through the chain with the pitch effect enabled at +5 semitones. HeadlessChrome 151 also passed native/WASM full-chain output comparison within documented relative bounds, but its 1 ms timer fallback makes browser CPU percentages coarse. Windows build/runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum values include host scheduling noise; re-run after every DSP change and record device routes separately.
