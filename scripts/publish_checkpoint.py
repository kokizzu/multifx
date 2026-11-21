#!/usr/bin/env python3
"""Safely publish the reviewed DSP checkpoint to upstream main."""

from __future__ import annotations

import argparse
from difflib import unified_diff
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHECKPOINT = "all"
CHECKPOINT_PATHS = (
    ".gitignore",
    "ARCHITECTURE.md",
    "AUDIO_COMPATIBILITY.md",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "GOAL.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "Makefile",
    "PRESET_FORMAT.md",
    "README.md",
    "RESEARCH.md",
    "THIRD_PARTY_NOTICES.md",
    "apps/offline_render.cpp",
    "cmake/toolchains/mingw-w64.cmake",
    "include/tabletfx/effect.hpp",
    "include/tabletfx/channel_conversion.hpp",
    "include/tabletfx/parallel_mixer.hpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/level_meter.hpp",
    "include/tabletfx/signal_controls.hpp",
    "include/tabletfx/tuner.hpp",
    "tests/test_gain.cpp",
    "tests/test_channel_conversion.cpp",
    "tests/test_parallel_mixer.cpp",
    "tests/test_peak_limiter.cpp",
    "tests/test_tremolo.cpp",
    "tests/test_volume_pedal.cpp",
    "tests/test_wah.cpp",
    "tests/test_noise_gate.cpp",
    "tests/test_clean_boost.cpp",
    "tests/test_mid_overdrive.cpp",
    "tests/test_high_gain_distortion.cpp",
    "tests/test_round_fuzz.cpp",
    "tests/test_delay.cpp",
    "tests/test_chorus.cpp",
    "tests/test_envelope_wah.cpp",
    "include/tabletfx/effects/filters.hpp",
    "include/tabletfx/effects/hall_reverb.hpp",
    "include/tabletfx/effects/plate_reverb.hpp",
    "include/tabletfx/effects/spring_reverb.hpp",
    "include/tabletfx/effects/mid_overdrive.hpp",
    "include/tabletfx/effects/high_gain_distortion.hpp",
    "include/tabletfx/effects/round_fuzz.hpp",
    "include/tabletfx/effects/clean_boost.hpp",
    "include/tabletfx/effects/chorus.hpp",
    "include/tabletfx/effects/envelope_wah.hpp",
    "include/tabletfx/effects/pedal_compressor.hpp",
    "include/tabletfx/audio/wasapi_backend.hpp",
    "include/tabletfx/effects/compressor.hpp",
    "include/tabletfx/effects/three_band_eq.hpp",
    "include/tabletfx/effects/peak_limiter.hpp",
    "include/tabletfx/effects/tremolo.hpp",
    "include/tabletfx/effects/volume_pedal.hpp",
    "include/tabletfx/effects/wah.hpp",
    "include/tabletfx/effects/poly_pitch_shifter.hpp",
    "include/tabletfx/wav.hpp",
    "scripts/audio_fixtures.py",
    "scripts/audio_regression.py",
    "scripts/bootstrap_docs.py",
    "scripts/check_repo_diff.py",
    "scripts/doctor.py",
    "scripts/publish_checkpoint.py",
    "scripts/repo_status.py",
    "scripts/run_cmake.py",
    "scripts/test_audio_fixtures.py",
    "scripts/test_offline_cli.py",
    "scripts/validate_docs.py",
    "scripts/web.py",
    "src/effects/poly_pitch_shifter.cpp",
    "src/effects/mid_overdrive.cpp",
    "src/effects/high_gain_distortion.cpp",
    "src/effects/round_fuzz.cpp",
    "src/effects.cpp",
    "src/channel_conversion.cpp",
    "src/parallel_mixer.cpp",
    "src/effects/filters.cpp",
    "src/effects/clean_boost.cpp",
    "src/effects/chorus.cpp",
    "src/effects/envelope_wah.cpp",
    "src/effects/pedal_compressor.cpp",
    "src/effects/peak_limiter.cpp",
    "src/effects/tremolo.cpp",
    "src/effects/volume_pedal.cpp",
    "src/effects/wah.cpp",
    "src/level_meter.cpp",
    "src/signal_controls.cpp",
    "src/tuner.cpp",
    "src/audio/wasapi_backend.cpp",
    "src/web/wasm_chain.cpp",
    "src/wav.cpp",
    "tests/test_dsp.cpp",
    "tests/test_compressor.cpp",
    "tests/test_three_band_eq.cpp",
    "tests/test_filters.cpp",
    "tests/test_pedal_compressor.cpp",
    "tests/test_level_meter.cpp",
    "tests/test_signal_controls.cpp",
    "tests/test_tuner.cpp",
    "tests/test_poly_pitch_shifter.cpp",
    "tests/wasapi_compile.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "web/wav_decoder.js",
    "web/worklet-smoke.js",
    "third_party/stft-pitch-shift/Arctangent.h",
    "third_party/stft-pitch-shift/Cepster.h",
    "third_party/stft-pitch-shift/FFT.h",
    "third_party/stft-pitch-shift/LICENSE.txt",
    "third_party/stft-pitch-shift/Normalizer.h",
    "third_party/stft-pitch-shift/Pitcher.h",
    "third_party/stft-pitch-shift/RFFT.h",
    "third_party/stft-pitch-shift/Resampler.h",
    "third_party/stft-pitch-shift/STFT.h",
    "third_party/stft-pitch-shift/StftPitchShiftCore.h",
    "third_party/stft-pitch-shift/Timer.h",
    "third_party/stft-pitch-shift/Vocoder.h",
    "third_party/stft-pitch-shift/StftPitchShift/Arctangent.h",
    "third_party/stft-pitch-shift/StftPitchShift/Cepster.h",
    "third_party/stft-pitch-shift/StftPitchShift/FFT.h",
    "third_party/stft-pitch-shift/StftPitchShift/Normalizer.h",
    "third_party/stft-pitch-shift/StftPitchShift/Pitcher.h",
    "third_party/stft-pitch-shift/StftPitchShift/RFFT.h",
    "third_party/stft-pitch-shift/StftPitchShift/Resampler.h",
    "third_party/stft-pitch-shift/StftPitchShift/STFT.h",
    "third_party/stft-pitch-shift/StftPitchShift/StftPitchShiftCore.h",
    "third_party/stft-pitch-shift/StftPitchShift/Timer.h",
    "third_party/stft-pitch-shift/StftPitchShift/Vocoder.h",
    "include/tabletfx/effects/flanger.hpp",
    "include/tabletfx/effects/phaser.hpp",
    "include/tabletfx/effects/vibrato.hpp",
    "include/tabletfx/effects/overdrive.hpp",
    "src/effects/phaser.cpp",
    "src/effects/vibrato.cpp",
    "src/effects/overdrive.cpp",
    "tests/test_phaser.cpp",
    "tests/test_vibrato.cpp",
    "tests/test_overdrive.cpp",
    "src/effects/flanger.cpp",
    "src/effects/hall_reverb.cpp",
    "src/effects/room_reverb.cpp",
    "src/effects/plate_reverb.cpp",
    "src/effects/spring_reverb.cpp",
    "tests/test_flanger.cpp",
    "tests/test_hall_reverb.cpp",
    "tests/test_room_reverb.cpp",
    "tests/test_plate_reverb.cpp",
    "tests/test_spring_reverb.cpp",
)

