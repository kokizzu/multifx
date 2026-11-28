#!/usr/bin/env python3
"""Safely publish the reviewed DSP checkpoint to upstream main."""

from __future__ import annotations

import argparse
from difflib import unified_diff
from pathlib import Path
import subprocess
import sys
import tempfile


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
    "scripts/audio_regression.py",
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
    "include/tabletfx/effects/dimension_chorus.hpp",
    "src/effects/dimension_chorus.cpp",
    "tests/test_dimension_chorus.cpp",
    "include/tabletfx/effects/four_band_resonator.hpp",
    "src/effects/four_band_resonator.cpp",
    "tests/test_four_band_resonator.cpp",
    "include/tabletfx/effects/diatonic_harmonizer.hpp",
    "src/effects/diatonic_harmonizer.cpp",
    "tests/test_diatonic_harmonizer.cpp",
    "include/tabletfx/effects/grain_delay.hpp",
    "src/effects/grain_delay.cpp",
    "tests/test_grain_delay.cpp",
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
FRACTAL_ROTARY_DEPTH_CHECKPOINT_PATHS = (
    "include/tabletfx/effects/rotary_speaker.hpp",
    "src/effects/rotary_speaker.cpp",
    "tests/test_rotary_speaker.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "scripts/publish_checkpoint.py",
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
    "apps/offline_render.cpp",
    "include/tabletfx/effects/ducking_delay.hpp",
    "src/effects/ducking_delay.cpp",
    "tests/test_ducking_delay.cpp",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "GOAL.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "scripts/publish_checkpoint.py",
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
    "include/tabletfx/effects/delay_timing.hpp",
    "src/effects.cpp",
    "src/effects/auto_swell_delay.cpp",
    "src/effects/filtered_delay.cpp",
    "src/effects/multi_delay.cpp",
    "src/effects/pitch_sequence.cpp",
    "src/effects/plex_delay.cpp",
    "src/effects/reverse_delay.cpp",
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
    "scripts/publish_checkpoint.py",
)
SWEEP_DELAY_CHECKPOINT_PATHS = (
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
FORMANT_FILTER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/formant_filter.hpp",
    "src/effects/formant_filter.cpp",
    "tests/test_formant_filter.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
FM9_FORMANT_MORPH_CHECKPOINT_PATHS = tuple(
    path for path in FORMANT_FILTER_CHECKPOINT_PATHS if path != "Makefile"
) + ("make/publish.mk", "research-state.json")
FORMANT_CONTROL_LFO_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "include/tabletfx/effects/formant_filter.hpp",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "research-state.json",
    "scripts/audio_regression.py",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/formant_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_formant_filter.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
HEADRUSH_PRIME_DROP_TUNE_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "web/app.js",
    "web/index.html",
)
FENDER_STEREO_FLANGER_PHASE_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/flanger.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/flanger.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_flanger.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FENDER_FLANGER_TAP_DIVISION_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/flanger.hpp",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/flanger.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_flanger.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FENDER_PHASER_90_TAP_DIVISION_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/phaser.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/phaser.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_phaser.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
PHASER_STAGE_COUNTS_STEREO_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/headrush_phaser_stereo_bench.cpp",
    "include/tabletfx/effects/phaser.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "src/effects/phaser.cpp",
    "tests/test_phaser.cpp",
    "web/app.js",
    "web/index.html",
)
FENDER_TREMOLO_LFO_CLIP_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/tremolo.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_tremolo.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FENDER_TREMOLO_DUTY_CYCLE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/fender_tremolo_duty_cycle_bench.cpp",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/tremolo.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_tremolo.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FENDER_MEMORY_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/fender_memory_delay_bench.cpp",
    "include/tabletfx/effects/modulated_delay.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/modulated_delay.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_modulated_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
TONE_MASTER_SMALL_HALL_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/tone_master_small_hall_bench.cpp",
    "include/tabletfx/effects/tone_master_small_hall.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/tone_master_small_hall.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_tone_master_small_hall.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
TONE_MASTER_MODULATED_LARGE_HALL_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/tone_master_modulated_large_hall_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/tone_master_modulated_large_hall.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/tone_master_modulated_large_hall.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_tone_master_modulated_large_hall.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FRACTAL_SOURCE_CORRECTION_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
)
FRACTAL_SYNTH_OUTPUT_LEVEL_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/fractal_three_voice_synth.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "src/effects/fractal_three_voice_synth.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_fractal_three_voice_synth.cpp",
    "web/app.js",
    "web/audio-worklet.js",
)
HARMONIC_RESONATOR_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/harmonic_resonator.hpp",
    "src/effects/harmonic_resonator.cpp",
    "tests/test_harmonic_resonator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
MULTIBAND_COMPRESSOR_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/multiband_compressor.hpp",
    "src/effects/multiband_compressor.cpp",
    "tests/test_multiband_compressor.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
BIT_CRUSHER_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/bit_crusher.hpp",
    "src/effects/bit_crusher.cpp",
    "tests/test_bit_crusher.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
DIMENSION_CHORUS_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/dimension_chorus.hpp",
    "src/effects/dimension_chorus.cpp",
    "tests/test_dimension_chorus.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
TRIPLE_DOUBLE_CHORUS_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/triple_double_chorus.hpp",
    "src/effects/triple_double_chorus.cpp",
    "tests/test_triple_double_chorus.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "EFFECTS_MATRIX.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
FOUR_BAND_RESONATOR_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/four_band_resonator.hpp",
    "src/effects/four_band_resonator.cpp",
    "tests/test_four_band_resonator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "scripts/publish_checkpoint.py",
)
FRACTAL_RESONATOR_EFFECT_CHECKPOINT_PATHS = (
    "apps/resonator_bench.cpp",
    "CMakeLists.txt",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/resonator.hpp",
    "src/effects/resonator.cpp",
    "tests/test_resonator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/checkpoint_line_selectors.py",
    "scripts/test_checkpoint_line_selectors.py",
    "scripts/test_checkpoint_line_replacements.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
FRACTAL_RESONATOR_INPUT_GAIN_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "include/tabletfx/effects/four_band_resonator.hpp",
    "src/effects/four_band_resonator.cpp",
    "tests/test_four_band_resonator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
DIATONIC_HARMONIZER_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/diatonic_harmonizer.hpp",
    "src/effects/diatonic_harmonizer.cpp",
    "tests/test_diatonic_harmonizer.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
GRAIN_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/grain_delay.hpp",
    "src/effects/grain_delay.cpp",
    "tests/test_grain_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
PITCH_ARPEGGIATOR_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/diatonic_harmonizer.hpp",
    "include/tabletfx/effects/pitch_arpeggiator.hpp",
    "src/effects.cpp",
    "src/effects/diatonic_harmonizer.cpp",
    "src/effects/pitch_arpeggiator.cpp",
    "tests/test_diatonic_harmonizer.cpp",
    "tests/test_pitch_arpeggiator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
PITCH_FEEDBACK_DELAY_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/pitch_feedback_delay.hpp",
    "src/effects.cpp",
    "src/effects/pitch_feedback_delay.cpp",
    "tests/test_pitch_feedback_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
FEEDBACK_GENERATOR_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/feedback_generator.hpp",
    "src/effects/feedback_generator.cpp",
    "tests/test_feedback_generator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
FEEDBACK_GENERATOR_MODES_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/feedback_generator_bench.cpp",
    "include/tabletfx/effects/feedback_generator.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "src/effects/feedback_generator.cpp",
    "tests/test_feedback_generator.cpp",
    "web/app.js",
    "web/index.html",
)
SHIMMER_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/shimmer_reverb.hpp",
    "src/effects/shimmer_reverb.cpp",
    "tests/test_shimmer_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "README.md",
    "scripts/publish_checkpoint.py",
)
CONVOLUTION_REVERB_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effect.hpp",
    "include/tabletfx/effects/convolution_reverb.hpp",
    "src/effects/convolution_reverb.cpp",
    "tests/test_convolution_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/audio_regression.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "README.md",
    "scripts/publish_checkpoint.py",
)
HEADRUSH_CVERB_TEN_SECOND_IR_CHECKPOINT_PATHS = tuple(
    path for path in CONVOLUTION_REVERB_CHECKPOINT_PATHS if path != "Makefile"
) + (
    "make/publish.mk",
    "research-state.json",
    "scripts/run_cmake.py",
)
MONO_GUITAR_SYNTH_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/mono_guitar_synth.hpp",
    "src/effects.cpp",
    "src/effects/mono_guitar_synth.cpp",
    "tests/test_mono_guitar_synth.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
FLAGSHIP_CATALOG_CHECKPOINT_PATHS = (
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "RESEARCH.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "research-state.json",
    "README.md",
    "scripts/publish_checkpoint.py",
)
FIVE_BAND_EQ_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/five_band_eq.hpp",
    "tests/test_five_band_eq.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/audio_regression.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
PHRASE_LOOPER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/looper.hpp",
    "tests/test_looper.cpp",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "scripts/publish_checkpoint.py",
)
NOTE_GENERATOR_CHECKPOINT_PATHS = (
    "Makefile",
    "make/publish.mk",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/note_generator.hpp",
    "tests/test_note_generator.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
ENHANCER_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/enhancer.hpp",
    "tests/test_enhancer.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "research-state.json",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "scripts/publish_checkpoint.py",
)
POLY_SUSTAIN_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/poly_sustain.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/validate_docs.py",
    "scripts/web.py",
    "src/effects/poly_sustain.cpp",
    "src/effects/pitch_sequence.cpp",
    "include/tabletfx/effects/pitch_sequence.hpp",
    "src/web/wasm_chain.cpp",
    "tests/test_pitch_sequence.cpp",
    "tests/test_poly_sustain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
STEP_FILTER_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/step_filter_delay.hpp",
    "src/effects/step_filter_delay.cpp",
    "tests/test_step_filter_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/inspect_volume_pan_path.py",
    "scripts/publish_checkpoint.py",
)
FRACTAL_CHORUS_TEMPO_CHECKPOINT_PATHS = (
    "apps/fractal_chorus_tempo_bench.cpp",
    "apps/offline_render.cpp",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects/chorus.hpp",
    "src/effects/chorus.cpp",
    "tests/test_chorus.cpp",
    "scripts/inspect_fractal_chorus_tempo.py",
    "scripts/run_cmake.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "scripts/publish_checkpoint.py",
)
FRACTAL_CHORUS_VOICE_COUNT_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "research-state.json",
    "apps/fractal_chorus_tempo_bench.cpp",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/chorus.hpp",
    "scripts/test_offline_cli.py",
    "src/effects/chorus.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_chorus.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
FRACTAL_CHORUS_DELAY_RANGE_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "research-state.json",
    "apps/fractal_chorus_tempo_bench.cpp",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/chorus.hpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/chorus.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_chorus.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
VOLUME_PAN_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/volume_pan.hpp",
    "scripts/inspect_volume_pan_path.py",
    "src/effects/volume_pan.cpp",
    "tests/test_volume_pan.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
STEP_TREMOLO_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/step_tremolo.hpp",
    "src/effects/step_tremolo.cpp",
    "tests/test_step_tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/inspect_effect_api.py",
    "scripts/inspect_web_effect_path.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

STEREO_FILTER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/stereo_filter.hpp",
    "src/effects/stereo_filter.cpp",
    "tests/test_stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "scripts/web.py",
    "scripts/inspect_effect_api.py",
    "scripts/inspect_effects_doc_status.py",
    "scripts/inspect_web_effect_path.py",
    "scripts/inspect_publish_checkpoint.py",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
DEFERRED_AGENT_DRAFT_PATHS = (
    "include/tabletfx/effects/octave_fuzz.hpp",
    "src/effects/octave_fuzz.cpp",
    "tests/test_octave_fuzz.cpp",
    "include/tabletfx/effects/manual_thru_zero_flanger.hpp",
    "src/effects/manual_thru_zero_flanger.cpp",
    "tests/test_manual_thru_zero_flanger.cpp",
    "include/tabletfx/effects/enhancer.hpp",
    "tests/test_enhancer.cpp",
    "include/tabletfx/effects/de_esser.hpp",
    "src/effects/de_esser.cpp",
    "tests/test_de_esser.cpp",
    "include/tabletfx/effects/megatap_delay.hpp",
    "src/effects/megatap_delay.cpp",
    "tests/test_megatap_delay.cpp",
    "include/tabletfx/effects/pugilist.hpp",
    "src/effects/pugilist.cpp",
    "tests/test_pugilist.cpp",
    "include/tabletfx/effects/celestial_reverb.hpp",
    "src/effects/celestial_reverb.cpp",
    "tests/test_celestial_reverb.cpp",
    "include/tabletfx/effects/pitch_ring_mod.hpp",
    "src/effects/pitch_ring_mod.cpp",
    "tests/test_pitch_ring_mod.cpp",
    "include/tabletfx/effects/stereo_crossover.hpp",
    "src/effects/stereo_crossover.cpp",
    "tests/test_stereo_crossover.cpp",
    "include/tabletfx/effects/tape_echo.hpp",
    "src/effects/tape_echo.cpp",
    "tests/test_tape_echo.cpp",
    "scripts/inspect_effect_draft.py",
)
RESONANT_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "GOAL.md",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/resonant_delay.hpp",
    "src/effects/resonant_delay.cpp",
    "tests/test_resonant_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
