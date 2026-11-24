# Latency, quality, and DSP-load protocol

## Fender Tone Master Pro EQ5 Graphic-inspired Equalizer (2026-11-24)

The project-authored five-band peaking EQ uses fixed 80/240/750/2,200/6,600 Hz centers and a separate overall Gain control. Bands and overall Gain default to 0 dB with project-selected ±12 dB bounds and fixed Q=1. It has 224 B owned state at 48 kHz stereo, no fixed algorithmic latency, and no process-time allocation. These topology and controls do not reproduce Fender DSP or settings.

An isolated Release benchmark on an AMD Ryzen 9 5950X at 48 kHz stereo/128-frame blocks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max of 0.1054/0.1227/0.3221/0.8700/1.1606% of the deadline. `make test` covers centers/defaults/bounds, response, overall gain, stereo isolation, irregular blocks, reset, finite recovery, and zero allocation; CLI, local prerecorded Guitar-TECHS DI browser Play, full browser/native parity, and Windows MinGW core build pass. The 104-row browser comparison reports maximum absolute error 6.4534569e-6 and RMS error 1.2677618e-6. These are desktop processor-call measurements, not tablet/device guarantees. No human listening result is available.

## Line 6 Helix Stadium Double Take-inspired Modulation (2026-11-24)

The project-authored Double Take processor uses four preallocated delay voices with a 24 ms maximum ring. The default two-double configuration uses Slop 0.25, Sensitivity 0.55, Dry 0.85, Wet 0.15, and Mono Source. Project bounds, dynamic envelope response, timing drift, voice placement, delay values, and mixing do not claim Line 6 settings or DSP parity. Stereo processor-owned state is 9,504 B at 48 kHz. The dry path is immediate; wet voices use a 7–10.75 ms base delay with up to ±2.5 ms modulation. Construction allocates the fixed ring; process() performs no allocation.

An isolated Release benchmark on an AMD Ryzen 9 5950X at 48 kHz stereo/128-frame blocks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max of 0.2528/0.3424/0.4823/0.6792/2.1484% of the deadline. This is processor-call timing on that desktop, not an Android/Windows live-route or end-to-end measurement. Native tests cover defaults/bounds, all Source choices, immediate dry timing, wet onset, irregular callback partitioning, reset, finite recovery, and no allocation. Browser Play smoke uses existing local prerecorded Guitar-TECHS DI and verifies measurable changed output; browser/native comparison and Windows MinGW core cross-build pass. No human listening or physical-device result is available.

## Project-authored Fractal-inspired Octave Divider (2026-11-24)

The fixed crossing-derived square-wave divider adds independent −12 and −24 semitone sub voices without an FFT, note tracker, delay line, or process-time allocation. Its measured state is 192 B at 48 kHz stereo, with zero fixed algorithmic latency. A Ryzen 9 5950X Release run (48 kHz stereo, 128-frame blocks, 2,000 warm-up and 5,000 measured blocks) measured p50/p95/p99/p99.9/max at 0.0503/0.0953/0.1050/0.2940/0.4571% of the 2.667 ms callback deadline. These processor-call timings are desktop estimates, not tablet or end-to-end latency evidence.

Native tests cover −12/−24 voice frequency, parameter bounds, stereo pan/mix, reset, irregular blocks, finite recovery, and no allocation. The offline CLI smoke uses a 220 Hz tone. `make web-test` passes the local prerecorded Guitar-TECHS DI Play smoke and full browser/native comparison; the 73-stage comparison reports maximum absolute error 6.568e-6 and RMS error 1.360e-6. Browser memory is fixed at 96 MiB; the complete chain reports 80,329,680 B processor state plus 10,364 B tuner state. The effect's individual WASM memory is included in that total. Windows MinGW core cross-build passes. No Fractal algorithm/settings/sound parity, human listening, Android measurement, or device test is established.

## Fractal Rotary Speaker Hi Depth behavior (2026-11-24)

The November 2026 FM9-family Blocks Guide (printed p. 98) says setting Hi Depth to zero can simulate drum-only operation; it does not publish numeric bounds or define the control mapping. The project now scales the horn/Doppler path with Hi Depth, so Rotor Length cannot move the horn at zero depth. A regression checks that zero-depth case and verifies that Rotor Length still changes the high-rotor output with nonzero depth. This is project-authored behavior, not Fractal DSP/settings/sound parity.