FEATURE_MODULE_PATHS = (
    "include/tabletfx/effects/optical_compressor.hpp",
    "src/effects/optical_compressor.cpp",
    "tests/test_optical_compressor.cpp",
    "include/tabletfx/effects/fet_compressor.hpp",
    "src/effects/fet_compressor.cpp",
    "tests/test_fet_compressor.cpp",
    "include/tabletfx/effects/guitar_graphic_eq.hpp",
    "src/effects/guitar_graphic_eq.cpp",
    "tests/test_guitar_graphic_eq.cpp",
    "include/tabletfx/effects/bass_graphic_eq.hpp",
    "src/effects/bass_graphic_eq.cpp",
    "tests/test_bass_graphic_eq.cpp",
    "include/tabletfx/effects/shelf_tilt_eq.hpp",
    "src/effects/shelf_tilt_eq.cpp",
    "tests/test_shelf_tilt_eq.cpp",
    "include/tabletfx/effects/auto_swell.hpp",
    "src/effects/auto_swell.cpp",
    "tests/test_auto_swell.cpp",
    "include/tabletfx/effects/ring_modulator.hpp",
    "src/effects/ring_modulator.cpp",
    "tests/test_ring_modulator.cpp",
    "include/tabletfx/effects/rotary_speaker.hpp",
    "src/effects/rotary_speaker.cpp",
    "tests/test_rotary_speaker.cpp",
    "include/tabletfx/effects/rhythmic_slicer.hpp",
    "src/effects/rhythmic_slicer.cpp",
    "tests/test_rhythmic_slicer.cpp",
    "include/tabletfx/effects/step_filter.hpp",
    "src/effects/step_filter.cpp",
    "tests/test_step_filter.cpp",
)

