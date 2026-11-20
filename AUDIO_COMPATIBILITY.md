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

`make audio-regression` passed with fixed Gain +2 dB, Noise Gate −72 dB, Compressor (−24 dB threshold, 3:1, 6 dB knee, 8/100 ms attack/release), Soft Drive 3, three-band EQ (+1.5/−1/+0.5 dB at 120/900/5000 Hz), and Delay 125 ms / 0.2 feedback / 0.1 mix. It processed the five EGFxSet samples (about 5 seconds each) and the 72.5-second Guitar-TECHS excerpt through all six processors. Each output was checked for a successful render and non-empty float32 WAV; temporary rendered files were removed after the run. The EGFxSet neck file has 236,465 frames versus 240,000 for the other pickup files because its source data chunk contains fewer frames; its extra terminal zero padding byte is handled by the WAV reader.

These results verify the deterministic offline Linux path only. No audio hardware, live backend, Android route, or Windows route has been probed.

## Windows cross-build

`make windows-cross-build` passed on this Linux host using MinGW-w64 GCC 13.0.0 and CMake 3.28.3. It compiled `tabletfx_core`, `tabletfx_offline.exe`, the `tabletfx_wasapi` static library, and the WASAPI compile-smoke executable. This is a compile check only; the Windows executable was not run.

The initial backend uses the default WASAPI capture and render endpoints in shared mode, accepts supported PCM/float endpoint mix formats, and requires matching endpoint sample rates. Device selection, sample-rate conversion, exclusive mode, app/UI integration, live routing, disconnect/reconnect behavior, and real-interface latency remain untested. The Windows row in the hardware table therefore remains **Untested**.

## Browser/WASM playback

Required but not implemented yet (`WEB-01`). The browser test must load local guitar audio, process blocks through the shared WebAssembly DSP chain in an AudioWorklet, and play the processed output when Play is pressed. Automated browser coverage must capture and check the processed output against the native offline renderer; it must not upload the input recording. Browser output and timing results will be recorded separately from native hardware tests.
