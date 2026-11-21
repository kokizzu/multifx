# Latency, quality, and DSP-load protocol

## Latest DSP verification (Triple-Double Chorus and 51-stage browser/audio chain, 2026-11-22)

Triple-Double Chorus has no fixed dry-path latency and owns 98,448 B at 48 kHz stereo. Its isolated Linux benchmark measured 0.4631% p50 / 0.4733% p95 / 0.6630% p99 / 1.1963% p99.9 / 1.7423% maximum of the 2.667 ms callback deadline. The 51-effect full chain plus signal controls/meters measured 17.5224% p50 / 19.0832% p95 / 22.5944% p99 / 25.3751% p99.9 / 26.5578% maximum and owns 6,507,712 B. The run used the Release build, Ryzen 9 5950X, GCC 13.3.0, 48 kHz stereo, 128-frame blocks, 2,000 warm-up blocks and 5,000 measured blocks. These are processor-call estimates, not live-device load. The HeadlessChrome 151 Play/native comparison passed with 51 stages, maximum absolute error 0.00000403326703235507 and RMS error 0.0000003046496675773319; browser chain state was 3,898,796 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. `playOutputCaptured=true`; the browser's 1 ms timer fallback makes per-effect CPU percentages coarse. Offline prerecorded guitar regression also passed for five pickup positions and the stereo Guitar-TECHS DI fixture.

## Earlier DSP verification (Dimension Chorus, Bit Crusher, Multiband Compressor, and 50-stage browser/audio chain, 2026-11-22)

Swept Filter Delay adds no fixed dry-path latency and owns 768,952 B for its two-second stereo ring. Its project-authored biquad band-pass sweep runs on the delayed wet output after the delay; dry audio passes immediately. The isolated Linux benchmark measured 0.5018% p95 of the 2.667 ms callback deadline at 48 kHz stereo/128 frames. Reverse Delay has no fixed dry-path latency; its rolling reverse output starts after the selected capture interval. Its current stereo state is 1,536,264 B. Dimension Chorus has no fixed dry-path latency and owns 11,712 B at 48 kHz stereo; it measures 0.0555% p50 / 0.0559% p95 / 0.0563% p99 / 0.1995% p99.9 / 0.2622% maximum. Its Version/Mode labels follow the cited manufacturer documentation, while its internal modulation profiles and DSP are project-authored. The Formant Filter is a manual four-band A/E/I/O/U spectral morph (not tracking or automatic humanizer movement); it owns 616 B. The project-authored Harmonic Resonator uses eight band-pass modes at integer multiples of a root frequency; it owns 688 B and measures 0.1939% p50 / 0.1958% p95 / 0.2981% p99 / 0.7988% p99.9 / 1.0819% maximum. Bit Crusher has no fixed algorithmic latency, owns 40 B, and measures 0.0386% p50 / 0.0386% p95 / 0.0390% p99 / 0.1894% p99.9 / 2.0067% maximum. The project-authored Multiband Compressor has no fixed algorithmic latency, owns 584 B, and measures 0.7264% p50 / 0.8059% p95 / 1.1873% p99 / 1.3954% p99.9 / 1.7888% maximum. The full 50-effect chain plus signal controls/meters measures 17.0209% p50 / 21.1431% p95 / 23.6466% p99 / 28.0362% p99.9 / 30.7531% max, with 6,409,264 B owned state. These are processor-call estimates, not measured live-device load. Prior full-chain measurements remain in the historical baseline below.