HARMONIC_TREMOLO_CHECKPOINT_PATHS = (
    "Makefile",
    "README.md",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/harmonic_tremolo.hpp",
    "src/effects/harmonic_tremolo.cpp",
    "tests/test_harmonic_tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
AUTO_SWELL_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/auto_swell_delay.hpp",
    "src/effects/auto_swell_delay.cpp",
    "tests/test_auto_swell_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
SHUFFLING_LOOPER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/shuffling_looper_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/shuffling_looper.hpp",
    "make/publish.mk",
    "research-state.json",
    "src/effects/shuffling_looper.cpp",
    "tests/test_shuffling_looper.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
TEN_TAP_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/ten_tap_delay.hpp",
    "src/effects/ten_tap_delay.cpp",
    "tests/test_ten_tap_delay.cpp",
    "include/tabletfx/effects/pitch_arpeggiator.hpp",
    "include/tabletfx/effects/reverse_delay.hpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
AUTO_PAN_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/auto_pan.hpp",
    "src/effects/auto_pan.cpp",
    "tests/test_auto_pan.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "Makefile",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
MEGATAP_DELAY_CHECKPOINT_PATHS = (
    "GOAL.md",
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/megatap_delay.hpp",
    "src/effects/megatap_delay.cpp",
    "tests/test_megatap_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
DE_ESSER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/de_esser.hpp",
    "src/effects/de_esser.cpp",
    "tests/test_de_esser.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
PUGILIST_CHECKPOINT_PATHS = (
    "GOAL.md",
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/pugilist.hpp",
    "src/effects/pugilist.cpp",
    "tests/test_pugilist.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
VIBE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/vibe.hpp",
    "src/effects/vibe.cpp",
    "tests/test_vibe.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
MULTI_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "GOAL.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/multi_delay.hpp",
    "src/effects/multi_delay.cpp",
    "tests/test_multi_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

PORTA_PREAMP_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "GOAL.md",
    "README.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/porta_preamp.hpp",
    "src/effects/porta_preamp.cpp",
    "tests/test_porta_preamp.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

PITCH_SEQUENCE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "README.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/pitch_sequence.hpp",
    "src/effects/pitch_sequence.cpp",
    "tests/test_pitch_sequence.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FENDER_PITCH_SEQUENCE_AUDIT_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FENDER_VIRTUAL_CAPO_MIX_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/fender_virtual_capo_mix_bench.cpp",
    "include/tabletfx/effects/poly_pitch_shifter.hpp",
    "src/effects/poly_pitch_shifter.cpp",
    "tests/test_poly_pitch_shifter.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

RHYTHM_TAP_DELAY_CHECKPOINT_PATHS = (
    "include/tabletfx/effects/ten_tap_delay.hpp",
    "src/effects/ten_tap_delay.cpp",
    "tests/test_ten_tap_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "README.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

PLEX_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "GOAL.md",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/plex_delay.hpp",
    "src/effects/plex_delay.cpp",
    "tests/test_plex_delay.cpp",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FLAGSHIP_DELAYS_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "include/tabletfx/effects/ducking_delay.hpp",
    "include/tabletfx/effects/plex_delay.hpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/ducking_delay.cpp",
    "src/effects/plex_delay.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_ducking_delay.cpp",
    "tests/test_plex_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

CELESTIAL_REVERB_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/celestial_reverb.hpp",
    "src/effects/celestial_reverb.cpp",
    "tests/test_celestial_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "scripts/test_offline_cli.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
CLOUD_REVERB_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "README.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/cloud_reverb.hpp",
    "src/effects/cloud_reverb.cpp",
    "tests/test_cloud_reverb.cpp",
    "scripts/test_offline_cli.py",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
FRACTAL_TZF_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/manual_thru_zero_flanger.hpp",
    "src/effects/manual_thru_zero_flanger.cpp",
    "tests/test_manual_thru_zero_flanger.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "README.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
FRACTAL_TZF_AUTO_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "include/tabletfx/effects/manual_thru_zero_flanger.hpp",
    "src/effects/manual_thru_zero_flanger.cpp",
    "tests/test_manual_thru_zero_flanger.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
MERCATOR_DIST_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/mercator_distortion.hpp",
    "src/effects/mercator_distortion.cpp",
    "tests/test_mercator_distortion.cpp",
    "scripts/audio_regression.py",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "README.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
FRACTAL_THREE_VOICE_SYNTH_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/fractal_three_voice_synth.hpp",
    "src/effects/fractal_three_voice_synth.cpp",
    "tests/test_fractal_three_voice_synth.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "README.md",
    "scripts/publish_checkpoint.py",
)
FRACTAL_SURROUND_DELAY_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "CMakeLists.txt",
    "apps/fractal_surround_delay_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/fractal_surround_delay.hpp",
    "src/effects/fractal_surround_delay.cpp",
    "tests/test_fractal_surround_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "scripts/publish_checkpoint.py",
)
FRACTAL_PLEX_SHIFT_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "CMakeLists.txt",
    "apps/fractal_plex_shift_bench.cpp",
    "include/tabletfx/effects/plex_delay.hpp",
    "src/effects/plex_delay.cpp",
    "tests/test_plex_delay.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "scripts/publish_checkpoint.py",
)
OCTOBOT_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/octobot.hpp",
    "src/effects/octobot.cpp",
    "tests/test_octobot.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
AM_RING_MOD_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/am_ring_mod.hpp",
    "src/effects/am_ring_mod.cpp",
    "tests/test_am_ring_mod.cpp",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)
TAPE_ECHO_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/tape_echo.hpp",
    "src/effects/tape_echo.cpp",
    "tests/test_tape_echo.cpp",
    "scripts/test_offline_cli.py",
    "scripts/audio_regression.py",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "scripts/inspect_effect_picker.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "AUDIO_COMPATIBILITY.md",
    "LATENCY_AND_QUALITY.md",
    "GOAL.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

OCTAVE_FUZZ_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/octave_fuzz.hpp",
    "src/effects/octave_fuzz.cpp",
    "tests/test_octave_fuzz.cpp",
    "scripts/test_offline_cli.py",
    "scripts/audio_regression.py",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FRACTAL_XVR_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/stereo_crossover.hpp",
    "src/effects/stereo_crossover.cpp",
    "tests/test_stereo_crossover.cpp",
    "scripts/test_offline_cli.py",
    "src/web/wasm_chain.cpp",
    "scripts/web.py",
    "scripts/audio_regression.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

ASHEVILLE_PATTRN_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/asheville_pattrn.hpp",
    "src/effects/asheville_pattrn.cpp",
    "tests/test_asheville_pattrn.cpp",
    "scripts/audio_regression.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

ROCK_BOX_DIST_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/rock_box_dist.hpp",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/rock_box_dist.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_rock_box_dist.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
)

ROCK_BOX_BOOST_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/rock_box_boost.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/rock_box_boost.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_rock_box_boost.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

STEREO_PANNER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/stereo_panner.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/stereo_panner.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_stereo_panner.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/studio_pan_delay_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/studio_pan_delay.hpp",
    "scripts/publish_checkpoint.py",
    "scripts/checkpoint_line_selectors.py",
    "scripts/test_checkpoint_line_selectors.py",
    "scripts/web.py",
    "src/effects/studio_pan_delay.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_studio_pan_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_VOLUME_PEDAL_MUTE_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects/volume_pedal.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/volume_pedal.cpp",
    "tests/test_volume_pedal.cpp",
    "web/app.js",
)

HEADRUSH_BBD_MODE_CHECKPOINT_PATHS = (
    "LATENCY_AND_QUALITY.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "scripts/publish_checkpoint.py",
    "research-state.json",
    "src/effects.cpp",
    "tests/test_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

DELUXE_LOOPER_CHECKPOINT_PATHS = (
    "make/publish.mk",
    "AUDIO_COMPATIBILITY.md",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/deluxe_looper.hpp",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "tests/test_deluxe_looper.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

XR1_CABINET_IR_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/cabinet_ir.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/cabinet_ir.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_cabinet_ir.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/wav_decoder.js",
)

XR1_GLOBAL_EQ_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/xr1_global_eq_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/five_band_eq.hpp",
    "include/tabletfx/effects/xr1_global_eq.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/xr1_global_eq.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_xr1_global_eq.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_OCTAVE_DIVIDER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/octave_divider.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/octave_divider.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_octave_divider.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

LINE6_DOUBLE_TAKE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/double_take.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/double_take.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_double_take.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_EQ5_GRAPHIC_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/eq5_graphic.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/eq5_graphic.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_eq5_graphic.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_OPTICAL_TREMOLO_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/optical_tremolo.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/optical_tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_optical_tremolo.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_BIAS_TREM_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/fractal_bias_trem.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/fractal_bias_trem.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_fractal_bias_trem.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_VIBRATONE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/vibratone.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/vibratone.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_vibratone.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_UNIVERSE_VIBE_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects/vibe.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/vibe.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_vibe.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

LINE6_FOUR_OSC_GENERATOR_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects/note_generator.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "tests/test_note_generator.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

LINE6_POLY_DETUNE_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/poly_detune.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/poly_detune.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_poly_detune.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_POLY_PITCH_RANGE_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "tests/test_poly_pitch_shifter.cpp",
    "web/app.js",
    "web/index.html",
)

FRACTAL_DIATONIC_CUSTOM_SCALE_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/diatonic_harmonizer.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "src/effects/diatonic_harmonizer.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_diatonic_harmonizer.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_PEDAL_SHIFTER_CHECKPOINT_PATHS = (
    "apps/offline_render.cpp",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "include/tabletfx/effects/poly_pitch_shifter.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/poly_pitch_shifter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_poly_pitch_shifter.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FENDER_PEDAL_DETUNE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/pedal_detune.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/pedal_detune.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_pedal_detune.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_TOUCH_WAH_MIX_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects/stereo_filter.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "src/effects/stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_stereo_filter.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FLAGSHIP_CONTROL_ROUTING_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/stereo_filter.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "src/effects/stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_delay.cpp",
    "tests/test_stereo_filter.cpp",
    "web/app.js",
    "web/index.html",
)

FRACTAL_RING_MOD_TRACKING_CHECKPOINT_PATHS = (
    "AUDIO_COMPATIBILITY.md",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "include/tabletfx/effects/ring_modulator.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "src/effects/ring_modulator.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_ring_modulator.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_PEQ_TYPES_SOLO_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "include/tabletfx/effects/five_band_eq.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "src/web/wasm_chain.cpp",
    "tests/test_five_band_eq.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

FRACTAL_FILTER_AUTO_WAH_CHECKPOINT_PATHS = (
    "include/tabletfx/effects/stereo_filter.hpp",
    "src/effects/stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_stereo_filter.cpp",
    "web/audio-worklet.js",
    "web/app.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FRACTAL_FILTER_ENVELOPE_CHECKPOINT_PATHS = (
    "include/tabletfx/effects/stereo_filter.hpp",
    "src/effects/stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_stereo_filter.cpp",
    "web/audio-worklet.js",
    "web/app.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FRACTAL_FILTER_LADDER_CHECKPOINT_PATHS = (
    "include/tabletfx/effects/stereo_filter.hpp",
    "src/effects/stereo_filter.cpp",
    "tests/test_stereo_filter.cpp",
    "src/web/wasm_chain.cpp",
    "web/audio-worklet.js",
    "web/app.js",
    "web/index.html",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

CHROMATIC_PITCH_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/chromatic_pitch_delay.hpp",
    "include/tabletfx/effects/diatonic_harmonizer.hpp",
    "include/tabletfx/effects/pitch_arpeggiator.hpp",
    "include/tabletfx/effects/poly_pitch_shifter.hpp",
    "src/effects/chromatic_pitch_delay.cpp",
    "src/effects/diatonic_harmonizer.cpp",
    "src/effects/pitch_arpeggiator.cpp",
    "src/effects/poly_pitch_shifter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_chromatic_pitch_delay.cpp",
    "tests/test_diatonic_harmonizer.cpp",
    "tests/test_pitch_arpeggiator.cpp",
    "tests/test_poly_pitch_shifter.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

STADIUM_MUTANT_FILTER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/envelope_wah.hpp",
    "include/tabletfx/effects/mutant_filter.hpp",
    "src/effects/envelope_wah.cpp",
    "src/effects/mutant_filter.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_mutant_filter.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

STADIUM_SEVEN_BAND_PEQ_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/stadium_seven_band_peq.hpp",
    "src/web/wasm_chain.cpp",
    "tests/test_stadium_seven_band_peq.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FRACTAL_CHROMATIC_HARMONIZER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/fractal_chromatic_harmonizer.hpp",
    "src/effects/fractal_chromatic_harmonizer.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_fractal_chromatic_harmonizer.cpp",
    "scripts/web.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

FRACTAL_DIATONIC_HARMONIZER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "GOAL.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/offline_render.cpp",
    "apps/fractal_diatonic_harmonizer_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/fractal_diatonic_harmonizer.hpp",
    "src/effects/fractal_diatonic_harmonizer.cpp",
    "src/effects/fractal_chromatic_harmonizer.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_fractal_chromatic_harmonizer.cpp",
    "tests/test_fractal_diatonic_harmonizer.cpp",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "scripts/test_offline_cli.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "web/style.css",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

LINE6_STADIUM_3OSC_RESEARCH_CHECKPOINT_PATHS = (
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "research-state.json",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
)

AXOM_SOURCE_AUDIT_CHECKPOINT_PATHS = (
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "RESEARCH.md",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
)

STADIUM_DELAY_TRAILS_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "src/effects.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_delay.cpp",
    "scripts/test_offline_cli.py",
    "scripts/web.py",
    "scripts/inspect_delay_trails.py",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "scripts/publish_checkpoint.py",
)

PRIME_REVERB_TAILS_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "make/publish.mk",
    "research-state.json",
    "apps/offline_render.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/hall_reverb.hpp",
    "include/tabletfx/effects/plate_reverb.hpp",
    "include/tabletfx/effects/spring_reverb.hpp",
    "src/effects/room_reverb.cpp",
    "src/effects/hall_reverb.cpp",
    "src/effects/plate_reverb.cpp",
    "src/effects/spring_reverb.cpp",
    "src/web/wasm_chain.cpp",
    "tests/reverb_bypass_test_helpers.hpp",
    "tests/test_room_reverb.cpp",
    "tests/test_hall_reverb.cpp",
    "tests/test_plate_reverb.cpp",
    "tests/test_spring_reverb.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
    "scripts/publish_checkpoint.py",
)

