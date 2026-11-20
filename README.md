# TabletFX

An offline guitar multi-effects application for Android, Linux, and Windows. The shared C++20 DSP core is intended to pair with a touch-friendly Qt Quick editor and native full-duplex audio backends (Oboe, JACK/PipeWire, and WASAPI).

Project status: requirements and architecture are being established; no live audio path or supported hardware configuration is claimed yet.

Start with [`GOAL.md`](GOAL.md), [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md), and [`ARCHITECTURE.md`](ARCHITECTURE.md). Run `make doctor` to inspect locally available toolchains. Run `make audio-fixtures-plan` to review the licensed prerecorded test audio and the selected external volume; `make audio-fixtures-fetch` downloads and verifies it outside the repository, then creates the ignored `testdata/reference_audio` symlink. Set `MULTIFX_AUDIO_DATA` to override automatic volume selection.