`make bench` on an AMD Ryzen 9 5950X, Release build, 48 kHz stereo, 128-frame blocks (2.667 ms deadline), 2,000 warm-up and 5,000 measured blocks reports p50/p95/p99/p99.9/max 0.4819/0.4917/0.6994/1.0369/1.5627% of the callback deadline. Owned state is 1,208 B; there is no fixed algorithmic latency, and `process()` does not allocate. This is a desktop processor-call estimate. FM9-family v12.0 download listings name FM9 Original and Turbo but not Mark II Turbo; installed firmware and device performance are unknown. [Blocks Guide](https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf) · [FM9 downloads](https://www.fractalaudio.com/fm9-downloads/)

## XRecer Cabinet IR loader (2026-11-24)

The project-authored IR Loader at picker ID 100 uses a user-supplied mono 24-bit/48 kHz PCM WAV with exactly 2,048 frames, matching the file shape stated in the XR1 FAQ. The FAQ does not state channel count or explain its “dual” variant; the project accepts one mono IR and applies it separately to mono/stereo host channels. A 64-frame uniform FFT partition adds 1.333 ms at 48 kHz, with no allocation in process(); measured owned stereo state is 54,248 B. Import, validation, resampling, and kernel setup occur off the audio callback.

make bench on an AMD Ryzen 9 5950X, Release, 48 kHz stereo/128-frame blocks (2.667 ms deadline), 2,000 warm-up and 5,000 measured blocks reports p50/p95/p99/p99.9/max 1.1652/1.4802/2.2793/3.1640/4.4071% of the deadline. This desktop processor-call estimate is not a live-device guarantee or a measurement of XR1 DSP. The project path passes native tests, browser/WASM local guitar-DI loading/playback checks, and a Windows MinGW core cross-build. Manual listening and hardware checks remain open. [XR1 FAQ](https://www.xrecer.com/pages/faq)

## Chromatic Pitch Delay and extended Arpeggiator range (2026-11-24)

The project-authored Chromatic Pitch Delay was measured in a Release build on an AMD Ryzen 9 5950X at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks). Its processor-call p50/p95/p99/p99.9/max was 10.4013/14.0888/16.7569/22.0945/26.3696% of one callback, with 1,262,216 B directly owned state. A wet pitch repeat has 256 samples (5.333 ms) of pitch-engine latency; dry remains immediate. `process()` allocates no memory. These are one desktop benchmark run and do not predict Android tablet load or end-to-end route latency.

The project Pitch Arpeggiator's ±24 scale-degree range benchmarked at 15.8172/25.3743/26.5185/38.7033/63.0741% p50/p95/p99/p99.9/max, with 1,024,888 B owned state, under the same settings. It is still monophonic and retains an approximately 26.7 ms first-note response estimate at 48 kHz. Its higher tail latency/load deserves device testing and optimization if the target tablet cannot maintain the audio deadline. The wider scale-degree clamp did not add per-callback memory or a new processor. Both figures are isolated processor-call measurements; multiple simultaneous effects add cost, and browser millisecond timing is too coarse for precise per-effect real-time guarantees.

## Dynamic Delay controls and modulation (2026-11-24)

The Fender-informed, project-authored controls were measured with the Release build on an AMD Ryzen 9 5950X at 48 kHz stereo and 128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks). Default Free Time with repeat cuts and modulation depth off measured p50/p95/p99/p99.9/max processor-call time of 0.1331/0.1335/0.1797/1.7003/2.8043% of that deadline. The filtered/modulated case (150/7,000 Hz repeat cuts, 1.25 Hz, 6.5 ms, Sine) measured 0.1823/0.1838/0.2355/1.6733/1.7640%. The fixed 2-second stereo processor owns 768,296 B in either case; `process()` performs no allocation. At this tested setting the additional filters and modulation stay below 0.24% p99 of the callback deadline on this desktop CPU. These are synthetic processor-call measurements, not Android/Windows live-route or round-trip latency evidence.

Native tests cover Sine/Triangle/Random/Triggered Ramp, delay-headroom limits, threshold retriggering, stereo linkage, reset determinism, callback partitioning, finite samples, and zero allocations. `make web-browser-test` captured actual local prerecorded Guitar-TECHS DI Play output in the categorized Delay row at index 39 with 120 BPM/eighth timing, 150/7,000 Hz repeat cuts, and 1.25 Hz/6.5 ms Sine modulation (peak 0.0614053, 71,430 active samples, difference RMS 0.0138586). The 73-active-stage browser/native comparison passed with max absolute error 6.683e-6 and RMS error 1.218e-6. WASM uses a fixed 96 MiB heap with 78,988,648 B processor state plus 10,364 B tuner state. Windows MinGW core cross-build passed.

The whole synthetic chain in the same benchmark run measured p50/p95 61.3303/163.4947% and 25,971,736 B owned state, so full-chain live safety is not established by this benchmark.
## Fractal Formant Morph and 93-entry picker (2026-11-23)

The existing project-authored Formant Filter retains its five-position A/E/I/O/U sweep and now adds a three-vowel mode with Start/Middle/End selections, a manual Control position, Resonance, Mix, Level, and stereo Balance. Project-authored filter profiles and bounds do not reproduce Fractal DSP or settings. The C++ processor has fixed 648 B state, no process-time allocation, and no added algorithmic delay. On the 48 kHz stereo/128-frame Release benchmark, one isolated processor used p50 0.0896%, p95 0.1425%, p99 0.1560%, p99.9 0.9098%, and maximum 1.4400% of the 2.667 ms callback deadline; this is a processor-call estimate, not a live audio-device measurement.

`make test` passed 96/96 CTest cases and 11 fixture-safety checks. `make web-browser-test` passed the 93-row categorized picker and 71-active-stage native/browser comparison using the existing local prerecorded stereo Guitar-TECHS direct-input recording (maximum absolute error 0.00000694227; RMS error 0.00000122863); the three-vowel Formant Play capture produced finite changed output. The full stereo chain used 32,959,916 B processor state plus 10,364 B tuner state inside a fixed 64 MiB WASM heap. Human listening and physical device checks remain open. The source guide does not name Mark II Turbo, and installed firmware is unknown.

## AM Ring Mod and 86-entry picker (2026-11-23)

The project-authored Modulation stage at index 83 has zero fixed latency and no process-time allocation. A separate HeadlessChrome 151 Play smoke on the generated local guitar fixture (713 Hz, depth 0.8, mix 0.83, level 0 dB) produced peak 0.7991722 and 47,996 active samples. The browser/native comparison keeps this stage disabled and remains at 70 active stages (maximum absolute error 0.0000139684; RMS error 0.0000026858). `make audio-regression` also passed with AM enabled on prerecorded pickup and direct-input files; full-chain levels are very quiet, so this is stability evidence. Full chain memory is 21,253,996 B plus 10,364 B tuner state in a fixed 24 MiB heap; no isolated AM memory or CPU result has been measured. This project-authored carrier-amplitude effect does not claim Line 6 model parity.
## Octobot and 85-entry picker (2026-11-23)