PITCH_RING_MOD_CHECKPOINT_PATHS = (
    "Makefile",
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/pitch_ring_mod_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/pitch_ring_mod.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/pitch_ring_mod.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_pitch_ring_mod.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

HEADRUSH_PHASER_STEREO_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/headrush_phaser_stereo_bench.cpp",
    "include/tabletfx/effects/phaser.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "src/effects/phaser.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_phaser.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
STADIUM_DYNAMICS_SIDECHAIN_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/stadium_dynamics_sidechain_bench.cpp",
    "include/tabletfx/effects/compressor.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/validate_docs.py",
    "scripts/web.py",
    "src/web/wasm_chain.cpp",
    "tests/test_compressor.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
HEADRUSH_PHASE_INVERT_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/headrush_phase_invert_bench.cpp",
    "include/tabletfx/effects.hpp",
    "include/tabletfx/effects/phase_invert.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/phase_invert.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_phase_invert.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FRACTAL_VCO_RESPONSE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/fractal_flanger_vco_bench.cpp",
    "include/tabletfx/effects/manual_thru_zero_flanger.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "src/effects/manual_thru_zero_flanger.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_manual_thru_zero_flanger.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
FENDER_HARMONIC_TREMOLO_MODE_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "apps/fender_harmonic_tremolo_mode_bench.cpp",
    "include/tabletfx/effects/harmonic_tremolo.hpp",
    "make/publish.mk",
    "research-state.json",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/harmonic_tremolo.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_harmonic_tremolo.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)

CHECKPOINTS = {
    "headrush-phase-invert": (
        HEADRUSH_PHASE_INVERT_CHECKPOINT_PATHS,
        "feat: add HeadRush Prime per-block Phase Invert control",
    ),
    "headrush-phaser-stereo": (
        HEADRUSH_PHASER_STEREO_CHECKPOINT_PATHS,
        "feat: add project-authored Phaser stereo spread control",
    ),
    "stadium-dynamics-sidechain": (
        STADIUM_DYNAMICS_SIDECHAIN_CHECKPOINT_PATHS,
        "feat: add project-limited compressor sidechain source",
    ),
    "fractal-vco-response": (
        FRACTAL_VCO_RESPONSE_CHECKPOINT_PATHS,
        "feat: add project-authored flanger VCO response controls",
    ),
    "fender-harmonic-tremolo-mode": (
        FENDER_HARMONIC_TREMOLO_MODE_CHECKPOINT_PATHS,
        "feat: add Harmonic Tremolo mode selector",
    ),
    "pitch-ring-mod": (
        PITCH_RING_MOD_CHECKPOINT_PATHS,
        "feat: add project-authored Pitch Ring Mod effect",
    ),
    "tone-master-small-hall": (
        TONE_MASTER_SMALL_HALL_CHECKPOINT_PATHS,
        "feat: add project-authored Modulated Small Hall",
    ),
    "tone-master-modulated-large-hall": (
        TONE_MASTER_MODULATED_LARGE_HALL_CHECKPOINT_PATHS,
        "feat: add project-authored Modulated Large Hall",
    ),
    "fender-3d-chorus": (
        (
            "CMakeLists.txt",
            "EFFECTS_MATRIX.md",
            "FLAGSHIP_EFFECT_COMPARISON.md",
            "IMPLEMENTATION_CHECKLIST.md",
            "LATENCY_AND_QUALITY.md",
            "apps/tone_master_pro_3d_chorus_bench.cpp",
            "include/tabletfx/effects.hpp",
            "include/tabletfx/effects/tone_master_pro_3d_chorus.hpp",
            "make/publish.mk",
            "RESEARCH.md",
            "research-state.json",
            "scripts/publish_checkpoint.py",
            "scripts/run_cmake.py",
            "scripts/web.py",
            "src/effects/tone_master_pro_3d_chorus.cpp",
            "src/web/wasm_chain.cpp",
            "tests/test_tone_master_pro_3d_chorus.cpp",
            "web/app.js",
            "web/audio-worklet.js",
            "web/index.html",
        ),
        "feat: add project-authored 3D Chorus modes",
    ),
    "axom-source-audit": (
        AXOM_SOURCE_AUDIT_CHECKPOINT_PATHS,
        "docs: audit Melofty Axom Max effect sources",
    ),
    "prime-reverb-tails": (
        PRIME_REVERB_TAILS_CHECKPOINT_PATHS,
        "feat: add optional reverb tails on bypass",
    ),
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
    "fractal-rotary-depth": (FRACTAL_ROTARY_DEPTH_CHECKPOINT_PATHS,
                             "fix: respect rotary Hi Depth at zero"),
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
    "formant-filter": (FORMANT_FILTER_CHECKPOINT_PATHS,
                       "feat: add project-authored formant filter"),
    "fm9-formant-morph": (
        FM9_FORMANT_MORPH_CHECKPOINT_PATHS,
        "feat: add project-authored three-vowel formant mode",
    ),
    "fm9-formant-control-lfo": (
        FORMANT_CONTROL_LFO_CHECKPOINT_PATHS,
        "feat: add project-authored Formant Control LFO",
    ),
    "headrush-prime-drop-tune": (
        HEADRUSH_PRIME_DROP_TUNE_CHECKPOINT_PATHS,
        "feat: add project-authored Drop Tune workflow",
    ),
    "fender-stereo-flanger-phase": (
        FENDER_STEREO_FLANGER_PHASE_CHECKPOINT_PATHS,
        "feat: add project-authored stereo Flanger phase control",
    ),
    "fender-flanger-tap-division": (
        FENDER_FLANGER_TAP_DIVISION_CHECKPOINT_PATHS,
        "feat: add Flanger tempo sync and Filter mode",
    ),
    "fender-phaser-90-tap-division": (
        FENDER_PHASER_90_TAP_DIVISION_CHECKPOINT_PATHS,
        "feat: add Phaser 90 Tap Division tempo sync",
    ),
    "phaser-stage-controls": (
        PHASER_STAGE_COUNTS_STEREO_CHECKPOINT_PATHS,
        "feat: extend Phaser stages and stereo control",
    ),
    "fender-tremolo-lfo-clip": (
        FENDER_TREMOLO_LFO_CLIP_CHECKPOINT_PATHS,
        "feat: add project-authored Tremolo LFO Clip",
    ),
    "fender-tremolo-duty-cycle": (
        FENDER_TREMOLO_DUTY_CYCLE_CHECKPOINT_PATHS,
        "feat: add project-authored Tremolo Duty Cycle",
    ),
    "fender-memory-delay": (
        FENDER_MEMORY_DELAY_CHECKPOINT_PATHS,
        "feat: complete flagship delay picker and add Memory Delay",
    ),
    "fractal-source-correction": (
        FRACTAL_SOURCE_CORRECTION_CHECKPOINT_PATHS,
        "docs: correct Fractal Chorus source notes",
    ),
    "fractal-synth-output-level": (
        FRACTAL_SYNTH_OUTPUT_LEVEL_CHECKPOINT_PATHS,
        "feat: add Fractal Synth block output Level",
    ),
    "harmonic-resonator": (HARMONIC_RESONATOR_CHECKPOINT_PATHS,
                           "feat: add project-authored harmonic resonator"),
    "multiband-compressor": (MULTIBAND_COMPRESSOR_CHECKPOINT_PATHS,
                              "feat: add project-authored multiband compressor"),
    "bit-crusher": (BIT_CRUSHER_CHECKPOINT_PATHS,
                    "feat: add project-authored bit crusher"),
    "dimension-chorus": (DIMENSION_CHORUS_CHECKPOINT_PATHS,
                          "feat: add project-authored dimension chorus"),
    "triple-double-chorus": (TRIPLE_DOUBLE_CHORUS_CHECKPOINT_PATHS,
                              "feat: add categorized triple-double chorus"),
    "four-band-resonator": (FOUR_BAND_RESONATOR_CHECKPOINT_PATHS,
                            "feat: add categorized four-band resonator"),
    "fractal-resonator-input-gain": (
        FRACTAL_RESONATOR_INPUT_GAIN_CHECKPOINT_PATHS,
        "feat: add Fractal-inspired resonator input gain",
    ),
    "diatonic-harmonizer": (DIATONIC_HARMONIZER_CHECKPOINT_PATHS,
                            "feat: add categorized diatonic harmonizer"),
    "grain-delay": (GRAIN_DELAY_CHECKPOINT_PATHS,
                    "feat: add project-authored grain delay"),
    "pitch-arpeggiator": (PITCH_ARPEGGIATOR_CHECKPOINT_PATHS,
                           "feat: add project-authored pitch arpeggiator"),
    "pitch-feedback-delay": (PITCH_FEEDBACK_DELAY_CHECKPOINT_PATHS,
                             "feat: add project-authored pitch feedback delay"),
    "feedback-generator": (FEEDBACK_GENERATOR_CHECKPOINT_PATHS,
                           "feat: add project-authored feedback generator"),
    "feedback-generator-modes": (
        FEEDBACK_GENERATOR_MODES_CHECKPOINT_PATHS,
        "feat: extend project feedback generator intervals",
    ),
    "shimmer-reverb": (SHIMMER_REVERB_CHECKPOINT_PATHS,
                       "feat: add project-authored shimmer reverb"),
    "convolution-reverb": (CONVOLUTION_REVERB_CHECKPOINT_PATHS,
                           "feat: extend convolution reverb controls and tails"),
    "headrush-cverb-ten-second-ir": (
        HEADRUSH_CVERB_TEN_SECOND_IR_CHECKPOINT_PATHS,
        "feat: support ten-second stereo C-Verb IRs in browser",
    ),
    "mono-guitar-synth": (MONO_GUITAR_SYNTH_CHECKPOINT_PATHS,
                          "feat: add project-authored mono guitar synth"),
    "flagship-catalog": (FLAGSHIP_CATALOG_CHECKPOINT_PATHS,
                         "docs: verify flagship effect inventory sources"),
    "five-band-eq": (FIVE_BAND_EQ_CHECKPOINT_PATHS,
                     "feat: add project-authored five-band EQ"),
    "phrase-looper": (PHRASE_LOOPER_CHECKPOINT_PATHS,
                      "feat: add threshold-triggered looper recording"),
    "note-generator": (NOTE_GENERATOR_CHECKPOINT_PATHS,
                       "feat: add categorized note generator"),
    "resonant-delay": (RESONANT_DELAY_CHECKPOINT_PATHS,
                       "feat: add project-authored resonant delay"),
    "harmonic-tremolo": (HARMONIC_TREMOLO_CHECKPOINT_PATHS,
                         "feat: add project-authored harmonic tremolo"),
    "auto-swell-delay": (AUTO_SWELL_DELAY_CHECKPOINT_PATHS,
                          "feat: add project-authored auto-swell delay"),
    "shuffling-looper": (SHUFFLING_LOOPER_CHECKPOINT_PATHS,
                          "feat: add looper capacity and beat quantization"),
    "ten-tap-delay": (TEN_TAP_DELAY_CHECKPOINT_PATHS,
                      "feat: add project-authored Ten-Tap Delay"),
    "auto-pan": (AUTO_PAN_CHECKPOINT_PATHS,
                 "feat: add project-authored auto-pan"),
    "megatap-delay": (MEGATAP_DELAY_CHECKPOINT_PATHS,
                      "feat: add project-authored Megatap delay"),
    "de-esser": (DE_ESSER_CHECKPOINT_PATHS,
                 "feat: add project-authored De-Esser"),
    "dual-engine-distortion": (
        PUGILIST_CHECKPOINT_PATHS,
        "feat: add project-authored dual-engine distortion",
    ),
    "vibe": (
        VIBE_CHECKPOINT_PATHS,
        "feat: add project-authored Vibe modulation",
    ),
    "multi-delay": (MULTI_DELAY_CHECKPOINT_PATHS,
                    "feat: add Multi-Delay time LFOs"),
    "porta-preamp": (PORTA_PREAMP_CHECKPOINT_PATHS,
                     "feat: add categorized project-authored Porta Preamp"),
    "pitch-sequence": (PITCH_SEQUENCE_CHECKPOINT_PATHS,
                       "feat: add categorized project-authored Pitch Sequence"),
    "rhythm-tap-delay": (RHYTHM_TAP_DELAY_CHECKPOINT_PATHS,
                         "feat: add project-authored Rhythm Tap delay"),
    "plex-delay": (PLEX_DELAY_CHECKPOINT_PATHS,
                   "feat: add Plex Delay kill dry control"),
    "flagship-delays": (
        FLAGSHIP_DELAYS_CHECKPOINT_PATHS,
        "feat: add flagship delay controls",
    ),
    "enhancer": (ENHANCER_CHECKPOINT_PATHS,
                 "feat: add categorized stereo enhancer"),
    "poly-sustain": (POLY_SUSTAIN_CHECKPOINT_PATHS,
                     "feat: add categorized Poly Sustain-inspired hold"),
    "step-filter-delay": (
        STEP_FILTER_DELAY_CHECKPOINT_PATHS,
        "feat: add categorized Step Filter Delay",
    ),
    "volume-pan": (
        VOLUME_PAN_CHECKPOINT_PATHS,
        "feat: add categorized Volume/Pan utility",
    ),
    "stereo-filter": (
        STEREO_FILTER_CHECKPOINT_PATHS,
        "feat: add categorized Stereo Filter",
    ),
    "step-tremolo": (
        STEP_TREMOLO_CHECKPOINT_PATHS,
        "feat: add categorized Step Tremolo",
    ),
    "stadium-tap-reset": (
        (
            "FLAGSHIP_EFFECT_COMPARISON.md",
            "IMPLEMENTATION_CHECKLIST.md",
            "LATENCY_AND_QUALITY.md",
            "RESEARCH.md",
            "include/tabletfx/effect.hpp",
            "include/tabletfx/effects/rotary_speaker.hpp",
            "include/tabletfx/effects/tremolo.hpp",
            "research-state.json",
            "scripts/publish_checkpoint.py",
            "scripts/web.py",
            "src/effects/rotary_speaker.cpp",
            "src/effects/tremolo.cpp",
            "src/web/wasm_chain.cpp",
            "tests/test_rotary_speaker.cpp",
            "tests/test_tremolo.cpp",
            "web/app.js",
            "web/audio-worklet.js",
            "web/index.html",
        ),
        "feat: add Stadium TAP modulation reset",
    ),
    "fractal-chorus-tempo": (
        FRACTAL_CHORUS_TEMPO_CHECKPOINT_PATHS,
        "feat: add project-authored Chorus tone controls",
    ),
    "fractal-chorus-voice-count": (
        FRACTAL_CHORUS_VOICE_COUNT_CHECKPOINT_PATHS,
        "feat: add project-authored Chorus voice count",
    ),
    "fractal-chorus-delay-range": (
        FRACTAL_CHORUS_DELAY_RANGE_CHECKPOINT_PATHS,
        "feat: add Chorus minimum delay and range controls",
    ),
    "celestial-reverb": (CELESTIAL_REVERB_CHECKPOINT_PATHS,
                         "feat: add project-authored Celestial Reverb"),
    "cloud-reverb": (CLOUD_REVERB_CHECKPOINT_PATHS,
                     "feat: add project-authored Cloud Reverb"),
    "fractal-tzf": (FRACTAL_TZF_CHECKPOINT_PATHS,
                    "feat: add project-authored manual through-zero flanger"),
    "fractal-tzf-auto": (
        FRACTAL_TZF_AUTO_CHECKPOINT_PATHS,
        "feat: add project-authored through-zero flanger modes",
    ),
    "mercator-dist": (MERCATOR_DIST_CHECKPOINT_PATHS,
                      "feat: add project-authored three-stage distortion"),
    "fractal-three-voice-synth": (
        FRACTAL_THREE_VOICE_SYNTH_CHECKPOINT_PATHS,
        "test: verify three-voice synth browser and Bass paths",
    ),
    "fractal-surround-delay": (
        FRACTAL_SURROUND_DELAY_CHECKPOINT_PATHS,
        "feat: add project-authored Fractal Surround Delay",
    ),
    "fractal-plex-shift": (
        FRACTAL_PLEX_SHIFT_CHECKPOINT_PATHS,
        "feat: add project-authored Plex Shift subset",
    ),
    "fractal-ring-mod-tracking": (
        FRACTAL_RING_MOD_TRACKING_CHECKPOINT_PATHS,
        "feat: add project-authored ring mod pitch tracking",
    ),
    "fractal-peq-types-solo": (
        FRACTAL_PEQ_TYPES_SOLO_CHECKPOINT_PATHS,
        "feat: add project-authored PEQ variants and solo",
    ),
    "fractal-filter-auto-wah": (
        FRACTAL_FILTER_AUTO_WAH_CHECKPOINT_PATHS,
        "feat: add categorized Fractal Auto-Wah",
    ),
    "fractal-filter-envelope": (
        FRACTAL_FILTER_ENVELOPE_CHECKPOINT_PATHS,
        "feat: add project-authored Envelope Filter",
    ),
    "fractal-filter-ladder": (
        FRACTAL_FILTER_LADDER_CHECKPOINT_PATHS,
        "feat: add project-authored Touch-Wah filter mode",
    ),
    "chromatic-pitch-delay": (
        CHROMATIC_PITCH_DELAY_CHECKPOINT_PATHS,
        "feat: add chromatic pitch delay and wide arpeggiator range",
    ),
    "octobot": (
        OCTOBOT_CHECKPOINT_PATHS,
        "feat: add project-authored Octobot effect",
    ),
    "am-ring-mod": (
        AM_RING_MOD_CHECKPOINT_PATHS,
        "feat: add project-authored AM ring modulation",
    ),
    "tape-echo": (
        TAPE_ECHO_CHECKPOINT_PATHS,
        "feat: add independent stereo Tape Echo timing",
    ),
    "octave-fuzz": (
        OCTAVE_FUZZ_CHECKPOINT_PATHS,
        "feat: add project-authored Octave Fuzz",
    ),
    "fractal-xvr": (
        FRACTAL_XVR_CHECKPOINT_PATHS,
        "feat: add project-authored stereo crossover",
    ),
    "asheville-pattrn": (
        ASHEVILLE_PATTRN_CHECKPOINT_PATHS,
        "feat: add project-authored Asheville Pattrn",
    ),
    "rock-box-dist": (
        ROCK_BOX_DIST_CHECKPOINT_PATHS,
        "feat: add categorized Rock Box Dist processor",
    ),
    "rock-box-boost": (
        ROCK_BOX_BOOST_CHECKPOINT_PATHS,
        "feat: add categorized Rock Box Boost processor",
    ),
    "stereo-panner": (
        STEREO_PANNER_CHECKPOINT_PATHS,
        "feat: add categorized Fractal-style stereo panner",
    ),
    "fender-studio-delay-pan": (
        FENDER_STUDIO_DELAY_PAN_CHECKPOINT_PATHS,
        "feat: add Studio Delay envelope pan mode mapping",
    ),
    "fender-volume-pedal-mute": (
        FENDER_VOLUME_PEDAL_MUTE_CHECKPOINT_PATHS,
        "fix: fully mute volume pedal at heel",
    ),
    "headrush-bbd-mode": (
        HEADRUSH_BBD_MODE_CHECKPOINT_PATHS,
        "feat: add project-authored BBD delay modes",
    ),
    "deluxe-looper": (
        DELUXE_LOOPER_CHECKPOINT_PATHS,
        "feat: add project-authored Deluxe Looper",
    ),
    "xr1-cabinet-ir": (
        XR1_CABINET_IR_CHECKPOINT_PATHS,
        "feat: add categorized Cabinet IR loader",
    ),
    "xr1-global-eq": (
        XR1_GLOBAL_EQ_CHECKPOINT_PATHS,
        "feat: add categorized XR1-inspired Global EQ",
    ),
    "fractal-octave-divider": (
        FRACTAL_OCTAVE_DIVIDER_CHECKPOINT_PATHS,
        "feat: add categorized Fractal-inspired Octave Divider",
    ),
    "line6-double-take": (
        LINE6_DOUBLE_TAKE_CHECKPOINT_PATHS,
        "feat: add categorized Double Take modulation",
    ),
    "fender-eq5-graphic": (
        FENDER_EQ5_GRAPHIC_CHECKPOINT_PATHS,
        "feat: add categorized Fender EQ5 Graphic",
    ),
    "fractal-optical-tremolo": (
        FRACTAL_OPTICAL_TREMOLO_CHECKPOINT_PATHS,
        "feat: add categorized Optical Tremolo variants",
    ),
    "fractal-bias-trem": (
        FRACTAL_BIAS_TREM_CHECKPOINT_PATHS,
        "feat: add categorized Bias Trem processor",
    ),
    "fender-vibratone": (
        FENDER_VIBRATONE_CHECKPOINT_PATHS,
        "feat: add categorized Fender Vibratone-inspired effect",
    ),
    "fender-universe-vibe": (
        FENDER_UNIVERSE_VIBE_CHECKPOINT_PATHS,
        "feat: add categorized Uni-Verse Vibe controls",
    ),
    "line6-four-osc-generator": (
        LINE6_FOUR_OSC_GENERATOR_CHECKPOINT_PATHS,
        "feat: add categorized 4 OSC Generator source",
    ),
    "line6-poly-detune": (
        LINE6_POLY_DETUNE_CHECKPOINT_PATHS,
        "feat: add categorized Poly Detune-inspired modulation",
    ),
    "fractal-poly-pitch-range": (
        FRACTAL_POLY_PITCH_RANGE_CHECKPOINT_PATHS,
        "feat: expose Fractal-range poly pitch transpose",
    ),
    "fractal-diatonic-custom-scale": (
        FRACTAL_DIATONIC_CUSTOM_SCALE_CHECKPOINT_PATHS,
        "feat: add categorized custom-scale harmonizer mode",
    ),
    "fender-pedal-shifter": (
        FENDER_PEDAL_SHIFTER_CHECKPOINT_PATHS,
        "feat: add categorized expression-controlled pitch sweep",
    ),
    "fender-pedal-detune": (
        FENDER_PEDAL_DETUNE_CHECKPOINT_PATHS,
        "feat: add categorized Fender-style pedal detune",
    ),
    "fractal-touch-wah-mix": (
        FRACTAL_TOUCH_WAH_MIX_CHECKPOINT_PATHS,
        "feat: add project-authored Touch-Wah Mix mode",
    ),
    "flagship-control-routing": (
        FLAGSHIP_CONTROL_ROUTING_CHECKPOINT_PATHS,
        "feat: add flagship filter and looper controls",
    ),
    "stadium-mutant-filter": (
        STADIUM_MUTANT_FILTER_CHECKPOINT_PATHS,
        "feat: add project-authored Mutant Filter effect",
    ),
    "stadium-seven-band-peq": (
        STADIUM_SEVEN_BAND_PEQ_CHECKPOINT_PATHS,
        "feat: add categorized seven-band parametric EQ",
    ),
    "fractal-chromatic-harmonizer": (
        FRACTAL_CHROMATIC_HARMONIZER_CHECKPOINT_PATHS,
        "feat: add project-authored four-voice chromatic harmonizer",
    ),
    "fractal-diatonic-harmonizer": (
        FRACTAL_DIATONIC_HARMONIZER_CHECKPOINT_PATHS,
        "feat: add project-authored diatonic harmonizer",
    ),
    "stadium-delay-trails": (
        STADIUM_DELAY_TRAILS_CHECKPOINT_PATHS,
        "feat: add optional delay tails on bypass",
    ),
    "line6-stadium-3osc-research": (
        LINE6_STADIUM_3OSC_RESEARCH_CHECKPOINT_PATHS,
        "docs: audit Stadium 3 OSC Synth model controls",
    ),
    "fender-pitch-sequence-audit": (
        FENDER_PITCH_SEQUENCE_AUDIT_CHECKPOINT_PATHS,
        "perf: benchmark project-authored pitch sequence",
    ),
    "fender-virtual-capo-mix": (
        FENDER_VIRTUAL_CAPO_MIX_CHECKPOINT_PATHS,
        "feat: add project-authored Virtual Capo Mix",
    ),
}

