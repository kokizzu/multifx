# Latency, quality, and DSP-load protocol

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

Measured with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks and 5,000 measured blocks per processor. Each effect row resets the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. Utility rows time their respective conversion, split, or mix operation, including the operation's validation and sample work. The full-chain row measures the integrated 16-effect path plus signal controls and both meters.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0045 | 0.0049 | 0.0094 | 0.0101 | 0.2381 | 40 |
| Output gain | 0.0045 | 0.0090 | 0.0097 | 0.0143 | 0.6293 | 40 |
| Volume pedal | 0.0053 | 0.0094 | 0.0101 | 0.0120 | 0.2464 | 56 |
| High-pass | 0.0326 | 0.0327 | 0.0382 | 0.1860 | 0.2832 | 64 |
| Low-pass | 0.0323 | 0.0326 | 0.0390 | 0.2123 | 0.5423 | 64 |
| Noise gate | 0.0195 | 0.0349 | 0.0375 | 0.0469 | 0.3630 | 64 |
| Pedal compressor | 0.1028 | 0.1451 | 0.1515 | 0.4575 | 1.2282 | 72 |
| Studio compressor | 0.0941 | 0.1245 | 0.1316 | 0.4174 | 1.6197 | 64 |
| Wah | 0.0469 | 0.0469 | 0.0473 | 0.2138 | 0.3476 | 104 |
| Envelope wah | 0.1129 | 0.1133 | 0.2682 | 0.4260 | 0.5272 | 616 |
| Clean boost | 0.0045 | 0.0045 | 0.0045 | 0.0097 | 0.2235 | 48 |
| Soft drive | 0.1245 | 0.1965 | 0.2033 | 0.4418 | 0.5741 | 16 |
| Tremolo | 0.0645 | 0.0649 | 0.0656 | 0.3716 | 0.3908 | 96 |
| Three-band parametric EQ | 0.0570 | 0.0574 | 0.0578 | 0.3679 | 0.5970 | 160 |
| Delay (2 s stereo allocation) | 0.0210 | 0.0217 | 0.0229 | 0.0323 | 0.9154 | 768,080 |
| Peak limiter | 0.0176 | 0.0293 | 0.0394 | 0.2149 | 0.3079 | 32 |
| Signal controls (input + dry/wet) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0030 | 80 |
| Input + output meters | 0.0154 | 0.0154 | 0.0244 | 0.0296 | 0.2231 | 72 |
| Stereo → mono (average) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.0019 | 0 |
| Stereo → mono (equal-power) | 0.0015 | 0.0015 | 0.0015 | 0.0015 | 0.3296 | 0 |
| Mono → stereo | 0.0023 | 0.0023 | 0.0023 | 0.0023 | 0.0034 | 0 |
| Parallel split (copy to two branches) | 0.0026 | 0.0026 | 0.0026 | 0.0027 | 0.2096 | 0 |
| Parallel mix (dry + two branches) | 0.0053 | 0.0056 | 0.0075 | 0.0094 | 0.3473 | 112 |
| Chromatic tuner monitor | 0.0139 | 0.5760 | 0.7324 | 1.0196 | 1.2252 | 10,400 |
| Full sixteen-effect chain + controls + meters | 0.7227 | 0.9627 | 1.0778 | 1.9917 | 8.4782 | 769,768 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Soft Drive → Tremolo → Parametric EQ → Delay → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full sixteen-effect row. These are the latest `make bench` results on 2026-11-21 using a Ryzen 9 5950X, GCC 13.3.0, Release, 48 kHz, 128-frame stereo blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Effect timing includes processor calls, both level-meter passes and their status reads, and signal-control/copy work; buffer setup is outside the timed region. The separate converter, parallel-routing, and tuner measurements include their validation and sample/analysis work. The tuner monitor uses 10,400 B on Linux and measures 0.0139% p50, 0.5760% p95, 0.7324% p99, 1.0196% p99.9 and 1.2252% maximum of the callback deadline; analysis is batched, so most blocks are cheap while periodic analysis batches are more expensive. At 48 kHz its 1,024-sample window is 85.3 ms; HeadlessChrome 151 detected a 440 Hz synthetic A4 within 120.9 ms while configured for A4=432 Hz. This is pitch-display response, not signal-path latency. The full-chain row does not include an active parallel graph or tuner analysis. Memory is processor objects plus signal controls, two meter objects, and directly owned delay storage; it excludes input/output scratch buffers, allocator metadata, UI, backend/runtime overhead, and caller-provided utility buffers. Signal controls use 80 B and measured 0.0015% p50 / 0.0015% p95 of the block deadline. Both level meters use 72 B and measured 0.0154% p50 / 0.0154% p95 together. The envelope wah uses 616 B and measured 0.1129% p50 / 0.1133% p95. The converter has no persistent storage; average downmix measured 0.0015% p50 / 0.0015% p95, equal-power downmix 0.0015% / 0.0015%, and mono upmix 0.0023% / 0.0023%. Parallel split measured 0.0026% / 0.0026%; the mixer uses 112 B and measured 0.0053% / 0.0056%. These are processor-call estimates, not a live callback or physical route. Maximum values include host scheduling noise. Re-run after every DSP change and record Android, Windows, and actual-interface results separately.