Project-authored Octobot has separate Direct, Down, and Sizzle branches. The local guitar Play smoke through browser/WASM produced finite output with peak 0.679103732 across 47,804 active samples. The 70-active-stage browser/native comparison passed with Octobot disabled because the offline renderer has no matching stage (maximum absolute error 0.0000139687; RMS error 0.0000026858). Full browser chain state was 21,253,940 B plus 10,364 B tuner state inside the fixed 24 MiB WASM heap; this is not Octobot's isolated memory cost. The project shifter's reported 256-sample latency is 5.333 ms at 48 kHz; the direct branch is delayed by the same 256 samples. No isolated CPU measurement, human listening, device-route check, or Fender DSP/control/sound parity has been established. Chrome 151 exposes only millisecond-fallback AudioWorklet timing here.

## Three-Stage Distortion and 83-entry picker (2026-11-23)

The project-authored Drive & fuzz stage has three smoothed soft-clipping gain stages with active Bass/Mid/Treble shaping, two Bright switches, and Level. Tests cover parameter bounds, reset, finite-sample recovery, no process-time allocation, and the expected effect on a driven signal. The native CLI smoke confirms finite output that differs from dry. `make web-test` passes the 83-entry picker and 70-active-stage browser/native comparison (maximum absolute error 0.0000139687; RMS error 0.0000026858); the separate local generated-guitar Play smoke at slot 80 captured peak 0.7934932 and 47,999 active samples. The full chain uses 20,630,044 B of processor state plus 10,364 B tuner state in a fixed 20 MiB WASM heap; this does not isolate the distortion's memory cost. No isolated CPU benchmark, human listening, device route, aliasing/quality evaluation, or Line 6 DSP parity has been established. Chrome 151 exposes only millisecond-fallback AudioWorklet timing here.

## Cloud Reverb and 82-entry picker (2026-11-23)

Project-authored Cloud Reverb uses bounded mono/stereo state, smoothed controls, and no allocation in `process()`. Its browser/WASM smoke ran on a generated guitar-like input; the general browser/native comparison disables it because the offline renderer has no matching stage. The 70-active-stage comparison reports 0.0000139687 maximum absolute error and 0.0000026858 RMS error. The separate Cloud smoke returned peak 0.0864311 with 47,997 active samples. Full-chain browser state was 20,629,216 B plus 10,364 B tuner state inside a fixed 20 MiB WASM heap; this is not Cloud's isolated memory cost. No isolated Cloud callback benchmark, listening result, or device-route measurement has been run. Chrome 151 exposes only millisecond-fallback AudioWorklet timing in this environment, which is too coarse for an isolated processor benchmark.

## Rhythm Tap Delay and 78-entry picker (2026-11-22)

The project-authored Rhythm Tap stage adds a preallocated ring capped at 1 MiB and a 20 s requested maximum; effective maximum depends on sample rate and mono/stereo storage. C++ processing has no per-block allocation. The browser chain uses 17,793,612 B of processor state plus 10,364 B tuner state in a fixed 20 MiB WASM heap. `make test` passed 81/81 CTest cases and `make web-test` passed the 78-entry picker, 70-stage browser/native parity, and separate Play capture on a generated local guitar-like fixture. The Rhythm Tap capture was peak 0.7162358 / 46,079 active samples; the effect is excluded from numeric parity because the offline CLI has no matching implementation. No Rhythm Tap isolated CPU benchmark, listening check, or live device-route measurement has been run. Chrome 151 exposes only millisecond-resolution worklet timing here; its per-stage percentages are too coarse for a processor benchmark.

## Pitch Sequence and 77-entry picker (2026-11-22)

Project-authored Pitch Sequence uses the shared STFT polyphonic pitch shifter, with a programmed semitone transposition per step and no note tracker or scale quantizer. Its pitch-shifter algorithm reports 5.33 ms nominal latency at 48 kHz; host buffering and end-to-end response have not been measured. The effect preallocates a 120 ms input-history ring for sample-loop Freeze and an aligned dry delay. Glitch offsets are deterministic pseudo-random pitch changes; Freeze is not spectral freezing. These are project-authored controls and DSP, not Fender model parity.

`make web-test` passed 81/81 CTest cases and 11 fixture-safety tests. The 77-entry categorized picker and 70-stage browser/native comparison passed; the local-guitar Pitch/Glitch/Freeze smoke captured peak 0.6217259 and 47,801 active samples at slot 74. The comparison keeps Pitch Sequence disabled because the native offline chain does not implement it. The full mono browser chain uses 16,744,464 B of processor state plus 10,364 B tuner state in a fixed 20 MiB heap. No isolated Pitch Sequence CPU benchmark, stereo-chain heap measurement, human listening, or device-route test has been run.

## Porta Preamp and 76-entry picker (2026-11-22)

The project-authored Porta Preamp uses two gain stages with soft clipping, fixed Bass/Treble shelves, and Level. Defaults are 6/3/0/0/0 dB. The gain stages range −12…+24 dB; Bass/Treble −12…+12 dB; Level −24…+12 dB. Control changes smooth over 10 ms. The implementation has fixed state, no delay line or fixed latency, and no per-process allocation. No isolated CPU measurement or human listening result is available.

