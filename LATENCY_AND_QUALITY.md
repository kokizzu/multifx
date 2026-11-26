# Latency, quality, and DSP-load protocol

## Fender Tone Master Pro Harmonic Tremolo mode switch (2026-11-27)

The Model Guide v1.1.100 / Rev. A, printed p. 33, names 1960 6G4 and 1963 6G4A but does not describe their circuit differences or publish numeric bounds/defaults. TabletFX preserves the existing 6G4 output and applies a smoothed, project-authored 1.25× effective-crossover shift for 6G4A (700→875 Hz); it does not claim to reproduce either historic circuit. The existing one-pole split and LFO remain in place. Native state is 136 B, browser/WASM effect state is 112 B, and the mode adds no fixed audio-path latency or process allocation.

Release benchmark: AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks and 20,000 measured callbacks. Values are processor time as a percentage of one callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Fixed added latency | Process allocations | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1960 6G4 (default) | 0.084% | 0.087% | 0.098% | 0.363% | 1.720% | 136 B | 0 frames | 0 | 0/20,000 |
| 1963 6G4A (project profile) | 0.084% | 0.085% | 0.094% | 0.318% | 1.157% | 136 B | 0 frames | 0 | 0/20,000 |

The focused browser test uses local prerecorded Guitar-TECHS DI and observes output-difference RMS 0.00164583899 between profiles. Browser timer granularity is insufficient for isolated CPU reporting, so the native benchmark provides the percentage figures. These desktop processor timings are not whole-chain or tablet guarantees. The focused native tests, browser/WASM playback, and MinGW Windows core/offline-renderer build pass. Human listening, physical Tone Master Pro comparison, and target-device timing remain manual.

## HeadRush Prime Orange Phaser Stereo project control (2026-11-27)

Prime v5.0.0 User Guide §4.4.2, printed p. 23/PDF p. 24, shows an Orange Phaser Stereo toggle but does not define its signal behavior, numeric settings, default, or persistence. The v5.2.0 firmware notes (October 2026) do not mention an Orange Phaser change; installed firmware is unknown. TabletFX adds a project-authored toggle to the existing Phaser: on preserves the processor's previous opposite L/R LFO phases, off links them. It adds no audio delay, buffer, or `process()` allocation. Native state is 296 B; browser/WASM reports 280 B.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks, and 20,000 measured callbacks. Values are full Phaser processor time as a percentage of one callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Fixed added latency | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Stereo on (default) | 0.099% | 0.141% | 0.147% | 0.578% | 1.618% | 296 B | 0 frames | 0/20,000 |
| Stereo off | 0.099% | 0.099% | 0.145% | 0.299% | 1.143% | 296 B | 0 frames | 0/20,000 |

The benchmark profiles include the full Phaser and do not isolate marginal toggle cost; short-run percentile variance is visible in the tail. The local prerecorded Guitar-TECHS DI browser smoke observes output-difference RMS 0.05312 between Stereo on/off. These desktop measurements are not a target-device or full-chain performance guarantee. Native, browser/WASM, and Windows build checks pass; listening and hardware comparison remain manual, and no HeadRush DSP or sound parity is claimed.

## Line 6 Helix Stadium Pitch Ring Mod project effect (2026-11-27)

The Stadium model catalogue lists Pitch Ring Mod as a Stereo Line 6 Original in Modulation, first appearing in v1.0. The live manual selector is v1.4 Rev D / v1.3 Rev B / v1.2, but no model-specific parameter table was available. The project-authored kernel uses a bounded monophonic autocorrelation tracker on the mono average of the channels. Its carrier is shared across stereo output; chords and noisy/polyphonic input can mislead the tracker. Project defaults are Carrier Ratio 1.0, Fallback Pitch 110 Hz, Tracking Confidence 0.55, Mix 0.5, and Level 0 dB. Project bounds are recorded in `IMPLEMENTATION_CHECKLIST.md`.

The audio path has no fixed delay, but pitch acquisition is not instantaneous: measured initial tracking warm-up is 42.7 ms, a 220→440 Hz note transition settles within 5 Hz in 48 ms, and silence releases to fallback in 26.7 ms. This is tracking response, not buffered audio latency.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), and 20,000 measured callbacks. Values are processor time as a percentage of one callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Project defaults | 1.0065% | 1.0812% | 1.2739% | 2.1676% | 4.3437% | 2,176 B | 0/20,000 |

The tracker uses approximately 7,180 B of temporary stack scratch during pitch estimation; this transient memory is not included in the object-state number. The browser reports 2,148 B effect state. Native processing allocates nothing. The local-Guitar-TECHS-DI browser/WASM smoke verifies finite output and distinct parameter profiles; the browser timer's millisecond fallback is too coarse to report isolated DSP percentage. These results are host measurements, not a target-device guarantee. Human listening, physical comparison, installed firmware, and tablet timing remain manual; no Line 6 DSP/settings/sound parity is claimed.

## Fender Tone Master Pro Modulated Large Hall project effect (2026-11-27)

The Fender Model Guide v1.1.100 / Rev. A, printed p. 55, lists Level, Decay, Dwell, Diffuse, Rate, Depth, High Cut, Low Cut, and Spillover. High/Low Cut are shelf gains. It gives no numeric defaults/ranges, shelf pivots, DSP, or latency. TabletFX uses project-chosen controls and a four-line modulated FDN; the immediate dry path has zero added fixed latency. Native tests verify `process()` has no allocations. Native state accounting is `sizeof(object)` plus vector capacity and excludes allocator metadata; WASM effect state uses the chain's `memory_bytes()` report.

Release benchmark: AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks and 20,000 measured callbacks. Cost is isolated processor time as a percentage of one callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Project default | 0.618% | 0.773% | 0.937% | 1.188% | 1.847% | 118,184 B | 0/20,000 |
| Maximum project decay/modulation | 0.618% | 0.656% | 0.895% | 1.179% | 1.806% | 118,184 B | 0/20,000 |

The browser reports 117,976 B of effect state at 48 kHz stereo. Its HeadlessChrome timing source was millisecond-granularity fallback, so its per-callback CPU readings are too coarse for an isolated effect percentage; the native benchmark above is the reported DSP budget. The browser smoke verifies two project profiles produce finite, different output from local prerecorded stereo Guitar-TECHS DI. These are desktop measurements and are not a tablet or full-chain performance guarantee. Human listening, physical Tone Master Pro comparison, and target-device timing remain manual.

## Fender Tone Master Pro 3D Chorus project modes (2026-11-26)

The exact-model Model Guide v1.1.100 / Rev. A, printed p. 42, documents four modes described from subtle to extreme; it does not specify their names, curves, numeric controls, stereo behavior, DSP, or latency. TabletFX implements four project-authored choices on one existing Chorus processor, defaulting to Mode 1. The implementation adds no separate fixed-delay stage or process-time allocation. At 48 kHz stereo the native state is 34,864 B and the focused browser/WASM effect state is 34,820 B.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks, and 20,000 measured callbacks. Values are elapsed processor time as a percentage of one callback deadline:

| Project mode | p50 | p95 | p99 | p99.9 | Max | Native state | Fixed added latency | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mode 1 (subtle default) | 0.0859% | 0.1354% | 0.1436% | 0.3536% | 6.5184% | 34,864 B | 0 frames | 0/20,000 |
| Mode 2 | 0.0859% | 0.1354% | 0.1470% | 0.4271% | 4.4090% | 34,864 B | 0 frames | 0/20,000 |
| Mode 3 | 0.0859% | 0.1073% | 0.1706% | 0.3649% | 4.1919% | 34,864 B | 0 frames | 0/20,000 |
| Mode 4 (most pronounced) | 0.0859% | 0.1073% | 0.1500% | 0.3326% | 0.7031% | 34,864 B | 0 frames | 0/20,000 |

Mode 1's maximum is an outlier relative to its p99.9; the measured maximum across all four modes remained below 6.52% of the callback deadline, with no deadline misses. These are isolated desktop processor timings, not whole-chain cost or an Android/tablet guarantee. `make test`, `make web-test`, `make windows-cross-build`, and `make bench` pass; the browser test checks all four categorized local Guitar-TECHS DI Play outputs. Manual listening and physical-device comparison remain open. No Fender DSP, settings, or sound parity is claimed.

## Line 6 Helix Stadium global TAP modulation reset (2026-11-26)

The live Helix Stadium Effects manual (selector v1.4 Rev D / v1.3 Rev B / v1.2) says one TAP press resets LFO-based modulation, with tremolos and rotary speakers as examples. It does not specify exact phase semantics. TabletFX routes a global UI action to the existing Tremolo and Rotary Speaker processors only. This is a user event, not a new audio effect: it adds no processor state, buffer, fixed latency, or work inside the per-sample processing loop. The worklet performs one virtual phase-reset dispatch across the existing processor list on a TAP press; its isolated event-handler CPU time is not separately benchmarked, so no callback percentage is claimed for that one-shot action.

`make test` passes the focused phase/state-preservation checks and all 117 CTests. `make web-test` passes local prerecorded Guitar-TECHS DI playback plus the WASM button-to-chain acknowledgement; the browser/native comparison has maximum absolute error 0.0000016615 and RMS error 0.0000003308. The complete browser chain reports 80,644,616 B processor state and 10,364 B tuner state in a fixed 100,663,296 B WASM heap. `make windows-cross-build` passes. These full-chain measurements do not estimate one-shot TAP event cost or target-device performance. Human listening/device checks and broader TAP coverage for other project LFO processors remain open. [Helix Stadium Effects manual](https://manuals.line6.com/en/helix-stadium/live/effect-blocks) · [v1.4.2 release notes](https://kb.line6.com/helix-stadium-1-4-release-notes)

## Fender Tone Master Pro Chorus Low Cut, High Cut, and Stereo Width (2026-11-26)

The Tone Master Pro Model Guide v1.1.100 / Rev. A, printed p. 41, lists Low Cut and High Cut for Chorus Mono and Stereo, with Width only for Stereo. It publishes no units, ranges, defaults, topology, DSP cost, or latency. Fender firmware history was checked through v1.8.87 (2026-11-23); its notes identify no specific change for these controls. Installed firmware is unknown. TabletFX's 0–1,200 Hz Low Cut, 0–20,000 Hz High Cut, and 0–200% Width controls (defaults: both cuts off, Width 100%) and processing are project-authored.

At 48 kHz stereo and 128-frame callbacks, a Ryzen 9 5950X Release run used 3,000 warm-up and 20,000 measured callbacks against a 2.667 ms deadline. Percentages below are complete Chorus callback cost, not marginal control cost:

| Profile | p50 / p95 / p99 / p99.9 / max callback budget (%) | Native state |
|---|---:|---:|
| Defaults (cuts off, Width 100%) | 0.082 / 0.083 / 0.085 / 0.279 / 0.900 | 34,848 B |
| Low Cut 200 Hz | 0.107 / 0.109 / 0.166 / 0.323 / 1.086 | 34,848 B |
| High Cut 6,000 Hz | 0.108 / 0.108 / 0.165 / 0.347 / 0.551 | 34,848 B |
| Width 200% | 0.086 / 0.087 / 0.138 / 0.368 / 1.135 | 34,848 B |
| Both cuts plus Width 200% | 0.145 / 0.147 / 0.210 / 0.449 / 2.193 | 34,848 B |

The benchmark adds no algorithmic delay and the tests confirm no processing allocations. Browser/WASM reports 34,812 B for Chorus effect state. Native tests, CLI compatibility checks, local Guitar-TECHS DI browser/WASM Play, full browser/native comparison, Windows MinGW cross-build, full test suite, and `make diff-check` pass. These are isolated desktop processor timings, not Android or full-chain load. Human listening, physical Fender comparison, and target-device performance remain manual; no Fender settings, DSP, or sound parity is claimed.

## Fender Tone Master Pro Electric Flanger Filter mode (2026-11-26)

The Tone Master Pro Model Guide v1.1.100 / Rev. A, printed p. 39, describes Electric Flanger Filter mode as retaining harmonic flanging without frequency modulation; Rate changes its tonality and Color adjusts intensity. It publishes no numeric ranges/defaults, DSP topology, or latency. Firmware history is checked through v1.8.87 (2026-11-23); installed firmware is unknown.

TabletFX adds a project-authored static short-comb Filter mode to the existing Flanger at Modulation ID 17. Flanger remains the default. In Filter mode, the project Rate position (0.05–8, default 0.25) maps linearly to a fixed 0.5–10 ms comb delay; project Color (0–0.9, default 0.35) sets positive feedback. Mix remains active; tempo division, movement depth, feedback polarity, and stereo phase are disabled. The existing delay ring is reused, so there is no new audio buffer, fixed latency, or process-time allocation. Native state is 4,840 B; focused browser/WASM state is 4,804 B.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.667 ms deadline), 3,000 warm-up blocks, and 20,000 measured blocks. The Filter profile was p50/p95/p99/p99.9/max 0.0308/0.0311/0.0495/0.0570/0.2291% of the callback budget. This is an isolated desktop processor measurement, not Android or live-chain performance, and it should not be interpreted as a measured CPU reduction relative to Flanger mode. Native and CLI tests, browser/WASM Play on local prerecorded guitar DI, Windows MinGW cross-build, and `make diff-check` pass. Physical Fender comparison and human listening remain manual; no Fender DSP/settings/sound parity is claimed.