PARTIAL_CHECKPOINT_HUNKS = {
    "fender-harmonic-tremolo-mode": {
        "CMakeLists.txt": ("fender_harmonic_tremolo_mode_bench",),
        "EFFECTS_MATRIX.md": ("Current Fender Tone Master Pro Harmonic Tremolo Mode Switch checkpoint",),
        "FLAGSHIP_EFFECT_COMPARISON.md": ("Modulation / harmonic tremolo",),
        "IMPLEMENTATION_CHECKLIST.md": (
            "Fender Tone Master Pro Harmonic Tremolo Mode Switch",
            "6G4/6G4A selector",
        ),
        "LATENCY_AND_QUALITY.md": ("Fender Tone Master Pro Harmonic Tremolo mode switch",),
        "RESEARCH.md": ("Mode Switch integration (2026-11-27)",),
        "make/publish.mk": ("test-fender-harmonic-tremolo-mode", "fender-harmonic-tremolo-mode"),
        "research-state.json": (
            "Fender Tone Master Pro Harmonic Tremolo Mode Switch",
            '"next_device": "XRecer XR1"',
            '"previous_checkpoint_commit": "a38ed39233d7aa27b45174110b1de8fe7bfe9944"',
        ),
        "scripts/publish_checkpoint.py": (
            "FENDER_HARMONIC_TREMOLO_MODE_CHECKPOINT_PATHS",
            '"fender-harmonic-tremolo-mode": (',
            '"fender-harmonic-tremolo-mode": {',
        ),
        "scripts/run_cmake.py": ("bench_fender_harmonic_tremolo_mode", "bench-fender-harmonic-tremolo-mode"),
        "scripts/web.py": ("test_fender_harmonic_tremolo_mode", "test-fender-harmonic-tremolo-mode"),
        "src/web/wasm_chain.cpp": (
            "configure_harmonic_tremolo",
            "parameters.mode",
        ),
        "web/app.js": ("runFenderHarmonicTremoloModeBrowserSmokeTest", "harmonicTremoloMode"),
        "web/audio-worklet.js": ("0.5, 120, 2, 0, 1, 0",),
        "web/index.html": ("harmonic-tremolo-mode", "1963 6G4A"),
    },
    "fractal-vco-response": {
        "CMakeLists.txt": ("fractal_flanger_vco_bench",),
        "EFFECTS_MATRIX.md": ("Fractal FM9-family Flanger VCO Response checkpoint",),
        "FLAGSHIP_EFFECT_COMPARISON.md": ("## Fractal FM9-family Through-Zero Flanger VCO Response comparison",),
        "IMPLEMENTATION_CHECKLIST.md": ("## Fractal FM9-family Flanger VCO Response",),
        "LATENCY_AND_QUALITY.md": ("## Fractal FM9-family Flanger VCO Response",),
        "RESEARCH.md": ("## Fractal FM9-family Through-Zero Flanger VCO Response",),
        "include/tabletfx/effects/manual_thru_zero_flanger.hpp": ("FlangerVcoResponse", "vco_exponent"),
        "make/publish.mk": ("fractal-vco-response", "bench-fractal-flanger-vco"),
        "research-state.json": (
            "Fractal FM9-family Through-Zero Flanger VCO Response",
            '"next_device": "Fender Tone Master Pro"',
            '"previous_checkpoint_commit": "0356aa96e83c691bf24925905d65590254cf05b1"',
        ),
        "scripts/publish_checkpoint.py": (
            "FRACTAL_VCO_RESPONSE_CHECKPOINT_PATHS",
            '"fractal-vco-response": (',
            '"fractal-vco-response": {',
            "Checkpoint-untracked-recovery",
        ),
        "scripts/run_cmake.py": ("bench_fractal_flanger_vco", "bench-fractal-flanger-vco"),
        "src/effects/manual_thru_zero_flanger.cpp": ("apply_vco_response", "vco_response"),
        "src/web/wasm_chain.cpp": ("configure_manual_thru_zero_flanger", "parameters.vco_exponent"),
        "tests/test_manual_thru_zero_flanger.cpp": ("test_vco_response_curves_and_neutral_exponent", "vco_exponent"),
        "web/app.js": ("manualThruZeroFlangerControls", "vcoResponseRms"),
        "web/audio-worklet.js": (
            "0.34, 8, 5, 0.5, 0, 3, 0.5, 10, 2, 0.25, 5, 0, 180, 0, 1",
        ),
        "web/index.html": ("manual-tzf-vco-response", "VCO Response · project curve mapping"),
    },
    "headrush-phaser-stereo": {
        "CMakeLists.txt": ("headrush_phaser_stereo_bench",),
        "EFFECTS_MATRIX.md": (
            "Current HeadRush Prime Orange Phaser Stereo checkpoint",
        ),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "## HeadRush Prime Orange Phaser Stereo control",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## HeadRush Prime Orange Phaser Stereo control",
        ),
        "LATENCY_AND_QUALITY.md": (
            "## HeadRush Prime Orange Phaser Stereo project control",
        ),
        "RESEARCH.md": (
            "## HeadRush Prime Orange Phaser Stereo control",
        ),
        "include/tabletfx/effects/phaser.hpp": ("bool stereo = true;",),
        "make/publish.mk": ("headrush-phaser-stereo",),
        "research-state.json": (
            "HeadRush Prime Orange Phaser Stereo control",
        ),
        "scripts/publish_checkpoint.py": (
            "HEADRUSH_PHASER_STEREO_CHECKPOINT_PATHS",
            '"headrush-phaser-stereo": (',
            '"headrush-phaser-stereo": {',
        ),
        "scripts/run_cmake.py": ("bench_headrush_phaser_stereo",),
        "src/effects/phaser.cpp": ("!parameters_.stereo",),
        "src/web/wasm_chain.cpp": (
            "std::uint32_t tap_division, std::uint32_t stereo",
            "parameters.stereo = stereo != 0U;",
        ),
        "tests/test_phaser.cpp": ("test_stereo_toggle_and_default_compatibility",),
        "web/app.js": (
            "phaserStereo",
            "projectStereoControl",
        ),
        "web/audio-worklet.js": ("phaserWithStereo",),
        "web/index.html": (
            "phaser-stereo",
            "HeadRush Prime · project-authored control roles",
        ),
    },
    "pitch-ring-mod": {
        "CMakeLists.txt": ("pitch_ring_mod",),
        "EFFECTS_MATRIX.md": (
            "Current Line 6 Helix Stadium Pitch Ring Mod checkpoint",
        ),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "## Line 6 Helix Stadium Pitch Ring Mod",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## Line 6 Helix Stadium Pitch Ring Mod",
        ),
        "LATENCY_AND_QUALITY.md": (
            "## Line 6 Helix Stadium Pitch Ring Mod project effect",
        ),
        "Makefile": ("bench-pitch-ring-mod", "test-pitch-ring-mod"),
        "RESEARCH.md": (
            "## Line 6 Helix Stadium Pitch Ring Mod source",
        ),
        "include/tabletfx/effects.hpp": ("pitch_ring_mod.hpp",),
        "make/publish.mk": ("pitch-ring-mod",),
        "research-state.json": (
            "Line 6 Helix Stadium Pitch Ring Mod",
            "Fender Tone Master Pro Modulated Large Hall",
            "Drum Machine controls and project-authored categorized generator",
        ),
        "scripts/publish_checkpoint.py": (
            "PITCH_RING_MOD_CHECKPOINT_PATHS",
            '"pitch-ring-mod": (',
            '"pitch-ring-mod": {',
        ),
        "scripts/run_cmake.py": ("pitch-ring-mod",),
        "scripts/web.py": ("pitch_ring_mod", "pitch-ring-mod"),
        "src/web/wasm_chain.cpp": (
            "pitch_ring_mod",
            "pitch-ring-mod",
        ),
        "web/app.js": ("pitchRingMod", "Pitch Ring Mod"),
        "web/audio-worklet.js": (
            "pitch-ring-mod",
            "pitchRingMod",
        ),
        "web/index.html": ("pitch-ring-mod", "Pitch Ring Mod"),
    },
    "tone-master-modulated-large-hall": {
        "CMakeLists.txt": ("tone_master_modulated_large_hall",),
        "EFFECTS_MATRIX.md": (
            "Current Fender Tone Master Pro Modulated Large Hall checkpoint",
        ),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "Research snapshot: **2026-11-27**",
            "## Fender Tone Master Pro Modulated Large Hall",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## Fender Tone Master Pro Modulated Large Hall",
        ),
        "LATENCY_AND_QUALITY.md": (
            "## Fender Tone Master Pro Modulated Large Hall project effect",
        ),
        "RESEARCH.md": (
            "## Fender Tone Master Pro Modulated Large Hall source",
        ),
        "include/tabletfx/effects.hpp": (
            "tone_master_modulated_large_hall.hpp",
        ),
        "make/publish.mk": (
            ".PHONY: publish-tone-master-modulated-large-hall-plan stage-tone-master-modulated-large-hall",
            "publish-tone-master-modulated-large-hall-plan:",
            "stage-tone-master-modulated-large-hall:",
        ),
        "research-state.json": (
            "Fender Tone Master Pro Modulated Large Hall",
        ),
        "scripts/publish_checkpoint.py": (
            "TONE_MASTER_MODULATED_LARGE_HALL_CHECKPOINT_PATHS",
            '"tone-master-modulated-large-hall": (',
            "Minimal-context checkpoint patches",
            "Keep context lines so Git anchors each selected edit",
            "Show the exact checkpoint patch before publication",
            "Validate staged minimal-context hunks",
        ),
        "scripts/run_cmake.py": ("tone_master_modulated_large_hall",),
        "scripts/web.py": ("tone_master_modulated_large_hall",),
        "src/web/wasm_chain.cpp": (
            "tone_master_modulated_large_hall",
            "effects_.size() + 27",
        ),
        "web/app.js": (
            "toneMasterModulatedLargeHall",
            "Modulated Large Hall",
        ),
        "web/audio-worklet.js": (
            "tone-master-modulated-large-hall",
            "configureToneMasterModulatedLargeHall",
        ),
        "web/index.html": (
            "tone-master-modulated-large-hall",
            "Modulated Large Hall",
        ),
    },
    "fender-3d-chorus": {
        "make/publish.mk": ("PUBLISH_CHECKPOINTS += fender-3d-chorus",),
    },
    "stadium-delay-trails": {
        "make/publish.mk": ("PUBLISH_CHECKPOINTS += stadium-delay-trails",),
    },
    "fractal-synth-output-level": {
        "make/publish.mk": ("PUBLISH_CHECKPOINTS += fractal-synth-output-level",),
    },
    "fractal-surround-delay": {
        "make/publish.mk": (
            "PUBLISH_CHECKPOINTS += fractal-surround-delay",
            "bench-fractal-surround-delay",
            "test-fractal-surround-delay",
        ),
        "scripts/publish_checkpoint.py": (
            "FRACTAL_SURROUND_DELAY_CHECKPOINT_PATHS",
            '"fractal-surround-delay": (',
            "Fractal Surround Delay wires",
            "Fractal Surround Delay checkpoint recovery",
            "bench-fractal-surround-delay",
            '"CMakeLists.txt"',
            '"include/tabletfx/effects.hpp"',
        ),
        "CMakeLists.txt": ("fractal_surround_delay",),
        "include/tabletfx/effects.hpp": ("fractal_surround_delay.hpp",),
        "src/web/wasm_chain.cpp": (
            "FractalSurroundDelay",
            "effects_.size() + 24",
        ),
        "scripts/run_cmake.py": ("fractal-surround-delay",),
        "scripts/web.py": ("fractal-surround-delay",),
        "web/app.js": (
            "fractalSurroundDelay",
            "Surround Delay",
            "19 of ${effectRows.length}",
        ),
        "web/audio-worklet.js": ("fractal-surround-delay",),
        "web/index.html": (
            "fractal-surround-time-ms",
            "Fractal FM9-family Surround Delay controls",
        ),
        "EFFECTS_MATRIX.md": ("## Fractal FM9-family Surround Delay",),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "## Flagship effect implementation checklist",
            "## Fractal FM9-family Surround Delay",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## Fractal FM9-family Surround Delay",
        ),
        "LATENCY_AND_QUALITY.md": (
            "## Fractal FM9-family Surround Delay",
        ),
        "RESEARCH.md": ("## Fractal FM9-family Surround Delay",),
        "research-state.json": ("Fractal FM9-family Surround Delay",),
    },
    "fractal-plex-shift": {
        "make/publish.mk": ("PUBLISH_CHECKPOINTS += fractal-plex-shift",),
        "scripts/publish_checkpoint.py": (
            "FRACTAL_PLEX_SHIFT_CHECKPOINT_PATHS",
            '"fractal-plex-shift": (',
            '"fractal-plex-shift": {',
        ),
        "CMakeLists.txt": ("fractal_plex_shift",),
        "include/tabletfx/effects/plex_delay.hpp": (
            "pitch_shift_enabled",
            "Plex Shift subset",
        ),
        "src/effects/plex_delay.cpp": (
            "read_pitch_history",
            "pitch_shift_enabled",
        ),
        "tests/test_plex_delay.cpp": (
            "test_plex_shift_default_identity_and_voice_pitch",
            "test_plex_shift_reverse_granules_and_bounds",
        ),
        "src/web/wasm_chain.cpp": (
            "configure_plex_shift",
            "tf_chain_set_plex_shift",
        ),
        "scripts/run_cmake.py": ("fractal_plex_shift",),
        "scripts/web.py": ("_tf_chain_set_plex_shift",),
        "web/app.js": ("runPlexDelayBrowserSmokeTest", "pitchShiftEnabled"),
        "web/audio-worklet.js": ("_tf_chain_set_plex_shift",),
        "web/index.html": ("plex-delay-type", "optional per-line pitch"),
        "EFFECTS_MATRIX.md": ("Current Fractal FM9-family Plex Shift checkpoint",),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "Current Fractal FM9-family Plex Shift checkpoint",
            "Fractal FM9-family Plex Shift: rechecked",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## Fractal FM9-family Plex Shift",
        ),
        "LATENCY_AND_QUALITY.md": (
            "## Fractal FM9-family Plex Shift project subset",
        ),
        "RESEARCH.md": ("## Fractal FM9-family Plex Shift",),
        "research-state.json": ("Fractal FM9-family Plex Shift",),
    },
    "fender-memory-delay": {
        "CMakeLists.txt": ("fender_memory_delay",),
        "include/tabletfx/effects/modulated_delay.hpp": (
            "input_gain_db",
            "grit_percent",
            "ModulatedDelayMode::vibrato",
            "spillover",
        ),
        "make/publish.mk": (
            "publish-fender-memory-delay-plan:",
            "stage-fender-memory-delay:",
            "publish-fender-memory-delay:",
        ),
        "scripts/publish_checkpoint.py": (
            "FENDER_MEMORY_DELAY_CHECKPOINT_PATHS",
            '"fender-memory-delay": (',
            '"fender-memory-delay": {',
            '"memoryDelay"',
            "remaining_diff",
            "checkpoint-marked hunks",
            "allow_no_match",
            "complete flagship delay picker",
        ),
        "scripts/run_cmake.py": ("fender_memory_delay",),
        "scripts/web.py": ("_tf_chain_set_fender_memory_delay",),
        "src/effects/modulated_delay.cpp": (
            "current_input_gain_",
            "current_grit_amplitude_",
            "process_bypassed",
        ),
        "src/web/wasm_chain.cpp": (
            "fender_memory_delay_",
            "_tf_chain_set_fender_memory_delay",
            "effect_id == 118",
        ),
        "tests/test_modulated_delay.cpp": (
            "test_memory_delay_gain_only_changes_the_recorded_wet_input",
            "test_memory_delay_spillover_keeps_the_dry_path_and_stops_recording",
        ),
        "web/app.js": (
            "fenderMemoryDelay",
            "fender-memory-delay",
            "memoryDelay",
            "Memory Delay",
        ),
        "web/audio-worklet.js": ("fender-memory-delay",),
        "web/index.html": (
            "fender-memory-delay",
            "Memory Delay controls",
            "Memory Delay · project-authored Fender control roles",
        ),
    },
    "tone-master-small-hall": {
        "make/publish.mk": (
            "PUBLISH_CHECKPOINTS += tone-master-small-hall",
            "unstage-%:",
        ),
        "scripts/publish_checkpoint.py": (
            "TONE_MASTER_SMALL_HALL_CHECKPOINT_PATHS",
            '"tone-master-small-hall": (',
            '"tone-master-small-hall": {',
            "def unstage(",
        ),
        "src/web/wasm_chain.cpp": (
            "ToneMasterSmallHall",
            "tone_master_small_hall",
            "effects_.size() + 26",
            "effect_id == 119",
        ),
        "scripts/run_cmake.py": ("tone_master_small_hall",),
        "scripts/web.py": ("tone_master_small_hall",),
        "web/app.js": (
            "toneMasterSmallHall",
            "Modulated Small Hall",
            "119 ordered",
        ),
        "web/audio-worklet.js": (
            "tone-master-small-hall",
            "configureToneMasterSmallHall",
        ),
        "web/index.html": (
            "tone-master-small-hall",
            "Modulated Small Hall",
        ),
        "EFFECTS_MATRIX.md": ("## Fender Tone Master Pro Modulated Small Hall",),
        "FLAGSHIP_EFFECT_COMPARISON.md": (
            "Modulated Small Hall",
            "Fractal FM9-family Plex Shift",
        ),
        "IMPLEMENTATION_CHECKLIST.md": (
            "## Fender Tone Master Pro — Modulated Small Hall",
        ),
        "LATENCY_AND_QUALITY.md": ("## Fender Tone Master Pro Modulated Small Hall",),
        "RESEARCH.md": ("Tone Master Pro Model Guide", "Modulated Small Hall"),
        "research-state.json": ("Fender Tone Master Pro Modulated Small Hall",),
    },
}


