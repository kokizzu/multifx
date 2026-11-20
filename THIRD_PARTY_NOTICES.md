# Third-party code, assets, and notices

The polyphonic pitch shifter uses an MIT-licensed upstream STFT pitch-shift implementation. No third-party binaries, models, impulse responses, or artwork are included.

Distribution target: GPLv3-compatible application with exact upstream licenses and notices preserved. GPL-2.0-only and unknown-license files are excluded unless a compatible permission is obtained. Source code licenses do not grant rights to presets, IRs, recordings, capture models, or artwork.

| Component | Exact revision/files | License | Changes | Platforms/build | Evidence |
|---|---|---|---|---|---|
| `jurihock/stftPitchShift` | Commit `70c96db087458f8631edbc1bb4068d42ea5b52cc`; headers under `third_party/stft-pitch-shift/` and `third_party/stft-pitch-shift/StftPitchShift/`; preserved `LICENSE.txt` | MIT; copyright Juergen Hock, 2022 | Vendored header-only base library. Added reset methods to `Vocoder` and `StftPitchShiftCore`; transient phase history is cleared while the precomputed synthesis phase offset is preserved. The GPL plugin/UI source is not included. | Shared C++ core, Linux native, MinGW Windows cross-build, and Emscripten/WASM | [Pinned upstream source](https://github.com/jurihock/stftPitchShift/tree/70c96db087458f8631edbc1bb4068d42ea5b52cc); [upstream MIT license](https://github.com/jurihock/stftPitchShift/blob/70c96db087458f8631edbc1bb4068d42ea5b52cc/LICENSE.txt) |
| EGFxSet Clean (`Clean.zip`, Zenodo 7044411) | Dataset release 1.0; MD5 `cdb1b401960f56becc8640387910e78a` | CC BY 4.0 | Planned local-only fixture; not fetched or redistributed | Linux offline tests | [Official project](https://egfxset.github.io/); [authors' paper](https://archives.ismir.net/ismir2022/latebreaking/000006.pdf) |
| Guitar-TECHS P3 music (`P3_music.zip`, Zenodo 14963133) | Dataset v1; MD5 `071ba80aecf00f4a31fbd167b3f22198` | CC BY 4.0 | Planned local-only fixture; not fetched or redistributed | Linux offline tests | [Zenodo](https://zenodo.org/records/14963133); [author paper](https://arxiv.org/abs/2501.03720) |

The audio above is not integrated or bundled with the application. Fetching it creates files outside the repository and a local-only ignored symlink. Retain the original attribution and license notices if any audio is later redistributed. See `RESEARCH.md` for content limitations.

Before integration, update this inventory and preserve corresponding-source obligations. Do not rely on a repository's root license as blanket permission for every file.