CHECKPOINT_PATHS = CHECKPOINT_PATHS + FEATURE_MODULE_PATHS
DLY08_CHECKPOINT_PATHS = (
    "Makefile",
    "scripts/publish_checkpoint.py",
    "apps/offline_render.cpp",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "include/tabletfx/effects.hpp",
    "src/effects.cpp",
    "tests/test_delay.cpp",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "RESEARCH.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "EFFECTS_MATRIX.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
)
PICKER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/index.html",
    "web/style.css",
)
ROOM_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "src/effects.cpp",
    "src/effects/room_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_room_reverb.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
)
HALL_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/hall_reverb.hpp",
    "src/effects/hall_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_hall_reverb.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
PLATE_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/plate_reverb.hpp",
    "src/effects/plate_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_plate_reverb.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
SPRING_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/spring_reverb.hpp",
    "src/effects/spring_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_spring_reverb.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
AUTO_SWELL_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/auto_swell.hpp",
    "src/effects/auto_swell.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_auto_swell.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
ROTARY_SPEAKER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/rotary_speaker.hpp",
    "src/effects/rotary_speaker.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_rotary_speaker.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
RING_MOD_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/ring_modulator.hpp",
    "src/effects/ring_modulator.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_ring_modulator.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
RHYTHMIC_SLICER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/rhythmic_slicer.hpp",
    "src/effects/rhythmic_slicer.cpp",
    "tests/test_rhythmic_slicer.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
)
STEP_FILTER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/step_filter.hpp",
    "src/effects/step_filter.cpp",
    "tests/test_step_filter.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
NOTCH_FILTER_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/notch_filter.hpp",
    "src/effects/notch_filter.cpp",
    "tests/test_notch_filter.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
REVERSE_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/delay_timing.hpp",
    "include/tabletfx/effects/reverse_delay.hpp",
    "src/effects/reverse_delay.cpp",
    "tests/test_reverse_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
)
DUCKING_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/ducking_delay.hpp",
    "src/effects/ducking_delay.cpp",
    "tests/test_ducking_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
MODULATED_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/modulated_delay.hpp",
    "src/effects/modulated_delay.cpp",
    "tests/test_modulated_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FILTERED_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "src/effects.cpp",
    "src/effects/filtered_delay.cpp",
    "tests/test_filtered_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
SWEEP_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "src/effects/sweep_delay.cpp",
    "tests/test_sweep_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