## Fractal FM9-family Synth block output Level (2026-11-26)

The November 2026 Fractal Audio Blocks Guide, printed pp. 4 and 102, documents the Synth block's shared Level and describes block Level as overall output. It does not publish a numeric Level range/default. FM9 v12.0 downloads list FM9 Original and Turbo, not Mark II Turbo by name; installed firmware is unknown.

TabletFX adds a project-authored −24…+12 dB Level, default 0 dB, with a 10 ms linear gain ramp. The scalar is applied to complete dry-plus-synth output. `pow()` runs during configuration, not in the audio callback. Processing adds one gain multiply per sample and no allocation, buffer, or fixed algorithmic latency. Native object state is 1,031,360 B, 16 B larger than before. This is total Synth state, including the tracker and its fixed analysis buffers.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.667 ms deadline), 2,000 warm-up blocks, and 5,000 measured blocks. Elapsed processor time is shown as a percentage of one callback deadline:

| Profile | p50 / p95 / p99 / p99.9 / max callback budget (%) | Native state |
|---|---:|---:|
| Three-voice tracked Synth, Level 0 dB | 11.4427 / 12.0082 / 12.3555 / 18.2566 / 19.9783 | 1,031,360 B |
| Three-voice tracked Synth, Level −12 dB | 9.0295 / 12.0416 / 12.4717 / 15.0525 / 21.1311 | 1,031,360 B |

The percentile spread and run-to-run variation do not isolate the extra gain multiply; these rows report full Synth usage, not marginal Level cost. No additional note-detection or synthesis work is introduced by the control. Native gain/ramp/bounds/allocation tests, HeadlessChrome local prerecorded Guitar-TECHS DI Play, browser/native comparison, and the Windows MinGW core/offline build pass. Manual listening, target-device timing/memory, and Fractal settings/DSP/sound parity remain open.

## Previous checkpoint — Line 6 Helix Stadium Delay Trails on bypass (2026-11-26)

The live Helix Stadium Effects manual's common-parameter table says Trails Off mutes delay repeats/reverb decay after bypass, while Trails On allows the tail to fade after bypass or a snapshot change. No default is stated. Manual labels are inconsistent: the selector/header says v1.4 Rev D, the cover says Firmware 1.4, and the footer says Rev E / v1.4.1. Separate hardware v1.4.2 notes are dated 2026-10-27. Installed firmware is unknown, and Simple Delay applicability remains unresolved.

TabletFX adds a default-off, project-authored Trails toggle to the existing Delay row at picker ID 21. Enabled bypass preserves the incoming dry signal and drains wet repeats through the existing Delay state without new excitation. It adds no slot, delay ring, or fixed algorithmic latency. The 48 kHz stereo, 2 s Delay state is 768,824 B whether the toggle is on or off; `process()` and the bypass drain path allocate no memory. Snapshot/preset changes are not implemented.

The isolated Release benchmark used an AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame blocks (2.667 ms deadline), 160 blocks of excitation, and 5,000 measured bypass callbacks. Values are elapsed processor time divided by the callback deadline:

| Profile | p50 / p95 / p99 / p99.9 / max callback budget (%) | Native state |
|---|---:|---:|
| Trails Off, bypassed | 0.0008 / 0.0011 / 0.0011 / 0.0011 / 0.0030 | 768,824 B |
| Trails On, draining | 0.1350 / 0.2858 / 0.3098 / 0.7028 / 1.1097 | 768,824 B |

The active drain adds 0.3087 percentage points at p99 and 0.6998 points at p99.9 compared with the bypassed/off path. It retains the zero-frame dry path and processes stored repeats until they decay. These processor-call timings are isolated desktop measurements, not live audio, Android/tablet, or hardware DSP estimates. The full native CTest suite, offline CLI regression, local prerecorded Guitar-TECHS browser/WASM Play test, full browser/native comparison, and Windows MinGW compile pass. Manual listening, device timing, physical behavior, and Line 6 DSP/sound parity remain open.

## Fractal FM9-family Chorus Delay Time/Minimum Delay and Delay Range (2026-11-26)

The November 2026 Fractal Audio Blocks Guide, printed pp. 25–26, documents Chorus Delay Time (shown as Minimum Delay on the Expert page) at 0.1–50 ms and Delay Range at 1–40 ms. It does not state defaults. The source is family-level; FM9 downloads list v12.0 for FM9 Original/Turbo, not Mark II Turbo by name. Exact applicability and installed firmware remain unconfirmed.

TabletFX adds project-authored Minimum Delay/Delay Range controls at the existing Modulation Chorus ID 16, with 0.1–50 / 1–40 ms project bounds and 8/14 ms defaults. The previous 15 ms center / 7 ms depth maps to 8/14, preserving the one-voice default sample-for-sample. The fixed ring covers the maximum 90 ms tap and is preallocated; native effect state is 34,752 B at default and both timing endpoints. No fixed algorithmic latency or processing allocation is added. Old C ABI, CLI center/depth flags, and browser parameter-array values continue to map through the compatibility conversion. These are project controls and do not claim Fractal algorithm or sound parity.

The dedicated make bench-fractal-chorus-tempo run used AMD Ryzen 9 5950X Release, 48 kHz stereo, 128-frame callbacks (2.667 ms deadline; 3,000 warm-up and 20,000 measured callbacks). Percentages are callback time divided by the deadline:

| Profile | p50 / p95 / p99 / p99.9 / max callback budget (%) | Native state |
|---|---:|---:|
| Free 0.8 Hz, 1 voice, 8/14 ms | 0.060 / 0.062 / 0.084 / 0.249 / 0.929 | 34,752 B |
| Quarter-note 120 BPM, 1 voice, 8/14 ms | 0.059 / 0.061 / 0.090 / 0.362 / 2.554 | 34,752 B |
| 1 voice, 8/14 ms | 0.060 / 0.061 / 0.092 / 0.320 / 2.551 | 34,752 B |
| 2 voices, 8/14 ms | 0.099 / 0.114 / 0.153 / 0.680 / 5.784 | 34,752 B |
| 4 voices, 8/14 ms | 0.171 / 0.174 / 0.288 / 1.617 / 2.333 | 34,752 B |
| 8 voices, 8/14 ms | 0.305 / 0.308 / 0.502 / 1.817 / 2.815 | 34,752 B |
| 1 voice, minimum endpoint 0.1/1 ms | 0.061 / 0.061 / 0.062 / 0.273 / 2.432 | 34,752 B |
| 1 voice, maximum endpoint 50/40 ms | 0.061 / 0.061 / 0.095 / 0.365 / 3.230 | 34,752 B |

The focused browser/WASM test played local prerecorded Guitar-TECHS DI at both endpoint settings; the full browser/native suite passed. Full make test (117/117 CTest), make web-test, make windows-cross-build, make diff-check, and the dedicated benchmark passed. These are isolated desktop measurements, not target-device guarantees. Human listening, physical-device behavior, installed firmware, and exact FM9 Mark II Turbo applicability remain open.


## Fractal FM9-family Chorus Number Of Voices (2026-11-26)

The November 2026 Fractal Audio Blocks Guide (printed pp. 25–26) places `Number Of Voices` on the Expert page and says it increases delay-line count up to eight. It suggests two voices for vintage chorus and up to eight for a lush ensemble but does not state a numeric minimum or default. Above two voices automatically selects SINE; choosing a non-SINE LFO resets the count to two. The same pages document Delay Time/Minimum Delay 0.1–50 ms and Delay Range 1–40 ms as separate controls. The source is family-level; FM9 downloads list v12.0, released July 7, 2026, for FM9 Original/Turbo only. Exact applicability to Mark II Turbo and installed firmware remain unconfirmed.

TabletFX adds a project-authored 1–8 voice count, default 1, to the existing Modulation Chorus at ID 16. One voice is sample-identical to the old signal path. Additional voices share the existing delay ring with evenly spaced sine-phase offsets and averaged wet output. This is not Fractal DSP or algorithm parity. Native state is 12,472 B at each count; browser/WASM effect state is 12,436 B. No new ring storage, fixed algorithmic latency, or process-time allocation is added.

The benchmark used AMD Ryzen 9 5950X Release, 48 kHz stereo, 128-frame callbacks (2.667 ms budget; 3,000 warm-up and 20,000 measured callbacks). Percentages are callback time divided by budget:

| Voice count | p50 / p95 / p99 / p99.9 / max callback budget (%) | Native state |
|---:|---:|---:|
| 1 | 0.059 / 0.076 / 0.109 / 0.303 / 0.615 | 12,472 B |
| 2 | 0.117 / 0.169 / 0.198 / 1.044 / 105.721 | 12,472 B |
| 4 | 0.174 / 0.177 / 0.178 / 0.422 / 0.745 | 12,472 B |
| 8 | 0.248 / 0.310 / 0.492 / 0.746 / 2.252 | 12,472 B |

The two-voice maximum is a host scheduling spike; its p50–p99.9 profile is not evidence of steady DSP cost above budget. Focused local Guitar-TECHS DI Play captures at 1, 4, and 8 voices produced RMS differences of 0.03633 from one to four and 0.01165 from four to eight. Focused native/offline/browser checks, full `make test` (117/117 CTest), full `make web-test`, `make windows-cross-build`, `make diff-check`, and the Chorus benchmark passed. These isolated desktop timings do not establish target-device performance. Human listening, physical Fractal comparison, installed firmware, and exact Mark II Turbo applicability remain open.

## Fender Tone Master Pro Tremolo Duty Cycle (2026-11-26)

The Model Guide v1.1.100 / Rev. A, printed p. 34, defines Duty Cycle as the percentage of time signal is on, but publishes no numeric bounds/default or interaction with Shape. TabletFX applies a project-authored 5–95% control, default 50%, only to Square Tremolo; at 50% it produces the exact pre-change `phase < 0.5` waveform. The control is separate from LFO Clip. It adds no fixed latency or processor buffer; native state is 128 B, focused browser/WASM effect state is 104 B, and processing allocates nothing.

`make bench-fender-tremolo-duty-cycle` measured AMD Ryzen 9 5950X Release, 48 kHz stereo, 128-frame blocks (2.667 ms callback deadline; 3,000 warm-up and 20,000 measured callbacks). Percentages are p50/p95/p99/p99.9/max of callback duration divided by deadline.

| Profile | CPU time p50/p95/p99/p99.9/max (µs) | Callback budget p50/p95/p99/p99.9/max (%) | State | Fixed latency / allocations |
|---|---:|---:|---:|---:|
| Square, 50% | 0.450 / 0.740 / 0.870 / 1.110 / 12.960 | 0.017 / 0.028 / 0.033 / 0.042 / 0.486 | 128 B | 0 samples / 0 |
| Square, 20% | 0.400 / 0.460 / 0.470 / 0.960 / 9.550 | 0.015 / 0.017 / 0.018 / 0.036 / 0.358 | 128 B | 0 samples / 0 |
| Sine control | 1.620 / 1.660 / 1.720 / 8.780 / 23.391 | 0.061 / 0.062 / 0.065 / 0.329 / 0.877 | 128 B | 0 samples / 0 |

Focused HeadlessChrome 151 Play on local prerecorded stereo Guitar-TECHS DI verified Square mode at 50% and 20%, with RMS output difference 0.03231. These are isolated desktop callback timings, not full-chain or target-device guarantees. Human listening and installed-device behavior remain open.

## HeadRush Prime Reverb Tails on bypass (2026-11-26)

