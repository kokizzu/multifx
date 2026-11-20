# Effects matrix

The canonical capability list is in `GOAL.md` section 10. The row-by-row implementation, automated-test, and device/listening status is tracked in [`IMPLEMENTATION_CHECKLIST.md`](IMPLEMENTATION_CHECKLIST.md). No effect is considered shipped merely because it appears in the catalog or UI.

For each implemented processor, record: stable ID/version; category and exact function; source revision and license; channel layout; parameters/defaults/units; sample rates; latency; tail and bypass policy; quality modes; resource cap; measured per-block CPU percentage; automated evidence; and manual listening/hardware evidence.

## Implemented project-authored processors

These are original in-repository DSP implementations; no third-party effect code or assets are linked. “Memory” is the processor object plus directly owned storage, excluding caller buffers, allocator metadata, and backend/runtime overhead. Linux CPU results are from the release processor-call benchmark and are not live-route measurements.

| Stable ID | Function and current controls | Channels / latency / memory | Automated evidence and limits |
|---|---|---|---|
| `gain` | Linear input gain; default 0 dB, bounded −96…+24 dB. | Interleaved channels; zero algorithmic latency; 16 B on this 64-bit build. | C++ transfer tests and benchmark. Parameter smoothing and block bypass are not yet implemented. |
| `noise-gate` | Linked peak detector with threshold, attenuation, attack, release; defaults −60 dB, −72 dB, 2 ms, 80 ms. | Interleaved channels; zero lookahead; 40 B on this build. | Finite-output, reset, and allocation tests. Hysteresis and hold are absent, so the full P0 gate specification is not complete. |
| `soft-drive` | `tanh` soft clip; default drive 1 (identity), range 1…20. | Interleaved channels; zero algorithmic latency; 16 B. | Finite-output and allocation tests; no oversampling, aliasing qualification, or product-level voicing claim. |
| `delay` | Feedback delay; default 350 ms, 0.25 feedback, 0.25 wet mix; maximum storage is set at construction. | Configured channel count; no fixed dry-path latency; wet repeats use the configured delay; storage is proportional to max time × rate × channels. | Impulse-position, allocation, and reset tests. The 2 s / 48 kHz / stereo benchmark owns 768,080 B including object and delay line. |
| `compressor` | Stereo-linked feed-forward studio compressor; threshold −18 dB, ratio 3:1, 6 dB soft knee, attack 8 ms, release 100 ms, makeup 0 dB; ranges are bounded in code. | Mono or multichannel interleaved input; no lookahead/algorithmic latency; fixed 64 B on this build. | Transfer-curve, stereo-link, parameter bounds, reset, non-finite input, variable-block, and zero-allocation tests. No lookahead or sidechain. |
| `three-band-parametric-eq` | Three independent peaking bands, each with frequency, gain and Q; defaults 120/800/5000 Hz, 0 dB, Q 0.707. Frequency 20…0.45×sample rate, gain ±18 dB, Q 0.25…10. | Mono/stereo; zero algorithmic latency; fixed 160 B on this build. | Center-response per band, neutral passthrough, stereo isolation, bounds, reset, variable-block, finite-output, and zero-allocation tests. Coefficients are prepared off the process path. |

## Remaining evidence

- Native Linux offline rendering and the local prerecorded-guitar corpus pass through Gain, Gate, Compressor, Soft Drive, EQ, and Delay. The regression retains summary metrics and removes temporary audio after the run.
- Linux desktop processor CPU and owned-memory figures are recorded in [`LATENCY_AND_QUALITY.md`](LATENCY_AND_QUALITY.md). Re-measure on each target CPU and after DSP changes.
- Human listening, native full-duplex hardware, Android, Windows runtime/audio, browser/WASM, presets, live graph switching, and the broader effect catalog remain unverified or unimplemented. A passing offline render does not establish live performance or listening acceptance.