def git(*args: str, capture: bool = True) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=capture, text=True, check=False
    )
    if result.returncode != 0:
        detail = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        raise SystemExit(detail or f"git {' '.join(args)} failed with status {result.returncode}")
    return result.stdout if capture else ""

PARTIAL_CHECKPOINT_HUNKS["headrush-phase-invert"] = {
    "CMakeLists.txt": ("tabletfx_phase_invert_tests", "headrush_phase_invert_bench"),
    "EFFECTS_MATRIX.md": ("## HeadRush Prime per-block Phase Invert",),
    "FLAGSHIP_EFFECT_COMPARISON.md": ("HeadRush Prime per-block Phase Invert control",),
    "IMPLEMENTATION_CHECKLIST.md": ("HeadRush Prime — per-block Phase Invert",),
    "LATENCY_AND_QUALITY.md": ("HeadRush Prime Phase Invert project control",),
    "RESEARCH.md": ("HeadRush Prime per-block Phase Invert (2026-11-27)",),
    "make/publish.mk": ("test-headrush-phase-invert", "headrush-phase-invert"),
    "research-state.json": (
        "HeadRush Prime per-block Phase Invert",
        "Audit the current exact-model documentation and firmware source",
    ),
    "scripts/publish_checkpoint.py": (
        "HEADRUSH_PHASE_INVERT_CHECKPOINT_PATHS",
        '"headrush-phase-invert": (',
        'PARTIAL_CHECKPOINT_HUNKS[\"headrush-phase-invert\"]',
    ),
    "scripts/run_cmake.py": ("bench_headrush_phase_invert", "bench-headrush-phase-invert"),
    "scripts/web.py": (
        "headrush_phase_invert_only",
        "test_headrush_phase_invert",
        "test-headrush-phase-invert",
    ),
    "src/web/wasm_chain.cpp": (
        "#include \"tabletfx/effects/phase_invert.hpp\"",
        "set_phase_invert",
        "phase_inverted(index)",
        "phase_invert_.process",
        "phase_invert_flags_",
    ),
    "web/app.js": (
        "const phaseInvertByEffect",
        "function populatePhaseInvertTargets()",
        "phaseInvert: testOptions?.phaseInvert",
        "phaseInvertEnabled.addEventListener",
        "runHeadRushPhaseInvertBrowserSmokeTest",
        "phase-invert-target",
    ),
    "web/audio-worklet.js": (
        "this.phaseInvert = config.phaseInvert",
        "phase-invert-error",
    ),
    "web/index.html": ("HeadRush Prime Advanced Block Edit", "phase-invert-target"),
}