EFFECT_MODULE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/formant_filter.hpp",
    "include/tabletfx/effects/harmonic_resonator.hpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/formant_filter.cpp",
    "src/effects/harmonic_resonator.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_formant_filter.cpp",
    "tests/test_harmonic_resonator.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "EFFECTS_MATRIX.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
) + FEATURE_MODULE_PATHS
FLAGSHIP_CATALOG_CHECKPOINT_PATHS = (
    "Makefile",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "RESEARCH.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "README.md",
    "scripts/publish_checkpoint.py",
)
CHECKPOINTS = {
    "all": (CHECKPOINT_PATHS, "feat: add multi-head delay, compressors, and EQs"),
    "dly08": (DLY08_CHECKPOINT_PATHS, "feat: add multi-head tape echo"),
    "effect-modules": (EFFECT_MODULE_CHECKPOINT_PATHS,
                       "feat: add optical, FET and graphic EQ processors"),
    "picker": (PICKER_CHECKPOINT_PATHS,
               "feat: add flagship comparison and categorized picker"),
    "room-reverb": (ROOM_REVERB_CHECKPOINT_PATHS,
                    "feat: add project-authored room reverb"),
    "hall-reverb": (HALL_REVERB_CHECKPOINT_PATHS,
                    "feat: add project-authored hall reverb"),
    "plate-reverb": (PLATE_REVERB_CHECKPOINT_PATHS,
                     "feat: add project-authored plate reverb"),
    "spring-reverb": (SPRING_REVERB_CHECKPOINT_PATHS,
                      "feat: add project-authored spring reverb"),
    "auto-swell": (AUTO_SWELL_CHECKPOINT_PATHS,
                   "feat: add project-authored auto swell"),
    "rotary-speaker": (ROTARY_SPEAKER_CHECKPOINT_PATHS,
                       "feat: add project-authored rotary speaker"),
    "ring-mod": (RING_MOD_CHECKPOINT_PATHS,
                 "feat: add project-authored ring modulation"),
    "rhythmic-slicer": (RHYTHMIC_SLICER_CHECKPOINT_PATHS,
                        "feat: add project-authored rhythmic slicer"),
    "step-filter": (STEP_FILTER_CHECKPOINT_PATHS,
                    "feat: add project-authored step filter"),
    "notch-filter": (NOTCH_FILTER_CHECKPOINT_PATHS,
                     "feat: add project-authored notch filter"),
    "reverse-delay": (REVERSE_DELAY_CHECKPOINT_PATHS,
                      "feat: add project-authored reverse delay"),
    "ducking-delay": (DUCKING_DELAY_CHECKPOINT_PATHS,
                      "feat: add project-authored ducking delay"),
    "modulated-delay": (MODULATED_DELAY_CHECKPOINT_PATHS,
                        "feat: add project-authored modulated delay"),
    "filtered-delay": (FILTERED_DELAY_CHECKPOINT_PATHS,
                       "feat: add project-authored filtered delay"),
    "sweep-delay": (SWEEP_DELAY_CHECKPOINT_PATHS,
                    "feat: add project-authored swept delay"),
    "flagship-catalog": (FLAGSHIP_CATALOG_CHECKPOINT_PATHS,
                         "docs: expand flagship effects catalog"),
}


def git(*args: str, capture: bool = True) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=capture, text=True, check=False
    )
    if result.returncode != 0:
        detail = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        raise SystemExit(detail or f"git {' '.join(args)} failed with status {result.returncode}")
    return result.stdout if capture else ""


def inspect(checkpoint: str) -> tuple[list[str], list[str], str, str]:
    if checkpoint not in CHECKPOINTS:
        raise SystemExit("Unknown checkpoint: " + checkpoint)
    checkpoint_paths, _ = CHECKPOINTS[checkpoint]
    checkpoint_paths = set(checkpoint_paths)
    branch = git("branch", "--show-current").strip()
    remote_url = git("remote", "get-url", "origin").strip()
    normalized = remote_url.lower().removesuffix(".git")
    if "github.com/kokizzu/multifx" not in normalized:
        raise SystemExit(f"origin does not identify kokizzu/multifx: {remote_url}")
    default_info = git("ls-remote", "--symref", "origin", "HEAD")
    default_ref = next((line.split()[1] for line in default_info.splitlines() if line.startswith("ref:")), "")
    if default_ref != "refs/heads/main":
        raise SystemExit(f"Upstream default branch is {default_ref or 'unknown'}, expected refs/heads/main")
    if branch != "main":
        raise SystemExit(f"Current branch is {branch or 'detached HEAD'}, expected main")
    upstream = git("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}").strip()
    if upstream != "origin/main":
        raise SystemExit(f"Tracked upstream is {upstream or 'none'}, expected origin/main")

    remote_main = next(
        (line.split()[0] for line in git("ls-remote", "origin", "refs/heads/main").splitlines() if line.strip()),
        "",
    )
    head = git("rev-parse", "HEAD").strip()
    if not remote_main or head != remote_main:
        raise SystemExit("Local main is not exactly at the current upstream main; synchronize safely before publishing")

    status = git("status", "--porcelain=v1", "--untracked-files=all")
    paths: list[str] = []
    staged: list[str] = []
    for line in status.splitlines():
        code = line[:2]
        path = line[3:]
        if path.startswith('"'):
            raise SystemExit(f"Unusual quoted path needs manual review: {line}")
        paths.append(path)
        if code[0] not in (" ", "?"):
            staged.append(path)
    unexpected_staged = sorted(set(staged) - checkpoint_paths)
    if unexpected_staged:
        raise SystemExit(
            "Refusing to publish with staged changes outside the selected checkpoint: "
            + ", ".join(unexpected_staged)
        )
    known_checkpoint_paths = set(CHECKPOINT_PATHS)
    for allowed_paths, _message in CHECKPOINTS.values():
        known_checkpoint_paths.update(allowed_paths)
    unexpected = sorted(set(paths) - known_checkpoint_paths)
    if unexpected:
        raise SystemExit("Dirty files outside the reviewed checkpoint: " + ", ".join(unexpected))
    workflows = [path for path in paths if path.startswith(".github/workflows/")]
    if workflows:
        raise SystemExit("Refusing to publish workflow changes: " + ", ".join(workflows))
    selected = sorted(set(paths) & checkpoint_paths)
    deferred = sorted(set(paths) - checkpoint_paths)
    if not selected:
        raise SystemExit("No changed files for checkpoint " + checkpoint)
    return selected, deferred, branch, remote_url


