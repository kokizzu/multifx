# Audio compatibility and hardware evidence

No audio hardware has been probed by this repository yet. All rows remain unverified until collected directly.

| Host | Required configuration | Observed device/OS/API/route | Build/audio test | Result |
|---|---|---|---|---|
| Android | itel VistaTab 11, Android 14; record actual ABI, build fingerprint and physical RAM; USB full-duplex interface | Not probed | Not run | Untested |
| Linux | Pop!_OS 22.04, x86_64; tested JACK/PipeWire route and interface | Not probed | Not run | Untested |
| Windows | Windows x64; WASAPI shared/exclusive route and interface | Not probed | Not run | Untested |

Record exact app commit, OS build, interface/firmware, USB/power arrangement, ports, sample rate, actual buffer/burst, granted sharing mode, xruns, test duration, and limitations. Never infer support from CPU model or a successful compile.

## Offline prerecorded audio

`make audio-fixtures-verify` passed for both external archives and the ignored local symlink `testdata/reference_audio`:

- EGFxSet Clean: 690 mono WAV files, one Strat and five positions (bridge, bridge-middle, middle, middle-neck, neck). The regression compares the same open high-E sample (`1-0.wav`) in each position.
- Guitar-TECHS P3 music: 32 WAV files; the test uses `P3_music/audio/directinput/directinput_01.wav`, a 48 kHz stereo performance excerpt. The dataset-level setup lists Player 3's Sire T7 with bridge-neck pickup selection; there is no separate per-file selector annotation.

`make audio-regression` passed with input Gain +2 dB, Volume Pedal at 92% using the audio taper (−96 dB heel, 0 dB toe), High-pass 80 Hz, Low-pass 12 kHz, Noise Gate at −72 dB threshold / −72 dB closed attenuation / 3 dB hysteresis / 2 ms attack / 20 ms hold / 80 ms release, Pedal Compressor defaults, Studio Compressor (−24 dB threshold, 3:1, 6 dB knee, 8/100 ms attack/release), Wah at 62% (350–2200 Hz, Q 4, 0 dB), Soft Drive 3, Tremolo 4 Hz / 20% depth / sine / free rate, three-band EQ (+1.5/−1/+0.5 dB at 120/900/5000 Hz), Delay 125 ms / 0.2 feedback / 0.1 mix, output Gain −1 dB, and Peak Limiter at −1 dBFS / 80 ms release. It processed the five EGFxSet samples (about 5 seconds each) and the 72.5-second Guitar-TECHS excerpt through all fourteen processors. Every output stayed below the limiter ceiling, was checked for a successful render and non-empty float32 WAV, and temporary rendered files were removed after the run. The EGFxSet neck file has 236,465 frames versus 240,000 for the other pickup files because its source data chunk contains fewer frames; its extra terminal zero padding byte is handled by the WAV reader.

These results verify the deterministic offline Linux path only. No audio hardware, live backend, Android route, or Windows route has been probed.

## Windows cross-build

`make windows-cross-build` passed on this Linux host after the noise-gate changes, using MinGW-w64 GCC 13.0.0 and CMake 3.28.3. It compiled `tabletfx_core`, `tabletfx_offline.exe`, the `tabletfx_wasapi` static library, and the WASAPI compile-smoke executable. This is a compile check only; the Windows executable was not run.

The initial backend uses the default WASAPI capture and render endpoints in shared mode, accepts supported PCM/float endpoint mix formats, and requires matching endpoint sample rates. Device selection, sample-rate conversion, exclusive mode, app/UI integration, live routing, disconnect/reconnect behavior, and real-interface latency remain untested. The Windows row in the hardware table therefore remains **Untested**.

## Browser/WASM playback

The browser test page for `WEB-01` reads a user-selected local audio file, processes it through the shared C++ DSP compiled as WebAssembly and hosted in an AudioWorklet, and routes Play output to the browser destination. Stop, rewind, block bypass controls, and dry/wet comparison are present. Audio stays local; the page has no microphone or upload path. `make web-test` passed automated native tests and a headless Chrome run on 2026-11-21.

The browser test uses HeadlessChrome 151, 48 kHz mono, 128-frame blocks (2.667 ms deadline), and a generated one-second guitar-like fixture. Its fourteen-stage chain enables Input trim, Volume Pedal, High-pass, Low-pass, Noise Gate, Pedal Compressor, Studio Compressor, Wah, Soft Drive, Tremolo, three-band EQ, Delay, Output level, and Peak Limiter. Browser capture matched the native offline renderer with maximum absolute error 0.000014023 and RMS error 0.000000448, inside the limits 0.0002 and 0.00002. The C++ chain state owns 516,136 bytes within a fixed 16 MiB WebAssembly linear-memory heap. The automated fixture is generated test audio, not a captured guitar performance; local prerecorded DI pickup samples are separately covered by `make audio-regression`.

In this run the browser reported average callback-budget percentages in processor order as `0 / 0 / 0 / 0.100 / 0 / 0.199 / 0.499 / 0 / 0.199 / 0 / 0 / 0 / 0 / 0.100%`, with a 1.197% whole-chain average. Peak readings quantized to `0` or `37.5%` per processor. This Chrome AudioWorklet did not expose `performance.now()`, so timing fell back to `Date.now()` at 1 ms resolution; repeated runs can shift a millisecond between processors. Treat these as troubleshooting estimates, not relative CPU benchmarks. Use the high-resolution Linux results in `LATENCY_AND_QUALITY.md` for local DSP comparisons.

The browser test captures the processed signal and confirms that the Play output path is connected, but it mutes physical playback during capture. Manual listening through a real browser output device remains open. These results do not establish native hardware or round-trip latency.
