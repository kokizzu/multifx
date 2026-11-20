# Audio compatibility and hardware evidence

No audio hardware has been probed by this repository yet. All rows remain unverified until collected directly.

| Host | Required configuration | Observed device/OS/API/route | Build/audio test | Result |
|---|---|---|---|---|
| Android | itel VistaTab 11, Android 14; record actual ABI, build fingerprint and physical RAM; USB full-duplex interface | Not probed | Not run | Untested |
| Linux | Pop!_OS 22.04, x86_64; tested JACK/PipeWire route and interface | Not probed | Not run | Untested |
| Windows | Windows x64; WASAPI shared/exclusive route and interface | Not probed | Not run | Untested |

Record exact app commit, OS build, interface/firmware, USB/power arrangement, ports, sample rate, actual buffer/burst, granted sharing mode, xruns, test duration, and limitations. Never infer support from CPU model or a successful compile.

## Offline prerecorded audio

The current plan uses CC BY 4.0 EGFxSet Clean samples for five pickup positions and the Guitar-TECHS DI music excerpt for performed phrases/chords. These datasets complement each other: EGFxSet is isolated monophonic notes, while Guitar-TECHS does not label each excerpt's pickup-selector position. Both archives remain outside Git and are connected through the ignored local symlink `testdata/reference_audio`. Source links, dataset versions, checksums, and use limits are recorded in `RESEARCH.md`. No audio fixture has been fetched or run through DSP yet.