`make test` passed 80/80 CTest cases. `make web-test` passed the 76-entry Amp / preamp picker and HeadlessChrome 151 local-guitar Play smoke at slot 73 (peak 0.8912509; 47,906 active samples). Browser/native parity excludes this project-authored stage because the offline CLI has no matching processor. Line 6's Stadium v1.4 notes document PortaBox 424 control labels but no ranges, defaults, or DSP; project settings and behavior do not claim Line 6 parity.

## Vibe and 74-entry picker (2026-11-22)

The project-authored Vibe processor follows the control concepts documented for Line 6 Helix Stadium Essex Vibe. It uses one or two swept first-order all-pass stages, a dry/wet depth blend, amplitude modulation, stereo spread, and a linear output level. Rate is 1.5 Hz default (project range 0.1–10 Hz), Depth 70%, Trem 20%, Stages 2, Spread 50%, Level 1.0 (0–2). Defaults, ranges, topology, and DSP are project choices. Processing uses fixed state and no delay line; no isolated callback CPU measurement or human listening result has been run.

`make test` passed 78/78 CTest cases and `make web-test` passed the 74-entry Modulation picker plus HeadlessChrome 151 local-guitar Play smoke through slot 71 (peak 0.4336884; 47,998 active samples). The offline comparison disables Vibe because it has no matching CLI reference. Line 6's v1.4 notes document control names but no numeric bounds/defaults/topology; no Line 6 DSP or sound parity is claimed.

## Dual-Engine Distortion and 73-entry picker (2026-11-22)

The project-authored Dual-Engine Distortion uses two independent soft-clipping/tone paths with parallel Blend or Series routing, optional low-shelf Bass Boost, and output Level. A/B Gain/Tone and Blend are normalized project values; Level is −24…+12 dB. Controls ramp over 10 ms. It uses fixed per-channel scalar state, adds no delay, and allocates nothing in `process()`. No isolated CPU/aliasing measurement or human listening result has been run.

`make test` passed 77/77 CTest cases. `make web-test` passed the 73-entry Drive & fuzz picker and HeadlessChrome 151 local-guitar Play smoke through slot 70 (peak 0.8912509; 47,999 active samples). The offline browser/native comparison disables this effect because the CLI reference lacks the stage. Fender's Tone Master Pro Model Guide v1.1.100 p. 27 documents Pugilist's control concepts but not numeric ranges/defaults or DSP details; all TabletFX bounds/processing are project-authored, and current firmware applicability is unverified.

## De-Esser and 72-entry picker (2026-11-22)

The project-authored split-band De-Esser uses a fixed band-pass detector per channel, attack/release envelope smoothing, and attenuation of the detected band. It has no lookahead or allocated audio buffer; no isolated callback CPU measurement has been run. The frequency, Q, threshold, maximum reduction, attack, release, DSP, and ranges are project-authored because the checked HeadRush product page does not publish its De-Esser settings.

`make test` passed 76/76 CTest cases. `make web-test` passed the 72-entry Vocal FX picker and HeadlessChrome 151 local-guitar Play smoke for slot 69 at 6.5 kHz (captured peak 0.4818548; 47,999 active samples). De-Esser is disabled in the 66-stage browser/native comparison because the offline reference has no equivalent stage. There is no voice fixture, intelligibility test, human listening result, isolated CPU benchmark, or hardware test; the guitar smoke only confirms the browser control-to-WASM output path.

## Quad Tape topology and 79-entry picker (2026-11-23)

The existing Multi-Delay adds project-authored Quad Tape-like playback using four fractional read heads. Motor Speed scales target echo spacing and causes a bounded, interpolated read-head move; project limits are 0.8–1.2× and the maximum slew tracks the requested speed change (capped at 0.2 samples per output sample). These choices do not reproduce Fractal tape processing. make test passes the motor bounds and synthetic 1 kHz pitch-bend tests at 0.8× and 0.9×. make web-test reports 18,843,104 B processor state plus 10,364 B tuner state in a fixed 20 MiB WASM heap; its millisecond-fallback timing is too coarse for an isolated Quad Tape CPU benchmark. The 512 KiB delay ring remains shared across the four lines.

## Multi-Delay and 75-entry picker (2026-11-22)

The project-authored four-line Multi-Delay uses a shared 512 KiB ring-storage budget. At 48 kHz this permits about 682 ms per requested line; the upper control bound remains 2,000 ms and is clamped to the storage-supported maximum. Constructor allocation is fixed and `process()` does not allocate. No isolated CPU benchmark has been run.

`make web-test` passed 79/79 CTest cases, the 75-entry picker checks, and local-guitar browser Play smoke with the stage at slot 72. Numerical browser/native parity disables this effect because the offline reference has no corresponding processor. These results do not establish useful loudness, musical quality, Fractal behavior, or performance on a live audio route.
## Megatap Delay and 71-entry picker (2026-11-22)

The project-authored Megatap Delay is a mono delay ring with mono/stereo input selection and dry-path channel preservation. Its fixed 1 MiB storage cap can hold the documented four-second tap line plus one-second predelay at 48 kHz; the available delay span falls at higher sample rates. It allocates at construction, not in `process()`. No isolated CPU benchmark or live-route measurement has been run.

`make web-test` passed the current native and browser/WASM checks, including 75/75 CTest cases, picker/category assertions, and HeadlessChrome 151 Play. The browser smoke played local guitar audio through Megatap at slot 68 using a 50 ms, four-tap setup; captured output had a 0.8912509 peak and 47,400 active samples. The 66-active-stage browser/native numerical comparison deliberately leaves Megatap disabled because its offline CLI reference is not implemented; the smoke test verifies audible-path data flow, not Megatap parity. No audio-device listening or quality conclusion follows from this synthetic fixture.

