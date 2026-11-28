"""Fail-closed exact changed-line selectors for narrowly scoped checkpoints."""

from __future__ import annotations

import re
from collections import Counter
import hashlib


LineSelectorOperation = str | dict[str, object]

PARTIAL_CHECKPOINT_LINE_SELECTORS: dict[
    str, dict[str, tuple[LineSelectorOperation, ...]]
] = {}

PARTIAL_CHECKPOINT_LINE_SELECTORS["fractal-resonator-effect"] = {
    "CMakeLists.txt": (
        "+    src/effects/resonator.cpp",
        "+add_executable(tabletfx_resonator_bench apps/resonator_bench.cpp)",
        "+target_link_libraries(tabletfx_resonator_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_resonator_tests tests/test_resonator.cpp)",
        "+    target_link_libraries(tabletfx_resonator_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_resonator_tests COMMAND tabletfx_resonator_tests)",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator block (2026-11-27)",
                "line_count": 6,
            }
        },
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator (2026-11-27)",
                "end": "+- [ ] Confirm the target device/installed firmware and measure sustained CPU on the intended runtime; v12.0 lists Original/Turbo but does not name Mark II Turbo.",
                "expected_line_count": 11,
            }
        },
    ),
    "LATENCY_AND_QUALITY.md": (
        {
            "span": {
                "start": "+## Fractal FM9-family Resonator processor-call benchmark (2026-11-27)",
                "line_count": 5,
            }
        },
    ),
    "include/tabletfx/effects.hpp": (
        '+#include "tabletfx/effects/resonator.hpp"',
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/resonator.hpp"',
        {
            "span": {
                "start": "+          resonator_(std::make_unique<tabletfx::Resonator>(",
                "line_count": 2,
            }
        },
        {"replace": {
            "before": "-        return effects_.size() + 27;",
            "current": "+    return effects_.size() + 33;",
            "selected": "+    return effects_.size() + 28;",
            "hunk_header": "@@ -460 +474,3 @@ public:",
            "allow_intervening_added_lines": True,
            "allow_cross_hunk_staged": True,
        }},
        {
            "span": {
                "hunk_headers": [
                    "@@ -621,0 +638,64 @@ public:",
                    "@@ -621,0 +625,4 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 4,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -979,0 +1060,39 @@ public:",
                    "@@ -979,0 +987,9 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 9,
            }
        },
        "+        resonator_->reset();",
        {
            "span": {
                "start": "+    void configure_resonator(const float* values,",
                "line_count": 48,
            }
        },
        "+        bytes += resonator_->memory_bytes();",
        {
            "span": {
                "hunk_headers": [
                    "@@ -4278,0 +4573,32 @@ public:",
                    "@@ -4278,0 +4345,6 @@ public:",
                ],
                "start": "+        if (index == 122U) {",
                "line_count": 6,
            }
        },
        "+        case 122U: return resonator_enabled_;",
        "+    std::unique_ptr<tabletfx::Resonator> resonator_;",
        "+    bool resonator_enabled_ = false;",
        {
            "span": {
                "start": "+TABLETFX_WASM_EXPORT void tf_chain_set_resonator(",
                "line_count": 6,
            }
        },
    ),
    "web/app.js": (
        '+  "Fractal Resonator · project-authored, not a pitch-tracked synth",',
        {
            "span": {
                "start": "+  // Serialized as the ResonatorParameters fields in the stable worklet ABI order.",
                "end": "+  ],",
                "end_after_start": True,
                "expected_line_count": 10,
            }
        },
        {
            "span": {
                "start": "+const resonatorControls = Array.from(",
                "end": "+);",
                "end_after_start": True,
                "expected_line_count": 6,
            }
        },
        {
            "span": {
                "start": "+function updateResonator() {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 26,
            }
        },
        {
            "span": {
                "start": "+for (const control of resonatorControls) {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        "+let browserSmokeBarrierSequence = 0;",
        {
            "span": {
                "start": "+function waitForBrowserSmokeWorkletBarrier(expectedActive) {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 28,
            }
        },
        {
            "span": {
                "start": "+async function stopBrowserSmokePlaybackAndWait() {",
                "end": "+}",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        {
            "line": {
                "operation": "+    await stopBrowserSmokePlaybackAndWait();",
                "hunk_context": "async function runMercatorDistortionBrowserSmokeTest",
                "expected_count": 1,
            }
        },
        {
            "line": {
                "operation": "+      await stopBrowserSmokePlaybackAndWait();",
                "hunk_context": "async function runMercatorDistortionBrowserSmokeTest",
                "expected_count": 1,
            }
        },
        "-    const expectedEffectIds = Array.from({ length: 117 }, (_, index) => index);",
        {
            "replace": {
                "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "selected": "+    const expectedEffectIds = Array.from({ length: 123 }, (_, index) => index);",
            }
        },
        "-    if (EFFECT_NAMES.length !== 117 || effectRows.length !== 117 ||",
        {
            "replace": {
                "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "selected": "+    if (EFFECT_NAMES.length !== 123 || effectRows.length !== 123 ||",
            }
        },
        {
            "span": {
                "start": '+                  !EFFECT_NAMES[122]?.startsWith("Fractal Resonator · project-authored, not a pitch-tracked synth") ||',
                "end": '+                  DEFAULT_PARAMETERS.resonator.join(",") !== "0,0,1,110,164.81,220,329.63,110,0.55,0.55,0.55,0.55,2,2,2,2,0.4,0.4,0.4,0.4,-0.75,-0.25,0.25,0.75,1,1,1,1,0,0.8,1,1,1,0.35,0,0,0,0,0" ||',
                "expected_line_count": 4,
            }
        },
        "-                  initialEffectState[118] || initialEffectState[119] ||",
        "+                   initialEffectState[118] || initialEffectState[119] || initialEffectState[120] ||",
        {
            "replace": {
                "current": "+                      initialEffectState[121] || initialEffectState[122] || initialEffectState[123] ||",
                "selected": "+                      initialEffectState[121] || initialEffectState[122] ||",
            }
        },
        {
            "replace": {
                "before": '-                   throw new Error(`The effect picker must list 122 ordered, categorized effects (IDs 0–121), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, Memory Delay, both Tone Master halls, and Pitch Ring Mod; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[121]}, row121=${effectRows[121]?.dataset.category}/${effectRows[121]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[121]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "current": '+                     throw new Error(`The effect picker must list 126 ordered, categorized effects (IDs 0–125), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, both Tone Master halls, Pitch Ring Mod, Fractal Resonator, Studio Pan Delay, Essex Vibe, and Euclidean Delay; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[125]}, row125=${effectRows[125]?.dataset.category}/${effectRows[125]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[125]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "selected": '+                     throw new Error(`The effect picker must list 123 ordered, categorized effects (IDs 0–122), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, Memory Delay, both Tone Master halls, Pitch Ring Mod, and Fractal Resonator; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[122]}, row122=${effectRows[122]?.dataset.category}/${effectRows[122]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[122]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
            }
        },
        "+    // Fractal Resonator has a focused local guitar Play smoke and no offline CLI stage.",
        "+    parityEnabled[122] = false;",
        {
            "span": {
                "start": '+        if (new URLSearchParams(location.search).get("selftest") === "fractal-resonator") {',
                "end": "+        }",
                "end_after_start": True,
                "expected_line_count": 18,
            }
        },
        {
            "span": {
                "start": '+      testResult.textContent = "Checking Fractal Resonator with local guitar playback…";',
                "end": "+      );",
                "end_after_start": True,
                "expected_line_count": 4,
            }
        },
        "+      resonator: resonatorSmoke,",
    ),
}


PARTIAL_CHECKPOINT_LINE_SELECTORS["fender-studio-delay-pan"] = {
    "CMakeLists.txt": (
        "+    src/effects/studio_pan_delay.cpp",
        "+add_executable(tabletfx_studio_pan_delay_bench apps/studio_pan_delay_bench.cpp)",
        "+target_link_libraries(tabletfx_studio_pan_delay_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_studio_pan_delay_tests tests/test_studio_pan_delay.cpp)",
        "+    target_link_libraries(tabletfx_studio_pan_delay_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_studio_pan_delay_tests COMMAND tabletfx_studio_pan_delay_tests)",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        {
            "span": {
                "start": "+## Fender Tone Master Pro Studio Delay pan modes (2026-11-28)",
                "line_count": 7,
            }
        },
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        '+- [x] **Fender Tone Master Pro — Studio Pan Delay project-authored block (picker ID 123).** Verified Model Guide v1.1.100 / Rev. A p. 50 and firmware history: Studio Delay names Time, Tap Division, Feedback, Mix, Low/High Cut, Mod Rate/Depth/Mode, Pan Rate/Depth/Mode, and Spillover; v1.6.34 documents Time 30–2,000 ms, and current listed v1.8.87 notes contain no Studio Delay change. Project fixed-time stereo repeats with Sine/Triangle/Envelope pan modes; native tests, local-guitar browser/WASM Play/reset, full browser/native comparison, Windows MinGW cross-build, and benchmark pass. No Fender settings/DSP/sound parity; Android/Windows runtime and physical listening remain manual.',
    ),
    "EFFECTS_MATRIX.md": (
        {
            "span": {
                "hunk_header": "@@ -393,0 +394 @@ The November 2026 Blocks Guide lists Phaser for the shared Global Mix offset up",
                "start": "+| `studio-pan-delay` | Project-authored fixed-time stereo delay with panned repeats. Project controls: Time 30–2,000 ms (350), Feedback 0–0.9 (0.25), Mix 0–100% (25), Pan Rate 0.05–20 Hz (0.5), Pan Depth 0–100% (75), and Sine/Triangle/Envelope mode. Tone Master Pro Model Guide v1.1.100 / Rev. A p. 50 documents Time, Tap Division, Feedback, Mix, Low/High Cut, Mod Rate/Depth/Mode, Pan Rate/Depth/Mode, and Spillover; firmware v1.6.34 gives Time 30–2,000 ms. Later v1.8.87 notes (2026-11-23) list no Studio Delay changes. Other project ranges/defaults and Envelope behavior are project choices; no Fender settings/DSP/sound parity is claimed. | Stereo; two-second delay ring allocated only at activation. Native memory 768,288 B; WASM reports 208 B disabled/unprepared and 768,224 B active, including the 768,016 B ring. Zero fixed algorithmic latency; wet repeats use the Time setting. `process()` allocates nothing. | `make test` passed 13 fixture checks and 129/129 CTests. Full `make web-test` passed browser/WASM Play on local stereo prerecorded Guitar-TECHS DI, deterministic reset replay (zero RMS difference), wet/dry RMS difference 0.227378, and browser/native comparison; active state 768,224 B in a fixed 100,663,296 B WASM heap. `make windows-cross-build` passed the MinGW core/offline renderer, not Windows UI/runtime. Ryzen 9 5950X Release, 48 kHz stereo/128 frames (2.6667 ms), 0/20,000 deadline misses: Sine p50/p95/p99/p99.9/max 0.0979/0.1223/0.1526/0.3761/2.5156%; Triangle 0.0979/0.0990/0.1511/0.3296/0.7181%; Envelope 0.0979/0.0990/0.1005/0.3068/0.4448%. These desktop measurements do not predict target-device timing. |",
                "line_count": 1,
            }
        },
    ),
    "LATENCY_AND_QUALITY.md": (
        {
            "span": {
                "start": "+## Fender Tone Master Pro Studio Pan Delay — 2026-11-28",
                "line_count": 13,
            }
        },
    ),
    "include/tabletfx/effects.hpp": (
        '+#include "tabletfx/effects/studio_pan_delay.hpp"',
    ),
    "scripts/publish_checkpoint.py": (
        {
            "span": {
                "hunk_headers": [
                    "@@ -2350,3 +2374,3 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                    "@@ -2350,3 +2350,3 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                ],
                "start": '-    "apps/stereo_panner_bench.cpp",',
                "line_count": 6,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -2354 +2378,2 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                    "@@ -2354 +2354,2 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                ],
                "start": '-    "scripts/run_cmake.py",',
                "line_count": 3,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -2356,2 +2381,3 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                    "@@ -2356,2 +2357,3 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                ],
                "start": '-    "src/effects/stereo_panner.cpp",',
                "line_count": 5,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -2358,0 +2385 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                    "@@ -2358,0 +2361 @@ FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (",
                ],
                "start": '+    "web/audio-worklet.js",',
                "line_count": 1,
            }
        },
    ),
    "scripts/web.py": (
        '+        str(ROOT / "src" / "effects" / "studio_pan_delay.cpp"),',
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/studio_pan_delay.hpp"',
        '-              static_cast<std::uint16_t>(channels))) {',
        '+              static_cast<std::uint16_t>(channels))),',
        '+          studio_pan_delay_(std::make_unique<tabletfx::StudioPanDelay>(',
        '+              static_cast<float>(sample_rate),',
        {
            "replace": {
                "current": "+              tabletfx::StudioPanDelayParameters{.bypass = true})),",
                "selected": "+              tabletfx::StudioPanDelayParameters{.bypass = true})) {",
            }
        },
        "-    return effects_.size() + 28;",
        {
            "replace": {
                "current": "+    return effects_.size() + 33;",
                "selected": "+    return effects_.size() + 29;",
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -628,0 +642,63 @@ public:",
                    "@@ -628,0 +633,6 @@ public:",
                ],
                "start": "+        if (index == 123U) {",
                "line_count": 6,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -995,0 +1072,33 @@ public:",
                    "@@ -995,0 +1006,5 @@ public:",
                ],
                "start": "+        if (index == 123U) {",
                "line_count": 5,
            }
        },
        "+        studio_pan_delay_->reset();",
        {
            "span": {
                "hunk_headers": [
                    "@@ -3933,0 +4078,111 @@ public:",
                    "@@ -3933,0 +3950,15 @@ public:",
                ],
                "start": "+    void configure_studio_pan_delay(float delay_time_ms, float feedback,",
                "line_count": 15,
            }
        },
        "+        bytes += studio_pan_delay_->memory_bytes();",
        {
            "span": {
                "hunk_headers": [
                    "@@ -4350,0 +4609,26 @@ public:",
                    "@@ -4350,0 +4383,6 @@ public:",
                ],
                "start": "+        if (index == 123U) {",
                "line_count": 6,
            }
        },
        "+        case 123U: return studio_pan_delay_enabled_;",
        "+    std::unique_ptr<tabletfx::StudioPanDelay> studio_pan_delay_;",
        "+    bool studio_pan_delay_enabled_ = false;",
        {
            "span": {
                "hunk_headers": [
                    "@@ -6386,0 +6715,39 @@ TABLETFX_WASM_EXPORT void tf_chain_set_resonator(",
                    "@@ -6386,0 +6428,8 @@ TABLETFX_WASM_EXPORT void tf_chain_set_resonator(",
                ],
                "start": "+TABLETFX_WASM_EXPORT void tf_chain_set_studio_pan_delay(",
                "line_count": 8,
            }
        },
    ),
    "web/audio-worklet.js": (
        '+EFFECTS.push("studio-pan-delay");',
        {
            "span": {
                "hunk_headers": [
                    "@@ -651,0 +679,10 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                    "@@ -651,0 +653,3 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                ],
                "start": "+      this.configureStudioPanDelay(",
                "line_count": 3,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -1288,0 +1336,32 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                    "@@ -1288,0 +1293,8 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                ],
                "start": "+  configureStudioPanDelay(values) {",
                "line_count": 8,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -1365,0 +1461,30 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                    "@@ -1365,0 +1378,10 @@ class TabletFxProcessor extends AudioWorkletProcessor {",
                ],
                "start": '+        } else if (index === EFFECTS.indexOf("studio-pan-delay")) {',
                "line_count": 10,
            }
        },
        '+    } else if (message.type === "studio-pan-delay" && this.ready) {',
        "+      this.configureStudioPanDelay(message.value);",
    ),
    "web/app.js": (
        '+  "Studio Pan Delay · project-authored",',
        '+  studioPanDelay: [350, 0.25, 0.25, 0.5, 0.75, 0],',
        {
            "span": {
                "hunk_headers": [
                    "@@ -2454,0 +2541,37 @@ const pitchRingModControls = [",
                    "@@ -2454,0 +2457,8 @@ const pitchRingModControls = [",
                ],
                "start": "+const studioPanDelayControls = [",
                "line_count": 8,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -8232,0 +8444,130 @@ function updatePitchRingMod() {",
                    "@@ -8232,0 +8243,27 @@ function updatePitchRingMod() {",
                ],
                "start": "+function updateStudioPanDelay() {",
                "line_count": 27,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -8490,0 +8837,20 @@ for (const control of pitchRingModControls) {",
                    "@@ -8490,0 +8528,4 @@ for (const control of pitchRingModControls) {",
                ],
                "start": "+for (const control of studioPanDelayControls) {",
                "line_count": 4,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -18526,11 +19485,9 @@ async function runPitchRingModBrowserSmokeTest(",
                    "@@ -18537,0 +18579,9 @@ async function runBrowserSelfTest() {",
                ],
                "start": "+async function runStudioPanDelayBrowserSmokeTest(",
                "line_count": 9,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -18538,2 +19495,4 @@ async function runBrowserSelfTest() {",
                    "@@ -18540,0 +18591,4 @@ async function runBrowserSelfTest() {",
                ],
                "start": "+    await processorReady;",
                "line_count": 4,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -18541,23 +19500,1068 @@ async function runBrowserSelfTest() {",
                    "@@ -18564,0 +18617,149 @@ async function runBrowserSelfTest() {",
                ],
                "start": "+    const row = document.querySelector(`[data-effect=\"${effectIndex}\"]`)?.closest(\".effect-row\");",
                "line_count": 147,
                "start_occurrence": 1,
                "expected_start_count": [1, 3],
            }
        },
        {
            "replace": {
                "before": "-        EFFECT_NAMES.length !== 122 || effectMemory.length !== EFFECT_NAMES.length ||",
                "current": "+        EFFECT_NAMES.length <= effectIndex || effectMemory.length !== EFFECT_NAMES.length ||",
                "selected": "+        EFFECT_NAMES.length <= effectIndex || effectMemory.length !== EFFECT_NAMES.length ||",
                "hunk_headers": [
                    "@@ -18425 +19384 @@ async function runPitchRingModBrowserSmokeTest(",
                    "@@ -18425 +18466 @@ async function runPitchRingModBrowserSmokeTest(",
                ],
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": "-      stopSource(false);",
                "current": "+      await stopBrowserSmokePlaybackAndWait();",
                "selected": "+      await stopBrowserSmokePlaybackAndWait();",
                "hunk_headers": [
                    "@@ -18468 +19427 @@ async function runPitchRingModBrowserSmokeTest(",
                    "@@ -18468 +18509 @@ async function runPitchRingModBrowserSmokeTest(",
                ],
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": "-    const expectedEffectIds = Array.from({ length: 123 }, (_, index) => index);",
                "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "selected": "+    const expectedEffectIds = Array.from({ length: 124 }, (_, index) => index);",
                "hunk_headers": [
                    "@@ -18541,23 +19500,1068 @@ async function runBrowserSelfTest() {",
                    "@@ -18564,0 +18617,149 @@ async function runBrowserSelfTest() {",
                ],
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": "-    if (EFFECT_NAMES.length !== 123 || effectRows.length !== 123 ||",
                "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "selected": "+    if (EFFECT_NAMES.length !== 124 || effectRows.length !== 124 ||",
                "hunk_headers": [
                    "@@ -18541,23 +19500,1068 @@ async function runBrowserSelfTest() {",
                    "@@ -18564,0 +18617,149 @@ async function runBrowserSelfTest() {",
                ],
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -18680,0 +20685,9 @@ async function runBrowserSelfTest() {",
                    "@@ -18680,0 +18882,4 @@ async function runBrowserSelfTest() {",
                ],
                "start": '+                   !EFFECT_NAMES[123]?.startsWith("Studio Pan Delay · project-authored") ||',
                "line_count": 4,
            }
        },
        {
            "replace": {
                "before": '-                     throw new Error(`The effect picker must list 123 ordered, categorized effects (IDs 0–122), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, Memory Delay, both Tone Master halls, Pitch Ring Mod, and Fractal Resonator; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[122]}, row122=${effectRows[122]?.dataset.category}/${effectRows[122]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[122]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "current": '+                     throw new Error(`The effect picker must list 126 ordered, categorized effects (IDs 0–125), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, both Tone Master halls, Pitch Ring Mod, Fractal Resonator, Studio Pan Delay, Essex Vibe, and Euclidean Delay; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[125]}, row125=${effectRows[125]?.dataset.category}/${effectRows[125]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[125]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "selected": '+                     throw new Error(`The effect picker must list 124 ordered, categorized effects (IDs 0–123), including Fractal Diatonic Harmony, 3D Chorus, Surround Delay, both Tone Master halls, Pitch Ring Mod, Fractal Resonator, and Studio Pan Delay; existing effect IDs and optional-disabled defaults must stay stable. names=${EFFECT_NAMES.length}, rows=${effectRows.length}, ids=${effectIds.length}/${effectIds.at(-1)}, memory=${effectMemory.length}/${effectMemory[123]}, row123=${effectRows[123]?.dataset.category}/${effectRows[123]?.querySelector(".effect-category-tag")?.textContent}, enabled=${initialEffectState[123]}, audio=${audioStatus.textContent}/${audioStatus.dataset.state}.`);',
                "hunk_headers": [
                    "@@ -18722 +20744 @@ async function runBrowserSelfTest() {",
                    "@@ -18722 +18927 @@ async function runBrowserSelfTest() {",
                ],
                "allow_cross_hunk_staged": True,
            }
        },
        "+    parityEnabled[123] = false;",
        {
            "span": {
                "hunk_headers": [
                    "@@ -22168,0 +24443,12 @@ async function runBrowserSelfTest() {",
                    "@@ -22168,0 +22375,4 @@ async function runBrowserSelfTest() {",
                ],
                "start": '+      testResult.textContent = "Checking Studio Pan Delay with local guitar playback…";',
                "line_count": 4,
            }
        },
        '+      studioPanDelay: studioPanDelaySmoke,',
    ),
    "research-state.json": (
        {
            "span": {
                "hunk_headers": [
                    "@@ -37,0 +49,39 @@",
                    "@@ -37,0 +38,24 @@",
                ],
                "start": "+    {",
                "line_count": 24,
                "start_occurrence": 1,
                "expected_start_count": [1, 3],
            }
        },
        {
            "replace": {
                "before": "-    \"next_feature\": \"Audit the existing Multitap Delay at category Delay against the November 2026 Blocks Guide, then implement its documented Global Mix-eligible controls as a project-only feature where semantics are clear. Continue Phaser, Pitch, Plex Delay, Resonator, Reverb, Synth, and Ten-Tap Delay one at a time. Verify each model-specific Mix law and algorithm before implementing; do not infer Fractal settings or parity. Then continue exact-model HeadRush Prime, Melofty AXOM MAX, Fender Tone Master Pro, Line 6 Helix Stadium XL, and XRecer XR1 gaps. Do not add Prime Global EQ before independent output routes exist or XR1 FX loops before a real routing host exists.\",",
                "current": "+    \"next_feature\": \"After publishing the verified Studio Pan Delay ID 123 checkpoint, publish the already-integrated Essex Vibe ID 124, Helix Stadium Euclidean Delay ID 125, and Fender Cirrostratus Synthverb ID 126 in stable picker order, one tested checkpoint at a time. Do not start another picker entry until the current commit is on main.\",",
                "selected": "+    \"next_feature\": \"After this Studio Pan Delay ID 123 checkpoint reaches main, publish the local Essex Vibe at ID 124 as the next standalone checkpoint; continue stable picker order only after each commit is on main.\",",
                "hunk_header": "@@ -33,3 +44,3 @@",
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
    ),
    "web/index.html": (
        '+        <label class="effect-row" data-category="delay"><input type="checkbox" data-effect="123"><span class="effect-number">124</span><span class="effect-name">Studio Pan Delay · project-authored</span><span class="effect-default">350 ms · 25% wet · 0.5 Hz pan · disabled by default</span><span class="effect-category-tag">Delay</span></label>',
        {
            "span": {
                "hunk_headers": [
                    "@@ -662,0 +675,53 @@",
                    "@@ -662,0 +664,12 @@",
                ],
                "start": '+      <div class="effect-parameter-group" aria-label="Studio Pan Delay controls">',
                "line_count": 12,
            }
        },
    ),
}


class LineSelectionError(ValueError):
    """Raised when an exact checkpoint line operation is missing or ambiguous."""


_HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$")


def select_exact_line_operations(
    diff: str, operations: tuple[LineSelectorOperation, ...]
) -> str:
    """Select exact +/- lines and bounded marker-to-marker spans.

    A string operation is the complete diff line, including ``+`` or ``-``.
    A marker span is ``{"span": {"start": "+...", "end": "+..."}}``;
    both markers must occur exactly once in one zero-context hunk. A span can
    instead use ``line_count`` to select a fixed number of diff operations
    beginning with its unique start marker. A bounded span can also name an
    closing line. Every selected line must be an added/deleted operation;
    ``end_exclusive`` may leave the end marker out of the selected range. An
    exact ``hunk_header`` or stable ``hunk_context`` prefix and a 1-based
    ``end_occurrence`` for a repeated closing line. Every selected line must be an added/deleted operation;
    optional ``expected_line_count`` makes marker boundaries fail closed if
    the block changes. Missing, ambiguous, overlapping, or stale selectors
    fail closed.
    """
    if not operations:
        raise LineSelectionError("line selector has no operations")
    exact_operations = tuple(operation for operation in operations if isinstance(operation, str))
    replacements = tuple(
        operation["replace"] for operation in operations
        if isinstance(operation, dict) and "replace" in operation
    )
    scoped_lines = tuple(
        operation["line"] for operation in operations
        if isinstance(operation, dict) and "line" in operation
    )
    spans = tuple(
        operation["span"] for operation in operations
        if isinstance(operation, dict) and "span" in operation
    )
    ranges = tuple(
        operation["range"] for operation in operations
        if isinstance(operation, dict) and "range" in operation
    )
    if (
        len(exact_operations) + len(spans) + len(ranges) + len(replacements)
        + len(scoped_lines) != len(operations)
    ):
        raise LineSelectionError("unsupported selector operation")
    if any(not operation.startswith(("+", "-")) or operation.startswith(("+++", "---"))
           for operation in exact_operations):
        raise LineSelectionError("operations must be complete added/deleted diff lines")
    for span in spans:
        if not isinstance(span, dict):
            raise LineSelectionError("span selector must be an object")
        if not isinstance(span.get("start"), str) or not span["start"].startswith(("+", "-")):
            raise LineSelectionError("span selectors require an exact added/deleted start line")
        start_occurrence = span.get("start_occurrence", 1)
        expected_start_count = span.get("expected_start_count")
        if not isinstance(start_occurrence, int) or start_occurrence < 1:
            raise LineSelectionError("span start_occurrence must be a positive integer")
        valid_start_counts = (
            expected_start_count if isinstance(expected_start_count, (list, tuple))
            else (expected_start_count,)
        )
        if expected_start_count is not None and (
            not valid_start_counts
            or any(not isinstance(count, int) or count < 1 for count in valid_start_counts)
            or len(set(valid_start_counts)) != len(valid_start_counts)
        ):
            raise LineSelectionError("span expected_start_count must be a positive integer")
        if (
            "start_occurrence" in span or expected_start_count is not None
        ) and span.get("hunk_header") is None and span.get("hunk_headers") is None:
            raise LineSelectionError("repeated span starts require an exact hunk scope")
        line_count = span.get("line_count")
        if line_count is not None:
            if not isinstance(line_count, int) or line_count < 1:
                raise LineSelectionError("span line_count must be a positive integer")
            if any(key in span for key in (
                "end", "end_occurrence", "end_after_start", "end_exclusive",
                "expected_line_count",
            )):
                raise LineSelectionError("line-count spans cannot use end-marker options")
        elif (
            not isinstance(span.get("end"), str)
            or not span["end"].startswith(("+", "-"))
        ):
            raise LineSelectionError("marker spans require an exact added/deleted end line")
        occurrence = span.get("end_occurrence")
        if occurrence is None and line_count is None:
            occurrence = 1
        if occurrence is not None and (not isinstance(occurrence, int) or occurrence < 1):
            raise LineSelectionError("span end_occurrence must be a positive integer")
        end_after_start = span.get("end_after_start", False)
        if not isinstance(end_after_start, bool):
            raise LineSelectionError("span end_after_start must be boolean")
        if end_after_start and "end_occurrence" in span:
            raise LineSelectionError(
                "span cannot combine end_after_start with end_occurrence"
            )
        expected_count = span.get("expected_line_count")
        if expected_count is not None and (
            not isinstance(expected_count, int) or expected_count < 1
        ):
            raise LineSelectionError("span expected_line_count must be a positive integer")
        if "end_exclusive" in span and not isinstance(span["end_exclusive"], bool):
            raise LineSelectionError("span end_exclusive must be boolean")
        if span.get("hunk_header") is not None and not isinstance(span["hunk_header"], str):
            raise LineSelectionError("span hunk_header must be a string")
        hunk_headers = span.get("hunk_headers")
        if hunk_headers is not None and (
            not isinstance(hunk_headers, (tuple, list))
            or not hunk_headers
            or any(not isinstance(header, str) for header in hunk_headers)
            or len(set(hunk_headers)) != len(hunk_headers)
            or span.get("hunk_header") is not None
        ):
            raise LineSelectionError("span hunk_headers must be a unique non-empty list")
        hunk_context = span.get("hunk_context")
        if hunk_context is not None and (
            not isinstance(hunk_context, str)
            or not hunk_context
            or span.get("hunk_header") is not None
            or hunk_headers is not None
        ):
            raise LineSelectionError("span hunk_context must be non-empty and exclusive")
    for selected_range in ranges:
        if (
            not isinstance(selected_range, dict)
            or not isinstance(selected_range.get("hunk_header"), str)
            or not isinstance(selected_range.get("start_index"), int)
            or not isinstance(selected_range.get("end_index"), int)
            or selected_range["start_index"] < 0
            or selected_range["end_index"] <= selected_range["start_index"]
            or not isinstance(selected_range.get("sha256"), str)
            or len(selected_range["sha256"]) != 64
            or any(ch not in "0123456789abcdef" for ch in selected_range["sha256"])
        ):
            raise LineSelectionError("range selectors require a hunk, indices, and SHA-256")
    for replacement in replacements:
        if (
            not isinstance(replacement, dict)
            or not isinstance(replacement.get("current"), str)
            or not isinstance(replacement.get("selected"), str)
            or not replacement["current"].startswith("+")
            or not replacement["selected"].startswith("+")
            or replacement["current"].startswith("+++")
            or replacement["selected"].startswith("+++")
            or (
                "before" in replacement
                and (
                    not isinstance(replacement["before"], str)
                    or not replacement["before"].startswith("-")
                    or replacement["before"].startswith("---")
                )
            )
            or (
                "hunk_header" in replacement
                and not isinstance(replacement["hunk_header"], str)
            )
            or (
                "hunk_headers" in replacement
                and (
                    not isinstance(replacement["hunk_headers"], (tuple, list))
                    or not replacement["hunk_headers"]
                    or any(not isinstance(header, str) for header in replacement["hunk_headers"])
                    or len(set(replacement["hunk_headers"])) != len(replacement["hunk_headers"])
                    or "hunk_header" in replacement
                )
            )
            or (
                "current_occurrence" in replacement
                and (
                    not isinstance(replacement["current_occurrence"], int)
                    or replacement["current_occurrence"] < 1
                )
            )
            or (
                "expected_current_count" in replacement
                and (
                    not isinstance(replacement["expected_current_count"], int)
                    or replacement["expected_current_count"] < 1
                )
            )
            or (
                ("scope_start" in replacement) != ("scope_end" in replacement)
                or (
                    "scope_start" in replacement
                    and (
                        not isinstance(replacement["scope_start"], str)
                        or not replacement["scope_start"].startswith(("+", "-"))
                        or not isinstance(replacement["scope_end"], str)
                        or not replacement["scope_end"].startswith(("+", "-"))
                    )
                )
            )
            or (
                "allow_intervening_added_lines" in replacement
                and not isinstance(replacement["allow_intervening_added_lines"], bool)
            )
            or (
                "allow_intervening_changes" in replacement
                and not isinstance(replacement["allow_intervening_changes"], bool)
            )
            or (
                "allow_cross_hunk_staged" in replacement
                and not isinstance(replacement["allow_cross_hunk_staged"], bool)
            )
            or (
                "allow_staged_value_without_scope" in replacement
                and not isinstance(replacement["allow_staged_value_without_scope"], bool)
            )
            or (
                replacement.get("allow_cross_hunk_staged", False)
                and (
                    "before" not in replacement
                    or ("hunk_header" not in replacement and "hunk_headers" not in replacement)
                )
            )
            or (
                replacement.get("allow_intervening_added_lines", False)
                and (
                    "before" not in replacement
                    or ("hunk_header" not in replacement and "hunk_headers" not in replacement)
                )
            )
            or (
                replacement.get("allow_intervening_changes", False)
                and (
                    "before" not in replacement
                    or ("hunk_header" not in replacement and "hunk_headers" not in replacement)
                )
            )
        ):
            raise LineSelectionError(
                "replacement selectors require an exact added line and optional deletion"
            )
    for scoped_line in scoped_lines:
        if (
            not isinstance(scoped_line, dict)
            or not isinstance(scoped_line.get("operation"), str)
            or not scoped_line["operation"].startswith(("+", "-"))
            or scoped_line["operation"].startswith(("+++", "---"))
            or not isinstance(scoped_line.get("hunk_context"), str)
            or not scoped_line["hunk_context"]
            or not isinstance(scoped_line.get("expected_count"), int)
            or scoped_line["expected_count"] < 1
        ):
            raise LineSelectionError(
                "scoped line selectors require an exact operation, hunk context, and positive count"
            )

    header: list[str] = []
    hunks: list[tuple[str, list[str]]] = []
    current_header: str | None = None
    current_body: list[str] = []
    for line in diff.splitlines(keepends=True):
        if line.startswith("@@"):
            if current_header is not None:
                hunks.append((current_header, current_body))
            current_header = line
            current_body = []
        elif current_header is None:
            header.append(line)
        else:
            current_body.append(line)
    if current_header is not None:
        hunks.append((current_header, current_body))
    if not hunks:
        raise LineSelectionError("diff contains no change hunks")

    wanted = Counter(exact_operations)
    observed = Counter()
    selected_coordinates: set[tuple[int, int]] = set()
    for hunk_index, (_, body) in enumerate(hunks):
        for line_index, line in enumerate(body):
            operation = line.rstrip("\n")
            if operation in wanted:
                observed[operation] += 1
                selected_coordinates.add((hunk_index, line_index))
    invalid = {
        operation: {"expected": count, "found": observed.get(operation, 0)}
        for operation, count in wanted.items()
        if observed.get(operation, 0) != count
    }
    if invalid:
        raise LineSelectionError(
            "each exact line operation must occur at its listed multiplicity; counts=" + repr(invalid)
        )
    exact_selected_coordinates = set(selected_coordinates)

    for scoped_line in scoped_lines:
        matches = [
            (hunk_index, line_index)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if scoped_line["hunk_context"] in header_line
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == scoped_line["operation"]
        ]
        if len(matches) != scoped_line["expected_count"]:
            raise LineSelectionError(
                "scoped line multiplicity changed; "
                f"context={scoped_line['hunk_context']!r}, "
                f"expected={scoped_line['expected_count']}, found={len(matches)}"
            )
        selected_coordinates.update(matches)

    replacement_by_coordinate: dict[tuple[int, int], str] = {}
    replacement_coordinates: set[tuple[int, int]] = set()
    for replacement in replacements:
        scoped_header = replacement.get("hunk_header")
        scoped_headers = replacement.get("hunk_headers", ())
        scope_bounds: tuple[int, int, int] | None = None
        if "scope_start" in replacement:
            current_value_present = any(
                line.rstrip("\n") == replacement["current"]
                for header_line, body in hunks
                if (
                    (scoped_header is None and not scoped_headers)
                    or header_line.rstrip("\n") == scoped_header
                    or header_line.rstrip("\n") in scoped_headers
                )
                for line in body
            )
            if not (
                not current_value_present
                and replacement.get("allow_staged_value_without_scope", False)
            ):
                scope_markers = []
                for marker in (replacement["scope_start"], replacement["scope_end"]):
                    marker_matches = [
                        (hunk_index, line_index)
                        for hunk_index, (header_line, body) in enumerate(hunks)
                        if (
                            (scoped_header is None and not scoped_headers)
                            or header_line.rstrip("\n") == scoped_header
                            or header_line.rstrip("\n") in scoped_headers
                        )
                        for line_index, line in enumerate(body)
                        if line.rstrip("\n") == marker
                    ]
                    if len(marker_matches) != 1:
                        raise LineSelectionError(
                            "replacement scope markers must each occur exactly once; "
                            f"marker={marker!r}, found={len(marker_matches)}"
                        )
                    scope_markers.append(marker_matches[0])
                if (
                    scope_markers[0][0] != scope_markers[1][0]
                    or scope_markers[0][1] > scope_markers[1][1]
                ):
                    raise LineSelectionError(
                        "replacement scope markers must each occur exactly once; "
                        f"marker={marker!r}, found={len(marker_matches)}"
                    )
                scope_markers.append(marker_matches[0])
            if (
                scope_markers[0][0] != scope_markers[1][0]
                or scope_markers[0][1] > scope_markers[1][1]
            ):
                raise LineSelectionError(
                    "replacement scope markers must bound one ordered hunk span"
                )
            scope_bounds = (
                scope_markers[0][0], scope_markers[0][1], scope_markers[1][1]
            )

        def in_replacement_scope(hunk_index: int, line_index: int) -> bool:
            return scope_bounds is None or (
                hunk_index == scope_bounds[0]
                and scope_bounds[1] <= line_index <= scope_bounds[2]
            )

        current_matches = [
            (hunk_index, line_index)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if (
                (scoped_header is None and not scoped_headers)
                or header_line.rstrip("\n") == scoped_header
                or header_line.rstrip("\n") in scoped_headers
            )
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == replacement["current"]
            and in_replacement_scope(hunk_index, line_index)
        ]
        if not current_matches:
            # Verification receives the staged, already-selected value. Accept
            # that exact value idempotently so plan, stage, and verify share
            # one selector without broadening its whitelist.
            allow_cross_hunk_staged = replacement.get(
                "allow_cross_hunk_staged", False
            )
            selected_matches = [
                (hunk_index, line_index)
                for hunk_index, (header_line, body) in enumerate(hunks)
                if allow_cross_hunk_staged
                or (scoped_header is None and not scoped_headers)
                or header_line.rstrip("\n") == scoped_header
                or header_line.rstrip("\n") in scoped_headers
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == replacement["selected"]
                and in_replacement_scope(hunk_index, line_index)
            ]
            if (
                replacement.get("allow_staged_value_without_scope", False)
                and "scope_start" in replacement
                and not current_matches
                and len(selected_matches) != 1
            ):
                raise LineSelectionError(
                    "staged replacement value must occur exactly once without its scope markers; "
                    f"found={len(selected_matches)}"
                )
            if allow_cross_hunk_staged and len(selected_matches) != 1:
                raise LineSelectionError(
                    "staged replacement value must occur exactly once; "
                    f"found={len(selected_matches)}"
                )
            if len(selected_matches) == 1:
                selected_coordinate = selected_matches[0]
                if "before" in replacement:
                    before_matches = [
                        (hunk_index, line_index)
                        for hunk_index, (header_line, body) in enumerate(hunks)
                        if allow_cross_hunk_staged
                        or (scoped_header is None and not scoped_headers)
                        or header_line.rstrip("\n") == scoped_header
                        or header_line.rstrip("\n") in scoped_headers
                        for line_index, line in enumerate(body)
                        if line.rstrip("\n") == replacement["before"]
                    ]
                    if len(before_matches) != 1:
                        raise LineSelectionError(
                            "replacement deleted line must occur exactly once"
                        )
                    before_coordinate = before_matches[0]
                    if before_coordinate[0] != selected_coordinate[0]:
                        if not allow_cross_hunk_staged:
                            raise LineSelectionError(
                                "replacement lines must be in the same diff hunk"
                            )
                        if selected_coordinate[0] != before_coordinate[0] + 1:
                            raise LineSelectionError(
                                "cross-hunk staged replacement must use adjacent hunks"
                            )
                    elif abs(before_coordinate[1] - selected_coordinate[1]) != 1:
                        raise LineSelectionError(
                            "staged replacement lines must be adjacent in the same diff hunk"
                        )
                    replacement_coordinates.add(before_coordinate)
                replacement_by_coordinate[selected_coordinate] = replacement["selected"]
                replacement_coordinates.add(selected_coordinate)
                continue
        expected_current_count = replacement.get("expected_current_count")
        if expected_current_count is not None and len(current_matches) != expected_current_count:
            raise LineSelectionError(
                "replacement current-line multiplicity changed; "
                f"expected={expected_current_count}, found={len(current_matches)}"
            )
        current_occurrence = replacement.get("current_occurrence", 1)
        if len(current_matches) < current_occurrence:
            raise LineSelectionError(
                "replacement current line occurrence is missing; "
                f"current={replacement['current']!r}; hunk={scoped_header or scoped_headers!r}"
            )
        if (
            len(current_matches) > 1
            and "current_occurrence" not in replacement
            and expected_current_count is None
        ):
            raise LineSelectionError(
                "replacement current line must occur exactly once unless occurrence is scoped"
            )
        if current_occurrence > 1 and expected_current_count is None and scoped_header is None:
            raise LineSelectionError(
                "repeated replacement current lines require a hunk scope or expected count"
            )
        current_coordinate = current_matches[current_occurrence - 1]
        if "before" in replacement:
            before_matches = [
                (hunk_index, line_index)
                for hunk_index, (header_line, body) in enumerate(hunks)
                if (
                    (scoped_header is None and not scoped_headers)
                    or header_line.rstrip("\n") == scoped_header
                    or header_line.rstrip("\n") in scoped_headers
                )
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == replacement["before"]
            ]
            if len(before_matches) != 1:
                raise LineSelectionError(
                    "replacement deleted line must occur exactly once"
                )
            before_coordinate = before_matches[0]
            same_hunk = before_coordinate[0] == current_coordinate[0]
            adjacent = abs(before_coordinate[1] - current_coordinate[1]) == 1
            if not same_hunk:
                raise LineSelectionError(
                    "replacement lines must be in the same diff hunk"
                )
            if not adjacent:
                allow_added_only = replacement.get("allow_intervening_added_lines", False)
                allow_any_changes = replacement.get("allow_intervening_changes", False)
                if not allow_added_only and not allow_any_changes:
                    raise LineSelectionError(
                        "replacement lines must be adjacent in the same diff hunk"
                    )
                first = min(before_coordinate[1], current_coordinate[1])
                last = max(before_coordinate[1], current_coordinate[1])
                intervening = hunks[before_coordinate[0]][1][first + 1:last]
                if allow_added_only and (not intervening or any(
                    not line.startswith("+") or line.startswith("+++")
                    for line in intervening
                )):
                    raise LineSelectionError(
                        "intervening replacement lines must be additions"
                    )
            replacement_coordinates.add(before_coordinate)
        if current_coordinate in replacement_by_coordinate:
            raise LineSelectionError("replacement targets overlap")
        replacement_by_coordinate[current_coordinate] = replacement["selected"]
        replacement_coordinates.add(current_coordinate)

    for span in spans:
        start_marker = span["start"]
        eligible_hunks = [
            (hunk_index, header_line, body)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if (
                span.get("hunk_header") is None
                and span.get("hunk_headers") is None
                and span.get("hunk_context") is None
            )
            or header_line.rstrip("\n") == span.get("hunk_header")
            or header_line.rstrip("\n") in span.get("hunk_headers", ())
                span.get("hunk_context") is not None
                and header_line.rstrip("\n").startswith(span["hunk_context"])
        ]
        if (
            span.get("hunk_header") is not None
            or span.get("hunk_headers") is not None
            or span.get("hunk_context") is not None
        ) and len(eligible_hunks) != 1:
            raise LineSelectionError(
                "span hunk header must occur once; "
                f"expected={span.get('hunk_header') or span.get('hunk_headers')!r}; "
                f"found={len(eligible_hunks)}"
            )
        start_matches = [
            (hunk_index, line_index)
            for hunk_index, _, body in eligible_hunks
            for line_index, line in enumerate(body)
            if line.rstrip("\n") == start_marker
        ]
        expected_start_count = span.get("expected_start_count")
        expected_start_counts = (
            set(expected_start_count)
            if isinstance(expected_start_count, (list, tuple))
            else {expected_start_count}
        ) if expected_start_count is not None else None
        if expected_start_counts is not None and len(start_matches) not in expected_start_counts:
            raise LineSelectionError(
                "span start marker multiplicity changed; "
                f"expected={sorted(expected_start_counts)}, found={len(start_matches)}"
            )
        start_occurrence = span.get("start_occurrence", 1)
        if len(start_matches) < start_occurrence or (
            start_occurrence == 1 and expected_start_count is None and len(start_matches) != 1
        ):
            raise LineSelectionError(
                f"span start marker must occur once; found={len(start_matches)}; "
                f"marker={start_marker!r}; hunk={span.get('hunk_header')!r}"
            )
        start_hunk, start_index = start_matches[start_occurrence - 1]
        if "line_count" in span:
            end_hunk = start_hunk
            end_index = start_index + span["line_count"] - 1
            if end_index >= len(hunks[start_hunk][1]):
                raise LineSelectionError(
                    "line-count span extends beyond its zero-context hunk"
                )
        else:
            end_marker = span["end"]
            end_matches = [
                (hunk_index, line_index)
                for hunk_index, _, body in eligible_hunks
                for line_index, line in enumerate(body)
                if line.rstrip("\n") == end_marker
            ]
            if span.get("end_after_start", False):
                following_ends = [
                    (hunk_index, line_index)
                    for hunk_index, line_index in end_matches
                    if hunk_index == start_hunk and line_index > start_index
                ]
                if not following_ends:
                    raise LineSelectionError(
                        "span end marker missing after its start marker in the same hunk"
                    )
                end_hunk, end_index = following_ends[0]
            else:
                has_explicit_end_occurrence = "end_occurrence" in span
                end_occurrence = span.get("end_occurrence", 1)
                if not has_explicit_end_occurrence and len(end_matches) != 1:
                    raise LineSelectionError(
                        "span markers must each occur once; "
                        f"start=1, end={len(end_matches)}"
                    )
                if len(end_matches) < end_occurrence:
                    raise LineSelectionError(
                        f"span end marker missing; found={len(end_matches)}, "
                        f"requested occurrence={end_occurrence}"
                    )
                end_hunk, end_index = end_matches[end_occurrence - 1]
        if start_hunk != end_hunk or end_index < start_index:
            raise LineSelectionError("span markers must be ordered within the same hunk")
        end_exclusive = span.get("end_exclusive", False)
        selected_end = end_index if end_exclusive else end_index + 1
        if selected_end <= start_index:
            raise LineSelectionError("span contains no selected change lines")
        expected_count = span.get("expected_line_count")
        if expected_count is not None and selected_end - start_index != expected_count:
            raise LineSelectionError(
                f"span length changed; expected={expected_count}, found={selected_end - start_index}"
            )
        body = hunks[start_hunk][1]
        for line_index in range(start_index, selected_end):
            line = body[line_index]
            if not line.startswith(("+", "-")) or line.startswith(("+++", "---")):
                raise LineSelectionError("span includes a non-change diff line")
            coordinate = (start_hunk, line_index)
            if coordinate in selected_coordinates:
                raise LineSelectionError("selector operations overlap")
            selected_coordinates.add(coordinate)

    for selected_range in ranges:
        matching_hunks = [
            (hunk_index, body)
            for hunk_index, (header_line, body) in enumerate(hunks)
            if header_line.rstrip("\n") == selected_range["hunk_header"]
        ]
        if len(matching_hunks) != 1:
            raise LineSelectionError(
                f"range hunk header must occur once; found={len(matching_hunks)}"
            )
        hunk_index, body = matching_hunks[0]
        start_index = selected_range["start_index"]
        end_index = selected_range["end_index"]
        if end_index > len(body):
            raise LineSelectionError("range extends beyond its zero-context hunk")
        selected_lines = body[start_index:end_index]
        if any(
            not line.startswith(("+", "-")) or line.startswith(("+++", "---"))
            for line in selected_lines
        ):
            raise LineSelectionError("range includes a non-change diff line")
        actual_hash = hashlib.sha256("".join(selected_lines).encode("utf-8")).hexdigest()
        if actual_hash != selected_range["sha256"]:
            raise LineSelectionError("range contents do not match their SHA-256 whitelist")
        for line_index in range(start_index, end_index):
            coordinate = (hunk_index, line_index)
            if coordinate in selected_coordinates:
                raise LineSelectionError("selector operations overlap")
            selected_coordinates.add(coordinate)

    if replacement_coordinates & exact_selected_coordinates:
        raise LineSelectionError("replacement overlaps an exact line operation")
    selected_coordinates.update(replacement_coordinates)

    selected_hunks: list[str] = []
    selected_line_delta = 0

    def format_range(start: int, count: int) -> str:
        return str(start) if count == 1 else f"{start},{count}"

    for hunk_index, (hunk_header, body) in enumerate(hunks):
        match = _HUNK_RE.match(hunk_header.rstrip("\n"))
        if match is None:
            raise LineSelectionError("cannot parse unified diff hunk header")
        old_cursor = int(match.group(1))
        new_cursor = int(match.group(3))
        suffix = match.group(5)
        groups: list[tuple[int, int, list[str], int, int]] = []
        group_lines: list[str] = []
        group_old_start = 0
        group_delta_before = 0
        group_first_sign = ""
        group_deleted = 0
        group_added = 0

        def flush_group() -> None:
            nonlocal group_lines, group_deleted, group_added, group_first_sign
            if group_lines:
                if group_first_sign == "+":
                    group_new_start = group_old_start + group_delta_before + 1
                else:
                    group_new_start = (
                        group_old_start + group_delta_before - (0 if group_added else 1)
                    )
                groups.append((group_old_start, group_new_start, group_lines,
                               group_deleted, group_added))
            group_lines = []
            group_deleted = 0
            group_added = 0
            group_first_sign = ""

        for line_index, line in enumerate(body):
            if line.startswith("\\"):
                if group_lines:
                    group_lines.append(line)
                continue
            is_added = line.startswith("+") and not line.startswith("+++")
            is_deleted = line.startswith("-") and not line.startswith("---")
            if is_added or is_deleted:
                selected = (hunk_index, line_index) in selected_coordinates
                if selected:
                    if not group_lines:
                        group_old_start = old_cursor
                        group_delta_before = selected_line_delta
                        group_first_sign = "+" if is_added else "-"
                    selected_line = replacement_by_coordinate.get(
                        (hunk_index, line_index), line
                    )
                    if line.endswith("\n") and not selected_line.endswith("\n"):
                        selected_line += "\n"
                    group_lines.append(selected_line)
                    if is_added:
                        group_added += 1
                        selected_line_delta += 1
                    else:
                        group_deleted += 1
                        selected_line_delta -= 1
                else:
                    flush_group()
                if is_added:
                    new_cursor += 1
                else:
                    old_cursor += 1
                continue
            flush_group()
            if line.startswith(" "):
                old_cursor += 1
                new_cursor += 1

        flush_group()
        for old_start, new_start, lines, deleted, added in groups:
            selected_hunks.append(
                f"@@ -{format_range(old_start, deleted)} "
                f"+{format_range(new_start, added)} @@{suffix}\n"
                + "".join(lines)
            )

    if not selected_hunks:
        raise LineSelectionError("no exact selector operations matched")
    return "".join(header + selected_hunks)


PARTIAL_CHECKPOINT_LINE_SELECTORS["fractal-resonator-effect"].update({
    "web/audio-worklet.js": (
        '+EFFECTS.push("resonator");',
        {"span": {
            "start": "+      this.configureResonator(",
            "line_count": 11,
        }},
        {"span": {
            "start": "+  configureResonator(values) {",
            "line_count": 16,
        }},
        '+    } else if (message.type === "resonator" && this.ready) {',
        "+      this.configureResonator(message.value);",
        {
            "span": {
                "start": '-      this.port.postMessage({ type: "test-barrier-ack", token: message.token });',
                "line_count": 7,
            }
        },
    ),
    "web/index.html": (
        '+        <label class="effect-row" data-category="filter"><input type="checkbox" data-effect="122"><span class="effect-number">123</span><span class="effect-name">Fractal Resonator · project-authored, not a pitch-tracked synth</span><span class="effect-default">Four feedback-fed band-pass voices · disabled by default</span><span class="effect-category-tag">Filters &amp; wah</span></label>',
        {"span": {
            "start": "+      <div class=\"effect-parameter-group\" aria-label=\"Fractal Resonator controls\">",
            "line_count": 60,
        }},
    ),
    "research-state.json": (
        {
            "span": {
                "start": '+      "name": "Fractal FM9-family Resonator block — project-authored four-voice filter",',
                "line_count": 23,
            }
        },
    ),
    "make/publish.mk": (
        "+.PHONY: publish-fractal-resonator-effect-plan stage-fractal-resonator-effect publish-fractal-resonator-effect",
        "+publish-fractal-resonator-effect-plan:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "fractal-resonator-effect"',
        "+stage-fractal-resonator-effect:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py stage --checkpoint "fractal-resonator-effect"',
        "+publish-fractal-resonator-effect:",
        '+\t$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "fractal-resonator-effect"',
    ),
    "scripts/publish_checkpoint.py": (
        {
            "span": {
                "start": "+FRACTAL_RESONATOR_EFFECT_CHECKPOINT_PATHS = (",
                "end": "+)",
                "end_occurrence": 1,
                "expected_line_count": 21,
            }
        },
        {
            "span": {
                "start": '+CHECKPOINTS["fractal-resonator-effect"] = (',
                "end": "+)  # publish this feature as one bounded primary-branch checkpoint",
                "expected_line_count": 4,
            }
        },
    ),
})


PARTIAL_CHECKPOINT_LINE_SELECTORS["essex-vibe"] = {
    "CMakeLists.txt": (
        "+    src/effects/essex_vibe.cpp",
        "+add_executable(tabletfx_essex_vibe_bench apps/essex_vibe_bench.cpp)",
        "+target_link_libraries(tabletfx_essex_vibe_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_essex_vibe_tests tests/test_essex_vibe.cpp)",
        "+    target_link_libraries(tabletfx_essex_vibe_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_essex_vibe_tests COMMAND tabletfx_essex_vibe_tests)",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        {"span": {"start": "+## Line 6 Helix Stadium XL — Essex Vibe implementation checkpoint (2026-11-27)", "line_count": 5}},
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        {"span": {"start": "+## Line 6 Helix Stadium XL — Essex Vibe (2026-11-27)", "line_count": 8}},
    ),
    "LATENCY_AND_QUALITY.md": (
        {"span": {"start": "+## Line 6 Helix Stadium Essex Vibe — 2026-11-27", "line_count": 3}},
        {"span": {"start": "+## Essex Vibe browser/WASM playback checkpoint (2026-11-27)", "line_count": 3}},
        {"span": {"start": "+## Essex Vibe processor optimization rerun (2026-11-28)", "line_count": 3}},
    ),
    "make/publish.mk": (
        "+PUBLISH_CHECKPOINTS += essex-vibe",
    ),
    "research-state.json": (
        {"span": {"start": '+      "feature": "Essex Vibe categorized browser/WASM processor",', "line_count": 30}},
    ),
    "scripts/publish_checkpoint.py": (
        {"span": {"start": "+ESSEX_VIBE_CHECKPOINT_PATHS = (", "end": "+)", "end_after_start": True}},
        {"span": {"start": '+CHECKPOINTS["essex-vibe"] = (', "end": "+)", "end_after_start": True}},
        {"span": {"start": '+PARTIAL_CHECKPOINT_HUNKS["essex-vibe"] = {', "end": "+}", "end_after_start": True}},
    ),
    "scripts/run_cmake.py": (
        '+            "bench-essex-vibe",',
        '+    elif action == "bench-essex-vibe":',
        "+        bench_essex_vibe(cmake)",
    ),
    "scripts/web.py": (
        '+        "_tf_chain_set_essex_vibe",',
        '+        str(ROOT / "src" / "effects" / "essex_vibe.cpp"),',
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/essex_vibe.hpp"',
        {"span": {"start": "+          essex_vibe_(std::make_unique<tabletfx::EssexVibe>(", "line_count": 2}},
        {"span": {"hunk_headers": ["@@ -638,0 +648,57 @@ public:", "@@ -638,0 +642,5 @@ public:"], "start": "+        if (index == 124U) {", "line_count": 5}},
        {"span": {"hunk_headers": ["@@ -1010,0 +1077,28 @@ public:", "@@ -1010,0 +1019,5 @@ public:"], "start": "+        if (index == 124U) {", "line_count": 5}},
        "+        essex_vibe_->reset();",
        "+        essex_vibe_->reset_modulation_phase();",
        {"span": {"start": "+    void configure_essex_vibe(float rate_hz, float depth, float trem,", "line_count": 12}},
        "+        bytes += essex_vibe_->memory_bytes();",
        {"span": {"hunk_headers": ["@@ -4388,0 +4615,20 @@ public:", "@@ -4388,0 +4417,5 @@ public:"], "start": "+        if (index == 124U) {", "line_count": 5}},
        "+        case 124U: return essex_vibe_enabled_;",
        "+    std::unique_ptr<tabletfx::EssexVibe> essex_vibe_;",
        "+    bool essex_vibe_enabled_ = false;",
        {"span": {"start": "+TABLETFX_WASM_EXPORT void tf_chain_set_essex_vibe(", "line_count": 7}},
    ),
    "web/app.js": (
        {"span": {"start": "+const essexVibeControls = [", "line_count": 8}},
        {"span": {"start": "+function updateEssexVibe() {", "line_count": 26}},
        {"span": {"start": "+async function runEssexVibeBrowserSmokeTest(", "line_count": 129}},
        {
            "replace": {
                "current": "+        EFFECT_NAMES.length !== 127 || effectMemory.length !== EFFECT_NAMES.length ||",
                "selected": "+        EFFECT_NAMES.length !== 125 || effectMemory.length !== EFFECT_NAMES.length ||",
                "hunk_headers": [
                    "@@ -18764,2 +19647,922 @@ async function runStudioPanDelayBrowserSmokeTest(",
                    "@@ -18764,2 +18799,129 @@ async function runStudioPanDelayBrowserSmokeTest(",
                ],
                "scope_start": "+async function runEssexVibeBrowserSmokeTest(",
                "scope_end": "+      throw new Error(\"Essex Vibe must offer the one-stage and two-stage choices.\");",
                "current_occurrence": 1,
                "expected_current_count": 1,
            }
        },
        {
            "replace": {
                "before": "-    const expectedEffectIds = Array.from({ length: 124 }, (_, index) => index);",
                "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "selected": "+    const expectedEffectIds = Array.from({ length: 125 }, (_, index) => index);",
                "hunk_header": "@@ -18764,2 +19647,922 @@ async function runStudioPanDelayBrowserSmokeTest(",
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": "-    if (EFFECT_NAMES.length !== 124 || effectRows.length !== 124 ||",
                "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "selected": "+    if (EFFECT_NAMES.length !== 125 || effectRows.length !== 125 ||",
                "hunk_header": "@@ -18764,2 +19647,922 @@ async function runStudioPanDelayBrowserSmokeTest(",
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        "+  let essexVibeSmoke = null;",
        {"span": {"start": '+      testResult.textContent = "Checking Essex Vibe with local guitar playback…";', "line_count": 4}},
    ),
    "web/audio-worklet.js": (
        '+EFFECTS.push("essex-vibe");',
        '+    } else if (message.type === "essex-vibe" && this.ready) {',
        "+      this.configureEssexVibe(message.value);",
    ),
    "web/index.html": (
        '+        <label class="effect-row" data-category="modulation"><input type="checkbox" data-effect="124"><span class="effect-number">125</span><span class="effect-name">Essex Vibe · project-authored controls</span><span class="effect-default">0.8 Hz · 2 stages · disabled by default</span><span class="effect-category-tag">Modulation</span></label>',
        {"span": {"start": '+      <div class="effect-parameter-group" aria-label="Essex Vibe controls">', "line_count": 12}},
    ),
    "scripts/checkpoint_line_selectors.py": (
        {"span": {"hunk_header": "@@ -801,0 +802,12 @@ def select_exact_line_operations(", "start": "+            or (", "line_count": 12}},
        {"span": {"hunk_header": "@@ -913,0 +926,38 @@ def select_exact_line_operations(", "start": "+        scope_bounds: tuple[int, int, int] | None = None", "line_count": 38}},
        "+            and in_replacement_scope(hunk_index, line_index)",
        "+                and in_replacement_scope(hunk_index, line_index)",
        {"span": {"start": '+PARTIAL_CHECKPOINT_LINE_SELECTORS["essex-vibe"] = {', "end": "+}", "end_after_start": True}},
    ),
}
PARTIAL_CHECKPOINT_LINE_SELECTORS["euclidean-delay"] = {
    "CMakeLists.txt": (
        "+    src/effects/euclidean_delay.cpp",
        "+add_executable(tabletfx_euclidean_delay_bench apps/euclidean_delay_bench.cpp)",
        "+target_link_libraries(tabletfx_euclidean_delay_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_euclidean_delay_tests tests/test_euclidean_delay.cpp)",
        "+    target_link_libraries(tabletfx_euclidean_delay_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_euclidean_delay_tests COMMAND tabletfx_euclidean_delay_tests)",
    ),
    "EFFECTS_MATRIX.md": (
        {"span": {"start": "+## Line 6 Helix Stadium Euclidean Delay — project-authored implementation", "line_count": 3}},
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        {"span": {"start": "+## Line 6 Helix Stadium XL — Euclidean Delay (2026-11-28)", "line_count": 9}},
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        {"span": {"start": "+## Line 6 Helix Stadium XL — Euclidean Delay (2026-11-28)", "line_count": 12}},
    ),
    "LATENCY_AND_QUALITY.md": (
        {"span": {"start": "+## Line 6 Helix Stadium Euclidean Delay processor-call benchmark (2026-11-27)", "line_count": 12}},
    ),
    "Makefile": (
        "+include make/effects.mk",
        "+include make/research.mk",
    ),
    "RESEARCH.md": (
        {"span": {"start": "+## Line 6 Helix Stadium Euclidean Delay (2026-11-27)", "line_count": 5}},
    ),
    "include/tabletfx/effects.hpp": (
        '+#include "tabletfx/effects/euclidean_delay.hpp"',
    ),
    "make/publish.mk": (
        "+PUBLISH_CHECKPOINTS += euclidean-delay",
    ),
    "scripts/publish_checkpoint.py": (
        {"span": {"start": "+EUCLIDEAN_DELAY_CHECKPOINT_PATHS = (", "end": "+)", "end_after_start": True, "expected_line_count": 29}},
        {"span": {"start": '+CHECKPOINTS["euclidean-delay"] = (', "end": "+)", "end_after_start": True, "expected_line_count": 4}},
        {"span": {"start": '+PARTIAL_CHECKPOINT_HUNKS["euclidean-delay"] = {', "end": "+}", "end_after_start": True, "expected_line_count": 23}},
    ),
    "research-state.json": (
        '+  "previous_checkpoint_commit": "0d5804262e69d43a8a191dc353488cd1dfe71549",',
        {
            "span": {
                "hunk_context": "@@ -9,",
                "start": '-    "name": "Fractal FM9-family Global Mix — Phaser subset",',
                "line_count": 2,
            }
        },
        {"span": {"hunk_context": "@@ -9,", "start": '+    "name": "Line 6 Helix Stadium XL — Euclidean Delay",', "line_count": 1}},
        {"span": {"hunk_context": "@@ -12 ", "start": '-    "picker_index": 68,', "line_count": 1}},
        {"span": {"hunk_context": "@@ -13,", "start": '+    "picker_index": 125,', "line_count": 1}},
        {"span": {"hunk_headers": ["@@ -15,5 +14,9 @@", "@@ -15,8 +15,13 @@"], "start": '-      "title": "Fractal Audio Systems Blocks Guide and FM9 downloads",', "line_count_by_header": {"@@ -15,5 +14,9 @@": 5, "@@ -15,8 +15,13 @@": 6}}},
        {"span": {"hunk_headers": ["@@ -15,5 +14,9 @@", "@@ -15,8 +15,13 @@"], "start": '+      "title": "Helix Stadium model catalogue, Effects manual, Blocks manual, and 1.4 release notes",', "line_count_by_header": {"@@ -15,5 +14,9 @@": 9, "@@ -15,8 +15,13 @@": 10}}},
        {"span": {"hunk_headers": ["@@ -21,2 +24,3 @@", "@@ -15,8 +15,13 @@"], "start": '-    "documented_controls": "Global Mix is an opt-in to the shared Effects Mix offset, up to ±50%. Megatap also lists Mix, Level, Balance, Bypass/Bypass Mode, Input Select/Gain, Kill Dry, and spillover. The guide does not state how Global Mix combines with Megatap Mix.",', "line_count": 2}},
        {"span": {"hunk_headers": ["@@ -21,2 +24,3 @@", "@@ -15,8 +15,13 @@"], "start": '+    "documented_controls": "Mono/Stereo listing and category identity only; exact model-specific controls and numeric values are not published in the checked manual.",', "line_count": 3}},
        {
            "span": {
                "hunk_context": "@@ -24,",
                "start": '-      "native": "make test-fractal-megatap-global-mix passes focused defaults, disabled identity, offset, clamps, storage, and zero-process-allocation coverage; make test passes all 123 CTest cases.",',
                "end": '+    }',
                "end_after_start": True,
            }
        },
        {"span": {"hunk_context": "@@ -33,", "start": '-    "next_device": "Fractal FM9 Mark II Turbo",', "line_count": 2}},
        {"span": {"hunk_context": "@@ -33,", "start": '+    "next_device": "HeadRush Prime",', "line_count": 3}},
        '-    "next_feature": "After this Studio Pan Delay ID 123 checkpoint reaches main, publish the local Essex Vibe at ID 124 as the next standalone checkpoint; continue stable picker order only after each commit is on main.",',
        {
            "span": {
                "hunk_context": "@@ -1713,",
                "start": '+      "name": "Line 6 Helix Stadium XL — Euclidean Delay",',
                "end": "+      }",
                "end_after_start": True,
            }
        },
        {
            "span": {
                "hunk_context": "@@ -1740,",
                "start": '-    "name": "Fractal FM9-family Global Mix — Phaser subset",',
                "end": '+    "name": "Line 6 Helix Stadium XL — Euclidean Delay",',
                "end_after_start": True,
            }
        },
        {"span": {"hunk_context": "@@ -1743 ", "start": '-    "picker_index": 68,', "line_count": 1}},
        {"span": {"hunk_context": "@@ -1744,", "start": '+    "picker_index": 125,', "line_count": 1}},
        {"span": {"hunk_headers": ["@@ -1746,5 +1873,9 @@", "@@ -1746,8 +1789,13 @@"], "start": '-      "title": "Fractal Audio Systems Blocks Guide and FM9 downloads",', "line_count_by_header": {"@@ -1746,5 +1873,9 @@": 5, "@@ -1746,8 +1789,13 @@": 6}}},
        {"span": {"hunk_headers": ["@@ -1746,5 +1873,9 @@", "@@ -1746,8 +1789,13 @@"], "start": '+      "title": "Helix Stadium model catalogue, Effects manual, Blocks manual, and 1.4 release notes",', "line_count_by_header": {"@@ -1746,5 +1873,9 @@": 9, "@@ -1746,8 +1789,13 @@": 10}}},
        {"span": {"hunk_headers": ["@@ -1752,2 +1883,3 @@", "@@ -1746,8 +1789,13 @@"], "start": '-    "documented_controls": "Global Mix is an opt-in to the shared Effects Mix offset, up to ±50%. Megatap also lists Mix, Level, Balance, Bypass/Bypass Mode, Input Select/Gain, Kill Dry, and spillover. The guide does not state how Global Mix combines with Megatap Mix.",', "line_count": 2}},
        {"span": {"hunk_headers": ["@@ -1752,2 +1883,3 @@", "@@ -1746,8 +1789,13 @@"], "start": '+    "documented_controls": "Mono/Stereo listing and category identity only; exact model-specific controls and numeric values are not published in the checked manual.",', "line_count": 3}},
        {
            "span": {
                "hunk_context": "@@ -1755,",
                "start": '-      "native": "make test-fractal-megatap-global-mix passes focused defaults, disabled identity, offset, clamps, storage, and zero-process-allocation coverage; make test passes all 123 CTest cases.",',
                "end": '+    }',
                "end_after_start": True,
            }
        },
    ),
    "scripts/checkpoint_line_selectors.py": (
        {"span": {"hunk_context": "@@ -825,", "start": "+            or (", "line_count": 4}},
        {"span": {"hunk_context": "@@ -928,", "start": "-            scope_markers = []", "line_count": 51}},
        {"span": {"hunk_context": "@@ -993,", "start": "+            if (", "line_count": 10}},
        {
            "span": {
                "start": '+PARTIAL_CHECKPOINT_LINE_SELECTORS["euclidean-delay"] = {',
                "end": "+}",
                "end_after_start": True,
            }
        },
        '-    exact ``hunk_header`` and a 1-based ``end_occurrence`` for a repeated',
        '+    exact ``hunk_header`` or stable ``hunk_context`` prefix and a 1-based',
        '+    ``end_occurrence`` for a repeated closing line. Every selected line must be an added/deleted operation;',
        {
            "span": {
                "start": '+        hunk_context = span.get("hunk_context")',
                "end": '+            raise LineSelectionError("span hunk_context must be non-empty and exclusive")',
                "end_after_start": True,
            }
        },
        '+                and span.get("hunk_context") is None',
        '+                span.get("hunk_context") is not None',
        '+                and header_line.rstrip("\\n").startswith(span["hunk_context"])',
        '+            or span.get("hunk_context") is not None',
    ),
    "scripts/test_checkpoint_line_selectors.py": (
        {
            "span": {
                "start": '+class EuclideanDelaySelectorTests(unittest.TestCase):',
                "end": '+        self.assertNotIn("HeadRush Prime current catalog", selected)',
                "end_after_start": True,
            }
        },
        {"span": {"hunk_context": "@@ -116,", "start": "+    def test_scoped_replacement_accepts_unique_staged_value_without_scope_markers(self) -> None:", "line_count": 23}},
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/euclidean_delay.hpp"',
        {
            "span": {
                "hunk_context": "@@ -646,",
                "start": "+        if (index == 125U) {",
                "end": "+        }",
                "end_after_start": True,
            }
        },
        {
            "span": {
                "hunk_context": "@@ -1023,",
                "start": "+        if (index == 125U) {",
                "end": "+            return;",
                "end_after_start": True,
            }
        },
        "+        if (euclidean_delay_) euclidean_delay_->reset();",
        {
            "span": {
                "hunk_context": "@@ -3991,",
                "start": "+    void configure_euclidean_delay_filters(float low_cut_hz,",
                "end": "+    }",
                "end_after_start": True,
            }
        },
        {
            "span": {
                "hunk_context": "@@ -3991,",
                "start": "+    void configure_euclidean_delay(float step_time_ms, float steps,",
                "end": "+    }",
                "end_after_start": True,
            }
        },
        "+        if (euclidean_delay_) bytes += euclidean_delay_->memory_bytes();",
        {
            "span": {
                "hunk_context": "@@ -4421,",
                "start": "+        if (index == 125U) {",
                "end": "+        }",
                "end_after_start": True,
            }
        },
        "+        case 125U: return euclidean_delay_enabled_ && euclidean_delay_ != nullptr;",
        "+    std::unique_ptr<tabletfx::EuclideanDelay> euclidean_delay_;",
        "+    tabletfx::EuclideanDelayParameters euclidean_delay_parameters_{};",
        "+    bool euclidean_delay_enabled_ = false;",
        {
            "span": {
                "hunk_context": "@@ -6478,",
                "start": "+TABLETFX_WASM_EXPORT void tf_chain_set_euclidean_delay(",
                "end": "+}",
                "end_after_start": True,
            }
        },
    ),
    "scripts/run_cmake.py": (
        '+            "bench-euclidean-delay",',
        '+    elif action == "bench-euclidean-delay":',
        "+        bench_euclidean_delay(cmake)",
        {"span": {"start": "+def bench_euclidean_delay(cmake: str) -> None:", "line_count": 10}},
    ),
    "scripts/web.py": (
        '+        "_tf_chain_set_euclidean_delay",',
        '+        str(ROOT / "src" / "effects" / "euclidean_delay.cpp"),',
        "+                 euclidean_delay_only: bool = False,",
        "+        if euclidean_delay_only:",
        '+            url = base_url + "/web/index.html?selftest=euclidean-delay"',
        '+        "test-euclidean-delay",',
        '+    elif action == "test-euclidean-delay":',
        "+        browser_test(euclidean_delay_only=True)",
    ),
    "web/audio-worklet.js": (
        '+EFFECTS.push("euclidean-delay");',
        {"span": {"start": "+      this.configureEuclideanDelay(", "line_count": 4}},
        {"span": {"start": "+  configureEuclideanDelay(values) {", "line_count": 7}},
        {"span": {"start": '+        } else if (index === EFFECTS.indexOf("euclidean-delay")) {', "line_count": 10}},
        '+    } else if (message.type === "euclidean-delay" && this.ready) {',
        "+      this.configureEuclideanDelay(message.value);",
    ),
    "web/app.js": (
        '+  "Euclidean Delay · project-authored controls",',
        "+  euclideanDelay: [125, 8, 3, 0, 0.25, 0.25, 0, 20, 20000, 0],",
        {"span": {"start": "+const euclideanDelayControls = [", "line_count": 12}},
        {"span": {"start": "+function updateEuclideanDelay() {", "line_count": 46}},
        {"span": {"start": "+for (const control of euclideanDelayControls) {", "line_count": 4}},
        {"span": {"start": "+async function runEuclideanDelayBrowserSmokeTest(", "line_count": 38}},
        {"replace": {
            "current": "+        EFFECT_NAMES.length !== 127 || effectMemory.length !== EFFECT_NAMES.length ||",
            "selected": "+        EFFECT_NAMES.length !== 126 || effectMemory.length !== EFFECT_NAMES.length ||",
            "scope_start": "+async function runEuclideanDelayBrowserSmokeTest(",
            "scope_end": '+      throw new Error(`Euclidean Delay picker/default/memory check failed: category=${row?.dataset.category}, parameters=${DEFAULT_PARAMETERS.euclideanDelay.join(",")}, memory=${effectMemory.length}/${effectMemory[effectIndex]}.`);',
            "allow_staged_value_without_scope": True,
        }},
        "+  let euclideanDelaySmoke = null;",
        {"replace": {
            "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
            "selected": "+    const expectedEffectIds = Array.from({ length: 126 }, (_, index) => index);",
        }},
        {"replace": {
            "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
            "selected": "+    if (EFFECT_NAMES.length !== 126 || effectRows.length !== 126 ||",
        }},
        "+    // Euclidean Delay has a focused local guitar Play smoke and no offline CLI stage.",
        "+    parityEnabled[125] = false;",
        {"span": {"start": '+        if (new URLSearchParams(location.search).get("selftest") === "euclidean-delay") {', "line_count": 16}},
        {"span": {"start": '+      testResult.textContent = "Checking Euclidean Delay with local guitar playback…";', "line_count": 4}},
    ),
    "web/index.html": (
        '+        <label class="effect-row" data-category="delay"><input type="checkbox" data-effect="125"><span class="effect-number">126</span><span class="effect-name">Euclidean Delay · project-authored controls</span><span class="effect-default">125 ms · 8 steps · 3 pulses · disabled by default</span><span class="effect-category-tag">Delay</span></label>',
        {"span": {"start": '+      <div class="effect-parameter-group" aria-label="Euclidean Delay controls">', "line_count": 16}},
    ),
}
PARTIAL_CHECKPOINT_LINE_SELECTORS["synth-texture-reverb"] = {
    "CMakeLists.txt": (
        "+    src/effects/synth_texture_reverb.cpp",
        "+add_executable(tabletfx_synth_texture_reverb_bench",
        "+    apps/synth_texture_reverb_bench.cpp)",
        "+target_link_libraries(tabletfx_synth_texture_reverb_bench PRIVATE tabletfx_core)",
        "+    add_executable(tabletfx_synth_texture_reverb_tests",
        "+        tests/test_synth_texture_reverb.cpp)",
        "+    target_link_libraries(tabletfx_synth_texture_reverb_tests PRIVATE tabletfx_core)",
        "+    add_test(NAME tabletfx_synth_texture_reverb_tests",
        "+        COMMAND tabletfx_synth_texture_reverb_tests)",
    ),
    "make/publish.mk": (
        "+PUBLISH_CHECKPOINTS += synth-texture-reverb",
    ),
    "scripts/web.py": (
        '+        "_tf_chain_set_synth_texture_reverb",',
        '+        str(ROOT / "src" / "effects" / "synth_texture_reverb.cpp"),',
        "+                 synth_texture_reverb_only: bool = False) -> None:",
        "+    compiler = emscripten_cpp()",
        "+        elif synth_texture_reverb_only:",
        '+            url = base_url + "/web/index.html?selftest=synth-texture-reverb"',
        '+        "test-synth-texture-reverb",',
        '+    elif action == "test-synth-texture-reverb":',
        "+        browser_test(synth_texture_reverb_only=True)",
    ),
    "scripts/run_cmake.py": (
        '+            "bench-synth-texture-reverb",',
        '+    elif action == "bench-synth-texture-reverb":',
        "+        bench_synth_texture_reverb(cmake)",
        {
            "span": {
                "hunk_headers": [
                    "@@ -411,0 +487,62 @@ def bench_fractal_phaser_global_mix(cmake: str) -> None:",
                    "@@ -411,0 +415,10 @@ def bench_fractal_phaser_global_mix(cmake: str) -> None:",
                ],
                "start": "+def bench_synth_texture_reverb(cmake: str) -> None:",
                "line_count": 10,
            }
        },
    ),
    "scripts/checkpoint_line_selectors.py": (
        {
            "span": {
                "start": '+PARTIAL_CHECKPOINT_LINE_SELECTORS["synth-texture-reverb"] = {',
                "line_count": 196,
            }
        },
    ),
    "src/web/wasm_chain.cpp": (
        '+#include "tabletfx/effects/synth_texture_reverb.hpp"',
        {
            "replace": {
                "before": "-    return effects_.size() + 29;",
                "current": "+    return effects_.size() + 33;",
                "selected": "+    return effects_.size() + 30;",
                "hunk_headers": [
                    "@@ -471 +474 @@ public:",
                    "@@ -471 +472 @@ public:",
                ],
                "allow_intervening_added_lines": True,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -672,0 +676,27 @@ public:",
                    "@@ -672,0 +674,27 @@ public:",
                ],
                "start": "+        if (index == 126U) {",
                "line_count": 27,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -1058,0 +1091,12 @@ public:",
                    "@@ -1058,0 +1087,11 @@ public:",
                ],
                "start": "+        if (index == 126U) {",
                "line_count": 11,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -4050,0 +4128,23 @@ public:",
                    "@@ -4050,0 +4090,21 @@ public:",
                ],
                "start": "+    void configure_synth_texture_reverb(",
                "line_count": 21,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -4523,0 +4626,7 @@ public:",
                    "@@ -4523,0 +4584,7 @@ public:",
                ],
                "start": "+        if (index == 126U) {",
                "line_count": 7,
            }
        },
        "+        case 126U: return synth_texture_reverb_enabled_ && synth_texture_reverb_ != nullptr;",
        "+    std::unique_ptr<tabletfx::effects::SynthTextureReverb> synth_texture_reverb_;",
        "+    tabletfx::effects::SynthTextureReverbParameters synth_texture_reverb_parameters_{};",
        "+    bool synth_texture_reverb_enabled_ = false;",
        {
            "span": {
                "hunk_headers": [
                    "@@ -6596,0 +6742,10 @@ TABLETFX_WASM_EXPORT void tf_chain_set_euclidean_delay(",
                    "@@ -6596,0 +6668,10 @@ TABLETFX_WASM_EXPORT void tf_chain_set_euclidean_delay(",
                ],
                "start": "+TABLETFX_WASM_EXPORT void tf_chain_set_synth_texture_reverb(",
                "line_count": 10,
            }
        },
    ),
    "web/app.js": (
        '+  "Synth Texture Reverb · project-authored controls",',
        "+  synthTextureReverb: [0.25, 0.45, 4.5, 7200, 0, 240, 0],",
        {
            "span": {
                "hunk_headers": [
                    "@@ -2474,0 +2557,9 @@ const essexVibeControls = [",
                    "@@ -2474,0 +2477,9 @@ const essexVibeControls = [",
                ],
                "start": "+const synthTextureReverbControls = [",
                "line_count": 9,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -8317,0 +8497,30 @@ function updateEssexVibe() {",
                    "@@ -8317,0 +8329,28 @@ function updateEssexVibe() {",
                ],
                "start": "+function updateSynthTextureReverb() {",
                "line_count": 28,
            }
        },
        {
            "span": {
                "hunk_headers": [
                    "@@ -8629,0 +8849,8 @@ for (const control of euclideanDelayControls) {",
                    "@@ -8629,0 +8669,4 @@ for (const control of euclideanDelayControls) {",
                ],
                "start": "+for (const control of synthTextureReverbControls) {",
                "line_count": 4,
            }
        },
        {
            "replace": {
                "before": "-    const expectedEffectIds = Array.from({ length: 126 }, (_, index) => index);",
                "current": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "selected": "+    const expectedEffectIds = Array.from({ length: 127 }, (_, index) => index);",
                "hunk_header": "@@ -19031,2 +20540,29 @@ async function runEuclideanDelayBrowserSmokeTest(",
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": "-    if (EFFECT_NAMES.length !== 126 || effectRows.length !== 126 ||",
                "current": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "selected": "+    if (EFFECT_NAMES.length !== 127 || effectRows.length !== 127 ||",
                "hunk_header": "@@ -19031,2 +20540,29 @@ async function runEuclideanDelayBrowserSmokeTest(",
                "allow_intervening_changes": True,
                "allow_cross_hunk_staged": True,
            }
        },
        {
            "replace": {
                "before": '-    if (reverbIds.join(",") !== "28,29,30,31,57,58,77,79,119,120" || effectCount.value !== `10 of ${effectRows.length}` ||',
                "current": '+      if (reverbIds.join(",") !== "28,29,30,31,57,58,77,79,119,120,126" || effectCount.value !== `11 of ${effectRows.length}` ||',
                "selected": '+      if (reverbIds.join(",") !== "28,29,30,31,57,58,77,79,119,120,126" || effectCount.value !== `11 of ${effectRows.length}` ||',
                "hunk_headers": [
                    "@@ -20594 +22145 @@ async function runEuclideanDelayBrowserSmokeTest(",
                    "@@ -20594 +20637 @@ async function runEuclideanDelayBrowserSmokeTest(",
                ],
            }
        },
        "+        // Synth Texture Reverb has a focused local guitar Play smoke and no offline CLI stage.",
        "+        parityEnabled[126] = false;",
        {
            "span": {
                "hunk_headers": [
                    "@@ -21410,0 +22968,205 @@ async function runEuclideanDelayBrowserSmokeTest(",
                    "@@ -21410,0 +21456,205 @@ async function runEuclideanDelayBrowserSmokeTest(",
                ],
                "start": '+      if (new URLSearchParams(location.search).get("selftest") === "synth-texture-reverb") {',
                "line_count": 205,
            }
        },
    ),
}
def select_checkpoint_line_patch(checkpoint: str, path: str, diff: str) -> str | None:
    """Apply a checkpoint/path exact-line selector, or return None if absent."""
    operations = PARTIAL_CHECKPOINT_LINE_SELECTORS.get(checkpoint, {}).get(path)
    if operations is None:
        return None
    return select_exact_line_operations(diff, operations)