def git(*args: str, capture: bool = True) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=capture, text=True, check=False
    )
    if result.returncode != 0:
        detail = "\n".join(
            part.strip() for part in (result.stdout, result.stderr)
            if part and part.strip()
        )
        raise SystemExit(detail or f"git {' '.join(args)} failed with status {result.returncode}")
    return result.stdout if capture else ""


def select_patch_hunks(diff: str, markers: tuple[str, ...]) -> tuple[str, int, int]:
    lines = diff.splitlines(keepends=True)
    header: list[str] = []
    hunks: list[list[str]] = []
    current: list[str] | None = None
    for line in lines:
        if line.startswith("@@"):
            if current is not None:
                hunks.append(current)
            current = [line]
        elif current is None:
            header.append(line)
        else:
            current.append(line)
    if current is not None:
        hunks.append(current)

    # Fractal Surround Delay wires the picker across several separate file regions.
    selected = [
        hunk for hunk in hunks
        if any(
            line.startswith("+") and not line.startswith("+++") and marker in line
            for line in hunk for marker in markers
        )
    ]
    if not selected:
        raise SystemExit(
            "Expected at least one checkpoint-specific patch hunk, found none"
        )
    return "".join(header + [line for hunk in selected for line in hunk]), len(hunks), len(selected)


CHECKPOINTS["fractal-resonator-effect"] = (
    FRACTAL_RESONATOR_EFFECT_CHECKPOINT_PATHS,
    "feat: add project-authored Fractal Resonator",
)  # publish this feature as one bounded primary-branch checkpoint
PARTIAL_CHECKPOINT_HUNKS["phaser-stage-controls"] = {
    "EFFECTS_MATRIX.md": ("4/6/8/10/12 stages",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "Phaser stage choices and HeadRush Prime Stereo toggle (2026-11-27)",
        "4/8/10/12 are available at existing Modulation ID 18",
        "project-authored Stereo toggle is now in the existing Modulation Phaser",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "six Play captures include Stereo On/Off",
        "## Fender Tone Master Pro Phaser stage choices (2026-11-27)",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Phaser stage-count and stereo control measurements (2026-11-27)",
    ),
    "RESEARCH.md": (
        "## HeadRush Prime Orange Phaser Stereo control (2026-11-27)",
        "## Fender Tone Master Pro Phaser stages (2026-11-27)",
    ),
    "apps/headrush_phaser_stereo_bench.cpp": ("profile=phaser_stage_count_",),
    "include/tabletfx/effects/phaser.hpp": (
        "bool stereo = true;",
        "kMaximumStages = 12",
    ),
    "make/publish.mk": (
        "phaser-stage-controls",
        "bench-headrush-phaser-stereo",
    ),
    "research-state.json": (
        '"previous_checkpoint_commit": "c6463464671a0a5084fcafdcb766716df7fc76dd"',
        '"name": "Fender Phaser stage choices and HeadRush Prime Orange Phaser Stereo control"',
        '"next_feature": "Implement the project-scoped Dynamics Sidechain detector',
    ),
    "scripts/publish_checkpoint.py": (
        "PHASER_STAGE_COUNTS_STEREO_CHECKPOINT_PATHS",
        '"phaser-stage-controls": (',
        'PARTIAL_CHECKPOINT_HUNKS["phaser-stage-controls"]',
    ),
    "src/effects/phaser.cpp": (
        "parameters.stages != 10",
        "!parameters_.stereo",
    ),
    "tests/test_phaser.cpp": (
        "phaser rejected ten stages",
        "test_stereo_toggle_and_default_compatibility",
        "selectable 4/6/8/10/12-stage phasers",
    ),
    "web/app.js": (
        "phaserStages.options.length !== 5",
        "stageComparison",
    ),
    "web/index.html": (
        "graphic showing 4/8/10/12",
        'option value="10">10 stages',
        "phaser-stereo",
    ),
}


def stage_partial_path(
    path: str, markers: tuple[str, ...], *, allow_no_match: bool = False
) -> None:
    # Fractal Surround Delay checkpoint recovery reuses the exact marker selection.
    # Minimal-context checkpoint patches keep adjacent, unrelated edits out of the index.
    diff = git("diff", "--no-ext-diff", "--unified=1", "--", path)
    try:
        patch, _, _ = select_patch_hunks(diff, markers)
    except SystemExit as error:
        if allow_no_match and "Expected at least one checkpoint-specific patch hunk" in str(error):
            return
        raise SystemExit(f"Cannot stage {path}: {error}") from error
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", prefix="multifx-checkpoint-", suffix=".patch",
            delete=False,
        ) as temporary:
            temporary.write(patch)
            temporary_path = Path(temporary.name)
        # Minimal-context hunks are used to isolate adjacent user work. Git
        # requires --unidiff-zero to apply a patch with no context lines.
        apply_args = ["apply", "--cached"]
        if context == 0:
            apply_args.append("--unidiff-zero")
        git(*apply_args, str(temporary_path), capture=False)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def stage_checkpoint_paths(checkpoint: str, paths: list[str]) -> None:
    pre_staged = set(git("diff", "--cached", "--name-only").splitlines())
    if pre_staged:
        if not pre_staged.issubset(set(paths)):
            raise SystemExit(
                "Refusing to stage over unrelated index changes: " + ", ".join(sorted(pre_staged))
            )
        # Fractal Surround Delay checkpoint recovery may resume after a partial
        # attempt, but only when each staged partial file still matches markers.
        verify_partial_staging(checkpoint, pre_staged)
        partial = PARTIAL_CHECKPOINT_HUNKS.get(checkpoint, {})
        # Checkpoint-untracked-recovery: git diff omits untracked files, so stage
        # an explicitly listed new checkpoint file before inspecting its diff.
        status = git("status", "--porcelain=v1", "--untracked-files=all")
        untracked = {
            line[3:] for line in status.splitlines() if line.startswith("?? ")
        }
        for path in paths:
            if path in untracked:
                git("add", "--", path, capture=False)
                continue
            remaining_diff = git("diff", "--no-ext-diff", "--unified=3", "--", path)
            if not remaining_diff:
                continue
            markers = partial.get(path)
            if markers is None:
                git("add", "--", path, capture=False)
            else:
                # Resume a partially staged path by selecting only remaining
                # checkpoint-marked hunks rather than broad-staging its edits.
                stage_partial_path(
                    path, markers, context=partial_checkpoint_context(checkpoint, path),
                    allow_no_match=True,
                )
        return

    partial = PARTIAL_CHECKPOINT_HUNKS.get(checkpoint, {})
    status = git("status", "--porcelain=v1", "--untracked-files=all")
    untracked = {
        line[3:] for line in status.splitlines() if line.startswith("?? ")
    }
    for path in paths:
        if path in untracked:
            # A new file has no diff hunks until Git knows about it. The
            # checkpoint path allowlist is the explicit scope for staging it.
            git("add", "--", path, capture=False)
            continue
        markers = partial.get(path)
        if markers is None:
            git("add", "--", path, capture=False)
            continue

        stage_partial_path(
            path, markers, context=partial_checkpoint_context(checkpoint, path)
        )


def verify_partial_staging(
    checkpoint: str, staged_paths: set[str] | None = None
) -> None:
    for path, markers in PARTIAL_CHECKPOINT_HUNKS.get(checkpoint, {}).items():
        if staged_paths is not None and path not in staged_paths:
            # Fractal Surround Delay checkpoint recovery validates staged files only.
            continue
        # Validate staged minimal-context hunks with the same selector used during staging.
        context = partial_checkpoint_context(checkpoint, path)
        diff = git("diff", "--cached", "--no-ext-diff", f"--unified={context}", "--", path)
        try:
            _, hunk_count, selected_hunk_count = select_patch_hunks(diff, markers)
        except SystemExit as error:
            # Fractal Surround Delay checkpoint recovery identifies a bad staged file.
            raise SystemExit(f"Invalid staged hunks in {path}: {error}") from error
        if selected_hunk_count != hunk_count:
            raise SystemExit(
                f"Refusing to publish unmarked staged hunks in {path}: "
                f"selected {selected_hunk_count} of {hunk_count}"
            )


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
    workflows = [path for path in checkpoint_paths if path.startswith(".github/workflows/")]
    if workflows:
        raise SystemExit("Refusing to include workflow paths in a checkpoint: " + ", ".join(workflows))
    selected = sorted(set(paths) & checkpoint_paths)
    deferred = sorted(set(paths) - checkpoint_paths)
    if not selected:
        raise SystemExit("No changed files for checkpoint " + checkpoint)
    return selected, deferred, branch, remote_url


def plan(checkpoint: str) -> list[str]:
    paths, deferred, branch, remote_url = inspect(checkpoint)
    git("diff", "--check", "--", *paths)
    git("diff", "--cached", "--check", "--", *paths)
    print(f"Repository: {remote_url}")
    print(f"Branch: {branch} (verified upstream default; local HEAD matches origin/main)")
    base_commit = git("rev-parse", "HEAD").strip()
    base_tree = git("rev-parse", "HEAD^{tree}").strip()
    print(f"Base commit: {base_commit}")
    print(f"Base tree: {base_tree}")
    print("Base file modes:")
    for entry in git("ls-tree", "-r", "HEAD", "--", *paths).splitlines():
        print("  " + entry)
    print("Checkpoint: " + checkpoint)
    print("Commit message: " + CHECKPOINTS[checkpoint][1])
    print("Exact changed paths:")
    for path in paths:
        print("  " + path)
    stat = git("diff", "--stat", "--", *paths)
    print(stat.rstrip() or "Untracked files are listed above; no tracked diff stat yet.")
    status = git("status", "--porcelain=v1", "--untracked-files=all")
    untracked = {line[3:] for line in status.splitlines() if line.startswith("?? ")}
    staged_paths = set(git("diff", "--cached", "--name-only").splitlines())
    partial = PARTIAL_CHECKPOINT_HUNKS.get(checkpoint, {})
    # Show the exact checkpoint patch before publication, including partial-file selections.
    print("Exact patch selected for this checkpoint:")
    for path in paths:
        context = partial_checkpoint_context(checkpoint, path) if path in partial else 1
        staged_diff = git("diff", "--no-ext-diff", f"--unified={context}", "--cached", "--", path)
        unstaged_diff = git("diff", "--no-ext-diff", f"--unified={context}", "--", path)
        diff = staged_diff if path in staged_paths else unstaged_diff
        if path in partial and diff:
            try:
                diff, _, _ = select_patch_hunks(diff, partial[path])
            except SystemExit:
                diff = ""
        if diff:
            print(diff, end="" if diff.endswith("\n") else "\n")
        elif path in untracked:
            contents = (ROOT / path).read_text(encoding="utf-8").splitlines(keepends=True)
            print("".join(unified_diff([], contents, fromfile="/dev/null", tofile="b/" + path)), end="")
    print("No workflow files are included. Only the paths above will be staged.")
    if deferred:
        print("Preserved outside-checkpoint paths (not staged):")
        for path in deferred:
            print("  " + path)
    return paths