The Prime User Guide v5.0.0 documents per-effect reverb/delay tails continuing after bypass (printed p. 11 §4.1.3). TabletFX adds project-authored, default-off `Tails on bypass` to its existing Room, Hall, Plate, and Spring Reverbs. When enabled, the reverb network advances with zero new excitation and adds only stored wet tail over the unchanged dry path. It reuses existing delay state, adds no fixed latency or delay storage, and allocates nothing during processing. The v5.0.0 Eleven Reverb screen lists Decay, Tone, Pre-Delay, Mix, and Tails, but the pictured values are examples rather than ranges/defaults. The v5.2.0 guide could not be read in this pass; installed firmware is unknown. No Eleven algorithm, DSP, or settings parity is claimed.

`make bench` measured active processing and the bypass-tail drain sequence on AMD Ryzen 9 5950X Release, 48 kHz stereo, 128-frame callbacks (2.667 ms deadline). Each drain case follows 96 active excitation blocks with zero-input bypass blocks. Percentages are processor callback time divided by the deadline; values are p50/p95/p99/p99.9/max.

| Reverb | Active CPU (%) | Tail-drain CPU (%) | Owned state (active = drain) | Drain aggregate RMS |
|---|---:|---:|---:|---:|
| Room | 0.1418 / 0.1436 / 0.2213 / 0.4478 / 2.3828 | 0.1414 / 0.1433 / 0.2359 / 0.4223 / 1.2390 | 115,696 B | 0.0084 |
| Hall | 0.1913 / 0.2591 / 0.3236 / 0.8614 / 2.3168 | 0.3083 / 0.3863 / 0.5753 / 0.7613 / 2.1417 | 193,764 B | 0.0090 |
| Plate | 0.4046 / 0.5325 / 0.6769 / 0.8198 / 0.9657 | 0.8659 / 0.9038 / 1.1828 / 1.7749 / 5.3259 | 147,496 B | 0.0004 |
| Spring | 0.7107 / 1.0002 / 1.0613 / 1.4858 / 2.8396 | 0.9698 / 1.4269 / 1.6122 / 1.7817 / 3.0856 | 51,700 B | 0.0031 |

The one-profile maxima are desktop processor-call measurements, not full-chain or target-device bounds. Browser tail-output verification uses a local prerecorded guitar DI, and native tests cover all four reverbs; real-device behavior and human listening remain open.

## Fender Tone Master Pro Stereo Flanger L/R Phase (2026-11-26)

Fender's Model Guide v1.1.100 / Rev. A, printed p. 38, lists L/R Phase on Stereo Flanger and describes its left/right phasing role, but publishes no numeric range/default. TabletFX adds a project-authored 0–360° right-channel LFO offset, default 180° to preserve the existing opposing modulation. It follows the Flanger's 10 ms parameter ramp, adds no delay buffer or fixed latency, and keeps process() allocation-free. Native stereo state is 4,816 B; the focused WebAssembly effect reports 4,784 B at 48 kHz. The browser full chain continues to pass native/WASM comparison.

The isolated Flanger benchmark on AMD Ryzen 9 5950X Release at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up/5,000 measured blocks) reports p50/p95/p99/p99.9/max callback use of 0.0533/0.0536/0.0536/0.2595/0.7339%. These are processor-call timings, not a complete-chain or target-device guarantee. HeadlessChrome 151 Play with local stereo prerecorded Guitar-TECHS DI captured 48,000 frames at 0° and 180°: output difference RMS 0.060924, 47,809 samples changed, no extra stage or buffer. Native, CLI, browser/WASM, and Windows MinGW checks pass. Manual listening and device behavior remain open; these values are project measurements, not Fender performance claims.

## HeadRush Prime Drop Tune project workflow (2026-11-26)

The project-authored Drop Tune selector reuses the existing Pitch picker ID 22 `PolyPitchShifter`: whole-note downward offsets −1…−12, Mix locked to 100%, and no extra processor instance or DSP allocation. It shifts the entire signal uniformly; it does not model string-specific tunings or Prime's Legacy/Refined algorithms. At 48 kHz stereo the existing shifter has 256 samples / 5.333 ms algorithmic latency. Its measured native state is 496,080 B and focused browser effect state is 491,472 B; Drop Tune adds no state beyond that one shifter. The full wet output retains the shifter latency.

On AMD Ryzen 9 5950X Release at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks), the dedicated −12-semitone/full-wet profile used 10.1545/13.2390/16.0808/19.6764/34.8081% at p50/p95/p99/p99.9/max. This is isolated processor-call timing, not a complete-chain or target-device estimate. The focused HeadlessChrome 151 Play smoke used a local prerecorded guitar DI (48,000 stereo frames), checked integer −1…−12 bounds and locked wet mix, and compared two captures: measured browser state 491,472 B, zero extra shifter instances, and difference RMS 0.0594638 between −1 and −12 output. `make test`, the focused browser test, and the Windows MinGW core/offline-renderer build pass. These measurements describe TabletFX, not HeadRush performance. Manual guitar listening, target-device timing, and per-preset/scene persistence remain open.

## Fractal FM9-family Formant Control LFO (2026-11-26)

The existing project-authored Formant Filter (Filters & wah, picker ID 43) now has a default-off LFO source for its three-vowel Control morph. The project choices are Sine/Triangle/Square, 0.05–8 Hz with 0.5 Hz default, and a 0–100% Control range with 0/100% endpoints. Control coefficients update every 32 frames and retain the existing 10 ms smoothing. The processor adds no audio buffer or fixed latency; its fixed native state is 680 B (the focused browser reports 664 B), and native tests verify no process allocation.

On AMD Ryzen 9 5950X Release at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks), the manual profile uses 0.1125/0.1721/0.1946/0.5880/1.6020% and the active LFO profile uses 0.1613/0.2318/0.2985/0.6079/1.3935% at p50/p95/p99/p99.9/max. The LFO path costs 0.0488 percentage points at p50 and 0.1039 at p99 in this isolated test; both profiles remain below 1% at p99 on this desktop. This is processor-call timing, not an Android or complete-chain guarantee.

Native tests, the focused categorized browser/WASM Play smoke using local prerecorded Guitar-TECHS DI, and the Windows MinGW core/offline build pass. The official November 2026 Blocks Guide permits Formant Control assignment to LFO/expression/modifier; the FM9 Owner’s Manual v3.x (2022-08-17 per downloads page) documents LFO controls but not this project mapping. The project’s LFO values, bounds, defaults, and coefficient-update interval are independent choices. Human listening and device checks remain open; no Fractal settings, DSP, or sound parity is claimed.

## Fractal FM9-family Dual/Quad Diatonic Harmony (2026-11-25)

The November 2026 Blocks Guide (printed pp. 76 and 80) describes Dual Diatonic (2 voices), Quad Diatonic (4), and Quad Diatonic Delay (4) as intelligent monophonic shifters. TabletFX's project-authored Pitch effect at index 115 uses a single-note YIN-like tracker to drive two or four generated voices; the voice count does not mean chord tracking. Its STFT shifter adds 256 frames / 5.333 ms fixed audio latency at 48 kHz. Conservative tracker-plus-shifter control-response bounds are 21.333 ms for 220 Hz and 29.333 ms for low E (82.407 Hz), already above the provisional ≤20 ms target before host, browser, device and output buffering. On the existing prerecorded EGFxSet guitar DI, the tested takes accepted notes after 1,408 / 2,688 / 1,536 samples (34.667 / 61.333 / 37.333 ms); matched note counts and cent errors are tracker test results, not a sound-quality or latency claim. The measured response gate is therefore **not met**.

The processor uses 728,984 B native stereo state without voice delays; two 2-second delay lines raise it to 1,496,992 B, and four raise it to 2,265,000 B. The browser creates the processor only when enabled; its focused no-delay smoke reports 724,036 B after enable. Processing itself has no heap allocation. A full browser/native comparison with local prerecorded Guitar-TECHS DI passed for 79 optional processors (maximum absolute error 1.6867683e−6; RMS error 2.9085876e−7). The browser reports 81,115,396 B processor state plus 10,364 B tuner state within 100,663,296 B fixed WASM memory for that full chain. These full-chain totals are not the incremental memory cost of this effect.

An isolated Release benchmark on an AMD Ryzen 9 5950X at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks per profile) measured the following project processor callback load. Inputs are synthetic steady 220 Hz and 82.407 Hz tones; these timings are not Android/device or complete-chain results.

| Profile | Native memory | Response bound | p50 / p95 / p99 / p99.9 / max callback use | Deadline misses |
|---|---:|---:|---:|---:|
| Dual, 220 Hz, no delay | 728,984 B | 21.333 ms | 12.768 / 16.511 / 18.991 / 22.640 / 34.109% | 0/5,000 |
| Dual, low E, no delay | 728,984 B | 29.333 ms | 14.382 / 18.588 / 21.365 / 26.137 / 32.306% | 0/5,000 |
| Quad, 220 Hz, no delay | 728,984 B | 21.333 ms | 22.099 / 28.010 / 31.360 / 39.018 / 63.458% | 0/5,000 |
| Quad, low E, no delay | 728,984 B | 29.333 ms | 24.088 / 30.562 / 34.355 / 41.665 / 63.661% | 0/5,000 |
| Dual, 220 Hz, two 2-second delays | 1,496,992 B | 21.333 ms | 12.465 / 15.746 / 17.827 / 22.602 / 30.293% | 0/5,000 |
| Quad, 220 Hz, four 2-second delays | 2,265,000 B | 21.333 ms | 21.349 / 27.051 / 30.699 / 38.103 / 49.026% | 0/5,000 |
| Dual, low E, two 2-second delays | 1,496,992 B | 29.333 ms | 14.448 / 18.711 / 21.929 / 29.585 / 46.133% | 0/5,000 |
| Quad, low E, four 2-second delays | 2,265,000 B | 29.333 ms | 23.088 / 29.862 / 34.084 / 41.589 / 110.787% | 1/5,000 |

CPU columns are percent of one 2.667 ms callback deadline. The Quad/low-E/four-delay profile had a single deadline overrun; its maximum is not hidden by its percentile values. `make test`, full `make web-test`, and `make windows-cross-build` pass. Human listening, real playing, Android device measurements, and physical FM9 validation remain open. This project implementation makes no Fractal DSP/settings/sound parity claim. The November 2026 guide is FM9-family evidence; firmware v12.0 lists FM9 Original/Turbo but not Mark II Turbo by name, and installed firmware is unknown.

## Line 6 Helix Stadium XL seven-band Parametric EQ-inspired project effect (2026-11-25)

The project-authored Equalizer row at picker ID 114 uses seven independently configured second-order filter sections with fixed per-channel state. Coefficients are prepared when controls change, outside audio processing. It owns **944 B**, adds **zero fixed algorithmic latency**, and allocates nothing during `process()`. The worst-case benchmark enables one low cut and six high cuts at the project's maximum 36 dB/oct slope.

On an AMD Ryzen 9 5950X Release build at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks), `make bench` recorded p50/p95/p99/p99.9/max callback use of **0.7924/0.8295/1.0114/1.2855/1.7550%**. This is isolated desktop processor-call timing, not whole-chain or target-device performance. Native DSP and the local prerecorded guitar-DI browser/WASM path pass automated checks, including the Windows MinGW cross-build. Human listening and device behavior remain unverified; these project measurements do not establish Line 6 DSP/settings parity.

## Fractal FM9-family Volume/Pan utility (2026-11-25)

The existing project-authored Utility stage is picker ID 93. It performs volume, taper, balance, stereo input selection, independent L/R pan, Output Level, and pass/mute bypass; Auto-Swell remains a separate effect. All ranges, defaults, curve approximations, and routing are project choices. The utility owns **80 B**, adds **zero fixed latency**, and makes no process-time allocation. The focused local guitar-DI Play test verifies audible Utility-row output.

On an AMD Ryzen 9 5950X Release build at 48 kHz stereo/128-frame blocks (2.667 ms callback deadline; 2,000 warm-up and 5,000 measured blocks), `make bench` reports p50/p95/p99/p99.9/max at **0.0180/0.0180/0.0382/0.3428/1.2195%** of the deadline. This is isolated processor-call timing, not whole-chain or target-device performance. Human listening and physical Fractal behavior remain open; the November 2026 FM9-family guide does not publish numeric bounds/defaults or curve laws, and v12.0 does not name Mark II Turbo explicitly.

## Fractal FM9-family Resonator Input Gain (2026-11-25)

The existing Four-band Resonator at picker ID 49 now has project-authored Input Gain. It defaults to 0 dB with a project-selected −24…+12 dB range; the June 2023 Blocks Guide, printed p. 81, documents only the wet-input role and unchanged dry signal, not numeric bounds/defaults. The cached gain scales samples before the resonator's band routing and delay excitation. Dry signal and the existing feedback return are not multiplied. The gain control adds no processor stage, fixed algorithmic latency, or process-time allocation; total processor state is 31,144 B at 48 kHz stereo.