The current HeadlessChrome 151 mono Play/native comparison enables Auto Swell, Rotary Speaker, Ring Modulator, Rhythmic Slicer, Step Filter, static Notch Filter, manual Formant Filter, Harmonic Resonator, three-band compression, Bit Crusher (8-bit depth, divisor 4, 0.65 wet), Dimension Chorus (Version C, Mode 1, 50% Mix, 0 dB project-authored Level), Reverse Delay, Ducking Delay, Modulated Delay, Filtered Delay, Room and Hall Reverb, Tape Echo, the four-head multi-head route, and compressor/EQ stages in the 50-stage chain. Formant Filter uses vowel position 1.5, Q 6, mix 0.72, and output level −2 dB. Harmonic Resonator uses 220 Hz root, 0.8 s decay, 0.7 brightness, and 0.65 mix. Multiband Compressor uses 500/4,000 Hz crossovers, 12 dB/oct slope, and distinct low/mid/high thresholds, ratios, knees, attacks, releases, levels, Peak/RMS detectors, and mute states. Filtered Delay uses 350 ms Time, 0.25 Feedback/Mix, 120 Hz Low Cut, and 6,000 Hz High Cut. Swept Filter Delay uses a project-authored biquad band-pass sweep on the delayed wet output, with immediate dry audio. Reverse Delay uses 350 ms free Time, 0.25 Feedback, 0.35 Mix, 10 ms Crossfade, Run on, repeat cuts off, Forward Level 0, and Spillover off; the browser/WASM setter also carries tempo mode, division, tempo, cuts, forward level, and spillover. Ducking Delay uses 350 ms Time, 0.25 Feedback/Mix, −30 dB Threshold, 12 dB Attenuation, and 250 ms Release; Modulated Delay uses 350 ms Time, 0.25 Feedback/Mix, 0.8 Hz Rate, 2 ms Sine depth, 180° Spread, and −24 dB trigger threshold. Maximum absolute error was 0.000005093228537589312 and RMS error was 0.00000037695482632659376. The browser Play capture reported `playOutputCaptured=true`; the stage chain owns 3,800,352 B and tuner state is reported separately as 10,364 B inside the fixed 16 MiB WASM heap. The mono fixture centers panned echoes; stereo CLI/core tests verify stereo placement. The browser 1 ms timer fallback makes its live CPU percentage coarse. Older test and benchmark history remains below.

Status: Linux desktop processor-call and owned-memory measurements are recorded below. A browser AudioWorklet also reports per-processor and whole-chain callback load, but HeadlessChrome 151 falls back to a 1 ms timer, so those live browser percentages are coarse. Native hardware round-trip, Windows/Android routes, and sustained live callback measurements remain unverified.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Earlier Linux desktop DSP baseline (before DLY-20, 2026-11-21)

The saved processor rows below are the prior snapshot before DLY-20 and were measured on 2026-11-21 with `make bench` on an AMD Ryzen 9 5950X 16-Core Processor, GCC 13.3.0, Release build, 48 kHz, 128-frame blocks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks per processor. Filtered Delay is the new isolated row for the 44-stage integration; the saved full-chain CPU and memory row remains from the prior 43-effect run. Ducking Delay, Modulated Delay, Reverse Delay, Notch Filter, Step Filter, Rhythmic Slicer, Ring Modulator, Rotary Speaker, Auto Swell, and that prior full-chain row were refreshed for the 43-effect integration; other individual rows preserve earlier same-day readings. The pitch effect is benchmarked in mono and stereo; other processor rows use stereo unless the operation says otherwise. Effect rows reset the same deterministic buffer before timing `Effect::process()`; source-buffer copy and setup are outside that interval. Tail values vary with host scheduling; treat the median/p95 as processor-call estimates, not live audio-route usage.