## Auto-Pan and 70-entry picker (2026-11-22)

Auto-Pan uses an equal-power stereo pan law with project-authored Rate 0.5 Hz (0.05–20 Hz), Depth 75% (0–100%), square/sine/triangle Type, optional tempo sync at 30–300 BPM, and a project-selected Tap Division. It adds no fixed latency, uses fixed object state, and does not allocate in `process()`. These values and DSP are project-authored; Fender's Tone Master Pro Model Guide v1.1.100 gives Orbit Stereo Panner control names and waveforms but no numeric bounds, defaults, or topology. No isolated Auto-Pan CPU or memory benchmark, listening check, or hardware test has been run.

`make test` passed 74/74 CTest cases. `make web-test` passed HeadlessChrome 151 browser/WASM Play/native comparison at 48 kHz/128 frames with 66 active processors (maximum absolute error 0.0000147457; RMS error 0.00000286683), categorized control checks, and captured Play output. The browser WASM processor state is 14,859,928 B plus 10,364 B tuner state within a fixed 16 MiB heap. The comparison fixture is mono, so browser numerical parity does not exercise stereo movement or establish audible quality. The overall synthetic full-chain maximum remains above one callback deadline as documented in the benchmark tables below.

## Previous checkpoint: Ten-Tap Delay and 69-entry picker (2026-11-22)

`make test` passed documentation validation, 11 audio-fixture safety tests, and 73/73 CTest cases. `make web-test` passed HeadlessChrome 151 browser/WASM Play/native comparison at 48 kHz/128 frames with 66 active processors (maximum absolute error 0.0000147457; RMS error 0.00000286683). Ten-Tap is included in the categorized Delay chain. The browser processor state is 14,859,844 B plus 10,364 B tuner state inside the fixed 16 MiB heap; browser Play output was captured. No isolated Ten-Tap CPU measurement or human listening/hardware check was run. The synthetic chain output remains quiet, so numerical parity is not evidence of useful loudness or musical quality.

## Previous checkpoint: Shuffling Looper and 68-entry picker (2026-11-22)

The categorized browser picker now has 68 rows; Shuffling Looper is effect slot 65, followed by Output level at 66 and Peak limiter at 67. Its fixed mono/stereo loop storage is included in a 14,475,504 B browser/WASM processor-chain state estimate. The browser/native Play comparison covers the 64 default-active processors and reports maximum absolute error 0.0000146203 and RMS error 0.00000285181; the looper is disabled in that parity chain and is exercised separately by its transport smoke test. No isolated CPU benchmark or live-route measurement has been run for Shuffling Looper. The test page uses a fixed 16 MiB WebAssembly heap and reports 10,364 B for tuner state separately.

## Previous DSP verification (Auto-Swell Delay and 67-entry picker, 2026-11-22)

The project-authored Auto-Swell Delay has no added fixed dry-path latency, allocates its fixed delay storage at construction, and does not allocate in `process()`. At 48 kHz stereo, the 2 s ring uses 770,080 B. The isolated synthetic processor-call benchmark measured 0.2145% p50 / 0.2160% p95 / 0.2175% p99 / 0.4763% p99.9 / 2.7717% maximum of a 2.667 ms callback. This is not a live-device route measurement.

The synthetic 64-stage full chain reported 62.8599% p50 / 81.0416% p95 / 100.8492% p99 / 126.6740% p99.9 / 154.3636% maximum with 12,054,680 B owned state; its maximum exceeds one callback deadline. This stress measurement does not establish real-time safety on hardware.

`make test` passed 71/71 CTest cases plus offline CLI checks. `make web-test` passed the categorized 67-entry picker and HeadlessChrome 151 browser/WASM Play/native comparison over 64 active processors (maximum absolute error 0.0000146203; RMS error 0.00000285181); processor memory was 13,706,624 B plus 10,364 B tuner state within 16 MiB. `make audio-regression` passed five pickup positions plus Guitar-TECHS DI stereo/mono using local prerecorded fixtures; bridge, stereo-DI, and mono-DI output RMS were 6.90773e-6, 6.05366e-6, and 1.30725e-5. Output is very quiet. Browser timing has millisecond-fallback resolution, and no human listening or live hardware result is established.

## Previous DSP verification (Harmonic Tremolo and 66-entry picker, 2026-11-22)

Harmonic Tremolo is a project-authored complementary low/high-band tremolo with opposing LFO phases. Its 48 kHz stereo/128-frame isolated synthetic benchmark measured 0.0776% p50 / 0.0795% p95 / 0.0825% p99 / 0.2944% p99.9 / 0.8258% maximum of a 2.667 ms callback and 136 B processor-owned memory. The effect has no fixed algorithmic latency; construction owns fixed state and process() allocates nothing.

The synthetic full-chain benchmark reported 63.3250% p50 / 83.6152% p95 / 100.9092% p99 / 110.6711% p99.9 / 135.7905% maximum and 11,284,600 B owned memory. Its maximum exceeds one callback deadline; this processor-call stress result is not evidence of real-time safety on a device.

make test passed 70/70 CTest cases plus offline CLI checks. make web-test passed browser Play/native parity in HeadlessChrome 151 with maximum absolute error 0.0000177590 and RMS error 0.00000292105; the 63-active-processor browser chain used 13,320,996 B plus 10,364 B tuner state within the fixed 16 MiB WASM heap. make audio-regression passed the 63-processor chain across five pickup positions and stereo/mono DI. Output remains very quiet, and browser callback timing has millisecond fallback resolution. No listening, live-device, or Fender sound/control parity is established.

