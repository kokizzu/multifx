# Latency, quality, and DSP-load protocol

## Latest DSP verification (Auto Swell and the 35-stage chain, 2026-11-21)

This run supersedes the earlier delay, full-chain, browser-parity, and memory values retained in the historical note below. Tape Echo reuses the existing delay ring, adds no processor stage or process-time allocation, and leaves the dry path immediate. The native `Delay` object is 104 B larger than the DLY-06 build to hold the fixed saturation/LFO/ramp state, independent of whether Tape is selected. On an AMD Ryzen 9 5950X / GCC 13.3.0 / Release / 48 kHz / 128-frame configuration, Auto Swell measured 0.0270% p50 / 0.0270% p95 and owns 1,104 B; Room Reverb measured 0.1507% / 0.1766% p50/p95 and owns 115,688 B; Hall Reverb 0.2321% / 0.2351% and 193,748 B; Plate Reverb 0.5029% / 0.5074% and 147,488 B; Spring Reverb 0.8996% / 1.2994% and 51,684 B. The 35-effect chain measured 15.2689% p50 / 19.0479% p95 / 21.8560% p99 / 30.0713% p99.9 / 51.2673% max, with 1,794,728 B owned state. The maximum reflects host scheduling noise; median and p95 are more representative processor-call estimates. The maximum reflects host scheduling noise; median and p95 are more representative processor-call estimates.

The current HeadlessChrome 151 mono Play/native comparison enables Auto Swell, Room and Hall Reverb, Tape Echo, and the four-head multi-head route, with the compressor/EQ stages enabled in the 35-stage chain. Maximum absolute error was 0.000446412, RMS error was 0.000030333 (0.4750% of reference RMS), and chain-owned state was 1,484,348 B plus 10,364 B tuner state inside fixed 16 MiB WASM linear memory. This passed the 6% peak and 7.5% RMS bounds. The mono fixture centers panned echoes; stereo CLI/core tests verify stereo placement. The browser 1 ms timer fallback makes its live CPU percentage coarse. Older test and benchmark history remains below.

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Linux desktop DSP baseline (2026-11-21)

The saved processor rows below were measured on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. Auto Swell and the full-chain row were refreshed after integration; other individual rows preserve their earlier same-day readings. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0064 | 0.0064 | 0.0079 | 0.2288 | 40 |
| Output gain | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2097 | 40 |
| Volume pedal | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 56 |
| High-pass | 0.0371 | 0.0375 | 0.0379 | 0.1751 | 0.2010 | 64 |
| Low-pass | 0.0375 | 0.0379 | 0.0450 | 0.2610 | 0.9777 | 64 |
| Noise gate | 0.0229 | 0.0390 | 0.0409 | 0.0604 | 0.4358 | 64 |
| Auto Swell | 0.0270 | 0.0270 | 0.0315 | 0.1087 | 0.2546 | 1,104 |
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
| Delay, normal (2 s stereo allocation) | 0.1103 | 0.1155 | 0.2153 | 0.3465 | 0.7313 | 768,720 |
| Room Reverb (large, 1.2 s decay) | 0.1736 | 0.1796 | 0.3225 | 0.4706 | 0.5430 | 115,688 |
| Hall Reverb (3.2 s decay) | 0.2213 | 0.2246 | 0.3750 | 0.4538 | 0.7462 | 193,748 |
| Delay, BBD-style mono | 0.0919 | 0.0938 | 0.0941 | 0.3019 | 0.3124 | 384,716 |
| Delay, BBD-style stereo | 0.1418 | 0.1493 | 0.1500 | 0.3536 | 0.3683 | 768,720 |
| Delay, Tape mono | 0.1658 | 0.1699 | 0.1714 | 0.3806 | 0.5442 | 384,716 |
| Delay, Tape stereo | 0.2614 | 0.2771 | 0.2794 | 0.5194 | 0.6420 | 768,720 |
| Delay, ping-pong (2 s stereo allocation) | 0.1399 | 0.2599 | 0.3105 | 0.6236 | 1.1629 | 768,720 |
| Delay, independent dual mono (two 2 s lines) | 0.1185 | 0.1196 | 0.1958 | 0.4324 | 0.8040 | 768,720 |
| Delay, independent dual stereo (two 2 s lines) | 0.1395 | 0.1470 | 0.1496 | 0.4032 | 0.4916 | 768,720 |
| Delay, four-tap mono (2 s ring) | 0.1493 | 0.1515 | 0.1519 | 0.3705 | 0.7676 | 384,716 |
| Delay, four-tap stereo (2 s ring) | 0.2134 | 0.2171 | 0.2231 | 0.4834 | 0.5292 | 768,720 |
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
| Optical compressor | 0.1534 | 0.1545 | 0.1857 | 0.3623 | 0.9210 | 232 |
| FET compressor | 0.1035 | 0.1043 | 0.1043 | 0.3416 | 0.4163 | 72 |
| Guitar graphic EQ (7-band) | 0.1324 | 0.1328 | 0.2187 | 0.3341 | 0.4875 | 304 |
| Bass graphic EQ (7-band) | 0.1305 | 0.1316 | 0.1317 | 0.3274 | 0.4279 | 304 |
| Shelf/tilt EQ | 0.0405 | 0.0409 | 0.0409 | 0.2141 | 0.2472 | 96 |
| Delay, multi-head (four heads, 110 ms spacing) | 0.2040 | 0.2134 | 0.4077 | 0.6101 | 1.1149 | 768,720 |
| Full 35-effect chain + controls + meters | 15.2689 | 19.0479 | 21.8560 | 30.0713 | 51.2673 | 1,794,728 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Optical Compressor → FET Compressor → Guitar Graphic EQ → Bass Graphic EQ → Shelf/Tilt EQ → Room Reverb → Hall Reverb → Plate Reverb → Spring Reverb → Auto Swell → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. In the latest Linux run, mono uses 5.3199% p50 / 6.6606% p95 of the callback deadline with 311,056 B estimated state; stereo uses 10.9428% / 14.0456% and 494,016 B. The latest 35-effect stereo chain uses 15.2689% p50 / 19.0479% p95 and 1,794,728 B; its maximum was 51.2673% in this run, reflecting host scheduling outliers. Auto Swell has no fixed algorithmic latency and uses 0.0270% / 0.0270% p50/p95 with 1,104 B state. Room, Hall, Plate, and Spring Reverb also add no fixed dry-path latency; their latest CPU and memory values appear in the table above. The local prerecorded regression passed across five clean pickup-position samples and a 72.5-second stereo DI excerpt through all 35 effects, with Tape Echo and multi-head routing in separate passes. HeadlessChrome 151 passed the Auto Swell + Room + Hall + Plate Reverb + Tape + four-head Play/native comparison at 0.000446412 maximum absolute error and 0.000030333 RMS error. Windows runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum benchmark values include host scheduling noise; measure device routes separately.