An AMD Ryzen 9 5950X Release benchmark at 48 kHz stereo/128-frame blocks (2.667 ms callback deadline, 2,000 warm-up and 5,000 measured blocks) recorded unity p50/p95/p99/p99.9/max at **0.3484/0.5359/0.7466/1.9940/3.0263%** of the deadline; +12 dB measured **0.4283/0.5625/0.7350/1.8207/2.6288%**. This is isolated processor-call timing on a desktop, not whole-chain or target-device performance. The focused browser/WASM smoke played local prerecorded guitar DI at 0 and +6 dB, with RMS difference 0.0103064 and peak difference 0.0429585 over 95,706 active samples. Full native tests and the Windows MinGW core/offline build pass. Human listening, physical FM9 behavior, exact Mark II Turbo firmware applicability, and device timing remain open. The project control is in-chain; it does not reproduce any preset-global/output-bus assignment. No Fractal settings, DSP, or sound parity is claimed.

## Fender Tone Master Pro Pitch Sequencer coverage benchmark (2026-11-25)

The existing project-authored Pitch Sequence at Pitch picker ID 74 combines programmed Pitch, Glitch, and sample-loop Freeze steps. Its project-reported pitch-shifter latency is **256 frames / 5.333 ms at 48 kHz**; it adds no further sequence buffer. Native owned state is **542,480 B** at 48 kHz stereo. In `make bench`, a three-step mixed Pitch/Glitch/Freeze sequence at 48 kHz stereo/128 frames (2.667 ms callback deadline; 2,000 warm-up and 5,000 measured blocks) used **13.7265/17.0997/20.0034/25.8018/34.1402%** of the callback at p50/p95/p99/p99.9/max. This is isolated processor-call timing on an AMD Ryzen 9 5950X Release build, not a mobile or whole-chain guarantee. The random-noise benchmark input is for repeatable load coverage; the browser Play test separately uses local prerecorded guitar DI. Fender documents the effect's broad creative behaviors but publishes neither its parameter table nor DSP/latency, so these figures describe only the project implementation.

## Fractal FM9 family — four-voice chromatic harmonizer (2026-11-25)

TabletFX's project-authored Pitch row at picker ID 112 uses four fixed-interval STFT pitch voices. The asymmetric 2048/128 analysis/synthesis path reports 256 frames of algorithmic latency, or **5.333 ms at 48 kHz**. This does not include audio-device, browser, or output buffering. The processor has no process-time allocation. Lazy state accounting is 675,480 B native with delay off and 670,556 B for the browser/WASM effect; requesting all four two-second voice delay lines raises native state to 2,211,496 B. The first enable allocates and prewarms the processor in the worklet control handler; target-device first-use timing is unmeasured.

On an AMD Ryzen 9 5950X Release build at 48 kHz stereo/128-frame blocks (2.667 ms callback deadline; 2,000 warm-up, 5,000 measured), `make bench` reports p50/p95/p99/p99.9/max at **19.8103/26.3085/30.3267/35.6049/39.2801%** of the callback deadline. This is isolated processor-call timing, not a whole-chain load or mobile-device guarantee. Full native tests, focused browser/WASM Play with local prerecorded guitar DI, full browser suite, and Windows MinGW core/offline build pass. Human listening, physical FM9 validation, firmware applicability to Mark II Turbo, and target-device real-time behavior remain open.

## Line 6 Helix Stadium Mutant Filter-inspired effect (2026-11-25)

TabletFX's project-authored Mutant Filter row reuses the existing sample-by-sample envelope/TPT filter core. It has no block buffer, added fixed algorithmic latency, or processing-time allocation; reported native object state is 624 B. In an AMD Ryzen 9 5950X Release benchmark at 48 kHz stereo with 128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks), `make bench` recorded p50/p95/p99/p99.9/max at **0.1073/0.1211/0.1339/0.4118/0.9383%** of the callback deadline. This is processor-call timing on that desktop, not a live device guarantee. The browser/WASM test plays a local prerecorded guitar-DI fixture and verifies distinct Low-pass, Band-pass, and High-pass output. Human listening and target-device checks remain open. Stadium's v1.4 catalogue identifies Mutant Filter as Mono/Stereo, based on Mu-Tron III and first appearing in firmware 1.0, but the checked manual does not publish its exact parameters, DSP cost, or latency; these project metrics do not establish Stadium parity.

## Fractal Touch-Wah / Touch-Ladder physical detector source (2026-11-25)

The existing Stereo Filter at picker index 96 can detect its Touch-Wah or Touch-Ladder envelope from either the processed block input or the original chain input. The physical-input option passes a read-only view of the already-available raw input; it adds no audio copy, buffer, processor stage, or fixed algorithmic latency. The control value adds 8 B to the filter object's reported state (38,968 B total at 48 kHz stereo). This is the project's routing interpretation of the family guide, not a separate physical jack or Fractal routing emulation.

`make bench` on AMD Ryzen 9 5950X, Release, 48 kHz stereo/128-frame blocks (2.667 ms deadline), 2,000 warm-up and 5,000 measured blocks, recorded p50/p95/p99/p99.9/max at **0.3023/0.4178/0.5666/1.4787/4.6944%** of the deadline. This is isolated native processor-call timing; the peak is a scheduler/measurement outlier candidate and device performance is unknown. Focused native/browser tests verify a measurable output difference between Block and Physical sources with local prerecorded guitar; full browser/native parity and Windows cross-build pass. No human listening or target-device latency data is available.

## Related Melofty AXOM MAX Filtered Delay controls (2026-11-25)

The AXOM app V1.9.2 notes mention 1/64–1/1 divisions and High/Low Cut but do not identify the model or control ranges. TabletFX's existing Filtered Delay already offers tempo sync and repeat-path cuts; the new tests validate that path and add no extra processor or memory. The `make bench` Filtered Delay row reports existing processor use of 768,864 B and p50/p95/p99/p99.9/max of 0.1365/0.2715/0.3323/0.6270/3.1332% of a 2.667 ms callback. These figures characterize TabletFX's effect, not Melofty hardware or an incremental change from the controls.

## Line 6 Stadium XL 1 Switch Looper controller profile (2026-11-25)

The one-switch controller profile drives the existing Deluxe Looper; it adds no audio processor stage, recording buffer, or signal latency. The profile uses project UI timing (280 ms double press, 1 s hold). The audio callback and looper memory are those of Deluxe Looper and were not remeasured specifically for the alternate controller profile. Browser interaction tests cover transport and undo/redo/clear states. Physical Stadium footswitch behavior remains a manual check.

## Fractal FM9 Touch-Wah MIX interpretation (2026-11-24)

The November 2026 Fractal Blocks Guide (p. 46) lists Touch-Wah modes LOWPASS, BANDPASS, HIGHPASS, and MIX, but gives no MIX topology, amount range, or default. TabletFX defines MIX as a linear dry/band-pass crossfade with a project-selected 50% default, a 0–100% range, and a 10 ms blend ramp. It uses the existing Touch-Wah filter, adds one scalar parameter/state value and no buffer, delay, or fixed latency; shared filter state remains 38,960 B at 48 kHz stereo. This is not Fractal DSP or control parity.

On AMD Ryzen 9 5950X, GCC 13.3 Release, 48 kHz stereo/128-frame (2.667 ms) blocks, 2,000 warm-up and 5,000 measured blocks, the isolated processor measured p50/p95/p99/p99.9/max **0.3064/0.3195/0.4174/0.5363/1.1738%** of the callback deadline. This is desktop processor-call timing, not full-chain or target-device load. The processor adds no fixed algorithmic delay. Human listening and target-device/round-trip checks remain open.

## Fender Tone Master Pro Pedal Detune interpretation (2026-11-24)

Pedal Detune at Pitch picker index 110 reuses the existing `PolyDetune` core. The project defaults to Shallow, 50% POSITION, a 0.25 Hz modulation rate, two voices, 75% width, and 0 dB level; project depths are 0.35 ms in Shallow and 1.4 ms in Deep. These mappings are not published Fender settings. The dry signal is immediate and the wet micro-delay is about 5 ms. At 48 kHz stereo the processor owns 3,672 B natively and the browser reports 3,632 B. The native test confirms no processing-time allocations.

`make bench` on AMD Ryzen 9 5950X, Release, 48 kHz stereo/128-frame blocks (2.667 ms callback), with 2,000 warm-up and 5,000 measured blocks, measured p50/p95/p99/p99.9/max at **0.0563/0.0570/0.0836/0.2460/0.6330%** of the callback deadline. This is an isolated processor-call estimate on that desktop, not full-chain or target-device performance. Focused browser/WASM Play used the local prerecorded Guitar-TECHS DI at 48 kHz stereo/48,000 frames; dry-to-Shallow difference RMS was 0.02655, Shallow-to-Deep was 0.09162, and full POSITION to zero-POSITION was 0.09069. Peak output remained below 0.241. Full chain parity, listening, and hardware checks remain open.

## Fractal-family Diatonic custom-scale extension (2026-11-24)

The existing Diatonic Harmonizer now supports a project-authored custom scale of 4–8 ascending semitone offsets in the native processor, offline CLI, and categorized Pitch browser/WASM control at picker index 50. It reuses the same one-voice processor and audio path; no separate block, voice, sample buffer, or fixed latency was added. Native owned state is 1,024,664 B in stereo (the focused browser reports 1,020,012 B effect memory). Processing remains allocation-free. The shared shifter contributes 5.333 ms nominal buffering at 48 kHz, while the tracker frame plus shifter gives an estimated 26.7 ms first-note response before host/device buffers; this exceeds the provisional ≤20 ms live-transposition target.

The focused local prerecorded Guitar-TECHS DI browser/WASM Play check verified the Pitch category, index 50, a four-note custom scale [0, 2, 5, 7], and captured output change (peak 0.314677, difference RMS 0.0705676, 93,944 changed samples). `make test` passes 111/111 CTest tests and 11/11 audio-fixture safety checks; Windows MinGW core/offline build passes. The custom-mode test does not validate guitar note accuracy or sound quality.

An AMD Ryzen 9 5950X, GCC 13.3.0, Release `make bench` run at 48 kHz stereo/128 frames (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) measured custom mode at p50/p95/p99/p99.9/max 15.9815/19.8729/24.5211/29.4923/34.2197% of the callback deadline. The same-run 13-preset C-major configuration measured 15.9001/21.1528/26.7656/33.0583/46.7383%. These are isolated native processor-call timings; they do not predict Android, Windows runtime, or other device performance. Human listening, chord/transient behavior, and target-device latency remain open.

## Existing polyphonic pitch shifter — Fractal-range profile (2026-11-24)

The browser Pitch row (index 22) and offline-renderer pitch stage now configure the existing `PolyPitchShifter` with a ±24-semitone limit; neutral remains 0 semitones. Other `PolyPitchShifter` instances retain their existing per-instance profiles. A wider limit creates no second engine: it adds no processor state or extra fixed latency. The shifter itself has 5.333 ms algorithmic latency at 48 kHz, owns 494,016 B in the native stereo benchmark, and reports 489,416 B for the focused WASM effect.

The June 2023 FM9-family Blocks Guide documents up to ±24 semitones per voice for Dual/Quad Chromatic types. The project still has one global shift control, not independent two/four-voice routing or Fractal algorithm parity. A Ryzen 9 5950X Release benchmark with +24 semitones at 48 kHz stereo/128-frame blocks (2.667 ms deadline; 2,000 warm-up, 5,000 measured) recorded p50/p95/p99/p99.9/max at 10.2614/13.7768/17.9694/26.8763/40.8795% of the callback deadline. This is an isolated desktop processor-call measure; the p99.9 and maximum tail, whole-chain load, and mobile hardware performance still warrant caution. Native tests check ±24 endpoint tones and chords. Focused browser Play on local prerecorded Guitar-TECHS DI compares captured ±12 and ±24 outputs for both polarities; Windows MinGW build passes. Human listening and target-device checks remain open.

## Project-authored Line 6 Helix Stadium Poly Detune-inspired Modulation (2026-11-24)

The project effect is a short modulated-delay widener, not a note tracker or static pitch shifter. Its direct path is immediate; the wet path uses up to two modulated taps around a 5 ms base delay. Defaults are two taps, 0.25 Hz, 0.75 ms depth, 0.75 width, 25% mix, and 0 dB level. Project bounds and smoothing are not Line 6 controls. The effect uses one preallocated shared ring, does not evaluate trigonometric functions per sample, and allocates nothing in `process()`. At 48 kHz stereo it owns 3,656 B natively; the browser reports 3,620 B of effect memory. This is a project-authored approximation, not Line 6 DSP or sound parity.