def publish(checkpoint: str) -> None:
    paths, _, _, _ = inspect(checkpoint)
    staged_paths = set(git("diff", "--cached", "--name-only").splitlines())
    if not staged_paths:
        stage_checkpoint_paths(checkpoint, paths)
        staged_paths = set(git("diff", "--cached", "--name-only").splitlines())
    if staged_paths != set(paths):
        raise SystemExit("Staged paths differ from the reviewed checkpoint; refusing to publish")
    verify_partial_staging(checkpoint)
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


def stage(checkpoint: str) -> None:
    paths, deferred, branch, remote_url = inspect(checkpoint)
    stage_checkpoint_paths(checkpoint, paths)
    staged_paths = set(git("diff", "--cached", "--name-only").splitlines())
    if staged_paths != set(paths):
        raise SystemExit(
            "Staged paths differ from the selected checkpoint; "
            "review the index and stop before commit"
        )
    verify_partial_staging(checkpoint)
    git("diff", "--cached", "--check")
    print(f"Repository: {remote_url}")
    print(f"Branch: {branch} (verified upstream default; local HEAD matches origin/main)")
    print(f"Base commit: {git('rev-parse', 'HEAD').strip()}")
    print("Checkpoint: " + checkpoint)
    print("Commit message: " + CHECKPOINTS[checkpoint][1])
    print("Exact staged paths:")
    for path in paths:
        print("  " + path)
    stat = git("diff", "--cached", "--stat", "--", *paths)
    print(stat.rstrip() or "No staged diff stat.")
    print("Full staged patch:")
    patch = git("diff", "--cached", "--", *paths)
    print(patch, end="" if patch.endswith("\n") else "\n")
    if deferred:
        print("Preserved outside-checkpoint paths (not staged):")
        for path in deferred:
            print("  " + path)
    print("Staged only; no commit or push was performed.")


def unstage(checkpoint: str) -> None:
    paths, _, branch, remote_url = inspect(checkpoint)
    staged_paths = set(git("diff", "--cached", "--name-only").splitlines())
    if not staged_paths:
        print("No staged paths to unstage.")
        return
    if not staged_paths.issubset(set(paths)):
        raise SystemExit(
            "Refusing to unstage paths outside the selected checkpoint: "
            + ", ".join(sorted(staged_paths - set(paths)))
        )
    git("restore", "--staged", "--", *sorted(staged_paths), capture=False)
    print(f"Repository: {remote_url}")
    print(f"Branch: {branch} (verified upstream default)")
    print("Removed only staged state for these checkpoint paths; worktree contents were preserved:")
    for path in sorted(staged_paths):
        print("  " + path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "stage", "unstage", "publish"))
    parser.add_argument("--checkpoint", choices=tuple(CHECKPOINTS),
                        default=DEFAULT_CHECKPOINT)
    args = parser.parse_args()
    if args.mode == "plan":
        plan(args.checkpoint)
    elif args.mode == "stage":
        stage(args.checkpoint)
    else:
        publish(args.checkpoint)


PARTIAL_CHECKPOINT_HUNKS["axom-source-audit"] = {
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "Flagship effect implementation checklist (source status 2026-11-27)",
        "V1.9.2 dated 2026-10-26",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "Melofty AXOM MAX — current effect-source audit (2026-11-27)",
    ),
    "RESEARCH.md": (
        "MELOFTY downloads page](https://www.meloftyaudio.com/support/downloads), rechecked 2026-11-27",
    ),
    "research-state.json": (
        '"previous_checkpoint_commit": "c10e62fd87908f584112cf52c35df9e7c164a396"',
        '"next_device": "Fender Tone Master Pro"',
        '"next_feature": "Extend the existing categorized Phaser',
    ),
    "scripts/publish_checkpoint.py": (
        '"make/publish.mk"',
        'PARTIAL_CHECKPOINT_HUNKS["axom-source-audit"]',
    ),
}

PARTIAL_CHECKPOINT_HUNKS["stadium-dynamics-sidechain"] = {
    "CMakeLists.txt": ("stadium_dynamics_sidechain_bench",),
    "apps/stadium_dynamics_sidechain_bench.cpp": ('run_profile({"Block input (default)"',),
    "EFFECTS_MATRIX.md": ("pre-effect dry chain for threshold detection",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "## Line 6 Helix Stadium Dynamics Sidechain",
        "project-limited Studio Compressor detector selector",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "Status marks in the first column",
        "DYN-15",
        "Project-authored Studio Compressor controls:",
    ),
    "LATENCY_AND_QUALITY.md": ("## Line 6 Helix Stadium Dynamics Sidechain",),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += stadium-dynamics-sidechain",
        "test-stadium-dynamics-sidechain",
    ),
    "RESEARCH.md": ("## Line 6 Helix Stadium Dynamics Sidechain",),
    "research-state.json": (
        "Line 6 Helix Stadium Dynamics Sidechain project mode",
        '"next_device": "Fractal FM9 Mark II Turbo"',
    ),
    "scripts/publish_checkpoint.py": (
        "STADIUM_DYNAMICS_SIDECHAIN_CHECKPOINT_PATHS",
        '"stadium-dynamics-sidechain": (',
        'PARTIAL_CHECKPOINT_HUNKS["stadium-dynamics-sidechain"]',
        "PARTIAL_CHECKPOINT_CONTEXT",
        "partial_checkpoint_context",
    ),
    "scripts/run_cmake.py": ("bench_stadium_dynamics_sidechain",),
    "scripts/validate_docs.py": ("row_pattern = re.compile",),
    "scripts/web.py": ("stadium_dynamics_sidechain_only", "test-stadium-dynamics-sidechain"),
    "include/tabletfx/effects/compressor.hpp": ("process_with_detector",),
    "src/web/wasm_chain.cpp": (
        "set_compressor_sidechain_source",
        "compressor_pre_effect_detector_",
    ),
    "tests/test_compressor.cpp": (
        "process_with_detector",
        "test_aliased_block_input_is_identical_and_allocation_free",
    ),
    "web/app.js": ("compressorSidechainSource", "updateCompressorSidechainSource"),
    "web/audio-worklet.js": ("compressor-sidechain-source",),
    "web/index.html": (
        "compressor-sidechain-source",
        "Separate Instrument, Mic, Return, USB, and S/PDIF sidechain buses are not available",
    ),
}

# Some adjacent independent edits share a normal-context diff hunk. Keep the
# Sidechain browser parameter isolated from the pending Phaser stereo defaults.
PARTIAL_CHECKPOINT_CONTEXT = {
    "stadium-dynamics-sidechain": {"web/app.js": 0},
}


def partial_checkpoint_context(checkpoint: str, path: str) -> int:
    return PARTIAL_CHECKPOINT_CONTEXT.get(checkpoint, {}).get(path, 1)


FRACTAL_GLOBAL_MIX_CHORUS_CHECKPOINT_PATHS = (
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_chorus_tempo_bench.cpp",
    "include/tabletfx/effects/chorus.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/web.py",
    "src/effects/chorus.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_chorus.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
CHECKPOINTS["fractal-global-mix-chorus"] = (
    FRACTAL_GLOBAL_MIX_CHORUS_CHECKPOINT_PATHS,
    "feat: add project Global Mix offset to Chorus",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-chorus"] = {
    "EFFECTS_MATRIX.md": ("## Fractal FM9-family Global Mix — Chorus subset",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "## Fractal FM9-family Global Mix — initial Chorus coverage",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Global Mix — Chorus subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Global Mix — Chorus subset",
    ),
    "RESEARCH.md": ("## Fractal FM9-family Global Mix source comparison",),
    "research-state.json": (
        "Fractal FM9-family Global Mix — Chorus subset",
        "Complete and publish the Chorus Global Mix subset",
    ),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += fractal-global-mix-chorus",
        "test-fractal-global-mix-chorus",
        "publish-plan-fractal-global-mix-chorus",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_GLOBAL_MIX_CHORUS_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-global-mix-chorus"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-chorus"]',
        "Minimal-context hunks are used to isolate adjacent user work",
        "part and part.strip()",
    ),
    "scripts/web.py": (
        "fractal_global_mix_chorus_only",
        "test-fractal-global-mix-chorus",
    ),
    "src/web/wasm_chain.cpp": (
        "global_mix_enabled",
        "global_mix_offset = global_mix_offset",
    ),
    "web/app.js": ("chorusGlobalMix", "runFractalGlobalMixChorusBrowserSmokeTest"),
    "web/audio-worklet.js": ("chorusGlobalMix", "globalMixEnabled"),
    "web/index.html": ("chorus-global-mix-enabled", "project Global Effects Mix offset"),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-global-mix-chorus"] = {
    "make/publish.mk": 0,
    "scripts/publish_checkpoint.py": 0,
    "scripts/web.py": 0,
    "src/web/wasm_chain.cpp": 0,
    "web/app.js": 0,
    "web/audio-worklet.js": 0,
    "web/index.html": 0,
    "scripts/publish_checkpoint.py": 1,
}

FRACTAL_GLOBAL_MIX_FLANGER_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_global_mix_flanger_bench.cpp",
    "include/tabletfx/effects/flanger.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/flanger.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_flanger.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
CHECKPOINTS["fractal-global-mix-flanger"] = (
    FRACTAL_GLOBAL_MIX_FLANGER_CHECKPOINT_PATHS,
    "feat: add project Global Mix offset to Flanger",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-flanger"] = {
    "CMakeLists.txt": ("tabletfx_fractal_global_mix_flanger_bench",),
    "EFFECTS_MATRIX.md": ("## Fractal FM9-family Global Mix — Flanger subset",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "## Fractal FM9-family Global Mix — Flanger subset",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Global Mix — Flanger subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Global Mix — Flanger subset",
    ),
    "RESEARCH.md": (
        "TabletFX implements project-authored subsets on existing Chorus",
    ),
    "research-state.json": ("Fractal FM9-family Global Mix — Flanger subset",),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += fractal-global-mix-flanger",
        "test-fractal-global-mix-flanger",
        "publish-plan-fractal-global-mix-flanger",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_GLOBAL_MIX_FLANGER_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-global-mix-flanger"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-flanger"]',
    ),
    "scripts/run_cmake.py": ("bench-fractal-global-mix-flanger",),
    "scripts/web.py": (
        "fractal_global_mix_flanger_only",
        "test-fractal-global-mix-flanger",
    ),
    "include/tabletfx/effects/flanger.hpp": (
        "Project interpretation of Fractal's Global Mix feature",
    ),
    "src/effects/flanger.cpp": (
        "Fractal documents a global ±50% offset but not its equation.",
    ),
    "src/web/wasm_chain.cpp": (
        "parameters.global_mix_enabled = global_mix_enabled != 0U;",
    ),
    "tests/test_flanger.cpp": (
        "test_global_mix_offset_is_opt_in_clamped_and_mode_scoped",
    ),
    "web/app.js": (
        "flangerGlobalMixEnabled",
        "runFractalGlobalMixFlangerBrowserSmokeTest",
    ),
    "web/audio-worklet.js": ("flangerGlobalMix",),
    "web/index.html": (
        "flanger-global-mix-enabled",
        "The Global Mix control applies only in Flanger mode.",
    ),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-global-mix-flanger"] = {
    "CMakeLists.txt": 0,
    "EFFECTS_MATRIX.md": 0,
    "FLAGSHIP_EFFECT_COMPARISON.md": 0,
    "IMPLEMENTATION_CHECKLIST.md": 0,
    "LATENCY_AND_QUALITY.md": 0,
    "RESEARCH.md": 0,
    "research-state.json": 0,
    "make/publish.mk": 0,
    "scripts/publish_checkpoint.py": 1,
    "scripts/run_cmake.py": 0,
    "scripts/web.py": 0,
    "src/web/wasm_chain.cpp": 0,
    "web/app.js": 0,
    "web/audio-worklet.js": 0,
    "web/index.html": 0,
}

FRACTAL_GLOBAL_MIX_DELAY_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_global_mix_delay_bench.cpp",
    "include/tabletfx/effects.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
CHECKPOINTS["fractal-global-mix-delay"] = (
    FRACTAL_GLOBAL_MIX_DELAY_CHECKPOINT_PATHS,
    "feat: add project Global Mix offset to Delay",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-delay"] = {
    "CMakeLists.txt": ("tabletfx_fractal_global_mix_delay_bench",),
    "EFFECTS_MATRIX.md": ("## Fractal FM9-family Global Mix — Delay subset",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "## Fractal FM9-family Global Mix — Delay subset",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Global Mix — Delay subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Global Mix — Delay subset",
    ),
    "RESEARCH.md": (
        "TabletFX implements project-authored subsets on existing Chorus",
    ),
    "research-state.json": ("Fractal FM9-family Global Mix — Delay subset",),
    "apps/fractal_global_mix_delay_bench.cpp": ("delay_global_mix_enabled_plus_20pp",),
    "include/tabletfx/effects.hpp": (
        "configure_global_mix_offset(bool enabled, float offset)",
    ),
    "make/publish.mk": (
        "fractal-global-mix-delay",
        "test-fractal-global-mix-delay",
        "publish-plan-fractal-global-mix-delay",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_GLOBAL_MIX_DELAY_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-global-mix-delay"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-global-mix-delay"]',
        "A new file has no diff hunks until Git knows about it.",
    ),
    "scripts/run_cmake.py": (
        "bench-fractal-global-mix-delay",
        "def bench_fractal_global_mix_delay(cmake: str)",
    ),
    "scripts/web.py": (
        "fractal_global_mix_delay_only",
        "test-fractal-global-mix-delay",
    ),
    "src/effects.cpp": ("void Delay::configure_global_mix_offset",),
    "src/web/wasm_chain.cpp": (
        "configure_delay_global_mix",
        "tf_chain_set_delay_global_mix",
    ),
    "tests/test_delay.cpp": (
        "test_global_mix_offset_is_opt_in_and_keeps_base_mix",
        "test_global_mix_offset_is_additive_clamped_and_smoothed",
    ),
    "web/app.js": (
        'const delayGlobalMixEnabled = document.querySelector',
        'DEFAULT_PARAMETERS.delayGlobalMix = [globalMixEnabled ? 1 : 0, globalMixOffset]',
        'const globalMixEnabled = delayGlobalMixEnabled.checked;',
        'const globalMixOffset = Number(delayGlobalMixOffset.value) / 100;',
        'delayGlobalMixEnabled, delayGlobalMixOffset]',
        "async function runFractalGlobalMixDelayBrowserSmokeTestIsolated",
        "runFractalGlobalMixDelayBrowserSmokeTestIsolated(",
    ),
    "web/audio-worklet.js": ("delayGlobalMix",),
    "web/index.html": ("delay-global-mix-enabled",),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-global-mix-delay"] = {
    "CMakeLists.txt": 0,
    "EFFECTS_MATRIX.md": 0,
    "FLAGSHIP_EFFECT_COMPARISON.md": 0,
    "IMPLEMENTATION_CHECKLIST.md": 0,
    "LATENCY_AND_QUALITY.md": 0,
    "RESEARCH.md": 0,
    "research-state.json": 0,
    "include/tabletfx/effects.hpp": 0,
    "make/publish.mk": 0,
    "scripts/publish_checkpoint.py": 1,
    "scripts/run_cmake.py": 0,
    "scripts/web.py": 0,
    "src/effects.cpp": 0,
    "src/web/wasm_chain.cpp": 0,
    "tests/test_delay.cpp": 0,
    "web/app.js": 0,
    "web/audio-worklet.js": 0,
    "web/index.html": 0,
}