def plan(checkpoint: str) -> list[str]:
    paths, deferred, branch, remote_url = inspect(checkpoint)
    git("diff", "--check")
    git("diff", "--cached", "--check")
    print(f"Repository: {remote_url}")
    print(f"Branch: {branch} (verified upstream default; local HEAD matches origin/main)")
    print("Checkpoint: " + checkpoint)
    print("Commit message: " + CHECKPOINTS[checkpoint][1])
    print("Exact changed paths:")
    for path in paths:
        print("  " + path)
    stat = git("diff", "--stat", "--", *paths)
    print(stat.rstrip() or "Untracked files are listed above; no tracked diff stat yet.")
    status = git("status", "--porcelain=v1", "--untracked-files=all")
    untracked = {line[3:] for line in status.splitlines() if line.startswith("?? ")}
    print("Full reviewed patch:")
    for path in paths:
        staged_diff = git("diff", "--cached", "--", path)
        unstaged_diff = git("diff", "--", path)
        diff = staged_diff + unstaged_diff
        if diff:
            print(diff, end="" if diff.endswith("\n") else "\n")
        elif path in untracked:
            contents = (ROOT / path).read_text(encoding="utf-8").splitlines(keepends=True)
            print("".join(unified_diff([], contents, fromfile="/dev/null", tofile="b/" + path)), end="")
    print("No workflow files are included. Only the paths above will be staged.")
    if deferred:
        print("Deferred changed paths for other checkpoints:")
        for path in deferred:
            print("  " + path)
    return paths


def publish(checkpoint: str) -> None:
    paths, _, _, _ = inspect(checkpoint)
    git("add", "--", *paths, capture=False)
    planned_paths = plan(checkpoint)
    if planned_paths != paths:
        raise SystemExit("Checkpoint paths changed while preparing publication; stopping before commit")
    git("diff", "--cached", "--check")
    print("Staged diff:")
    print(git("diff", "--cached", "--stat").rstrip())
    staged = set(git("diff", "--cached", "--name-only").splitlines())
    if staged != set(paths):
        raise SystemExit("Staged paths differ from reviewed checkpoint; stopping before commit")
    git("commit", "-m", CHECKPOINTS[checkpoint][1], capture=False)
    commit = git("rev-parse", "HEAD").strip()
    git("push", "origin", "main", capture=False)
    remote_main = next(
        (line.split()[0] for line in git("ls-remote", "origin", "refs/heads/main").splitlines() if line.strip()),
        "",
    )
    if remote_main != commit:
        raise SystemExit(f"Push verification failed: local {commit}, remote {remote_main or 'missing'}")
    print(f"Published and verified commit {commit} on origin/main")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "publish"))
    parser.add_argument("--checkpoint", choices=tuple(CHECKPOINTS),
                        default=DEFAULT_CHECKPOINT)
    args = parser.parse_args()
    if args.mode == "plan":
        plan(args.checkpoint)
    else:
        publish(args.checkpoint)


if __name__ == "__main__":
    main()
