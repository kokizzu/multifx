# Implementation checklist

Generated once from `GOAL.md`. The requirement text is preserved from the source prompt.

Mark an implementation and automated-test box only after the code and relevant automated checks pass. Mark device/listening only with direct evidence from the named hardware and route. The requirement box is complete only when all required evidence exists.

| Done | ID | Priority | Requirement | Implementation | Automated | Device/listening | Evidence or remaining limit |
|---|---|---|---|---|---|---|---|
| [ ] | `UTL-01` | P0 | Input trim and output gain, with units and smooth changes. | [x] | [x] | [ ] | Shared Gain processor supports separate pre-chain trim and post-chain output level with dB units and a 10 ms ramp. Automated ramp/retarget/block/reset/no-allocation tests and browser/native output comparison pass. Linux CPU/memory recorded; native product UI, live hardware and listening remain open. |
| [ ] | `UTL-02` | P0 | Volume pedal, selectable position, expression curve and endpoints. | [x] | [x] | [ ] | Project-authored volume pedal provides 0–1 position, independently selectable heel/toe dB endpoints, linear-amplitude or audio/dB taper, and a 10 ms gain ramp. Full toe is unity by default; heel defaults to −96 dB. Bounds, both curves, endpoints, retargeting, variable blocks, channels, reset, finite output, and no-allocation tests pass. Native controller handoff and hardware/listening remain open. |
| [ ] | `UTL-03` | P0 | Independent block bypass, global bypass, input mute, and panic mute. | [x] | [x] | [ ] | Browser chain has independent per-block toggles. Shared `SignalControls` adds a 10 ms global dry-bypass crossfade and 10 ms input mute/unmute ramps; panic mute silences immediately and fades back over 10 ms. CTest verifies the ramps, dry/wet retention, bounds, stereo behavior, panic silence/release, and no allocations. The native signal-control object is 80 B; latest Linux p50/p95 is 0.0015/0.0023% of a 2.667 ms block deadline. Browser toggles are wired to WASM; manual device/listening checks remain open. |
| [ ] | `UTL-04` | P0 | Mono/stereo conversion with explicit summing policy. | [x] | [x] | [ ] | Shared stateless converter supports 1↔2 channel interleaved float audio, duplicate mono upmix, and average (default) or equal-power downmix. Same-layout copies, exact-start in-place expansion/compaction, invalid layout/frame/output checks, partial-overlap rejection, and no-allocation behavior pass CTest. Offline renderer exposes `--output-channels preserve|mono|stereo` and `--mono-sum average|equal-power`; automated CLI tests cover both laws, and prerecorded stereo DI renders through the full chain after average downmix. WASM startup self-check and Windows cross-build pass. Linux p50/p95 is 0.0015/0.0015% for average downmix, 0.0011/0.0015% for equal-power downmix, and 0.0030/0.0034% for mono upmix; no persistent state. Manual device/listening checks remain open. |
| [ ] | `UTL-05` | P0 | Splitter, parallel mixer, branch gains, pan/balance, and phase inversion. | [x] | [x] | [ ] | `split_to_parallel` copies mono/stereo interleaved frames into caller-owned branches. `ParallelMixer` combines dry + two branches with independent gains, stereo balance, phase inversion, dry-only default, and smoothed control updates (10 ms default). Stateless audio operation, no allocation, no algorithmic latency; mixer state 112 B. CTest covers layouts, gains, balance, polarity, variable-block ramps, aliases, invalid buffers, and allocations; WASM startup self-check and Windows cross-build pass. Linux p50/p95 is 0.0019/0.0023% for split and 0.0064/0.0071% for mix. This utility is not yet wired into the preset graph/live scheduler; device/listening checks remain open. |
| [ ] | `UTL-06` | P1 | A/B route switching and crossfade. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-07` | P1 | Frequency-crossover and level/envelope-dependent split. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-08` | P0 | Input/output peak/RMS meters, clip indication, and per-block diagnostics. | [x] | [x] | [ ] | Shared `LevelMeter` reports per-block peak, RMS, frames, channels, invalid-sample count, clip state/latch, and clipped-block count with lock-free 32-bit atomics and no allocation. Input is measured before trim/mute; output after gain and optional limiter. Latest Linux pair cost is 0.0146% p50 / 0.0146% p95 of a 2.667 ms deadline and 72 B total. Browser shows dBFS meters, latched clipping, and latest-block diagnostics; `make web-test` compares a metered non-silent block against decoded input and captured processed output. Analog interface clipping and live-device behavior remain unverified. |
| [ ] | `UTL-09` | P0 | Chromatic tuner with a 50–1500 Hz candidate pitch range, adjustable A4 reference (400–480 Hz), audible passthrough, and optional click-free 10 ms output mute. | [x] | [x] | [ ] | Fixed-storage YIN-style analysis runs on decimated input with an 85.3 ms window at 48 kHz and no algorithmic audio delay. Synthetic native tests pass at 55 Hz, 1500 Hz, and low E2; behavior at the 50 Hz lower boundary and on chords/noisy guitar input is unverified. Latest Linux Release benchmark: 0.0083% p50 / 0.4358% p95 / 0.9405% max of a 2.667 ms block deadline; 10,400 B state. Browser/WASM chain parity plus an A4=440 Hz / reference A4=432 Hz check passes; it reports A4 at +31.8 cents and the UI updates within 122.8 ms in HeadlessChrome 151. Native tests cover reference bounds, silence, stereo, mute ramps, and no allocation. Manual guitar/noise tracking and hardware/device checks remain open. |
| [ ] | `UTL-10` | P1 | Global output EQ and selectable destination profiles. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-11` | P1 | Delay alignment/sample delay for branches and external routes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-12` | P2 | External send/return only when the interface exposes sufficient I/O; measure its extra latency. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-13` | P1 | Mid/side and stereo-width utility with mono-compatibility checks. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UTL-14` | P0 | Configurable output protection and controlled mute/unmute ramps. | [x] | [x] | [ ] | Configurable −1 dBFS sample-peak protection is implemented under `DYN-11`. `SignalControls` adds a 10 ms input mute/unmute ramp and immediate panic mute with a 10 ms release ramp; CTest covers both. Manual device/listening checks remain open. |
| [ ] | `DYN-01` | P0 | Noise gate with threshold, hysteresis, attack, hold, release. | [x] | [x] | [ ] | Linked peak gate now has separate open/close thresholds, configurable dB hysteresis, attack/release smoothing, and a sample-count hold period. Focused tests cover threshold crossings, the hysteresis band, hold timing, reset, stereo linking, non-finite input, parameter bounds, and no allocations; CLI and browser controls are wired. Latest Linux CPU is p50 0.0232% / p95 0.0233% of a 2.667 ms block deadline, owned state 64 B. Live hardware/listening remains open. |
| [ ] | `DYN-02` | P1 | Fast hard gate for high-gain rhythms. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-03` | P1 | Downward expander for gentler noise control. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-04` | P2 | Adaptive noise reduction with declared latency and guitar-tail preservation tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-05` | P0 | Guitar pedal compressor with sensitivity, sustain, attack, level. | [x] | [x] | [ ] | In-house four-control pedal compressor implemented with bounded defaults, stereo-linked detection, and no lookahead. Transfer/control, bounds, reset, non-finite, irregular-block, and no-allocation tests pass. Linux per-effect CPU/memory recorded; live hardware/listening and native UI integration remain open. |
| [ ] | `DYN-06` | P1 | Studio compressor with threshold, ratio, knee, attack, release, makeup. | [x] | [x] | [ ] | In-house compressor implemented; CTest transfer/stereo/bounds/reset/finite/variable-block/no-allocation checks pass. Linux CPU/memory results recorded; hardware/listening evidence remains open. |
| [ ] | `DYN-07` | P1 | Optical-style compression. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-08` | P1 | FET-style compression. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-09` | P1 | Parallel compression with time-aligned dry blend. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-10` | P2 | Multiband compression with crossover/recombination tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-11` | P0 | Peak limiter; document any lookahead latency. | [x] | [x] | [ ] | Project-authored linked-peak safety limiter defaults to −1 dBFS, uses immediate attack and an 80 ms release, and has zero lookahead/algorithmic latency. Same-sample impulse, step, stereo-link, release, bounds, reset, variable-block, non-finite/extreme-input, and no-allocation tests pass. Linux CPU/memory measured; platform runtime and listening remain open. |
| [ ] | `DYN-12` | P1 | Transient shaper for attack and sustain. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-13` | P1 | Automatic swell/slow attack with retrigger control. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-14` | P1 | Sustain enhancement, distinguished from a frozen audio buffer. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-15` | P1 | Sidechain/envelope ducking. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DYN-16` | P2 | De-esser for a separately enabled vocal path. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-01` | P0 | Low-cut/high-pass and high-cut/low-pass filters. | [x] | [x] | [ ] | In-house second-order Butterworth high-pass (80 Hz default) and low-pass (12 kHz default) implemented with bounded cutoff and fixed stereo state. Response, bounds, channel isolation, variable blocks, reset, non-finite recovery, and no-allocation tests pass. Linux per-effect CPU/memory recorded; coefficient changes reset state, so live cutoff smoothing/listening and native UI remain open. |
| [ ] | `EQF-02` | P0 | Three/five-band parametric EQ with frequency, gain, Q. | [x] | [x] | [ ] | In-house three-band peaking EQ implemented with frequency/gain/Q per band; CTest response/channel/bounds/reset/finite/variable-block/no-allocation checks pass. Linux CPU/memory results recorded; hardware/listening evidence remains open. |
| [ ] | `EQF-03` | P1 | Guitar graphic EQ with selectable band layouts. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-04` | P1 | Bass graphic EQ with appropriate frequency coverage. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-05` | P1 | Shelving and tilt EQ. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-06` | P1 | Notch filter for narrow problem frequencies. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-07` | P2 | Dynamic EQ. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-08` | P0 | Pedal wah with range, resonance, position, and level. | [x] | [x] | [ ] | Generic expression wah uses a logarithmic 350–2200 Hz sweep, Q 4, level 0 dB, and 10 ms control ramps. Frequency response, bounds, monotonic sweep, block invariance, stereo isolation, reset, invalid-input recovery, and no-allocation tests pass. Linux CPU/memory measured; hardware/listening remains open. |
| [ ] | `EQF-09` | P1 | Multiple audited wah voicings, including bass wah. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-10` | P0 | Envelope/touch wah with sensitivity and up/down sweep. | [x] | [x] | [ ] | In-house touch-sensitive resonant band-pass with 0.5 sensitivity, −18…−60 dBFS threshold mapping, 350–2200 Hz range, Q 4, 5/80 ms envelope attack/release, and 5 ms frequency smoothing. Sensitivity and both directions pass CTest/CLI checks for sweep, bounds, stereo linking, irregular blocks, finite recovery, and zero allocations. Latest Linux p50/p95 is 0.1069/0.1204% of a 2.667 ms deadline; state is 616 B. Browser/WASM comparison, prerecorded regression, Windows cross-build, and 22-stage chain checks pass. Manual listening remains open. |
| [ ] | `EQF-11` | P1 | LFO auto-wah. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-12` | P1 | Resonant low/band/high-pass filter with envelope/LFO control. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-13` | P1 | Vowel/formant filter and humanizer-style movement. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-14` | P1 | Step/sequenced filter. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-15` | P2 | Sample-and-hold random filter. | [ ] | [ ] | [ ] | Not started |
| [ ] | `EQF-16` | P1 | Wah plus distortion compound preset with editable underlying blocks. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-01` | P0 | Clean boost. | [x] | [x] | [ ] | Transparent 0…+24 dB gain stage with unity default and a 10 ms ramp; placed before Soft Drive. Reuses the tested Gain processor. Transfer/polarity, bounds, reset, ramp, variable blocks, no-allocation, and offline CLI tests pass. Latest Linux p50/p95 is 0.0049/0.0053% of a 2.667 ms callback deadline; owned state 48 B. Hardware/listening remains open. |
| [ ] | `DRV-02` | P1 | Treble booster. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-03` | P0 | Low-gain transparent-style overdrive. | [x] | [x] | [ ] | Generic symmetric rational soft clip; defaults 0.15 drive / −3 dB output / 35% wet, with 10 ms parameter smoothing and bounded controls (drive 0…0.5, level −24…+12 dB, mix 0…100%). Shared C++ processor, offline CLI, and browser/WASM chain are integrated; processing allocates nothing and adds no algorithmic latency. A 48 kHz stereo instance owns 64 B. Native tests cover curve symmetry/saturation, dry mix, bounds, stereo-linked ramps, block invariance, reset, invalid audio, and no allocation. CLI, browser/native parity, prerecorded pickup/DI rendering, Windows MinGW cross-build, and Linux benchmark pass. Latest Linux p50/p95 is 0.0217/0.0379% of a 2.667 ms callback deadline. Manual listening and physical-device checks remain open. |
| [ ] | `DRV-04` | P0 | Mid-focused soft-clipping overdrive. | [x] | [x] | [ ] | Generic pre-clipping mid band from the difference of fixed 300 Hz and 1.8 kHz one-pole low-pass branches, followed by symmetric rational soft clipping. Defaults: 0.18 drive / +6 dB mid boost / −4 dB level / 40% mix; bounded controls and 10 ms parameter smoothing. Native effect/CLI and browser/WASM chain implemented; no algorithmic latency or process-time allocations, 104 B fixed processor state. Native tests, CLI frequency-tilt test, browser/native Play-capture parity, 23-stage prerecorded pickup/DI regression, Windows MinGW cross-build, and Linux CPU/memory benchmark pass. Measured 0.0375% p50 / 0.0379% p95 of a 2.667 ms deadline; aliasing quality and human/device listening remain open. |
| [ ] | `DRV-05` | P1 | Asymmetric-clipping overdrive. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-06` | P1 | Blues-style dynamic overdrive. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-07` | P1 | Hard-clipping/op-amp distortion. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-08` | P0 | High-gain distortion with useful pre/post filtering. | [x] | [x] | [ ] | Project-authored generic rational high-gain clipper with pre high-pass/low-pass and post tone low-pass. Defaults: drive 0.75, 100 Hz pre low cut, 9 kHz pre high cut, 5.2 kHz post tone, −9 dB level, 100% wet, 10 ms parameter smoothing. Shared native/WASM processor is available in offline CLI, browser controls, Linux prerecorded regression, and the Windows x64 cross-build; 136 B state, zero fixed latency and no process-time allocations. Tests cover filters, clipping, stereo independence, smoothing/block invariance, exact dry bypass, bounds, reset, invalid audio, and allocation. Linux p50/p95/p99 is 0.0382/0.0386/0.0623% of a 2.667 ms deadline. No oversampling/aliasing-quality claim; listening and physical-device checks remain open. |
| [ ] | `DRV-09` | P1 | Tube/preamp-style drive. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-10` | P1 | Bass drive with retained low-frequency/dry content. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-11` | P0 | Round/transistor-style fuzz. | [x] | [x] | [ ] | Generic rounded symmetric rational fuzz is integrated in the shared C++ chain, offline CLI, browser/WASM Play chain, Linux prerecorded regressions, and Windows MinGW cross-build. Defaults are 0.55 fuzz, −6 dB level, 85% wet, and 10 ms smoothing; fixed state 64 B, zero algorithmic latency, and no process-time allocation. Native tests cover curve symmetry/monotonicity, exact dry mix, bounds, extreme/invalid samples, stereo, smoothing/block invariance, reset, and allocation. Linux p50/p95/p99 is 0.0308/0.0311/0.0312% of a 2.667 ms deadline. Human/device listening remains open; this generic style does not claim a specific transistor circuit emulation. |
| [ ] | `DRV-12` | P1 | Germanium-style and silicon-style fuzz variants where source supports them. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-13` | P1 | Sustaining multi-stage fuzz. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-14` | P1 | Gated/bias-starved fuzz. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-15` | P1 | Octave-up fuzz. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-16` | P2 | Sub-octave fuzz compound sound. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-17` | P1 | Tape/tube-style saturation with gain compensation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-18` | P1 | Waveshaper/wavefolder. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-19` | P1 | Bit-depth reduction. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-20` | P1 | Sample-rate reduction with controllable filtering. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-21` | P2 | Lo-fi degradation/noise/wow-flutter combinations with independently adjustable components. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DRV-22` | P1 | Oversampling quality modes for nonlinear processors that need them; measure aliasing and added cost. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-01` | P0 | Mono/stereo chorus. | [x] | [x] | [ ] | Project-authored 0.8 Hz sine-LFO chorus; defaults 15 ms base delay, 7 ms depth, 30% wet. The wet tap is bounded to 1–30 ms; delay/depth/mix retarget over 10 ms; stereo channels use opposite LFO phase. The 48 kHz stereo instance owns 12,448 B, preallocated during construction; `process()` allocates nothing and the dry path adds no fixed latency. Native tests cover impulse timing, modulation, dry mix, mono/stereo isolation, bounds, smoothing, variable-block invariance, reset and finite recovery. CLI, prerecorded DI, browser/WASM native parity, Linux CPU/memory measurement and Windows cross-build pass. Hardware/listening remain open. |
| [ ] | `MOD-02` | P1 | Multi-voice/tri-chorus. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-03` | P1 | Dimension-style spatial chorus. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-04` | P1 | Bass chorus with low-band preservation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-05` | P0 | Flanger with feedback/polarity control. | [x] | [x] | [ ] | Project-authored short modulated delay; defaults 0.25 Hz, 2 ms base, 1 ms depth, 35% positive feedback, 35% wet. Rate 0.05–8 Hz, base delay 0.5–10 ms, depth bounded to keep the wet delay inside its 12 ms ring, feedback 0–90% with positive/negative polarity, mix 0–100%; delay/depth/feedback/mix ramp over 10 ms. Stereo uses opposite LFO phase and isolated feedback. The 48 kHz stereo instance owns 4,792 B; process() is allocation-free and dry-path latency is zero. Native tests cover delay timing, repeat polarity/level, dry signal, modulation, stereo isolation, bounds, smoothing, reset, block invariance, finite recovery and no allocations. CLI, browser/WASM comparison, prerecorded DI, Linux CPU/memory measurement and Windows cross-build pass. Hardware/listening remain open. |
| [ ] | `MOD-06` | P2 | Through-zero flanging with disclosed delay requirements. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-07` | P0 | Phaser with selectable stage count where implemented. | [x] | [x] | [ ] | Project-authored modulated all-pass network with selectable 4/6/8 stages; defaults 0.5 Hz, 250–1600 Hz sweep, 0.2 feedback, and 50% wet. Rate 0.05–8 Hz, bounded frequency endpoints, feedback ±0.7, mix 0–100%; continuous controls ramp over 10 ms. Stereo channels use opposing sweeps; changing stage count clears filter history. State is fixed at 280 B for stereo 48 kHz; process() allocates nothing and the dry path adds no fixed latency. Tests cover notch cancellation, distinct stage responses, stage-count state reset, modulation, stereo isolation, dry passthrough, parameter bounds, smoothing/block invariance, reset, finite recovery, feedback stability, and no allocations. CLI, browser/WASM parity, prerecorded DI, Linux benchmark, and Windows cross-build pass. Latest Linux p50/p95 is 0.0671/0.0675% of a 2.667 ms callback deadline. Hardware/listening remain open. |
| [ ] | `MOD-08` | P1 | Envelope/step phaser. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-09` | P1 | Vibe-style phase modulation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-10` | P0 | Vibrato. | [x] | [x] | [ ] | Project-authored sine-LFO fractional-delay vibrato; defaults 5 Hz, 0.35 ms peak depth, 100% wet, and 10 ms smoothing. Rate 0.1–10 Hz, depth 0–1.5 ms, mix 0–100%; the preallocated stereo ring uses a fixed 2 ms center delay and a 0.5–3.5 ms wet-tap range at maximum depth. Stereo shares modulation; dry mix is immediate. Tests cover wet center delay/dry mix, stereo isolation, both pitch directions, smoothing/block partition invariance, bounds/layouts, reset, finite output, and zero process allocations. CLI, browser/WASM parity, prerecorded DI regression, Windows cross-build, and Linux benchmark pass; stereo state is 1,696 B, p50/p95 load 0.0427/0.0431% of a 2.667 ms deadline. Only manual listening and device checks remain. |
| [ ] | `MOD-11` | P0 | Amplitude tremolo with waveform and sync. | [x] | [x] | [ ] | Sine/triangle/square tremolo supports 0.05–20 Hz free rate or BPM sync (quarter-note beat units), 0–100% depth, stereo-linked LFO, and a 10 ms depth ramp. Waveforms, sync math, phase continuity, depth ramp, bounds, variable blocks, reset, finite output, and no-allocation tests pass. Linux CPU/memory measured; hardware/listening remains open. |
| [ ] | `MOD-12` | P1 | Harmonic tremolo. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-13` | P1 | Bias/optical-style tremolo variants. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-14` | P1 | Stereo auto-pan. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-15` | P1 | Rotary-speaker simulation with slow/fast acceleration and balance. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-16` | P1 | Rhythmic slicer/chopper. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-17` | P1 | Ring modulation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-18` | P2 | Frequency shifting in Hz, clearly distinct from pitch shifting. | [ ] | [ ] | [ ] | Not started |
| [ ] | `MOD-19` | P1 | Envelope-controlled modulation source reusable by supported blocks. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-01` | P0 | Digital delay, mono/stereo, tempo sync. | [x] | [x] | [ ] | Project-authored feedback delay retains the 350 ms free-time default and adds 20–300 BPM sync with twelve beat divisions. Delay taps crossfade and feedback/mix ramp over 10 ms; the ring is fixed at construction and processing allocates nothing. Native tests cover all divisions, timing, stereo isolation, feedback, ramps, bounds, finite recovery, reset, block partitions, and no allocation. CLI, browser AudioWorklet/WASM Play/native parity, prerecorded DI regression, Windows cross-build, and Linux benchmark pass; 2 s / 48 kHz / stereo state is 768,304 B and normal-mode p50/p95 CPU is 0.0341/0.0345% of a 2.667 ms deadline. Real-device use and listening remain open. |
| [ ] | `DLY-02` | P0 | Slapback delay. | [x] | [x] | [ ] | Selectable Slapback quick preset sets 110 ms, 12% feedback, and 25% wet on the existing Digital Delay processor; controls remain editable and the preset adds zero processor stages, memory, or callback CPU. The offline CLI test checks dry, first-echo, and second-echo levels; the browser self-test checks the preset values and routes the same delay control message. `make test` and `make web-test` pass. Device playback/listening remains open. |
| [ ] | `DLY-03` | P0 | Stereo ping-pong. | [x] | [x] | [ ] | Added selectable stereo cross-feedback routing to the existing Digital Delay. A 10 ms routing crossfade avoids a discontinuity when changing modes; mono input keeps same-channel delay behavior. Alternating left/right repeats, crossfade, mono fallback, reset, and no-allocation behavior pass native tests; a stereo CLI impulse test verifies first and second repeat routing/level. Browser AudioWorklet/WASM control and Play/native parity pass on the mono fixture, which verifies the setter and mono fallback rather than stereo alternation. Windows MinGW cross-build and prerecorded guitar regression pass. At 48 kHz / 128 stereo frames the mode measures 0.0533% p50 / 0.0536% p95 of the 2.667 ms deadline and reuses the 768,304 B 2 s stereo ring; it adds no processor stage or memory. Hardware/listening remains open. |
| [ ] | `DLY-04` | P1 | Independent dual delay. | [x] | [x] | [ ] | The existing Delay stage now offers two independent lines with separate A/B times, feedback, and wet levels. Stereo uses A on left and B on right; mono feeds both preallocated lines and sums their wet taps at half level. Time/parameter/routing changes ramp over 10 ms; the mode adds no stage or callback allocation. Native tests verify distinct 4/6 ms taps, per-line feedback, stereo isolation, mono summing, crossfade, reset, bounds, and no allocation. The offline CLI impulse test, browser AudioWorklet/WASM Play/native comparison, Linux benchmark, MinGW Windows cross-build, and prerecorded pickup/DI regression pass. At 48 kHz / 128 frames CPU is 0.0420% p50 / 0.0424% p95 mono and 0.0529% / 0.0533% stereo; both use 768,304 B for two 2 s lines. Browser mono-chain memory includes this second line even while another routing mode is selected, keeping callback changes allocation-free. Hardware/listening remains open. |
| [ ] | `DLY-05` | P1 | Multi-tap/pattern delay with per-tap levels and pans. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-06` | P0 | Analog/bucket-brigade-style delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-07` | P0 | Tape echo with controllable saturation, wow/flutter. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-08` | P1 | Multi-head tape-style echo. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-09` | P1 | Reverse delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-10` | P1 | Ducking/dynamic delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-11` | P1 | Modulated delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-12` | P1 | Filtered delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-13` | P1 | Pitch-shifted feedback delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-14` | P1 | Swell delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-15` | P2 | Resonant delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-16` | P2 | Granular/glitch delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-17` | P1 | Hold/freeze delay with defined retrigger behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-18` | P1 | Lo-fi delay. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DLY-19` | P1 | Controlled self-oscillation mode with output protection and panic behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-01` | P0 | Small/large room modes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-02` | P0 | Hall. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-03` | P0 | Plate. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-04` | P0 | Algorithmic spring. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-05` | P1 | Additional spring voicings where distinct sources justify them. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-06` | P1 | Chamber/large-space ambience. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-07` | P1 | Early-reflection/ambience processor. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-08` | P1 | Modulated reverb. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-09` | P1 | Shimmer/pitch-fed reverb. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-10` | P2 | Lower-octave/pitch-darkened ambience. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-11` | P1 | Gated reverb. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-12` | P1 | Reverse/nonlinear reverb. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-13` | P1 | Freeze/infinite sustain with bounded behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-14` | P2 | Spectral/granular ambience. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-15` | P1 | Convolution reverb with licensed/user-owned IRs. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-16` | P1 | Ducking reverb. | [ ] | [ ] | [ ] | Not started |
| [ ] | `REV-17` | P1 | Dry/wet, predelay, damping, filtering, and tail policy consistently exposed where supported. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-01` | P0 | Polyphonic transpose with a target range of −12…+12 semitones and mandatory −6/+6 chord tests. | [x] | [x] | [ ] | Implemented in shared C++ and exposed in offline CLI and browser/WASM; Linux, browser/native comparison, and MinGW Windows cross-build checks pass. Native tests cover ±12 bounds, ±6 single notes and triads, stereo independence, reset, irregular callback sizes, no allocations, and bounded impulse latency. Linux 48 kHz stereo latency is nominally 5.333 ms; processor load is 10.9001% p50 / 13.0331% p95 on Ryzen 9 5950X. Local prerecorded pickup/DI regression passes at +5 semitones. Manual guitar listening and Android device/runtime validation remain open. |
| [ ] | `PIT-02` | P0 | Drop-tune/virtual-capo workflow using PIT-01, 100% wet, saved per preset/scene. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-03` | P1 | Monophonic pitch mode optimized for single notes, clearly labeled. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-04` | P0 | Fine detune in cents. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-05` | P1 | Stereo micro-pitch/doubler. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-06` | P1 | Polyphonic octave layers with independently mixed dry/up/down voices. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-07` | P1 | Classic monophonic sub-octave generator. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-08` | P1 | Fixed-interval harmony, independent voice level/pan. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-09` | P1 | Key/scale-aware monophonic harmony. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-10` | P2 | Two-voice intelligent harmony and user-defined scales. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-11` | P1 | Expression-controlled pitch bend/whammy; label its verified mono/poly behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-12` | P2 | Polyphonic continuous bend if it passes chord/latency tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-13` | P1 | Twelve-string-style doubling, with limitations documented. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-14` | P2 | Pitch sequencer/arpeggiated intervals. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-15` | P2 | Formant adjustment where supported, with independent tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-16` | P1 | Per-voice dry/wet, smoothing, quality/latency profile, and stable backend identity. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-17` | P2 | Wider ±24-semitone range only after separate quality/range qualification. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PIT-18` | P2 | Multiple independent polyphonic interval voices with individual level/pan, separately verified from single-voice transpose and octave-only layers. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-01` | P0 | Audio-driven synth voice using envelope/filter/waveshaping with immediate guitar response. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-02` | P1 | Monophonic tracked oscillator synth with saw/pulse/sine options and glide. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-03` | P1 | Multi-oscillator synth with detune, octave, mixer, filter, and envelopes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-04` | P2 | Polyphonic resynthesis that preserves played chords; require separate chord evidence. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-05` | P1 | Organ-style additive/octave layering. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-06` | P2 | String/pad synthesis. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-07` | P2 | Brass/lead/bass synth voicings as presets over real synthesis controls. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-08` | P2 | Waveform morphing/wavetable synthesis. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-09` | P2 | FM/phase-modulation synthesis. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-10` | P1 | Resonator/comb-filter instrument effect. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-11` | P1 | Freeze/sustain capture feeding a synth/filter path. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-12` | P2 | Controlled harmonic feedback simulation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-13` | P2 | Vocoder with built-in oscillator carrier and external carrier routing. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-14` | P2 | Monophonic guitar-to-MIDI with note-on/off/bend validation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-15` | P2 | Polyphonic guitar-to-MIDI as a separately tracked research requirement, not inherited from SYN-14. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-16` | P1 | MIDI-driven instrument source; it must work independently of guitar tracking. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-17` | P2 | Step/arpeggiator controls with tempo and note-off reliability. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-18` | P2 | Granular audio effect with bounded buffers and clear added latency. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYN-19` | P1 | Tone/noise generator for creative use and diagnostics, explicitly level-limited. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-01` | P1 | Electric-to-acoustic-style filtering/resonance simulation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-02` | P1 | Acoustic body IR loading with verified assets. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-03` | P1 | Acoustic preamp/EQ/feedback-notch compound rig. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-04` | P2 | Resonator/sitar-style timbral effect. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-05` | P2 | Bass-like guitar transformation using octave, EQ, and dynamics. | [ ] | [ ] | [ ] | Not started |
| [ ] | `INS-06` | P2 | Bowed/swelling string-like texture, with tracking limits documented. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-01` | P0 | Lightweight clean amp/preamp engine. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-02` | P1 | American clean and edge-of-breakup styles. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-03` | P1 | Small vintage/tweed-style breakup. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-04` | P1 | British chime/low-gain style. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-05` | P1 | British classic crunch/high-gain style. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-06` | P1 | Modern American/high-gain styles. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-07` | P1 | Bass amp/preamp styles. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-08` | P1 | Separately controllable preamp/power-amp behavior only where the engine actually implements it. | [ ] | [ ] | [ ] | Not started |
| [ ] | `AMP-09` | P1 | Input calibration, meaningful gain staging, and level matching. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-01` | P0 | Mono cabinet IR loader with safe rate conversion and bounds. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-02` | P1 | Stereo/dual-cab convolution and blend. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-03` | P1 | User-owned/cleared guitar and bass cabinet assets, categorized by actual provenance. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-04` | P1 | Low/high cuts, IR trim, level, polarity, and alignment. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-05` | P1 | Mic choice/position/distance only through real modeling or matching IR assets; no decorative controls. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAB-06` | P1 | Independent cab bypass per destination, suitable for a real cabinet versus headphones/full-range output. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-01` | P1 | NAM playback for a documented supported set of architectures. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-02` | P2 | AIDA-X model playback with separately validated formats. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-03` | P1 | Sample-rate/calibration metadata handling and model prewarming. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-04` | P1 | Amp-only versus amp-plus-cab model labeling to avoid unintended duplicate cabinet processing. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-05` | P2 | Linux-assisted capture/training workflow with explicit prerequisites; Android playback must remain independent of it. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAP-06` | P1 | Asset provenance, import/export, file hashes, and redistribution permissions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-01` | P2 | Monophonic vocal pitch correction with key/scale and correction speed. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-02` | P2 | Vocal harmony distinct from fixed guitar transpose. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-03` | P2 | Vocal doubler. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-04` | P2 | Vocal saturation/distortion. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-05` | P2 | Vocal stutter/gating. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-06` | P2 | De-esser integration and intelligibility checks. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-07` | P2 | Voiced/unvoiced handling, formant behavior where supported, and stable bypass. | [ ] | [ ] | [ ] | Not started |
| [ ] | `VOC-08` | P2 | Simultaneous guitar/vocal route only on a tested interface/channel configuration. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-01` | P0 | Build a canonical effect manifest with independent fields for audio channels, pitch mode, musical polyphony and its evidence, engine/backend, source/license, platform, parameters, latency, quality mode, and verification state. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-02` | P1 | Cross-reference every family above against the researched commercial sources. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-03` | P1 | Distinguish algorithm, mode, tonal variant, factory preset, amp capture, and IR in all counts. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-04` | P1 | Retain source aliases/renames for research without implying that original product names are licensed app assets. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-05` | P1 | Add further distinctive families found in the official versioned inventories, recording the gap and acceptance tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CAT-06` | P0 | Display experimental/missing/unverified modules honestly; no selectable inert blocks in a release catalog. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-01` | P0 | Create the source repository structure, architecture decisions, build instructions, and dependency lock. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-02` | P0 | Build the headless core and Linux application from a clean documented environment. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-03` | P0 | Produce an installable Android APK for the target tablet's observed ABI. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-04` | P0 | Pin a compatible released Qt/NDK/JDK/Gradle/CMake combination; record actual versions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-05` | P0 | Supply license texts, notices, corresponding-source instructions, and a machine-readable component/asset manifest. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-06` | P0 | Confirm the app plays installed presets without an account, network, subscription, or external processor. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-07` | P1 | Produce a Linux portable package and distribution-appropriate install option; validate on the supported baseline. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-08` | P1 | Add CI builds and meaningful headless DSP/preset tests; keep hardware results separate from CI results. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-09` | P1 | Provide versioned release notes, artifact checksums, rollback/recovery guidance, and known limitations. | [ ] | [ ] | [ ] | Not started |
| [ ] | `APP-10` | P0 | Use original UI assets and only cleared factory IR/model/audio content. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-01` | P0 | Implement a functional full-duplex dry-through path with meters and mute. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-02` | P0 | Implement actual device/channel selection and show the granted audio configuration. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-03` | P0 | Use Oboe's full-duplex/LiveEffect pattern: output first, matched input rate, startup synchronization, safe partial reads. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-04` | P0 | Recover input and output together after disconnect, with controlled silence and no surprise speaker route. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-05` | P0 | Keep engine lifetime independent of QML/Activity visibility; implement the correct Android foreground audio lifecycle. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-06` | P0 | Validate permission, foreground-service type/permission, notification, capture start restrictions, and shutdown behavior against the selected Android target. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-07` | P0 | Implement the prepared graph, bounded real-time buffers/events, and off-thread reclamation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-08` | P0 | Verify atomics are lock-free on each target ABI and queues obey their producer/consumer contract. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-09` | P0 | Handle event overflow without losing critical note-off/panic semantics. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-10` | P0 | Implement mono/stereo serial/parallel routing with deterministic scheduling. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-11` | P0 | Implement smooth continuous parameters and safe discrete configuration transitions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-12` | P0 | Define immediate dry bypass, latency-compensated bypass, mute, and tail-preserving versus tail-cut behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-13` | P1 | Implement per-node delay reporting and bounded parallel compensation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-14` | P0 | Record callback timing distributions, deadline overruns, xruns, and route-specific diagnostics off-thread. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-15` | P0 | Reject over-budget or invalid graph edits without damaging the previous live graph. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-16` | P1 | Provide Eco/Balanced/High profiles with actual algorithm settings and per-preset persistence; never silently change pitch quality. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-17` | P1 | Handle Linux server/rate/quantum changes and JACK/PipeWire disconnection. | [ ] | [ ] | [ ] | Not started |
| [ ] | `ENG-18` | P2 | Add optional Linux LV2 discovery/hosting with supported-feature reporting, scanning isolation, and crash limitations documented. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-01` | P0 | Deliver the browser/canvas/inspector layout on tablet and Linux. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-02` | P0 | Implement responsive phone layout without losing graph semantics. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-03` | P0 | Connect every visible control to real engine state and show actual engine feedback. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-04` | P0 | Implement add/move/reorder/duplicate/delete/replace with preview, cancel, and undo. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-05` | P0 | Supply touch, mouse, and keyboard editing alternatives. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-06` | P0 | Provide numeric parameter entry, reset, units, fine control, and stable focus. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-07` | P0 | Provide clear dirty/saved state and recover edits after interruption. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-08` | P1 | Implement locked Perform mode with scenes and labeled switches. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-09` | P1 | Implement screen-reader labels, contrast/scaling checks, reduced motion, and large targets. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-10` | P0 | Show measured/estimated/unmeasured latency distinctly, with stale-result handling. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-11` | P1 | Provide quick audition and cancel for model/asset replacement while keeping the original state recoverable. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UI-12` | P1 | Provide block favorites, reusable subchains, and optional linked shared blocks with explicit shared-edit consequences. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-01` | P0 | Save/reload the complete rig with stable processor, backend, and parameter IDs. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-02` | P0 | Restore −6 and +6 semitone presets exactly after restart. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-03` | P0 | Implement schema validation, atomic saving, backups, and migration fixtures. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-04` | P1 | Export/import a portable bundle with asset manifest and content hashes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-05` | P1 | Preserve unresolved blocks/assets as visible state; do not silently replace their tone. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-06` | P1 | Implement scenes with explicit shared/base/override scope, copy, reset, and compare. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-07` | P1 | Reorder scene/setlist entries without silently altering stable switch assignments. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRE-08` | P1 | Verify tails and preset crossfades under the actual CPU/memory budget. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-01` | P0 | Support basic USB MIDI learn for bypass, program change, and expression. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-02` | P1 | Support controller channels, bank select, CC ranges, momentary/latch, toggle/pickup behavior, and conflicts. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-03` | P1 | Support one control mapped to multiple parameters with ranges, curves, and inversion. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-04` | P1 | Implement tap tempo and musical subdivisions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-05` | P1 | Support MIDI clock where feasible; document clock master, sync loss, and restart policy. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-06` | P2 | Support Bluetooth MIDI after independent timing/reconnection tests. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-07` | P2 | Implement a reusable modulation/step sequencer with per-step value, duration, reset, smoothing, and tempo. | [ ] | [ ] | [ ] | Not started |
| [ ] | `CTL-08` | P1 | Handle controller hotplug, stuck notes, conflicting sources, and panic deterministically. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-01` | P1 | Metronome with selectable accent and routing. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-02` | P1 | Basic offline drum/rhythm accompaniment with cleared sounds. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-03` | P1 | Backing-track player with independent volume, looping, and A/B section repeat. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-04` | P2 | Backing-track tempo/key adjustment, separated from live-guitar latency claims. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-05` | P1 | Looper record/play/overdub/stop/clear/undo/redo with clear states. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-06` | P1 | Looper pre/post effects routing and explicit preset-change behavior. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-07` | P2 | Quantized looping, reverse, half-speed, and bounded multi-layer operation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-08` | P1 | Record dry DI and processed audio to WAV off-thread with alignment metadata. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-09` | P1 | Recover cleanly from storage-full, permission loss, and interrupted recording. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-10` | P1 | Export recordings and presets through supported Android/Linux file access. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-11` | P1 | Reamp existing user recordings through the same DSP graph for repeatable comparisons. | [ ] | [ ] | [ ] | Not started |
| [ ] | `PRC-12` | P0 | Ensure browsing/recording/import/decoding cannot block real-time audio. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-01` | P0 | Bound input file sizes, node/connection counts, IR lengths, model sizes, and decoded buffer memory. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-02` | P0 | Validate sample rates, channel layouts, finite numeric values, supported model architectures, and format versions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-03` | P0 | Keep native code out of preset import; only load bundled audited Android processors. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-04` | P1 | Verify archive paths, asset hashes, duplicate IDs, malformed files, and missing dependencies. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-05` | P1 | Keep device-specific physical port mapping separate from portable logical rig routing. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-06` | P1 | Provide a local diagnostic export with user-controlled inclusion of device/preset data. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-07` | P0 | Keep user audio private by default; no automatic upload or runtime network requirement. | [ ] | [ ] | [ ] | Not started |
| [ ] | `DAT-08` | P1 | Bound caches, release unused assets off-thread, and test low-memory recovery. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-01` | — | Instantiate, prepare, process, bypass, serialize, restore, and destroy multiple independent instances. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-02` | — | Validate documented ranges/defaults/units and all mode boundaries. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-03` | — | Check silence, impulses, DC, steps, sine/sweeps, noise, and representative DI audio where appropriate. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-04` | — | Reject or safely handle NaN/Infinity, malformed state, and out-of-range values. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-05` | — | Check output levels, clipping, DC offsets, gain compensation, and bounded feedback. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-06` | — | Verify channel mapping, mono summing, stereo linking, phase polarity, and no channel leakage. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-07` | — | Run at 44.1/48 kHz and any additionally advertised rate; only claim hardware rates actually supported. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-08` | — | Exercise different/irregular block sizes, cold starts, resets, and silence recovery. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-09` | — | Check parameter automation for zipper noise, bursts, loss of state, and unexpected discontinuity. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-10` | — | Measure or validate algorithm latency, declared tail, bypass policy, and compensation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-11` | — | Test shared/static state and reentrancy; instances must not corrupt each other. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-12` | — | Compare Android and Linux renders with suitable tolerances; explain differences due to architecture, random state, or backend versions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-13` | — | Verify state round trips after app restart and after schema migration. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-14` | — | Check the actual target CPU/memory/latency budget and retained listening samples. | [ ] | [ ] | [ ] | Not started |
| [ ] | `TST-15` | — | Confirm that every marketed knob/mode produces its documented behavior in the DSP. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-01` | — | Measure the dry app path electrically through the exact interface and device. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-02` | — | Record a direct reference and app-processed output using a suitable independent capture/loopback arrangement; account for the measurement fixture. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-03` | — | Measure the full reference rig and each advertised live pitch profile. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-04` | — | Use a validated onset/frequency-aware method for pitch-shifted signals. Naive correlation of differently pitched waveforms is insufficient. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-05` | — | State uncertainty, warmup policy, sample count, median, p95, p99, and worst observed latency. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-06` | — | Record sample rate, burst/buffer sizes, actual route/modes, interface/power, device build, algorithm/backend/version, and graph hash. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-07` | — | Separate algorithm-reported delay, software scheduling/adapter delay, and total measured physical round trip. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-08` | — | Repeat after warmup/thermal soak and after relevant route or quality changes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-09` | — | Store failed measurements too; no cherry-picked best number in the UI or documentation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `LAT-10` | — | Verify the measurement fixture using a known delay and demonstrate that it can distinguish expected changes. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-01` | — | Run the advertised reference chain for at least 60 minutes and retain a dropout recording. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-02` | — | Report p50/p95/p99/p99.9/max callback durations and deadline-overrun counts; p99 alone can hide failures. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-03` | — | Compare xrun counters with recorded audio; do not trust an unavailable or always-zero counter. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-04` | — | Drag/reorder blocks, browse models, save, change scenes, and record during audio playback. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-05` | — | Stress the expensive pitch + amp/capture + convolution + ambience combination within the declared profile. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-06` | — | Test USB unplug/replug, power interruption, device route change, and occupied device access. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-07` | — | Test Android permission denial/revocation, privacy toggle, foreground start/stop, notification, screen lock, background/restore, rotation, and Activity recreation. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-08` | — | Test interruptions and audio focus without an uncontrolled return at loud volume. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-09` | — | Test battery saver, low memory, storage-full, CPU contention, and thermal throttling; record conditions. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-10` | — | Test Linux audio-server restart, disconnect, rate/quantum changes, and MIDI-port hotplug. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-11` | — | Confirm playback works offline after a clean install with bundled assets. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-12` | — | Test malformed/oversized preset, IR, and model imports with bounded failure handling. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-13` | — | Verify no uncontrolled feedback, unexpected dry path, or automatic speaker monitoring after errors. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-14` | — | Run sanitizers in suitable development builds for memory/undefined behavior; use release builds for performance measurements. | [ ] | [ ] | [ ] | Not started |
| [ ] | `SYS-15` | — | Document device/interface support as verified, unsupported, or untested with evidence. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-01` | — | Complete add/select/edit/bypass/reorder/duplicate/delete/undo using touch, mouse, and keyboard. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-02` | — | Test invalid drops, cancelled drags, resource rejection, split/merge movement, and cross-lane channel mismatches. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-03` | — | Test phone/tablet/desktop layouts, rotation, scaling, contrast, focus, and screen-reader labels. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-04` | — | Save a complex preset with pitch, routing, assets, MIDI and scenes; restart and verify exact semantic state. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-05` | — | Export Android → Linux → Android; verify asset resolution, backend identity and equivalent sound within justified tolerances. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-06` | — | Test older schema migrations, changed plugin versions, unknown nodes, missing IR/model files, and incompatible pitch backends. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-07` | — | Verify that scene edits affect only the declared scope and scene reordering preserves controller mappings. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-08` | — | Test scene/preset transitions on held chords and long delay/reverb tails. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-09` | — | Verify the UI never reports a placeholder as active DSP or an estimate as measured latency. | [ ] | [ ] | [ ] | Not started |
| [ ] | `UXT-10` | — | Validate panic, tuner mute/unmute, expression endpoints and stuck-note recovery in Perform mode. | [ ] | [ ] | [ ] | Not started |
| [ ] | `WIN-01` | P0 | Build the shared application for Windows x64 with a native WASAPI backend. | [ ] | [ ] | [ ] | Not started |
| [ ] | `WIN-02` | P0 | Verify portable preset save/load and matching DSP state across Windows, Linux, and Android builds. | [ ] | [ ] | [ ] | Not started |
| [ ] | `WIN-03` | P0 | Run automated Windows compile, DSP, preset, and audio-backend tests; retain exact toolchain and results. | [ ] | [ ] | [ ] | Not started |
| [ ] | `WIN-04` | P0 | Verify live full-duplex guitar input/output on Windows with an actual supported interface; keep this manual check open until performed. | [ ] | [ ] | [ ] | Not started |
| [x] | `DSP-01` | P0 | Measure each processor's CPU use as a percentage of the callback deadline and report the complete benchmark configuration and percentiles. | [x] | [x] | [ ] | All twenty-five effect stages, the signal-control layer, and the 25-effect chain measured on Linux desktop; p50/p95/p99/p99.9/max, owned memory, and exact configuration recorded in `LATENCY_AND_QUALITY.md`. The 2 s / 48 kHz / stereo delay measures 0.0341% p50 / 0.0345% p95 normally, 0.0533% / 0.0536% in ping-pong mode, and 0.0529% / 0.0533% in independent dual mode; these modes reuse 768,304 B. Dual mono measures 0.0420% / 0.0424% with two lines and the same memory. Repeat on Android/Windows and after future DSP changes. |
| [x] | `LIN-01` | P0 | Provide a reproducible Linux WAV ingest/render/regression path using prerecorded audio and deterministic settings. | [x] | [x] | [ ] | `make audio-regression` rendered five EGFxSet pickup variants plus the 72.5-second Guitar-TECHS DI excerpt in stereo and downmixed-to-mono, each through all 25 processors including round fuzz, both overdrives, high-gain distortion, flanger, phaser, and vibrato; live hardware remains unprobed |
| [x] | `LIN-02` | P0 | Exercise the local prerecorded guitar corpus through implemented DSP and retain output metrics without publishing private audio. | [x] | [x] | [ ] | Local-only regression passed on five pickup-position note files and the 72.5-second DI performance in stereo and mono; render metrics were retained in command output and temporary WAV outputs removed; no user audio published |
| [x] | `DATA-01` | P0 | Keep licensed prerecorded guitar fixtures outside Git, verify their source checksums, and expose them through a local-only ignored symlink. | [x] | [x] | [ ] | EGFxSet Clean (431,036,829 bytes) and Guitar-TECHS P3 music (129,505,089 bytes) pass MD5 verification; 690 and 32 WAVs extracted outside Git; ignored symlink resolves to external storage |
| [x] | `DATA-02` | P0 | Run deterministic offline DSP regressions on documented clean pickup-position samples and a clean DI musical excerpt. | [x] | [x] | [ ] | `make audio-regression` passed for the same open high-E note across all five EGFxSet positions plus `directinput_01.wav`; source paths and processor settings are recorded in `AUDIO_COMPATIBILITY.md` |
| [x] | `WIN-05` | P0 | Provide a repeatable MinGW-w64 Windows x64 cross-build of the shared DSP/offline CLI and a WASAPI backend compile-smoke target. | [x] | [x] | [ ] | `make windows-cross-build` passed after phaser integration with MinGW GCC 13.0 and CMake 3.28.3; the shared DSP and offline CLI compiled. Windows execution/device tests remain separate |
| [ ] | `WEB-01` | P0 | Provide a browser/WASM guitar-audio test build whose Play control processes and plays the selected effect-chain output, with local-only files and automated audio-output comparison against native C++. | [x] | [x] | [ ] | `make web-test` passed on HeadlessChrome 151 at 48 kHz/128 frames; local-file decode, Worklet/WASM 25-stage chain including Round Fuzz and independent dual delay, Play capture, and native comparison passed. Maximum absolute error was 0.00607224 and RMS error 0.000697286 (1.0219% reference RMS), within 6% peak and 7.5% RMS bounds. Chain state reports 1,228,404 bytes plus 10,364 bytes tuner state within fixed 16 MiB WASM memory. Worklet timing falls back to 1 ms `Date.now()` and is coarse, not a processor-quality benchmark. Manual listening through a real browser output remains open. |

Total source requirements: 327; approved additions: 11.