An isolated AMD Ryzen 9 5950X Release benchmark at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max at 0.0548/0.0818/0.0862/0.3075/0.6544% of the callback deadline. This is processor-call timing on that desktop, not whole-chain, WebAssembly-device, or hardware performance. The modulated wet taps are delayed; there is no added fixed delay on the dry path.

Native tests, focused browser/WASM Play with local prerecorded Guitar-TECHS DI, full browser/native comparison, and Windows MinGW core/offline build pass. The focused browser capture used 48,000 stereo frames, with peak 0.1852493, 95,999 active samples, difference RMS 0.0218924, and 3,620 B reported effect memory. Full `make web-test` passes 111/111 CTest cases and 11/11 fixture-safety checks; its 110-entry/78-stage comparison reports maximum absolute error 3.400965e−6 and RMS error 5.880495e−7. Human listening and target-device checks remain open.

## Project-authored Line 6 4 OSC Generator-inspired Synth source (2026-11-24)

The source uses four fixed-phase oscillators with per-voice waveform, static MIDI note and octave controls; the sum replaces input before the downstream chain. Defaults are sine C3/G3/C4/E4 (MIDI 48/55/60/64), 0.15 voice levels, and 0.35 output. At 48 kHz stereo its native owned state is 176 B (the browser reports 168 B); it adds zero fixed algorithmic delay and `process()` allocates no memory. This is project-authored DSP and not Line 6's implementation.