FRACTAL_MEGATAP_GLOBAL_MIX_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_megatap_global_mix_bench.cpp",
    "include/tabletfx/effects/megatap_delay.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/megatap_delay.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_megatap_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/index.html",
)
CHECKPOINTS["fractal-megatap-global-mix"] = (
    FRACTAL_MEGATAP_GLOBAL_MIX_CHECKPOINT_PATHS,
    "feat: add project Global Mix option to Megatap Delay",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-megatap-global-mix"] = {
    "CMakeLists.txt": ("tabletfx_fractal_megatap_global_mix_bench",),
    "EFFECTS_MATRIX.md": ("| `megatap-delay` |",),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "| **Fractal FM9 Mark II Turbo** |",
        "Fractal FM9-family Megatap Delay Global Mix",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Megatap Delay Global Mix subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Megatap Delay Global Mix subset",
    ),
    "RESEARCH.md": (
        "## Fractal FM9-family Megatap Delay Global Mix subset",
    ),
    "apps/fractal_megatap_global_mix_bench.cpp": (
        "global-mix-enabled-plus-20pp",
    ),
    "include/tabletfx/effects/megatap_delay.hpp": ("global_mix_enabled",),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += fractal-megatap-global-mix",
        "test-fractal-megatap-global-mix:",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_MEGATAP_GLOBAL_MIX_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-megatap-global-mix"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-megatap-global-mix"]',
    ),
    "scripts/run_cmake.py": (
        "bench-fractal-megatap-global-mix",
        "def bench_fractal_megatap_global_mix(cmake: str)",
    ),
    "scripts/web.py": (
        "fractal_megatap_global_mix_only",
        '"test-fractal-megatap-global-mix"',
    ),
    "src/effects/megatap_delay.cpp": ("parameters_.global_mix_enabled",),
    "src/web/wasm_chain.cpp": ("megatap_global_mix_enabled",),
    "tests/test_megatap_delay.cpp": (
        "test_project_global_mix_offset_is_opt_in_bounded_and_allocation_free",
    ),
    "web/app.js": (
        "megatapDelayGlobalMixEnabled",
        "runFractalMegatapGlobalMixBrowserSmokeTest",
        'selftest=fractal-megatap-global-mix',
    ),
    "web/audio-worklet.js": (
        "0, 10, 0.5, 0, -70, 400, 15, 0, 0.25, 0, 0, 0, 0, 0",
    ),
    "web/index.html": (
        "megatap-delay-global-mix-enabled",
        "megatap-delay-global-mix-offset",
    ),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-megatap-global-mix"] = {
    "CMakeLists.txt": 0,
    "EFFECTS_MATRIX.md": 0,
    "FLAGSHIP_EFFECT_COMPARISON.md": 0,
    "IMPLEMENTATION_CHECKLIST.md": 0,
    "LATENCY_AND_QUALITY.md": 0,
    "RESEARCH.md": 0,
    "make/publish.mk": 0,
    "scripts/publish_checkpoint.py": 1,
    "scripts/run_cmake.py": 1,
    "scripts/web.py": 0,
    "src/effects/megatap_delay.cpp": 0,
    "src/web/wasm_chain.cpp": 0,
    "tests/test_megatap_delay.cpp": 0,
    "web/app.js": 0,
    "web/audio-worklet.js": 0,
    "web/index.html": 0,
}

FRACTAL_MULTITAP_GLOBAL_MIX_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_multitap_global_mix_bench.cpp",
    "include/tabletfx/effects/multi_delay.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/multi_delay.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_multi_delay.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/fractal_multitap_global_mix_test.js",
    "web/index.html",
)
CHECKPOINTS["fractal-multitap-global-mix"] = (
    FRACTAL_MULTITAP_GLOBAL_MIX_CHECKPOINT_PATHS,
    "feat: add project Global Mix option to Multitap Delay",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-multitap-global-mix"] = {
    "CMakeLists.txt": ("tabletfx_fractal_multitap_global_mix_bench",),
    "EFFECTS_MATRIX.md": (
        "## Fractal FM9-family Global Mix — Multitap Delay subset",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "Fractal FM9-family Multitap Delay Global Mix",
        "## Fractal FM9-family Multitap Delay Global Mix subset",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Multitap Delay Global Mix subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Multitap Delay Global Mix subset",
    ),
    "RESEARCH.md": (
        "## Fractal FM9-family Multitap Delay Global Mix subset",
    ),
    "research-state.json": (
        "Fractal FM9-family Global Mix — Multitap Delay subset",
        "ebae974bb4952ae27e5a89ab9fbd380564f4e089",
        "Fractal FM9-family Global Mix — Megatap Delay subset",
    ),
    "apps/fractal_multitap_global_mix_bench.cpp": (
        "global-mix-enabled-plus-20pp",
    ),
    "include/tabletfx/effects/multi_delay.hpp": ("bool global_mix_enabled",),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += fractal-multitap-global-mix",
        "test-fractal-multitap-global-mix:",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_MULTITAP_GLOBAL_MIX_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-multitap-global-mix"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-multitap-global-mix"]',
    ),
    "scripts/run_cmake.py": (
        "bench-fractal-multitap-global-mix",
        "def bench_fractal_multitap_global_mix(cmake: str)",
    ),
    "scripts/web.py": (
        "fractal_multitap_global_mix_only",
        '"test-fractal-multitap-global-mix"',
    ),
    "src/effects/multi_delay.cpp": ("effective_mix_ = parameters_.global_mix_enabled",),
    "src/web/wasm_chain.cpp": (
        "configure_multi_delay_global",
        "global_mix_offset = global_mix_offset",
    ),
    "tests/test_multi_delay.cpp": (
        "test_global_mix_defaults_disabled_identity_and_base_mix_storage",
        "test_global_mix_offsets_adjust_final_wet_dry_mix",
    ),
    "web/app.js": (
        "const multiDelayGlobalMixOffsetValue =",
        'selftest") === "fractal-multitap-global-mix"',
    ),
    "web/audio-worklet.js": (
        "Number(parameters.globalMixEnabled ?? 0)",
        "Number(parameters.globalMixOffset ?? 0)",
    ),
    "web/fractal_multitap_global_mix_test.js": (
        "export async function runFractalMultitapGlobalMixBrowserSmokeTest",
    ),
    "web/index.html": (
        "multi-delay-global-mix-enabled",
        "multi-delay-global-mix-offset",
    ),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-multitap-global-mix"] = {
    "CMakeLists.txt": 0,
    "EFFECTS_MATRIX.md": 0,
    "FLAGSHIP_EFFECT_COMPARISON.md": 0,
    "IMPLEMENTATION_CHECKLIST.md": 0,
    "LATENCY_AND_QUALITY.md": 0,
    "RESEARCH.md": 0,
    "research-state.json": 0,
    "make/publish.mk": 0,
    "scripts/publish_checkpoint.py": 1,
    "scripts/run_cmake.py": 1,
    "scripts/web.py": 0,
    "src/effects/multi_delay.cpp": 0,
    "src/web/wasm_chain.cpp": 1,
    "tests/test_multi_delay.cpp": 0,
    "web/app.js": 0,
    "web/audio-worklet.js": 1,
    "web/index.html": 1,
}

FRACTAL_PHASER_GLOBAL_MIX_CHECKPOINT_PATHS = (
    "CMakeLists.txt",
    "EFFECTS_MATRIX.md",
    "FLAGSHIP_EFFECT_COMPARISON.md",
    "IMPLEMENTATION_CHECKLIST.md",
    "LATENCY_AND_QUALITY.md",
    "RESEARCH.md",
    "research-state.json",
    "apps/fractal_phaser_global_mix_bench.cpp",
    "include/tabletfx/effects/phaser.hpp",
    "make/publish.mk",
    "scripts/publish_checkpoint.py",
    "scripts/run_cmake.py",
    "scripts/web.py",
    "src/effects/phaser.cpp",
    "src/web/wasm_chain.cpp",
    "tests/test_phaser.cpp",
    "web/app.js",
    "web/audio-worklet.js",
    "web/fractal_phaser_global_mix_test.js",
    "web/index.html",
)
CHECKPOINTS["fractal-phaser-global-mix"] = (
    FRACTAL_PHASER_GLOBAL_MIX_CHECKPOINT_PATHS,
    "feat: add project Global Mix offset to Phaser",
)
PARTIAL_CHECKPOINT_HUNKS["fractal-phaser-global-mix"] = {
    "CMakeLists.txt": ("tabletfx_fractal_phaser_global_mix_bench",),
    "EFFECTS_MATRIX.md": (
        "## Fractal FM9-family Global Mix — Phaser subset",
    ),
    "FLAGSHIP_EFFECT_COMPARISON.md": (
        "Fractal FM9-family Phaser Global Mix subset",
    ),
    "IMPLEMENTATION_CHECKLIST.md": (
        "## Fractal FM9-family Phaser Global Mix subset",
    ),
    "LATENCY_AND_QUALITY.md": (
        "## Fractal FM9-family Phaser Global Mix subset",
    ),
    "RESEARCH.md": (
        "## Fractal FM9-family Phaser Global Mix subset",
    ),
    "research-state.json": (
        "Fractal FM9-family Global Mix — Phaser subset",
        "3214f3da1c0ac85b498be7ba6175397c9eba5bf8",
    ),
    "apps/fractal_phaser_global_mix_bench.cpp": (
        "Project Phaser Global Mix benchmark",
    ),
    "include/tabletfx/effects/phaser.hpp": ("bool global_mix_enabled = false",),
    "make/publish.mk": (
        "PUBLISH_CHECKPOINTS += fractal-phaser-global-mix",
        "test-fractal-phaser-global-mix:",
        "bench-fractal-phaser-global-mix:",
    ),
    "scripts/publish_checkpoint.py": (
        "FRACTAL_PHASER_GLOBAL_MIX_CHECKPOINT_PATHS",
        'CHECKPOINTS["fractal-phaser-global-mix"]',
        'PARTIAL_CHECKPOINT_HUNKS["fractal-phaser-global-mix"]',
    ),
    "scripts/run_cmake.py": (
        "bench-fractal-phaser-global-mix",
        "def bench_fractal_phaser_global_mix(cmake: str)",
    ),
    "scripts/web.py": ("browser_test(fractal_phaser_global_mix_only=True)",),
    "src/effects/phaser.cpp": ("parameters_.global_mix_enabled",),
    "src/web/wasm_chain.cpp": (
        "configure_phaser",
        "global_mix_offset = global_mix_offset",
    ),
    "tests/test_phaser.cpp": (
        "test_project_global_mix_is_opt_in_and_preserves_base_mix",
        "test_project_global_mix_is_opt_in_and_preserves_base_mix();",
    ),
    "web/app.js": (
        "const phaserGlobalMixEnabled =",
        "Number(phaserGlobalMixOffset.value),",
        "phaserGlobalMixOffsetValue.value =",
        "phaserStereo, phaserGlobalMixEnabled, phaserGlobalMixOffset]",
        "DEFAULT_PARAMETERS.phaser.length !== 12",
        "phaserGlobalMixEnabled.checked || phaserGlobalMixOffset.value",
        'get("selftest") === "fractal-phaser-global-mix"',
        "runFractalPhaserGlobalMixBrowserSmokeTest",
    ),
    "web/audio-worklet.js": (
        "parameters.phaser || [0.5, 250, 1600, 0.2, 4, 0.5, 0, 120, 4, 1, 0, 0]",
    ),
    "web/fractal_phaser_global_mix_test.js": (
        "export async function runFractalPhaserGlobalMixBrowserSmokeTest",
    ),
    "web/index.html": (
        "phaser-global-mix-enabled",
        "phaser-global-mix-offset",
    ),
}
PARTIAL_CHECKPOINT_CONTEXT["fractal-phaser-global-mix"] = {
    "CMakeLists.txt": 0,
    "EFFECTS_MATRIX.md": 0,
    "FLAGSHIP_EFFECT_COMPARISON.md": 0,
    "IMPLEMENTATION_CHECKLIST.md": 0,
    "LATENCY_AND_QUALITY.md": 0,
    "RESEARCH.md": 0,
    "research-state.json": 0,
    "make/publish.mk": 1,
    "scripts/publish_checkpoint.py": 1,
    "scripts/run_cmake.py": 1,
    "scripts/web.py": 1,
    "src/effects/phaser.cpp": 0,
    "src/web/wasm_chain.cpp": 1,
    "tests/test_phaser.cpp": 1,
    "web/app.js": 1,
    "web/audio-worklet.js": 1,
    "web/index.html": 1,
}

if __name__ == "__main__":
    main()