## Previous DSP verification (Resonant Delay and 65-entry picker, 2026-11-22)

The project-authored Resonant Delay filters delayed audio through a per-channel band-pass biquad before output and feedback; dry input stays immediate. Controls are Time 350 ms (1–2,000 ms), Feedback 0.35 (0–0.95), Center 800 Hz (50…min(8,000 Hz, 0.45×sample rate)), Resonance Q 2 (0.5–12), and Mix 0.25 (0–1). Bounds/defaults and topology are project-authored, not HeadRush Prime RESO DELAY controls. A 2 s stereo ring uses 768,008 B at 48 kHz; construction allocates and process() does not. No isolated CPU benchmark was run. make web-test passed 69 CTest cases and the 65-entry picker/browser capture with a 62-active-processor native/WASM maximum absolute error of 0.0000121512 and RMS error 0.00000203987. make audio-regression passed five pickup positions plus stereo/mono DI through all 62 processors. Output is very quiet, so parity and finite audio do not establish useful musical level or sound quality. Manual listening, hardware-route performance, and device latency remain open.


## Latest DSP verification (Stereo Enhancer and 64-entry picker, 2026-11-22)

The project-authored Stereo Enhancer is integrated in the Stereo & image category. At 48 kHz stereo it allocates its delay ring at construction and process() performs no allocation. Modern and Stereoizer add no fixed latency; Classic intentionally delays the right channel by 0–20 ms while the direct path remains immediate. Mono buffers remain mono. The isolated Linux CPU cost and a human-listening/device result have not been measured. make web-test passed 68 CTest cases, offline CLI checks, and HeadlessChrome 151 Play/native comparison with Enhancer bypassed; the enabled offline CLI test confirms a changed stereo result. Browser processor memory is 12,936,760 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. Numerical parity does not establish sound quality, mono-to-stereo behavior, or Fractal parity.

## Latest DSP verification (Phrase Looper and 63-entry browser picker, 2026-11-22)

On a Ryzen 9 5950X Release build, the project-authored Shimmer Reverb measured 11.7097% p50 / 18.5503% p95 / 20.0691% p99 / 23.5008% p99.9 / 27.3648% maximum of a 2.667 ms callback at 48 kHz stereo/128 frames, with 647,792 B processor-owned memory. Its direct dry path is immediate; the pitch-return ring holds 128 frames (2.667 ms at 48 kHz) before pitch-shifter processing, which adds separate wet latency. This synthetic processor-call benchmark is not a live-route or listening measurement.

At 48 kHz stereo/128 frames, the project-authored five-band peaking/shelf/cut EQ measured 0.0998% p50 / 0.1005% p95 / 0.2104% p99 / 0.4301% p99.9 / 0.6533% maximum of a 2.667 ms callback and owned 680 B. It has no algorithmic latency; coefficients are prepared when parameters are configured, outside `process()`.

The earlier 0.5-second Convolution Reverb checkpoint used 128-frame partitions and measured 5.1043% p50 / 6.4862% p95 / 18.1629% maximum of a 2.667 ms callback, with 1,549,968 B processor-owned state. That measurement is superseded by the 10-second implementation below.

At 48 kHz stereo/128-frame callbacks, the 10-second maximum-length Convolution Reverb IR owns 15,461,240 B and uses a 1,024-frame partition (21.33 ms fixed algorithmic latency; not measured device round-trip latency). The Ryzen 9 5950X isolated maximum-IR benchmark measured 0.0382% p50 / 79.4286% p95 / 92.2945% p99 / 108.1668% p99.9 / 130.4420% maximum of the 2.667 ms callback. Its periodic bursts exceed the callback deadline, so glitch-free real-time playback is not established. The browser regression loads and replaces a 10-second stereo PCM WAV in the fixed 64 MiB WASM heap, checks independent left/right output and partition latency, and verifies that the Reverb picker category is preserved.

Phrase Looper storage is fixed at 192,000 frames: its two loop/history buffers use 1,536,000 B in mono or 3,072,000 B in stereo, plus object state. It adds no fixed algorithmic latency, and `process()` allocates nothing. Record/overdub/clear snapshot copies are issued through the AudioWorklet message handler between `process()` calls but still run on that worklet thread; no isolated CPU benchmark or glitch test has measured the scheduling impact.

The synthetic 61-stage native/offline stress chain measured 62.8345% p50 / 80.8368% p95 / 98.5455% p99 / 112.0174% p99.9 / 162.0700% maximum and owned 11,284,464 B; its maximum crossed one callback deadline. The 62-stage HeadlessChrome 151 browser/native Play capture, including Phrase Looper and with Note Generator disabled for phase-independent parity, passed with maximum absolute error 0.0000172342 and RMS error 0.00000269834. The browser loaded a local IR into WASM, decoded local input audio, captured processed Play output, and verified a nonempty loop recording; processor memory was 12,936,616 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. The same browser test separately enabled only Note Generator and measured A4 at 440 Hz with RMS 0.452548; no isolated CPU benchmark has measured it. Browser callback timing fell back to millisecond resolution, so its per-processor timing samples are too coarse for dependable CPU estimates. `make audio-regression` remains a separate 61-stage offline run and passed five EGFxSet pickup positions and stereo/mono Guitar-TECHS DI with a deterministic local 4,096-frame IR; all reports showed 61 processors. Bridge, stereo-DI, and mono-DI RMS were 9.43956e-6, 8.07260e-6, and 1.69201e-5. The browser/native and prerecorded output remain very low, so parity and bounded processing do not establish sound quality or useful musical output. Manual listening, real-guitar tracking, hardware-route performance, and round-trip latency remain open.
### Previous Feedback Generator and 58-stage browser chain (2026-11-22)