| Processor / chain | Median % | p95 % | p99 % | p99.9 % | Maximum % | Owned memory bytes |
|---|---:|---:|---:|---:|---:|---:|
| Input gain | 0.0056 | 0.0064 | 0.0064 | 0.0079 | 0.2288 | 40 |
| Output gain | 0.0049 | 0.0053 | 0.0053 | 0.0053 | 0.2097 | 40 |
| Volume pedal | 0.0060 | 0.0064 | 0.0064 | 0.0064 | 0.0064 | 56 |
| High-pass | 0.0371 | 0.0375 | 0.0379 | 0.1751 | 0.2010 | 64 |
| Low-pass | 0.0375 | 0.0379 | 0.0450 | 0.2610 | 0.9777 | 64 |
| Noise gate | 0.0229 | 0.0390 | 0.0409 | 0.0604 | 0.4358 | 64 |
| Auto Swell | 0.0214 | 0.0217 | 0.0319 | 0.0394 | 0.4249 | 1,104 |
| Rotary Speaker | 0.3942 | 0.5175 | 0.5847 | 0.7969 | 1.5863 | 1,208 |
| Ring Modulator | 0.0259 | 0.0263 | 0.0308 | 0.0360 | 0.8880 | 72 |
| Rhythmic Slicer | 0.0371 | 0.0521 | 0.0525 | 0.0784 | 0.2891 | 136 |
| Step Filter | 0.1065 | 0.1414 | 0.1459 | 0.3765 | 0.8213 | 1,200 |
| Static Notch Filter | 0.0548 | 0.0825 | 0.0859 | 0.2993 | 0.4463 | 96 |
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
| Reverse Delay, stereo (2 s maximum) | 0.0420 | 0.0600 | 0.1403 | 0.2918 | 0.4601 | 1,536,160 |
| Ducking Delay, stereo (2 s maximum) | 0.0986 | 0.0990 | 0.1013 | 0.3818 | 0.6604 | 768,184 |
| Modulated Delay, stereo (2 s maximum) | 0.2663 | 0.2963 | 0.3210 | 0.5445 | 0.9570 | 768,216 |
| Filtered Delay, stereo (2 s maximum) | — | 0.1200 | — | — | — | 768,832 |
| Full 42-effect chain at DLY-10 checkpoint + controls + meters | 16.1900 | 20.5858 | 24.1638 | 32.9570 | 87.9204 | 4,101,784 |
| Full 43-effect chain + controls + meters | 16.2747 | 21.7457 | 27.6349 | 33.5364 | 131.2431 | 4,870,000 |
| Full 44-effect chain + controls + meters | 16.4300 | 20.5149 | 23.6335 | 29.7180 | 30.8194 | 5,638,904 |

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Optical Compressor → FET Compressor → Guitar Graphic EQ → Bass Graphic EQ → Shelf/Tilt EQ → Room Reverb → Hall Reverb → Plate Reverb → Spring Reverb → Auto Swell → Rotary Speaker → Ring Modulator → Rhythmic Slicer → Step Filter → Static Notch Filter → Reverse Delay → Ducking Delay → Modulated Delay → Filtered Delay → Swept Filter Delay → Formant Filter → Harmonic Resonator → Multiband Compressor → Bit Crusher → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

Historical context: the 38-stage snapshot and the 39-, 40-, and 41-stage checkpoints below are older runs. The 44-stage benchmark row is the prior pre-DLY-20 full-chain measurement; the 45-stage row predates Formant Filter; the 46-stage run predates Harmonic Resonator; the 47-stage run predates Multiband Compressor. The latest 49-stage results are reported above.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. A fresh `make bench` run gives the 38-effect stereo chain 18.4332% p50 / 23.6950% p95 and 1,795,936 B; its 62.2003% maximum reflects host scheduling outliers. Rhythmic Slicer adds no fixed latency and uses 0.0382% / 0.0529% p50/p95 with 136 B state. Ring Modulator uses 0.0266% / 0.0270% with 72 B; Rotary Speaker uses 0.3942% / 0.5175% with 1,208 B; Auto Swell uses 0.0214% / 0.0217% with 1,104 B. Room, Hall, Plate, and Spring Reverb also add no fixed dry-path latency. The local prerecorded regression passed across five clean pickup-position samples and a 72.5-second stereo DI excerpt through all 38 effects, with Tape Echo and multi-head routing in separate passes. HeadlessChrome 151 passed the Rhythmic Slicer + Auto Swell + Rotary Speaker + Ring Modulator + Room + Hall + Plate Reverb + Tape + four-head Play/native comparison at 0.000515713 maximum absolute error and 0.0000248161 RMS error. Windows runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum benchmark values include host scheduling noise; measure device routes separately.
