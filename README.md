# TabletFX

An offline guitar multi-effects application for Android, Linux, and Windows, with a required browser/WASM test target. The C++20 DSP core is shared across native builds and the planned browser build; native audio backends are Oboe, JACK/PipeWire, and WASAPI.

Project status: the Linux offline core implements Gain, Noise Gate, Soft Drive, Delay, a studio compressor, and a three-band parametric EQ. Automated DSP/CLI tests, per-effect and full-chain CPU/memory benchmarks, and a prerecorded-guitar regression path are available. An initial Windows WASAPI backend and MinGW cross-build are present, but the backend is not yet integrated into the app UI or run on Windows hardware. The native application UI, browser/WASM playback target, Android/Linux live audio backends, and hardware configurations remain unverified.

Start with [`GOAL.md`](GOAL.md), [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md), and [`ARCHITECTURE.md`](ARCHITECTURE.md). Run `make doctor` to inspect locally available toolchains. Run `make audio-fixtures-plan` to review the licensed prerecorded test audio and the selected external volume; `make audio-fixtures-fetch` downloads and verifies it outside the repository, then creates the ignored `testdata/reference_audio` symlink. Set `MULTIFX_AUDIO_DATA` to override automatic volume selection.