Feedback Generator is project-authored and reuses the Mono Guitar Synth single-note tracker to produce a sine sustain. Its 48 kHz stereo/128-frame default Unison measurement owns 1,522,968 B and uses 11.5597% p50 / 15.0252% p95 / 19.0037% p99 / 24.8748% p99.9 / 46.3632% maximum of a 2.667 ms callback. Octave down/up additionally invoke the pitch shifter and have not been separately benchmarked. This is an isolated synthetic processor-call measurement, not device-route performance or proof of musical tracking.

The synthetic full stress chain measured 45.1569% p50 / 56.0070% p95 / 62.2363% p99 / 73.6962% p99.9 / 101.9552% maximum and owned 9,086,704 B; the maximum crossed one callback deadline. The 58-stage HeadlessChrome 151 browser/native Play capture passed with maximum absolute error 0.00002568498 and RMS error 0.00000416202. Local-file decode and processed Play capture passed; browser processor memory was 10,233,752 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. The all-stages output RMS remained extremely low, so the parity check does not establish useful musical level or sound quality. make audio-regression passed all five EGFxSet pickup-position renders and Guitar-TECHS stereo/mono DI at processors=58; bridge, stereo DI, and mono DI output RMS were about 1.26e-5, 1.00e-5, and 2.13e-5. This validates bounded processing, not useful musical level. Listening, real-guitar tracking, hardware-route performance, and round-trip latency remain open.

### Three-voice tracked synth benchmark (2026-11-23)

The project-authored three-voice tracked synth shares one monophonic detector across its three generated oscillators; it is not chord tracking. At 48 kHz stereo/128-frame blocks (2.667 ms deadline), a Ryzen 9 5950X/GCC 13.3.0 Release run with 2,000 warm-up and 5,000 measured calls used all three saw oscillators in Pitch + envelope mode and measured 11.6396% p50 / 14.9558% p95 / 16.1270% p99 / 20.7579% p99.9 / 26.4172% maximum, with 1,025,112 B owned state. The benchmark uses the suite's deterministic pseudorandom input buffer; it estimates processor-call cost, not real-guitar note accuracy or live-route performance. The tracker analyzes 1,024 samples (about 21.3 ms at 48 kHz) and has a fixed 80 ms release. Browser Play was separately exercised using a generated local guitar-like fixture; the main native/browser parity run leaves this project-specific stage disabled. User-recorded audio and human listening remain separate checks.

### Previous Mono Guitar Synth and 56-stage chain (2026-11-22)

The project-authored monophonic guitar oscillator was integrated in the Synth category alongside the existing tracked effects in Pitch. Its 1,024-sample pitch-analysis frame is about 21.3 ms at 48 kHz; this is not an end-to-end response or note-accuracy measurement. The effect offers sine/triangle/saw/fixed-square waveforms, Mix, Level, Gate Threshold, Attack, Release, and Portamento, but no variable pulse width, chord tracking, sampled tones, or flagship sound parity. On a Ryzen 9 5950X / GCC 13.3.0 Release build at 48 kHz stereo/128-frame blocks (2,000 warm-up and 5,000 measured calls), isolated Synth CPU was 11.8248% p50 / 15.4065% p95 / 18.9369% p99 / 23.8271% p99.9 / 26.4458% maximum of the 2.667 ms callback deadline; owned memory was 1,024,760 B. This synthetic processor benchmark does not establish live tracking quality or device suitability.

The full stress chain plus signal controls/meters measured 34.6378% p50 / 43.8957% p95 / 49.9198% p99 / 70.7078% p99.9 / 99.4793% maximum and owned 7,563,736 B. The standalone processor and stress chain use synthetic pseudorandom benchmark buffers; these are processor-call measurements, not a device route or guitar-quality measurement. The 56-stage browser/native Play capture passed on HeadlessChrome 151 with maximum absolute error 0.0000250265 and RMS error 0.00000515164; local file decoding and processed Play output capture both passed. `make audio-regression` also passed all five pickup positions and stereo DI at `processors=56`, with finite output under the limiter. However, the full stress chain output RMS was only 1.46e-5 on the bridge pickup and 1.16e-5 on stereo DI; this does not validate musical level or the synth's real-guitar tracking. Device performance, real-guitar tracking quality, listening, and actual round-trip latency remain open.

### Previous Grain Delay / 54-stage build (2026-11-22)

The project-authored Grain Delay owns 895,156 B in its 48 kHz stereo configuration. It has no fixed dry-path latency; wet grains are delayed and windowed. The previous Ryzen 9 5950X / GCC 13.3.0 Release benchmark at 48 kHz stereo/128-frame blocks recorded isolated Grain Delay p50/p95/p99/p99.9/max of 0.3079/0.3600/0.4976/0.8160/1.9407% of the 2.667 ms callback deadline. The earlier 54-stage browser/audio measurements remain historical. Processor-call measurements do not establish live-device load, musical output level, or listening quality.

### Previous 52-stage aggregate benchmark (2026-11-22)

The project-authored four-band feedback-delay Resonator adds no fixed dry-path latency and owns 31,136 B at 48 kHz stereo. The isolated Linux benchmark measured 0.3432% p50 / 0.5318% p95 / 0.6319% p99 / 1.9103% p99.9 / 2.0985% maximum of the 2.667 ms callback deadline. The full 52-effect chain plus signal controls/meters measured 18.1929% p50 / 23.6661% p95 / 27.9240% p99 / 33.2338% p99.9 / 38.1689% maximum and owns 6,538,848 B. This remains the 52-stage run; do not read those aggregate figures as the current 53-stage chain cost.

