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

`make audio-regression` passed with input Gain +2 dB, Volume Pedal at 92% using the audio taper (−96 dB heel, 0 dB toe), High-pass 80 Hz, Low-pass 12 kHz, Noise Gate −72 dB, Pedal Compressor defaults, Studio Compressor (−24 dB threshold, 3:1, 6 dB knee, 8/100 ms attack/release), Soft Drive 3, three-band EQ (+1.5/−1/+0.5 dB at 120/900/5000 Hz), Delay 125 ms / 0.2 feedback / 0.1 mix, output Gain −1 dB, and Peak Limiter at −1 dBFS / 80 ms release. It processed the five EGFxSet samples (about 5 seconds each) and the 72.5-second Guitar-TECHS excerpt through all twelve processors. Every output stayed below the limiter ceiling, was checked for a successful render and non-empty float32 WAV, and temporary rendered files were removed after the run. The EGFxSet neck file has 236,465 frames versus 240,000 for the other pickup files because its source data chunk contains fewer frames; its extra terminal zero padding byte is handled by the WAV reader.

These results verify the deterministic offline Linux path only. No audio hardware, live backend, Android route, or Windows route has been probed.

## Windows cross-build

`make windows-cross-build` passed on this Linux host using MinGW-w64 GCC 13.0.0 and CMake 3.28.3. It compiled `tabletfx_core`, `tabletfx_offline.exe`, the `tabletfx_wasapi` static library, and the WASAPI compile-smoke executable. This is a compile check only; the Windows executable was not run.

The initial backend uses the default WASAPI capture and render endpoints in shared mode, accepts supported PCM/float endpoint mix formats, and requires matching endpoint sample rates. Device selection, sample-rate conversion, exclusive mode, app/UI integration, live routing, disconnect/reconnect behavior, and real-interface latency remain untested. The Windows row in the hardware table therefore remains **Untested**.

## Browser/WASM playback

The first browser test page for `WEB-01` is implemented. It reads a user-selected local file in the browser, processes it through the shared C++ DSP compiled as WebAssembly and hosted in an AudioWorklet, and routes Play output to the browser destination. Stop, rewind, block bypass controls, and dry/wet comparison are present. Audio stays local; the page has no microphone or upload path. `make web-test` passed automated native tests and a headless Chrome run on 2026-11-21.

The browser test uses HeadlessChrome 151, 48 kHz mono, 128-frame blocks (2.667 ms deadline), and a generated one-second guitar-like fixture. The latest automated run enabled Input trim, High-pass, Low-pass, Noise Gate, Pedal Compressor, Studio Compressor, Soft Drive, three-band EQ, Delay, and Output level. Browser capture matched the native offline renderer with maximum absolute error 0.00008415 and RMS error 0.0000046834, inside the limits 0.0002 and 0.00002. The ten-processor C++ chain state uses 515,816 bytes within a fixed 16 MiB WebAssembly linear-memory heap. This is generated test audio, not a captured guitar performance; local prerecorded DI pickups are separately covered by `make audio-regression`.

One run reported average callback-budget percentages in order Input trim/High-pass/Low-pass/Gate/Pedal Compressor/Studio Compressor/Drive/EQ/Delay/Output level/whole-chain: `0.100 / 0.100 / 0.100 / 0 / 0.199 / 0.100 / 0.299 / 0 / 0.100 / 0.100 / 1.396%`; peak readings were `37.5 / 37.5 / 37.5 / 0 / 37.5 / 37.5 / 37.5 / 0 / 37.5 / 37.5 / 37.5%`. These are quantized 1 ms timer readings from a single one-second render; repeated runs shift values between processors, so they are troubleshooting estimates rather than usable relative CPU benchmarks.

This Chrome AudioWorklet did not expose `performance.now()`, so live per-effect timing fell back to `Date.now()` with 1 ms resolution. The resulting percentages are displayed as coarse and are not suitable for comparing very light processors; use the high-resolution Linux benchmark in `LATENCY_AND_QUALITY.md` for precise local DSP comparisons. The browser test mutes physical playback while capturing the processed signal, so manual listening through a real browser output device remains open. These results do not establish native hardware or round-trip latency.