An AMD Ryzen 9 5950X Release benchmark at 48 kHz stereo/128 frames (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max at 0.0709/0.1552/0.1706/0.4751/0.7286% of the callback deadline. These are isolated native processor-call measurements, not whole-chain, WebAssembly-device, or hardware performance.

Native allocation/bounds tests and focused browser/WASM Play using the existing local Guitar-TECHS DI fixture pass. The Play check confirms the Synth category, source off by default, captured output after downstream distortion, 95,998 active samples, peak 0.3548134, difference RMS 0.3102750, and 168 B reported effect memory. Full-chain browser parity intentionally keeps the independent source off. Human listening and target-device checks remain open.

## Project-authored Chorus quarter-note tempo control (2026-11-26)

The existing sine-LFO Chorus now accepts a project-authored Free Hz / quarter-note tempo mode. Free defaults to 0.8 Hz and preserves the 0.05–8 Hz manual rate bounds; quarter-note rate is BPM ÷ 60, with project tempo bounds of 20–300 BPM and a 120 BPM default. The November 2026 Fractal Blocks Guide p. 25 documents the Rate/Tempo relationship and `NONE` for manual control, but publishes no numeric ranges/defaults. Its separate `LFO1 SYNC` control is not implemented. No phase alignment or Fractal DSP/settings/sound parity is claimed.

At 48 kHz stereo, the project Chorus owns 12,456 B natively and reports 12,424 B for the WASM effect. Its dry path has zero fixed algorithmic latency; its wet delay is part of the chorus effect. A native test confirms `process()` allocates nothing in tempo-sync mode. On AMD Ryzen 9 5950X Release with 128-frame callbacks, 3,000 warm-up and 20,000 measured callbacks, Free 0.8 Hz p50/p95/p99/p99.9/max was 0.043/0.066/0.072/0.270/1.030% of the 2.667 ms callback deadline. Quarter-note/120 BPM was 0.043/0.069/0.073/0.264/0.510%. These are isolated desktop processor-call measurements, not target-device performance.

Native tests cover 120/60/20/300 BPM mapping, rate bounds/default preservation, audible sample differences, and zero allocation. Offline rendering distinguishes Free/120/60 BPM. Chromium 151 browser/WASM Play on the existing local stereo guitar-DI fixture captured all three modes; peaks were 0.1894/0.2084/0.1958 and RMS difference between Free→120 and 120→60 was 0.0573/0.0553. Full browser/native comparison across 71 active processors passed. Windows MinGW core and offline-renderer builds pass. Listening, target-device timing, and physical hardware comparison remain open.

## Project-authored Fender Uni-Verse Vibe-inspired Modulation (2026-11-24)

The project implementation uses two first-order all-pass stages, a swept phase coefficient, and a 10 ms control ramp. Defaults are 1.5 Hz, 65% intensity, unity linear Volume, Modern tone, and Chorus. Project bounds are 0.1–10 Hz, 0–100% intensity, and 0–2 linear Volume; local tempo sync uses 20–300 BPM and 12 project-chosen divisions. Vintage adds a project-selected 6 kHz one-pole low-pass. Vibrato selects the project wet-only phase path. These DSP choices are independent of Fender's documented control roles and do not claim algorithm or sound parity. The processor adds zero fixed algorithmic delay, owns 240 B of native state (224 B reported for the WASM effect), and allocates nothing during processing.

On an AMD Ryzen 9 5950X Release build at 48 kHz stereo with 128-frame callbacks (2.667 ms deadline, 2,000 warm-up and 5,000 measured blocks), the isolated processor recorded p50/p95/p99/p99.9/max of 0.0964/0.1032/0.1418/1.0774/2.8602% of callback deadline. This is desktop processor-call timing, not whole-chain or target-device load.

`make test`, focused browser/WASM Play with the local prerecorded Guitar-TECHS DI fixture, and the full `make web-test` pass. The full comparison has 108 picker entries and 78 optional active processors; maximum absolute native/browser error is 3.400965e−6 and RMS error is 5.880495e−7. Human listening and device-route verification remain open.

## Project-authored Fender Vibratone-inspired Modulation (2026-11-24)

The project effect reuses the Rotary Speaker processor with a project-selected single-pole output Tone filter and envelope-controlled Fast/Slow switching. Defaults: Drive 0%, Tone 80%, Level 0 dB, Fast 6.5 Hz, Slow 0.85 Hz, Speed Switch Slow, Ramp 650 ms, Sensitivity 50%, Dynamics off. Project ranges: Drive 0–100%, Tone mapped to 800 Hz–20 kHz, Level −24…+12 dB, Fast 1–10 Hz, Slow 0.1–5 Hz capped at half Fast, Ramp 50–3,000 ms, and Sensitivity mapped to a −5…−45 dB threshold. Dynamics uses project-selected 20 ms attack, 250 ms release, and 0.75 hysteresis. These are not Fender settings, topology, or sound parity. The product guide lists controls but no numerical bounds, dynamics behavior, rotor geometry, DSP, or latency.

At 48 kHz stereo, the native processor owns 1,304 B and the WASM chain reports 1,264 B effect memory. It adds zero fixed algorithmic latency and allocates no memory in `process()`. On an AMD Ryzen 9 5950X Release build, 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max of 0.4298/0.5963/0.8708/1.5859/1.9392% of the callback deadline. This is desktop processor-call timing, not tablet/device performance.

`make test` passes 110/110 CTest cases plus 11 fixture-safety tests. Focused browser Play uses local prerecorded Guitar-TECHS DI and verifies a Modulation-category effect at picker index 106, audible output change, and 1,264 B effect memory. Full `make web-test` passes the 107-entry picker and 78-stage native/browser comparison (max absolute error 3.400965e−6; RMS error 5.880495e−7); processor state is 80,341,868 B plus 10,364 B tuner state within fixed 100,663,296 B WASM memory. Windows MinGW core and offline renderer build pass. Native tests cover parameter bounds, manual/dynamics switching, envelope switching, tone/level behavior, reset, irregular blocks, finite output, and zero process-time allocation. No human listening or physical-device result is available.

## Project-authored Fractal Bias Trem (2026-11-24)

The effect owns 136 B of native state (the browser/WASM chain reports 132 B) at 48 kHz stereo, has zero fixed algorithmic latency, and allocates no memory in process(). Defaults and ranges are project choices; the curve is a bounded nonlinear modulation shape, not a tube-bias circuit model.

An isolated Release benchmark on an AMD Ryzen 9 5950X at 48 kHz stereo/128-frame blocks (2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max of 0.0570/0.1118/0.1256/0.7474/1.7708% of the deadline. This is desktop processor-call timing, not a tablet/device guarantee. Native test coverage checks bounds, waveform choices, trigger, stereo behavior, reset, finite output, irregular blocks, and no allocations. Focused and full browser/WASM tests exercise local prerecorded Guitar-TECHS DI and native/browser comparison; Windows MinGW core/offline build passes. No human listening or target-device result is available.


## Project-authored Fractal Optical / Neon Tremolo (2026-11-24)

At 48 kHz stereo the fixed processor object owns 216 B. Its process path has no fixed algorithmic latency and performs no allocation. A Ryzen 9 5950X Release benchmark with 128-frame blocks (2.667 ms callback deadline; 2,000 warm-up and 5,000 measured blocks) recorded p50/p95/p99/p99.9/max of 0.0743/0.0746/0.1204/0.2895/0.3638% of the deadline. These are isolated desktop processor-call timings, not Android, Windows, or end-to-end device measurements.

Native tests cover defaults/bounds, three distinct variants, all ten LFO mappings, deterministic random reset, tempo/phase/mix, stereo behavior, trigger, finite recovery, irregular callback sizes, and zero allocation. Offline CLI, local prerecorded Guitar-TECHS DI browser Play smoke, and full browser/native parity pass; the comparison's max absolute error is 6.2784e-6 and RMS error 1.2600e-6 across 76 optional processors. The chain reports 80,340,048 B processor state plus 10,364 B tuner state within fixed 100,663,296 B WASM memory. Windows MinGW core and offline renderer build pass. Optical response curves, LFO mappings, Neon pulse width, ranges, defaults, tempo/trigger/ducking behavior are project-authored. The Fractal guide does not publish exact Optical response curves or numeric ranges. No Fractal DSP/sound parity, human listening, or device test is claimed.

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

## XRecer Global EQ in-chain approximation (2026-11-25)

The project-authored Equalizer effect at picker ID 113 reuses the fixed five-band biquad core. It offers Stereo/Left/Right assignment, Peak/High-pass/Low-pass/High-shelf/Low-shelf types, per-band gain, frequency, Q, and bypass, plus master bypass/reset. The XR1 User Manual V1.1.0 names these control roles but publishes no numeric ranges/defaults or filter topology. Project choices are ±12 dB, 20–20,000 Hz capped at 0.45 × sample rate, Q 0.25–10, with neutral peaking defaults at 80/250/750/2,200/6,600 Hz. It is one signal-chain slot; selected-output routing across presets and the XR1 dry-DI bypass are outside its scope.

The processor owns 1,680 B, adds zero fixed algorithmic latency, and allocates nothing in `process()`. On an AMD Ryzen 9 5950X Release build at 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 5,000 warm-up/50,000 measured), neutral/default p50/p95/p99/p99.9/max is 0.001/0.001/0.001/0.001/0.296% of the callback; active five-band is 0.112/0.117/0.186/0.388/0.888%. This is isolated processor-call timing on one desktop CPU, not XR1 DSP or target-device performance. Native tests, full browser/WASM with prerecorded local guitar Play, and Windows MinGW cross-build pass. Human listening and hardware checks remain open. [XR1 User Manual](https://wiki.xrecer.com/docs/global-eq/index.html) · [XR1 FAQ](https://www.xrecer.com/pages/faq)

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


## Initial DSP verification (Stereo Enhancer and 64-entry picker, 2026-11-22)

The project-authored Stereo Enhancer is integrated in the Stereo & image category. At 48 kHz stereo it allocates its delay ring at construction and process() performs no allocation. Modern and Stereoizer add no fixed latency; Classic intentionally delays the right channel by 0–20 ms while the direct path remains immediate. Mono buffers remain mono. The isolated Linux CPU cost and a human-listening/device result have not been measured. make web-test passed 68 CTest cases, offline CLI checks, and HeadlessChrome 151 Play/native comparison with Enhancer bypassed; the enabled offline CLI test confirms a changed stereo result. Browser processor memory is 12,936,760 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. Numerical parity does not establish sound quality, mono-to-stereo behavior, or Fractal parity.

## Fractal FM9-family Enhancer verification (2026-11-25)

The current November 2026 Blocks Guide, p. 44, documents Modern, Classic, and Stereoizer and says Modern can convert mono to stereo. TabletFX's browser host duplicates mono input when Modern is enabled; a 12,000-frame local Guitar-TECHS DI Play smoke produced two-channel output with peak 0.299382 and mean left/right difference 0.057090. The core in-place mono processor remains pass-through, and native/live host routing is unverified. The project-authored stereo processor uses 4,016 B at 48 kHz and makes no processing-time allocation. On an AMD Ryzen 9 5950X Release build at 48 kHz stereo/128-frame callbacks (2.667 ms deadline), isolated p50/p95/p99/p99.9/max use was Modern 0.0424/0.0551/0.0578/0.2231/0.3683%, Classic 0.0338/0.0338/0.0540/0.2363/0.3038%, and Stereoizer 0.0506/0.0510/0.0656/0.2093/0.3240%. These processor-call timings are desktop estimates, not Android or live-route guarantees. Modern and Stereoizer add no fixed algorithmic latency; Classic's wet right channel uses a 7 ms default delay (0–20 ms), while the dry path is immediate. No Fractal DSP/settings/sound parity, hardware test, or human listening is claimed. Firmware v12.0 is listed for FM9 Original/Turbo, not Mark II Turbo by name; installed firmware is unknown.

## Latest DSP verification (Phrase Looper and 63-entry browser picker, 2026-11-22)

On a Ryzen 9 5950X Release build, the project-authored Shimmer Reverb measured 11.7097% p50 / 18.5503% p95 / 20.0691% p99 / 23.5008% p99.9 / 27.3648% maximum of a 2.667 ms callback at 48 kHz stereo/128 frames, with 647,792 B processor-owned memory. Its direct dry path is immediate; the pitch-return ring holds 128 frames (2.667 ms at 48 kHz) before pitch-shifter processing, which adds separate wet latency. This synthetic processor-call benchmark is not a live-route or listening measurement.

At 48 kHz stereo/128 frames, the project-authored five-band peaking/shelf/cut EQ measured 0.0998% p50 / 0.1005% p95 / 0.2104% p99 / 0.4301% p99.9 / 0.6533% maximum of a 2.667 ms callback and owned 680 B. It has no algorithmic latency; coefficients are prepared when parameters are configured, outside `process()`.

The earlier 0.5-second Convolution Reverb checkpoint used 128-frame partitions and measured 5.1043% p50 / 6.4862% p95 / 18.1629% maximum of a 2.667 ms callback, with 1,549,968 B processor-owned state. That measurement is superseded by the 10-second implementation below.

At 48 kHz stereo/128-frame callbacks, the 10-second maximum-length Convolution Reverb IR owns 15,461,240 B and uses a 1,024-frame partition (21.33 ms fixed algorithmic latency; not measured device round-trip latency). The Ryzen 9 5950X isolated maximum-IR benchmark measured 0.0382% p50 / 79.4286% p95 / 92.2945% p99 / 108.1668% p99.9 / 130.4420% maximum of the 2.667 ms callback. Its periodic bursts exceed the callback deadline, so glitch-free real-time playback is not established. The browser regression loads and replaces a 10-second stereo PCM WAV in the fixed 64 MiB WASM heap, checks independent left/right output and partition latency, and verifies that the Reverb picker category is preserved.

Phrase Looper storage is fixed at 192,000 frames: its two loop/history buffers use 1,536,000 B in mono or 3,072,000 B in stereo, plus object state. It adds no fixed algorithmic latency, and `process()` allocates nothing. Record/overdub/clear snapshot copies are issued through the AudioWorklet message handler between `process()` calls but still run on that worklet thread; no isolated CPU benchmark or glitch test has measured the scheduling impact.

The synthetic 61-stage native/offline stress chain measured 62.8345% p50 / 80.8368% p95 / 98.5455% p99 / 112.0174% p99.9 / 162.0700% maximum and owned 11,284,464 B; its maximum crossed one callback deadline. The 62-stage HeadlessChrome 151 browser/native Play capture, including Phrase Looper and with Note Generator disabled for phase-independent parity, passed with maximum absolute error 0.0000172342 and RMS error 0.00000269834. The browser loaded a local IR into WASM, decoded local input audio, captured processed Play output, and verified a nonempty loop recording; processor memory was 12,936,616 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. The same browser test separately enabled only Note Generator and measured A4 at 440 Hz with RMS 0.452548; no isolated CPU benchmark has measured it. Browser callback timing fell back to millisecond resolution, so its per-processor timing samples are too coarse for dependable CPU estimates. `make audio-regression` remains a separate 61-stage offline run and passed five EGFxSet pickup positions and stereo/mono Guitar-TECHS DI with a deterministic local 4,096-frame IR; all reports showed 61 processors. Bridge, stereo-DI, and mono-DI RMS were 9.43956e-6, 8.07260e-6, and 1.69201e-5. The browser/native and prerecorded output remain very low, so parity and bounded processing do not establish sound quality or useful musical output. Manual listening, real-guitar tracking, hardware-route performance, and round-trip latency remain open.
### Feedback Generator interval modes and 58-stage browser chain (2026-11-25 update)

Feedback Generator now uses one project-authored sine oscillator at one selected interval, driven by the existing monophonic tracker; the selected interval does not add stacked voices. At 48 kHz stereo, its state is 1,028,976 B. The inherited tracker analyzes about 55–1,100 Hz in a 1,024-sample frame; its reported 21.333 ms response bound misses the provisional ≤20 ms target. The oscillator avoids the prior pitch-shifter buffer but does not reduce the note detector's analysis response. On AMD Ryzen 9 5950X Release with 48 kHz stereo/128-frame (2.667 ms) callbacks, 1,000 warm-up and 3,000 measured blocks per profile, isolated callback load was:

| Interval mode | p50 | p95 | p99 | p99.9 | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| −12 semitones, 220 Hz | 10.677% | 14.860% | 18.411% | 22.402% | 57.810% |
| Unison, 220 Hz | 10.694% | 14.502% | 17.643% | 22.002% | 25.141% |
| +12 semitones, 220 Hz | 10.682% | 14.132% | 16.720% | 19.548% | 22.234% |
| +19 semitones, 220 Hz | 10.844% | 13.802% | 17.085% | 19.164% | 24.809% |
| +24 semitones, 220 Hz | 10.763% | 14.043% | 16.865% | 21.814% | 27.553% |
| +28 semitones, 220 Hz | 10.741% | 12.505% | 14.870% | 17.848% | 19.056% |
| +31 semitones, 220 Hz | 10.729% | 13.556% | 16.201% | 19.165% | 20.024% |
| +35 semitones, 220 Hz | 10.722% | 13.504% | 16.641% | 22.377% | 25.435% |
| −12 semitones, low E (82.4 Hz) | 10.755% | 13.538% | 15.862% | 20.368% | 24.541% |
| +35 semitones, low E (82.4 Hz) | 10.680% | 14.402% | 17.387% | 25.169% | 34.222% |

No callback deadline misses occurred in the 3,000 measured blocks per profile. These CPU percentages measure one processor call on this desktop; they do not predict full-chain, mobile, Windows hardware or live-route performance. The callback latency and detector response are separate: the oscillator adds no pitch-shifter buffering, while the tracker still determines first-note response.

The older default Unison measurement used 11.5597% p50 / 15.0252% p95 / 19.0037% p99 / 24.8748% p99.9 / 46.3632% maximum and 1,522,968 B state. It measured the prior implementation and is retained here for historical comparison; do not combine it with the current direct-oscillator figures.

The synthetic full stress chain measured 45.1569% p50 / 56.0070% p95 / 62.2363% p99 / 73.6962% p99.9 / 101.9552% maximum and owned 9,086,704 B; the maximum crossed one callback deadline. The 58-stage HeadlessChrome 151 browser/native Play capture passed with maximum absolute error 0.00002568498 and RMS error 0.00000416202. Local-file decode and processed Play capture passed; browser processor memory was 10,233,752 B plus 10,364 B tuner state in the fixed 16 MiB WASM heap. The all-stages output RMS remained extremely low, so the parity check does not establish useful musical level or sound quality. make audio-regression passed all five EGFxSet pickup-position renders and Guitar-TECHS stereo/mono DI at processors=58; bridge, stereo DI, and mono DI output RMS were about 1.26e-5, 1.00e-5, and 2.13e-5. This validates bounded processing, not useful musical level. Listening, real-guitar tracking, hardware-route performance, and round-trip latency remain open.

### Three-voice tracked synth benchmark (2026-11-25)

The project-authored synth shares one monophonic detector across three generated voices; this is not chord tracking. The detector path differs by Guitar/Bass selection. At 48 kHz, Guitar mode decimates to about 24 kHz and analyzes 512 samples (~21.3 ms); Bass mode analyzes 1,024 samples near 8 kHz (~128 ms) with 32 ms hops. Bass mode's observed response is about 0.1–0.22 s depending on buffering. These are analysis/control-response intervals, not fixed audio-path latency or measured end-to-end response. A fixed 80 ms release is applied to the envelopes.

On Ryzen 9 5950X/GCC 13.3 Release, 48 kHz stereo/128-frame blocks (2.667 ms deadline; 2,000 warm-up, 5,000 measured calls), all three voices active in Pitch + envelope mode measured **Guitar** p50/p95/p99/p99.9/max **11.7641/15.1125/17.0858/20.6417/22.3742%** and **Bass** **11.8406/17.3034/20.9259/27.9432/50.2461%** of the callback deadline. Owned processor state was **1,031,344 B**. Input was deterministic synthetic benchmark data; the Bass-mode peak is a material callback spike and these isolated CPU figures do not predict tablet/device behavior or tracking accuracy.

The browser smoke now plays the local prerecorded Guitar-TECHS DI through three active voices (Sine/Triangle/Square), Pitch + envelope tracking and Bass range at picker ID 78. It changed 95,596 samples with RMS difference 0.44038 versus dry input. `make web-test` also passed the wider native/WASM chain comparison. These establish operation and numeric parity, not useful musical levels, note accuracy, listening quality or performance on hardware. Real-bass input and human listening remain outstanding.

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

## Fender Tone Master Pro Pedal Shifter mode (2026-11-24)

The software Pedal Shifter mode reuses the existing `PolyPitchShifter` at Pitch index 22. Target/position control only changes the shifter's pitch ratio; it adds no second engine, effect state, buffer, processing stage, or fixed algorithmic latency. The underlying stereo processor owns 494,016 B natively and has nominal 5.333 ms buffering at 48 kHz; the focused browser reports 489,416 B for the effect. Native tests exercise repeated expression updates while allocation counting is active; `process()` remains allocation-free. Repeated identical pitch settings now return early without recalculating the shifter factor.

The refreshed `make bench` direct midpoint case uses `set_expression_position(12, 0.5)` (effective +6 semitones). On AMD Ryzen 9 5950X, Release, 48 kHz stereo, 128-frame blocks (2.667 ms callback), with 2,000 warm-up and 5,000 measured blocks, it measured p50/p95/p99/p99.9/max at **11.1318/14.1837/16.5968/22.5467/70.4258%** of the callback deadline and 494,016 B owned state. In the same run, the ordinary +5-semitone shifter measured 11.2316/14.6498/17.4455/25.0946/37.6191%, so the expression mode's typical audio cost is consistent with the same existing engine and does not establish a CPU reduction. The maximum was a large timing outlier; p50/p95 are processor-call estimates, not live-device results. Expression-message setter time, full-chain load, the browser/Windows runtime, and target-device performance are separate. Fender's guide supplies no CPU, memory, or latency figure for comparison. Existing 5.333 ms buffering consumes part of the provisional ≤20 ms live-transpose budget, before host/device buffering; no electrical round-trip or human listening measurement is claimed.

The provisional targets are ≤15 ms electrical round trip for the dry/basic path and ≤20 ms for an advertised live-transpose configuration, with acceptable chord quality. Measure the actual physical loop through the exact host/interface. Record buffer estimate, algorithm-reported delay, adapter/scheduling delay, and physical round trip separately.

For every processor benchmark, report CPU as `100 × processor_elapsed_time / audio_block_deadline`, not as a vague whole-process CPU percentage. Include processor ID/version, backend, CPU/architecture, compiler/build mode, sample rate, block sizes, channel layout, quality settings, warm-up, number of blocks, median, p95, p99, p99.9 and max; collect timings off the callback or into bounded preallocated telemetry. Report whole-chain callback load separately. Include memory after warm-up and over a sustained run.

Pitch assessment must test −12, −7, −6, −5, −2, −1, 0, +1, +2, +5, +6, +7, +12; 100% wet; low/open/barre/power chords, fast/muted attacks, bends, vibrato, harmonics, slides, decays; clean and driven input; transposer before drive; pitch accuracy, latency, transients, artifacts, dropouts, CPU and memory. Synthetic tests do not replace guitar recordings and human listening.

## Fender Tone Master Pro Volume Pedal exact heel mute (2026-11-25)

The existing Utility Volume Pedal now targets digital zero at exactly 0% position, following Fender's v1.3.74 release note. The control-endpoint check is computed during parameter updates, outside `Effect::process()`; the callback loop and object layout are unchanged. A fresh `make bench` Release run on an AMD Ryzen 9 5950X measured the normal pedal processor call (48 kHz stereo, 128 frames / 2.667 ms deadline; 2,000 warm-up and 5,000 measured blocks): p50/p95/p99/p99.9/max **0.0041/0.0041/0.0086/0.0094/0.2790%** of the callback budget, **56 B** owned state, and zero fixed algorithmic latency. This measurement is for the existing processor call, not an incremental cost estimate for the off-callback endpoint check. The exact-zero heel and full-toe browser tests use local prerecorded guitar DI; these desktop timings are not target-device guarantees.

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
| Delay, BBD Classic mono | 0.0814 | 0.0821 | 0.1140 | 0.2801 | 0.3124 | 384,820 |
| Delay, BBD Mod mono | 0.0803 | 0.0814 | 0.1320 | 0.2895 | 0.3116 | 384,820 |
| Delay, BBD Classic stereo | 0.1305 | 0.1350 | 0.2449 | 0.3499 | 0.3671 | 768,824 |
| Delay, BBD Mod stereo | 0.1309 | 0.1354 | 0.2640 | 0.4298 | 0.5550 | 768,824 |
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

## Fender Tone Master Pro Virtual Capo Mix (2026-11-25)

The project-authored Mix control reuses the existing `PolyPitchShifter` at Pitch picker index 22. Mix ranges from 0–100%, defaults to 100% shifted, and blends the shifted signal with the original input. Because the wet engine has a 256-frame nominal delay, the dry path uses one fixed 256-frame stereo history ring. This adds 2,048 B of ring storage and 2,064 B of total native stereo state after object alignment (494,016 B before Mix; 496,080 B with Mix). It adds no further fixed latency, no second shifter, and no process-time allocation. The mono state was not remeasured.

`make test` passed 115/115 CTest cases and 11/11 fixture-safety checks. `make web-build`, the focused `make web-test-fender-pedal-shifter` local Guitar-TECHS DI Play test at 0/50/100% Mix, and `make windows-cross-build` passed. The 50% browser output matched the captured wet/dry mean to 1.51e−9 RMS error and 7.45e−9 maximum error. The full `make web-browser-test` passed after its page deadline was raised to 180 seconds; browser/native comparison across 78 optional stages reported 3.4009653e−6 maximum absolute error and 5.8804948e−7 RMS error.

On AMD Ryzen 9 5950X Release at 48 kHz stereo, 128-frame callbacks (2.667 ms), 3,000 warm-up and 20,000 measured callbacks, full-wet CPU load was p50/p95/p99/p99.9/max 10.339/13.341/16.002/19.865/38.989%; 50% Mix was 10.597/13.989/17.349/21.551/35.216% of callback budget. Mix adds dry-ring reads/writes and output blending; these values measure the full processor, not the incremental Mix-only cost. They are desktop processor-call timings and do not predict Android/tablet performance. Nominal algorithm latency remains 256 frames / 5.333 ms at 48 kHz. Human listening and hardware/device checks remain open; Fender's measured latency, DSP, and sound parity are unknown.

## Current checkpoint: Stadium Shuffling Looper capacity (2026-11-25)

The existing categorized Shuffling Looper keeps a 4 s default and now supports user-selected capacity up to 60 s mono / 30 s stereo at 48 kHz. Maximum-capacity playback was benchmarked after recording a full-duration loop, with 16 slices, 32 sequence steps, active pitch/reverse/repeat/shuffle probabilities, smoothing, sequence drift, and low/high cuts. On an AMD Ryzen 9 5950X Release build, 48 kHz mono/stereo and 128-frame callbacks (2.667 ms deadline), 5,000 measured callbacks plus 1,000 warm-up callbacks yielded:

| Capacity | Effect state | Sample storage | Transition peak (old + replacement) | p50 | p95 | p99 | p99.9 | Max | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 s mono | 11,521,440 B | 11,520,000 B | 12,288,000 B | 0.0881% | 0.1444% | 0.1519% | 0.3559% | 0.4920% | 0/5,000 |
| 30 s stereo | 11,521,440 B | 11,520,000 B | 13,056,000 B | 0.1245% | 0.1256% | 0.2070% | 0.3836% | 0.4436% | 0/5,000 |

The transition sample-storage cap is 16 MiB. `process()` performs no allocation and the looper adds no fixed dry-path latency. The UI keeps 4 s as its default, reports actual accepted/clamped capacity, rejects unsafe truncation/allocation requests, and only resizes after source playback is stopped. Native tests cover limits and transition peaks; browser/WASM playback tests use the local prerecorded guitar fixture; the Windows MinGW cross-build passes. These isolated Linux desktop timings do not predict target-device behavior. Human listening, hardware timing, and live-device checks remain open.

## XRecer XR1-inspired Shuffling Looper beat scheduler (2026-11-25)

The XR1 FAQ says its Quantize aligns recording start and end to a selected beat, but gives no subdivisions, default, tempo-change policy, or firmware-specific controls. TabletFX therefore adds a project-authored Off/Beat selector to the categorized Shuffling Looper. Beat mode uses quarter-note boundaries, a project tempo of 40–300 BPM (120 BPM default), and latches tempo when recording is armed. Waiting for the start/stop edge does not interrupt live input monitoring or add fixed audio-path latency. The scheduler uses constant-size state and allocates nothing from `process()`. At 120 BPM, one beat is 24,000 frames at 48 kHz; requested capture capacity must fit a whole beat within the existing accepted buffer.

On an AMD Ryzen 9 5950X Release build at 48 kHz, 128-frame callbacks (2.667 ms), 1,000 warm-up and 5,000 measured callbacks per profile, the fixed scheduler state is 1,488 B. Maximum-profile effect memory is 11,521,488 B, including 11,520,000 B of sample storage; the old-plus-replacement transition peaks remain 12,288,000 B mono and 13,056,000 B stereo, below the 16 MiB transition cap.

| Channel/profile | p50 | p95 | p99 | p99.9 | Max | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono, quantize Off | 0.0803% | 0.1298% | 0.1759% | 0.3945% | 1.5919% | 0/5,000 |
| Mono, Beat | 0.0836% | 0.1346% | 0.1455% | 0.4234% | 1.5240% | 0/5,000 |
| Stereo, quantize Off | 0.1376% | 0.1391% | 0.2344% | 0.6402% | 2.0322% | 0/5,000 |
| Stereo, Beat | 0.1406% | 0.2276% | 0.2591% | 1.9227% | 2.4706% | 0/5,000 |

`make test`, `make web-build`, `make web-browser-test`, `make windows-cross-build`, and `make bench-shuffling-looper` pass. Browser playback uses the repository's local prerecorded guitar DI. These isolated Linux processor-call timings do not predict Android/tablet load or XR1 performance. Manual listening, target-device timing, and physical footswitch checks remain open; no XR1 DSP/settings/sound parity is claimed.

## Tone Master Pro Studio Delay Envelope pan-mode mapping (2026-11-25)

The existing categorized Stereo Panner now exposes an `Envelope` option that follows input strength and reduces the sine-pan excursion on louder passages. This maps the Tone Master Pro Model Guide v1.1.100 / Rev. A's Studio Delay Envelope Pan Mode description, but remains a standalone project panner; it is not a Studio Delay block, does not pan only wet repeats, and does not claim Fender DSP or sound parity.

`make test` passed documentation validation, 11/11 audio-fixture safety checks, and 117/117 CTest cases, including Envelope response, finite output, fixed state, and no allocation in `process()`. `make web-test-stereo-panner` passed HeadlessChrome 151 WASM playback capture from the local prerecorded stereo Guitar-TECHS DI; measured output peak was 0.242084, L/R difference RMS 0.039952, and the selected mode was Envelope. `make windows-cross-build` passed the MinGW core and offline renderer build.

The isolated Release benchmark used a synthetic stereo guitar-like signal, 3,000 warm-up and 20,000 measured 128-frame callbacks at 48 kHz, on an AMD Ryzen 9 5950X. The callback deadline is 2,666.667 μs. The effect object owns 120 B, allocates nothing during processing, and adds zero algorithmic latency. Percentages are elapsed effect-call time as a share of one callback deadline:

| Mode | p50 | p95 | p99 | p99.9 | Max | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sine | 0.373% | 0.474% | 0.587% | 0.807% | 4.954% | 0/20,000 |
| Envelope | 0.355% | 0.437% | 0.540% | 0.837% | 5.084% | 0/20,000 |

These are isolated desktop estimates, not tablet/Android performance. The broad `make web-browser-test` ran the panner playback check but later failed at the existing tuner response-time assertion (the A4 result was valid; the test's 300 ms limit was exceeded). The focused panner browser test passes. Human listening and live-device timing remain open.
## Initial Manual Through-Zero Flanger checkpoint (2026-11-23)

The project-authored manual through-zero flanger initially delayed both dry and wet paths. At 48 kHz, the declared maximum path latency was 384 frames with the browser 8 ms maximum-time setting or 960 frames at the supported 20 ms maximum. Host/graph compensation was not implemented.

The browser/WASM implementation reserved fixed 20 ms delay vectors. The first checkpoint estimated the 15,392 bytes of ring storage and 15,544-byte wasm32 state from source, rather than a runtime measurement. That estimate predates the Off/Auto/Manual controls and should not be used for the current processor size.

## Current Through-Zero Flanger Off/Auto/Manual checkpoint (2026-11-26)

At 48 kHz stereo the current native processor owns 15,664 B, including two fixed delay rings reserved for 20 ms. Construction allocates the rings; process() performs no heap allocation. Off leaves dry immediate and still delays the wet tap; Auto and Manual delay both paths. The maximum configured wet-path delay is 384 frames at the default 8 ms range and 960 frames at 20 ms. Host/graph latency compensation is not implemented.

On AMD Ryzen 9 5950X Release with 48 kHz stereo/128-frame callbacks (2.667 ms deadline; 2,000 warm-up and 5,000 measured callbacks), Manual mode measured p50/p95/p99/p99.9/max callback use of 0.1219/0.1223/0.1766/0.4208/1.2222%. Auto sine at 1 Hz and full depth measured 0.2321/0.2880/0.3578/0.5813/0.9372%. The Auto path adds 0.1102 percentage points at p50 and 0.1812 points at p99 versus Manual. This is isolated desktop processor timing, not a complete chain, Windows/Android device, or live-route guarantee.

The focused HeadlessChrome 151 browser/WASM Play test used local prerecorded Guitar-TECHS direct-input guitar, 48 kHz stereo and 48,000 frames. The effect remains in Modulation at index 81 with no extra stage. Auto vs Manual output RMS difference was 0.0370055 and Auto vs Off was 0.0314842; Auto peak was 0.165524, with 95,795 samples above 1e−6. `make web-test-fractal-tzf-auto` and `make windows-cross-build` pass, including all 117 CTest cases and the existing browser/native chain comparison. Listening, hardware behavior, firmware-specific comparison, and target-device timing remain manual.

## Fender Tone Master Pro Flanger Tap Division (2026-11-26)

Tempo sync extends the existing Flanger stage and does not add an effect slot, audio buffer, or fixed dry-path latency. The rate from BPM/division is calculated when parameters change, outside `process()`; the callback retains the existing flanger modulation path and performs no heap allocation. Native state is 4,832 B; browser/WASM effect state is 4,796 B.

The sync-specific benchmark profile uses quarter-note division at 120 BPM, 48 kHz stereo, 128-frame callbacks, and a 2.667 ms callback deadline on an AMD Ryzen 9 5950X Release build. It runs 2,000 warm-up and 5,000 measured callbacks. Percentages show elapsed processor-call time as a share of one callback deadline:

| Mode | p50 | p95 | p99 | p99.9 | Max | State | Fixed latency | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Quarter-note sync, 120 BPM | 0.0533% | 0.0795% | 0.0829% | 0.4763% | 2.3255% | 4,832 B | 0 frames | 0/5,000 |

`make test-fender-flanger-tap-division`, `make web-test-fender-flanger-tap-division`, `make windows-cross-build`, and `make bench` pass. Browser Play uses the local prerecorded stereo Guitar-TECHS DI. This isolated desktop processor-call timing does not predict Android/tablet load. Manual listening, physical Tone Master Pro comparison, and target-device timing remain open; no Fender DSP, settings, or sound parity is claimed.

## Fender Tone Master Pro Phaser 90 Tap Division (2026-11-26)

Tempo sync reuses the existing Phaser LFO and adds no effect stage, buffer, or fixed dry-path latency. BPM/division is converted to an LFO rate when parameters change, outside the audio callback. Native processor state remains 288 B (276 B for the browser/WASM effect state), and `process()` performs no heap allocation.

The sync benchmark uses quarter-note division at 120 BPM, 48 kHz stereo, 128-frame callbacks, and a 2.667 ms callback deadline on AMD Ryzen 9 5950X Release. It runs 2,000 warm-up and 5,000 measured callbacks. Percentages show processor-call time as a share of one callback deadline:

| Mode | p50 | p95 | p99 | p99.9 | Max | State | Fixed latency | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Quarter-note sync, 120 BPM | 0.0679% | 0.0990% | 0.1031% | 0.4144% | 1.5439% | 288 B | 0 frames | 0/5,000 |

`make test-fender-phaser-90-tap-division`, `make web-test-fender-phaser-90-tap-division`, `make windows-cross-build`, and `make bench` pass. Browser Play uses the local prerecorded stereo Guitar-TECHS DI. These isolated desktop timings do not predict Android/tablet performance. Manual listening, physical Tone Master Pro comparison, and target-device timing remain open; no Fender DSP, settings, or sound parity is claimed.

## Fender Tone Master Pro Tremolo LFO Clip (2026-11-26)

LFO clipping is applied to the existing normalized oscillator value before tremolo gain is calculated. Zero percent bypasses the clipping math and preserves the previous output exactly. At nonzero amounts, the project maps the selected percentage to a symmetric threshold from 1.0 to 0.5, clamps the bipolar LFO, and normalizes it back to its full range. The clip amount is prepared once per `process()` call; the callback uses no extra buffer, fixed latency, or heap allocation. Native state is 96 B; the browser/WASM effect state is 84 B.

The benchmark measures 100% clip on a sine LFO at the default 5 Hz/50% depth, 48 kHz stereo, and 128-frame callbacks on AMD Ryzen 9 5950X Release. The callback deadline is 2.667 ms; 2,000 warm-up and 5,000 measured callbacks are used. Percentages are processor-call time relative to one callback deadline:

| Mode | p50 | p95 | p99 | p99.9 | Max | Native state | Fixed latency | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Full LFO Clip | 0.0743% | 0.0750% | 0.0765% | 0.5374% | 2.0974% | 96 B | 0 frames | 0/5,000 |

The unmodified tremolo benchmark in the same run measured p50/p95/p99/p99.9/max 0.0645/0.0803/0.0806/0.5798/1.8045%, also with 96 B state. `make test-fender-tremolo-lfo-clip`, `make web-test-fender-tremolo-lfo-clip`, `make windows-cross-build`, and `make bench` pass. Browser Play uses local prerecorded stereo guitar DI. These isolated desktop timings do not predict Android/tablet load. Manual listening and target-device timing remain open; the project curve is not Fender DSP parity.

## Fractal FM9-family Surround Delay (2026-11-26)

The isolated benchmark uses an AMD Ryzen 9 5950X Release build, 48 kHz, stereo, 128-frame callbacks (2.667 ms deadline), 3,000 warm-up callbacks and 20,000 measured callbacks. The processor grows delay history only during parameter updates, not in `process()`. Default stereo owns 144,168 B natively; the browser/WASM smoke reports 144,124 B. The project caps each selected delay at four seconds; maximum-time stereo owns 1,536,168 B at 48 kHz. Dry audio has no fixed algorithmic latency; wet echoes follow their selected time.

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Default stereo | 0.0649% | 0.0803% | 0.0979% | 0.2606% | 1.4629% | 144,168 B | 0/20,000 |
| Short, rotating, high-feedback stereo | 0.0641% | 0.0668% | 0.0694% | 0.2445% | 1.0065% | 9,768 B | 0/20,000 |
| Maximum-time / ratio stereo | 0.1324% | 0.1537% | 0.1609% | 0.3559% | 0.4740% | 1,536,168 B | 0/20,000 |
| Default mono | 0.0244% | 0.0315% | 0.0323% | 0.0435% | 0.2385% | 144,168 B | 0/20,000 |

`make test`, `make web-test`, `make windows-cross-build`, and `make bench` pass. These are isolated processor-call measurements, not total-chain or device latency; the new effect's native/WASM parity remains untested because the offline CLI does not expose it. Manual listening and target-device timing remain open.

## Fender Tone Master Pro Memory Delay (2026-11-26)

At 48 kHz stereo, the project allocates 768,256 B of native effect state, including 768,024 B for the two-second delay ring; the browser/WASM effect reports 768,216 B. `process()` performs no heap allocations. The direct dry path has zero fixed algorithmic latency; repeats use the selected delay time. This is project memory and timing, not Fender hardware specifications.

The isolated benchmark uses AMD Ryzen 9 5950X Release, 48 kHz stereo, 128-frame callbacks (2.667 ms deadline), 3,000 warm-up and 20,000 measured callbacks. CPU percentages are processor-call time relative to one callback deadline:

| Profile | p50 | p95 | p99 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Default Chorus | 0.264% | 0.284% | 0.431% | 4.236% | 768,256 B | 0/20,000 |
| Gain + Grit Chorus | 0.240% | 0.318% | 0.362% | 1.236% | 768,256 B | 0/20,000 |
| Gain + Grit Vibrato | 0.240% | 0.329% | 0.381% | 1.759% | 768,256 B | 0/20,000 |

The browser/WASM local prerecorded Guitar-TECHS stereo DI test measured 768,216 B and passed two audible Play captures with distinct input and Chorus/Vibrato responses. `make test`, `make web-build`, `make web-browser-test`, `make windows-cross-build`, and `make bench` pass. These isolated desktop timings do not predict tablet performance. Human listening and target-device measurements remain open.

## Fender Tone Master Pro Modulated Small Hall (2026-11-26)

The project-authored effect uses 145,368 B of native processor state and reports 144,984 B from the browser/WASM chain. Its direct dry signal has no added fixed algorithmic latency. The native benchmark observed no heap allocation in processing. These are TabletFX measurements, not Fender hardware specifications.

On an AMD Ryzen 9 5950X Release build at 48 kHz stereo, 128-frame callbacks (2.667 ms deadline), 3,000 warm-up and 20,000 measured callbacks, isolated processor-call time relative to one callback deadline was:

| Profile | p50 | p95 | p99 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sane default | 1.308% | 1.956% | 2.055% | 14.703% | 145,368 B | 0/20,000 |
| Maximum decay and modulation | 1.310% | 1.571% | 1.934% | 8.119% | 145,368 B | 0/20,000 |

The maximum values are occasional host callback outliers; neither profile missed its measured deadline. This isolated desktop benchmark does not predict tablet load. The browser smoke used local prerecorded stereo Guitar-TECHS DI and captured two Play outputs that changed with input/profile. `make test`, `make web-build`, `make web-browser-test`, and the Windows MinGW `make windows-cross-build` pass. Human listening, physical hardware comparison, and target-device timing remain open; no Fender DSP or sound parity is claimed.

## Fractal FM9-family Plex Shift project subset (2026-11-26)

Plex Shift is a separate Fractal block from Plex Delay and Plex Detune. TabletFX places an optional project-authored granular shifter within the existing Plex Matrix Delay at Delay picker ID 76, off by default. It adds no processor or picker row. The wet tap path uses a 5.333 ms target grain; actual history lookback varies with pitch ratio and direction, so a single fixed wet-latency value is not specified. The unshifted dry path remains immediate. Pitch history is preallocated at construction, capped at 256 KiB, and included in the 1,132,152 B native Plex processor state at 48 kHz stereo. Processing allocates nothing. Master scales and project bounds are documented in `EFFECTS_MATRIX.md`; they are not manufacturer ranges/defaults.

The Release benchmark ran on an AMD Ryzen 9 5950X at 48 kHz stereo with 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks, and 20,000 measured callbacks per profile. Percentages show isolated processor-call time relative to one callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Native state | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Plex matrix, no pitch | 0.1324% | 0.2025% | 0.2434% | 0.5689% | 4.7803% | 1,132,152 B | 0/20,000 |
| One shifted line, Forward | 0.2145% | 0.3124% | 0.3675% | 0.9188% | 1.9463% | 1,132,152 B | 0/20,000 |
| Four shifted lines, Forward | 0.4688% | 0.6945% | 0.8340% | 1.5825% | 14.9479% | 1,132,152 B | 0/20,000 |
| Eight shifted lines, Forward | 0.7069% | 0.9803% | 1.1625% | 1.9602% | 10.3679% | 1,132,152 B | 0/20,000 |
| Eight shifted lines, Reverse | 0.6529% | 0.9514% | 1.2019% | 3.5836% | 7.8759% | 1,132,152 B | 0/20,000 |

These are isolated desktop CPU readings, not total-chain cost or Android/tablet performance. The maxima are occasional host outliers. `make test`, `make web-build`, `make web-browser-test`, `make windows-cross-build`, and `make bench` pass. Browser validation uses local prerecorded Guitar-TECHS DI. Human listening, physical Fractal comparison, firmware applicability, and target-device timing remain open; no manufacturer DSP or sound parity is claimed.

## Fractal FM9-family Flanger VCO Response project control (2026-11-27)

The VCO selector extends the existing Modulation Through-Zero Flanger at ID 81 and adds no new delay storage. Native state is 15,672 B at 48 kHz stereo, including the processor and its existing delay buffers. Processing has no heap allocation. The supported 8 ms maximum range means up to 384 frames of path latency at 48 kHz; this is unchanged by the VCO curve. Host/graph latency compensation is not implemented.

The Release benchmark used AMD Ryzen 9 5950X, 48 kHz stereo, 128-frame callbacks (2.6667 ms deadline), 3,000 warm-up callbacks, and 20,000 measured callbacks per profile. Percentages are isolated processor-call time relative to one callback deadline:

| Project profile | p50 | p95 | p99 | p99.9 | Max | Native state | Observed deadline overruns |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Linear | 0.179% | 0.264% | 0.282% | 0.633% | 18.595% | 15,672 B | 0/20,000 |
| Exponential, 1.5 | 0.296% | 0.489% | 0.561% | 0.939% | 9.319% | 15,672 B | 0/20,000 |
| Exponential, 4.0 | 0.298% | 0.440% | 0.711% | 1.416% | 2.500% | 15,672 B | 0/20,000 |
| Square Law | 0.193% | 0.283% | 0.405% | 0.992% | 1.993% | 15,672 B | 0/20,000 |

The Linear maximum is a scheduler outlier; every observed maximum was below one callback deadline. The exponential curve uses `std::pow` once per frame when selected; its p50 cost is approximately 0.12 percentage points above the Linear profile at the tested 1.5 exponent. This is a host-only benchmark, not a tablet, Windows runtime, or full-chain estimate. The browser/WASM local-DI test confirms distinct output profiles, but its timer resolution is not used for an isolated CPU percentage. The manufacturer does not document the curves or their CPU cost, and no Fractal DSP/settings/sound parity is claimed.

## HeadRush Prime Phase Invert project control (2026-11-27)

Prime v5.1.0 firmware notes added Phase Invert to Advanced Block Edit and mention Block Mix phase cancellation; v5.2.0 notes reproduce that v5.1.0 entry but do not document its parameter mapping or explicitly state unchanged retention. Project placement after the selected enabled stage, all-channel polarity flip, default-off setting, and bypass dormancy are independent design choices. This is a sign operation on the existing interleaved stage output: no buffer, fixed delay, or `process()` heap allocation is added. The helper object is 8 B; the per-chain packed masks for 122 stages use 16 B; browser UI state is 122 B.

Release benchmark: AMD Ryzen 9 5950X, 48 kHz stereo, 128 frames per callback (2.6667 ms budget), 3,000 warm-up and 20,000 measured helper calls. Values are elapsed helper time as a percentage of one audio callback deadline:

| Profile | p50 | p95 | p99 | p99.9 | Max | Helper state | Process allocations | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Sample-wise polarity inversion | 0.001500% | 0.001875% | 0.002625% | 0.003375% | 0.324750% | 8 B | 0 | 0/20,000 |

The maximum is a scheduler outlier. This isolated helper benchmark excludes UI, the full processor chain, and host scheduling; these desktop measurements do not predict target-device cost. Focused native, browser/WASM Play on local prerecorded guitar, and Windows MinGW core/offline-renderer checks pass. The Windows desktop UI/audio path is not yet integrated. Listening and Prime hardware comparison remain manual.
