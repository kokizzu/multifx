# TabletFX

An offline guitar multi-effects application for Android, Linux, and Windows, with a required browser/WASM test target. The C++20 DSP core is shared across native builds and the planned browser build; native audio backends are Oboe, JACK/PipeWire, and WASAPI.

Project status: the Linux offline core implements Gain, Noise Gate, Soft Drive, and Delay and has a prerecorded-guitar regression path. The native application UI, browser/WASM playback target, native live audio backends, and hardware configurations are not yet verified.

Start with [`GOAL.md`](GOAL.md), [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md), and [`ARCHITECTURE.md`](ARCHITECTURE.md). Run `make doctor` to inspect locally available toolchains. Run `make audio-fixtures-plan` to review the licensed prerecorded test audio and the selected external volume; `make audio-fixtures-fetch` downloads and verifies it outside the repository, then creates the ignored `testdata/reference_audio` symlink. Set `MULTIFX_AUDIO_DATA` to override automatic volume selection.