## Earlier DSP verification (Triple-Double Chorus and 51-stage browser/audio chain, 2026-11-22)

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
| Ring Modulator (Classic) | 0.0278 | 0.0281 | 0.0446 | 0.0465 | 0.2707 | 840 |
| Ring Modulator (SSB Upper) | 0.6686 | 0.9432 | 1.1449 | 1.2690 | 1.4798 | 840 |
| Ring Modulator (SSB Lower) | 0.6552 | 0.6559 | 1.0103 | 1.1887 | 1.3909 | 840 |
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
| Five-band parametric EQ | 0.0998 | 0.1005 | 0.2104 | 0.4301 | 0.6533 | 680 |
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

The effect sequence is Input Gain → Volume Pedal → High-pass → Low-pass → Noise Gate → Pedal Compressor → Studio Compressor → Wah → Envelope Wah → Clean Boost → Transparent Overdrive → Mid-focused Overdrive → High-gain Distortion → Round Fuzz → Soft Drive → Tremolo → Chorus → Flanger → Phaser → Vibrato → Parametric EQ → Delay → Poly Pitch Shifter → Optical Compressor → FET Compressor → Guitar Graphic EQ → Bass Graphic EQ → Shelf/Tilt EQ → Room Reverb → Hall Reverb → Plate Reverb → Spring Reverb → Auto Swell → Rotary Speaker → Ring Modulator → Rhythmic Slicer → Step Filter → Static Notch Filter → Reverse Delay → Ducking Delay → Modulated Delay → Filtered Delay → Swept Filter Delay → Formant Filter → Harmonic Resonator → Multiband Compressor → Bit Crusher → Dimension Chorus → Triple-Double Chorus → Four-band Resonator → Output Gain → Peak Limiter. Input mute and the input meter run before that sequence; the output meter and global dry/wet crossfade run after it. The tuner independently analyzes the original input and its optional mute stage runs after the output limiter; it is not included in the full-chain row. Effect timing includes processor calls, both meter passes and their status reads, and signal-control/copy work. Utility rows include their validation and sample work. The tuner monitor uses 10,400 B; its 1,024-sample window is 85.3 ms at 48 kHz, so its periodic analysis batches make tail percentiles higher than its median. This is pitch-display response, not signal-path latency. Memory estimates include processor objects and directly owned buffers, but exclude caller buffers, allocator metadata, input/output scratch buffers, UI, and backend/runtime overhead.

Historical context: the 38-stage snapshot and the 39-, 40-, and 41-stage checkpoints below are older runs. The 44-stage benchmark row is the prior pre-DLY-20 full-chain measurement; the 45-stage row predates Formant Filter; the 46-stage run predates Harmonic Resonator; the 47-stage run predates Multiband Compressor. The older 49-stage results are an earlier checkpoint; current 52-stage results are reported at the top.

The pitch shifter uses a 2,048-sample analysis window, 128-frame processing block, and 64-frame hop. It reports 256 samples (5.333 ms at 48 kHz) of buffered latency; an impulse regression bounds the observed output onset and peak around this delay. A fresh `make bench` run gives the 38-effect stereo chain 18.4332% p50 / 23.6950% p95 and 1,795,936 B; its 62.2003% maximum reflects host scheduling outliers. Rhythmic Slicer adds no fixed latency and uses 0.0382% / 0.0529% p50/p95 with 136 B state. Ring Modulator uses 0.0266% / 0.0270% with 72 B; Rotary Speaker uses 0.3942% / 0.5175% with 1,208 B; Auto Swell uses 0.0214% / 0.0217% with 1,104 B. Room, Hall, Plate, and Spring Reverb also add no fixed dry-path latency. The local prerecorded regression passed across five clean pickup-position samples and a 72.5-second stereo DI excerpt through all 38 effects, with Tape Echo and multi-head routing in separate passes. HeadlessChrome 151 passed the Rhythmic Slicer + Auto Swell + Rotary Speaker + Ring Modulator + Room + Hall + Plate Reverb + Tape + four-head Play/native comparison at 0.000515713 maximum absolute error and 0.0000248161 RMS error. Windows runtime, Android device performance, actual interface latency, and human listening remain unverified. Maximum benchmark values include host scheduling noise; measure device routes separately.
# Manual Through-Zero Flanger checkpoint

The project-authored manual through-zero flanger delays both dry and wet paths. At 48 kHz, the declared maximum path latency is 384 frames with the browser's default 8 ms maximum-time setting, or 960 frames when configured to the supported 20 ms maximum. The chain does not yet compensate this time-varying latency at the host/graph level.

The browser/WASM implementation reserves delay-vector capacity for the fixed 20 ms limit. Source layout gives 15,392 bytes for the two stereo delay vectors and an expected wasm32 `memory_bytes()` value of 15,544 bytes. These are source-derived estimates, not a runtime allocation measurement. The browser fixed heap was raised to 24 MiB with growth disabled after the 20 MiB build failed during the browser test; the 24 MiB browser test passes.

Native, CLI, prerecorded-input stability, and browser smoke checks pass. The browser Play smoke produced a finite signal (peak 0.1059155; 47,813 active samples) on a locally generated guitar fixture. Five EGFxSet mono pickup-position renders completed with the effect enabled, but their output levels were very quiet (peak range 0.00003294–0.00006639). These results establish processing and numeric stability only. No isolated CPU measurement, hardware latency measurement, human listening result, or Fractal device/DSP parity is established.
