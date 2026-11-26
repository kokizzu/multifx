#!/usr/bin/env python3
"""Bootstrap and verify the isolated Emscripten SDK used by the browser build."""

from __future__ import annotations

import argparse
import base64
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from urllib.parse import urlsplit
import zipfile
import wave


SDK_VERSION = "7.0.0"
SDK_ROOT = Path(os.environ.get("TABLETFX_EMSDK_ROOT", "/tmp/multifx-emsdk"))
ROOT = Path(__file__).resolve().parents[1]
WEB_BUILD_DIR = ROOT / "build" / "web"
SDK_ARCHIVE_URL = "https://github.com/emscripten-core/emsdk/archive/refs/heads/main.zip"


def emscripten_cpp() -> Path | None:
    path = shutil.which("em++")
    if path:
        return Path(path)
    candidate = SDK_ROOT / "upstream" / "emscripten" / "em++"
    return candidate if candidate.is_file() else None


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def verify_toolchain() -> None:
    compiler = emscripten_cpp()
    if compiler is None:
        raise SystemExit("Emscripten was not found; run `make web-toolchain-bootstrap` first")
    result = subprocess.run([str(compiler), "--version"], capture_output=True,
                            text=True, check=True)
    version = (result.stdout + result.stderr).strip().splitlines()[0]
    print(f"Emscripten C++: {compiler} ({version})")
    if SDK_VERSION not in version:
        raise SystemExit(f"Expected Emscripten {SDK_VERSION}, found: {version}")


def download_sdk_bootstrap(destination: Path) -> Path:
    if destination.exists():
        candidate = destination / "emsdk.py"
        if candidate.is_file():
            return candidate
        raise SystemExit(f"Refusing to replace existing incomplete SDK path: {destination}")

    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix="multifx-emsdk-bootstrap-", dir=destination.parent))
    archive_path = staging / "emsdk.zip"
    unpack_path = staging / "unpacked"
    unpack_path.mkdir()
    request = urllib.request.Request(
        SDK_ARCHIVE_URL,
        headers={"User-Agent": "multifx-web-toolchain-bootstrap/1.0"},
    )
    print(f"Downloading official Emscripten SDK bootstrap: {SDK_ARCHIVE_URL}", flush=True)
    with urllib.request.urlopen(request, timeout=60) as response, archive_path.open("wb") as output:
        shutil.copyfileobj(response, output, length=1024 * 1024)

    with zipfile.ZipFile(archive_path) as archive:
        unpack_root = unpack_path.resolve()
        for member in archive.infolist():
            target = (unpack_path / member.filename).resolve()
            if not target.is_relative_to(unpack_root):
                raise SystemExit(f"Unsafe archive member: {member.filename}")
        archive.extractall(unpack_path)

    entries = list(unpack_path.iterdir())
    if len(entries) != 1 or not entries[0].is_dir() or not (entries[0] / "emsdk.py").is_file():
        raise SystemExit("Unexpected layout in the official Emscripten SDK bootstrap archive")
    entries[0].rename(destination)
    shutil.rmtree(staging)
    return destination / "emsdk.py"


def bootstrap_toolchain() -> None:
    installed = emscripten_cpp()
    if installed:
        verify_toolchain()
        return
    sdk_script = download_sdk_bootstrap(SDK_ROOT)
    run([sys.executable, str(sdk_script), "install", SDK_VERSION], cwd=SDK_ROOT)
    run([sys.executable, str(sdk_script), "activate", SDK_VERSION], cwd=SDK_ROOT)
    verify_toolchain()


def build_web() -> None:
    compiler = emscripten_cpp()
    if compiler is None:
        raise SystemExit("Emscripten was not found; run `make web-toolchain-bootstrap` first")
    WEB_BUILD_DIR.mkdir(parents=True, exist_ok=True)
    exports = [
        "_malloc", "_free",
        "_tf_convert_channels", "_tf_channel_conversion_self_test",
        "_tf_parallel_mixer_self_test",
        "_tf_chain_create", "_tf_chain_destroy", "_tf_chain_effect_count",
        "_tf_chain_input_buffer", "_tf_chain_work_buffer",
        "_tf_chain_output_buffer", "_tf_chain_memory_bytes",
        "_tf_chain_effect_memory_bytes", "_tf_chain_set_enabled", "_tf_chain_set_mix",
        "_tf_chain_begin", "_tf_chain_process_effect", "_tf_chain_finish",
        "_tf_chain_reset", "_tf_chain_reset_modulation_phase",
        "_tf_chain_set_compressor",
        "_tf_chain_set_optical_compressor", "_tf_chain_set_fet_compressor",
        "_tf_chain_set_guitar_graphic_eq", "_tf_chain_set_bass_graphic_eq",
        "_tf_chain_set_shelf_tilt_eq", "_tf_chain_set_room_reverb",
        "_tf_chain_set_hall_reverb",
        "_tf_chain_set_plate_reverb",
        "_tf_chain_set_spring_reverb",
         "_tf_chain_set_auto_swell",
        "_tf_chain_set_rotary_speaker",
         "_tf_chain_set_ring_modulator",
        "_tf_chain_set_rhythmic_slicer",
        "_tf_chain_set_step_filter",
         "_tf_chain_set_asheville_pattrn",
        "_tf_chain_set_drive",
        "_tf_chain_set_clean_boost",
        "_tf_chain_set_overdrive",
        "_tf_chain_set_mid_overdrive",
        "_tf_chain_set_high_gain_distortion",
        "_tf_chain_set_round_fuzz",
        "_tf_chain_set_eq", "_tf_chain_set_five_band_eq",
        "_tf_chain_set_delay", "_tf_chain_delay_time_ms",
        "_tf_chain_delay_storage_peak",
        "_tf_chain_delay_tap_sample",
        "_tf_chain_delay_write_frame", "_tf_chain_effect_enabled",
        "_tf_chain_set_delay_trails_on_bypass",
        "_tf_chain_set_delay_tempo",
        "_tf_chain_set_delay_bbd_style", "_tf_chain_set_delay_bbd_mode",
        "_tf_chain_set_delay_tape_echo",
        "_tf_chain_set_delay_ping_pong",
        "_tf_chain_set_delay_dual",
        "_tf_chain_set_delay_multitap", "_tf_chain_set_delay_multitap_tap",
        "_tf_chain_set_delay_tape_heads",
        "_tf_chain_set_pitch", "_tf_chain_set_pitch_mix", "_tf_chain_set_pitch_expression",
        "_tf_chain_set_filters",
        "_tf_chain_set_pedal_compressor", "_tf_chain_set_input_gain",
        "_tf_chain_set_output_gain", "_tf_chain_ramp_input_gain",
        "_tf_chain_ramp_output_gain", "_tf_chain_process_output_gain",
        "_tf_chain_process_output_limiter", "_tf_chain_set_volume_pedal",
        "_tf_chain_set_noise_gate",
        "_tf_chain_set_limiter", "_tf_chain_set_notch_filter",
        "_tf_chain_set_formant_filter",
        "_tf_chain_set_harmonic_resonator",
        "_tf_chain_set_multiband_crossover",
        "_tf_chain_set_multiband_band",
        "_tf_chain_set_bit_crusher",
        "_tf_chain_set_dimension_chorus",
        "_tf_chain_set_tone_master_pro_3d_chorus",
        "_tf_chain_set_fractal_surround_delay",
        "_tf_chain_set_triple_double_chorus",
        "_tf_chain_set_four_band_resonator",
        "_tf_chain_set_diatonic_harmonizer",
        "_tf_chain_set_diatonic_custom_scale",
        "_tf_chain_set_pitch_arpeggiator",
        "_tf_chain_set_mono_guitar_synth",
        "_tf_chain_set_note_generator",
        "_tf_chain_set_four_osc_generator",
        "_tf_chain_set_poly_detune",
        "_tf_chain_set_pedal_detune",
        "_tf_chain_set_mutant_filter",
        "_tf_chain_set_fractal_chromatic_harmonizer",
        "_tf_chain_set_fractal_diatonic",
        "_tf_chain_set_fractal_diatonic_custom_scale",
        "_tf_chain_set_fractal_diatonic_voice",
        "_tf_chain_set_fractal_three_voice_synth",
        "_tf_chain_set_enhancer",
        "_tf_chain_deluxe_looper_command",
        "_tf_chain_set_deluxe_looper_options",
        "_tf_chain_deluxe_looper_state",
        "_tf_chain_deluxe_looper_frames",
        "_tf_chain_shuffling_looper_command",
        "_tf_chain_set_shuffling_looper_options",
        "_tf_chain_shuffling_looper_state",
        "_tf_chain_shuffling_looper_frames",
        "_tf_chain_shuffling_looper_steps",
        "_tf_chain_set_ten_tap_delay_options",
        "_tf_chain_set_ten_tap_delay_level",
        "_tf_chain_set_rhythm_tap_delay",
        "_tf_chain_set_plex_delay_global",
        "_tf_chain_set_plex_delay_advanced",
        "_tf_chain_set_plex_delay_line",
        "_tf_chain_set_auto_pan",
        "_tf_chain_set_megatap_delay",
        "_tf_chain_set_de_esser",
        "_tf_chain_set_dual_engine_distortion",
        "_tf_chain_set_vibe",
        "_tf_chain_set_uni_verse_vibe",
        "_tf_chain_set_multi_delay_line",
        "_tf_chain_set_multi_delay_global",
        "_tf_chain_set_multi_delay_topology",
        "_tf_chain_set_multi_delay_time_lfo",
        "_tf_chain_set_porta_preamp",
        "_tf_chain_set_pitch_sequence",
        "_tf_chain_set_poly_sustain",
        "_tf_chain_set_step_filter_delay",
        "_tf_chain_set_volume_pan",
        "_tf_chain_set_stereo_filter",
        "_tf_chain_set_step_tremolo",
        "_tf_chain_set_resonant_delay",
        "_tf_chain_set_grain_delay",
        "_tf_chain_set_pitch_feedback_delay",
        "_tf_chain_set_chromatic_pitch_delay",
        "_tf_chain_set_feedback_generator",
        "_tf_chain_set_shimmer_reverb",
        "_tf_chain_set_celestial_reverb",
        "_tf_chain_set_cloud_reverb",
        "_tf_chain_set_mercator_dist",
        "_tf_chain_set_manual_thru_zero_flanger",
    "_tf_chain_set_octobot",
    "_tf_chain_set_am_ring_mod",
    "_tf_chain_set_tape_echo",
    "_tf_chain_set_octave_fuzz",
        "_tf_chain_set_rock_box_dist",
        "_tf_chain_set_rock_box_boost",
        "_tf_chain_set_stereo_crossover",
        "_tf_chain_set_convolution_reverb",
        "_tf_chain_load_convolution_reverb_ir",
        "_tf_chain_load_cabinet_ir",
        "_tf_chain_cabinet_ir_latency_frames",
        "_tf_chain_set_octave_divider",
        "_tf_chain_set_double_take",
        "_tf_chain_set_eq5_graphic",
        "_tf_chain_set_xr1_global_eq",
        "_tf_chain_set_stadium_seven_band_peq",
        "_tf_chain_set_optical_tremolo",
        "_tf_chain_set_fractal_bias_trem",
        "_tf_chain_set_vibratone",
        "_tf_chain_set_reverse_delay", "_tf_chain_set_reverse_delay_run",
        "_tf_chain_restart_reverse_delay", "_tf_chain_set_ducking_delay",
        "_tf_chain_set_modulated_delay", "_tf_chain_set_fender_memory_delay",
        "_tf_chain_set_tone_master_small_hall",
        "_tf_chain_set_filtered_delay",
        "_tf_chain_set_sweep_delay",
        "_tf_chain_set_wah", "_tf_chain_set_tremolo",
        "_tf_chain_set_harmonic_tremolo",
        "_tf_chain_set_auto_swell_delay",
        "_tf_chain_set_chorus",
        "_tf_chain_set_chorus_min_range",
        "_tf_chain_set_flanger",
        "_tf_chain_set_phaser",
        "_tf_chain_set_vibrato",
        "_tf_chain_set_envelope_wah",
        "_tf_chain_set_global_bypass", "_tf_chain_set_input_mute",
        "_tf_chain_set_panic_mute",
        "_tf_chain_meter_peak", "_tf_chain_meter_rms", "_tf_chain_meter_flags",
        "_tf_chain_meter_invalid_samples", "_tf_chain_clear_meter_clip",
        "_tf_chain_set_tuner_reference", "_tf_chain_set_tuner_muted",
        "_tf_chain_tuner_generation", "_tf_chain_tuner_valid",
        "_tf_chain_tuner_note", "_tf_chain_tuner_frequency",
        "_tf_chain_tuner_cents", "_tf_chain_tuner_confidence",
        "_tf_chain_process_tuner", "_tf_chain_tuner_memory_bytes",
    ]
    command = [
        str(compiler), "-std=c++20", "-O3", "-flto", "-fno-rtti", "-fexceptions",
        "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow",
        "-I", str(ROOT / "include"),
        "-isystem", str(ROOT / "third_party" / "stft-pitch-shift"),
        str(ROOT / "src" / "effects.cpp"),
        str(ROOT / "src" / "channel_conversion.cpp"),
        str(ROOT / "src" / "parallel_mixer.cpp"),
        str(ROOT / "src" / "tuner.cpp"),
        str(ROOT / "src" / "effects" / "clean_boost.cpp"),
        str(ROOT / "src" / "effects" / "mid_overdrive.cpp"),
        str(ROOT / "src" / "effects" / "high_gain_distortion.cpp"),
        str(ROOT / "src" / "effects" / "round_fuzz.cpp"),
        str(ROOT / "src" / "effects" / "overdrive.cpp"),
        str(ROOT / "src" / "effects" / "chorus.cpp"),
        str(ROOT / "src" / "effects" / "tone_master_pro_3d_chorus.cpp"),
        str(ROOT / "src" / "effects" / "fractal_surround_delay.cpp"),
        str(ROOT / "src" / "effects" / "flanger.cpp"),
        str(ROOT / "src" / "effects" / "phaser.cpp"),
        str(ROOT / "src" / "effects" / "vibrato.cpp"),
        str(ROOT / "src" / "effects" / "envelope_wah.cpp"),
        str(ROOT / "src" / "signal_controls.cpp"),
        str(ROOT / "src" / "effects" / "filters.cpp"),
        str(ROOT / "src" / "effects" / "peak_limiter.cpp"),
        str(ROOT / "src" / "effects" / "poly_pitch_shifter.cpp"),
        str(ROOT / "src" / "effects" / "chromatic_pitch_delay.cpp"),
        str(ROOT / "src" / "effects" / "pedal_compressor.cpp"),
        str(ROOT / "src" / "effects" / "optical_compressor.cpp"),
        str(ROOT / "src" / "effects" / "fet_compressor.cpp"),
        str(ROOT / "src" / "effects" / "guitar_graphic_eq.cpp"),
        str(ROOT / "src" / "effects" / "bass_graphic_eq.cpp"),
        str(ROOT / "src" / "effects" / "shelf_tilt_eq.cpp"),
        str(ROOT / "src" / "effects" / "room_reverb.cpp"),
        str(ROOT / "src" / "effects" / "hall_reverb.cpp"),
        str(ROOT / "src" / "effects" / "plate_reverb.cpp"),
        str(ROOT / "src" / "effects" / "spring_reverb.cpp"),
         str(ROOT / "src" / "effects" / "auto_swell.cpp"),
         str(ROOT / "src" / "effects" / "rotary_speaker.cpp"),
         str(ROOT / "src" / "effects" / "ring_modulator.cpp"),
        str(ROOT / "src" / "effects" / "rhythmic_slicer.cpp"),
        str(ROOT / "src" / "effects" / "step_filter.cpp"),
         str(ROOT / "src" / "effects" / "asheville_pattrn.cpp"),
        str(ROOT / "src" / "effects" / "notch_filter.cpp"),
        str(ROOT / "src" / "effects" / "formant_filter.cpp"),
        str(ROOT / "src" / "effects" / "harmonic_resonator.cpp"),
        str(ROOT / "src" / "effects" / "multiband_compressor.cpp"),
        str(ROOT / "src" / "effects" / "bit_crusher.cpp"),
        str(ROOT / "src" / "effects" / "dimension_chorus.cpp"),
        str(ROOT / "src" / "effects" / "triple_double_chorus.cpp"),
        str(ROOT / "src" / "effects" / "four_band_resonator.cpp"),
        str(ROOT / "src" / "effects" / "diatonic_harmonizer.cpp"),
        str(ROOT / "src" / "effects" / "pitch_arpeggiator.cpp"),
        str(ROOT / "src" / "effects" / "mono_guitar_synth.cpp"),
        str(ROOT / "src" / "effects" / "fractal_three_voice_synth.cpp"),
        str(ROOT / "src" / "effects" / "poly_detune.cpp"),
        str(ROOT / "src" / "effects" / "pedal_detune.cpp"),
        str(ROOT / "src" / "effects" / "mutant_filter.cpp"),
        str(ROOT / "src" / "effects" / "fractal_chromatic_harmonizer.cpp"),
        str(ROOT / "src" / "effects" / "fractal_diatonic_harmonizer.cpp"),
        str(ROOT / "src" / "effects" / "grain_delay.cpp"),
        str(ROOT / "src" / "effects" / "pitch_feedback_delay.cpp"),
        str(ROOT / "src" / "effects" / "feedback_generator.cpp"),
        str(ROOT / "src" / "effects" / "shimmer_reverb.cpp"),
        str(ROOT / "src" / "effects" / "convolution_reverb.cpp"),
        str(ROOT / "src" / "effects" / "cabinet_ir.cpp"),
        str(ROOT / "src" / "effects" / "octave_divider.cpp"),
        str(ROOT / "src" / "effects" / "double_take.cpp"),
        str(ROOT / "src" / "effects" / "eq5_graphic.cpp"),
        str(ROOT / "src" / "effects" / "xr1_global_eq.cpp"),
        str(ROOT / "src" / "effects" / "optical_tremolo.cpp"),
        str(ROOT / "src" / "effects" / "fractal_bias_trem.cpp"),
        str(ROOT / "src" / "effects" / "vibratone.cpp"),
        str(ROOT / "src" / "effects" / "reverse_delay.cpp"),
        str(ROOT / "src" / "effects" / "ducking_delay.cpp"),
        str(ROOT / "src" / "effects" / "modulated_delay.cpp"),
        str(ROOT / "src" / "effects" / "tone_master_small_hall.cpp"),
        str(ROOT / "src" / "effects" / "filtered_delay.cpp"),
        str(ROOT / "src" / "effects" / "resonant_delay.cpp"),
        str(ROOT / "src" / "effects" / "sweep_delay.cpp"),
        str(ROOT / "src" / "effects" / "tremolo.cpp"),
        str(ROOT / "src" / "effects" / "harmonic_tremolo.cpp"),
        str(ROOT / "src" / "effects" / "auto_swell_delay.cpp"),
        str(ROOT / "src" / "effects" / "shuffling_looper.cpp"),
        str(ROOT / "src" / "effects" / "ten_tap_delay.cpp"),
        str(ROOT / "src" / "effects" / "auto_pan.cpp"),
        str(ROOT / "src" / "effects" / "megatap_delay.cpp"),
        str(ROOT / "src" / "effects" / "de_esser.cpp"),
        str(ROOT / "src" / "effects" / "pugilist.cpp"),
        str(ROOT / "src" / "effects" / "vibe.cpp"),
        str(ROOT / "src" / "effects" / "multi_delay.cpp"),
        str(ROOT / "src" / "effects" / "plex_delay.cpp"),
        str(ROOT / "src" / "effects" / "celestial_reverb.cpp"),
        str(ROOT / "src" / "effects" / "cloud_reverb.cpp"),
        str(ROOT / "src" / "effects" / "mercator_distortion.cpp"),
        str(ROOT / "src" / "effects" / "manual_thru_zero_flanger.cpp"),
        str(ROOT / "src" / "effects" / "octobot.cpp"),
        str(ROOT / "src" / "effects" / "am_ring_mod.cpp"),
        str(ROOT / "src" / "effects" / "tape_echo.cpp"),
        str(ROOT / "src" / "effects" / "octave_fuzz.cpp"),
        str(ROOT / "src" / "effects" / "rock_box_dist.cpp"),
        str(ROOT / "src" / "effects" / "rock_box_boost.cpp"),
        str(ROOT / "src" / "effects" / "stereo_panner.cpp"),
        str(ROOT / "src" / "effects" / "stereo_crossover.cpp"),
        str(ROOT / "src" / "effects" / "stereo_filter.cpp"),
        str(ROOT / "src" / "effects" / "step_tremolo.cpp"),
        str(ROOT / "src" / "effects" / "porta_preamp.cpp"),
        str(ROOT / "src" / "effects" / "pitch_sequence.cpp"),
        str(ROOT / "src" / "effects" / "poly_sustain.cpp"),
        str(ROOT / "src" / "effects" / "step_filter_delay.cpp"),
        str(ROOT / "src" / "effects" / "volume_pan.cpp"),
        str(ROOT / "src" / "effects" / "volume_pedal.cpp"),
        str(ROOT / "src" / "effects" / "wah.cpp"),
        str(ROOT / "src" / "level_meter.cpp"),
        str(ROOT / "src" / "web" / "wasm_chain.cpp"),
        "-o", str(WEB_BUILD_DIR / "tabletfx_wasm.js"),
        "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sSINGLE_FILE=1",
        "-sENVIRONMENT=web,worker,worklet", "-sALLOW_MEMORY_GROWTH=0",
        "-sINITIAL_MEMORY=100663296", "-sSTACK_SIZE=262144",
        "-sNO_EXIT_RUNTIME=1", "-sFILESYSTEM=0",
        "-sEXPORTED_FUNCTIONS=" + json.dumps(exports),
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32",
    ]
    run(command, cwd=ROOT)
    size = (WEB_BUILD_DIR / "tabletfx_wasm.js").stat().st_size
    print(f"Built shared C++ DSP module: {WEB_BUILD_DIR / 'tabletfx_wasm.js'} ({size} bytes)")


class LocalWebHandler(SimpleHTTPRequestHandler):
    test_files: dict[str, Path] = {}
    request_paths: list[str] = []

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def translate_path(self, path: str) -> str:
        request_path = urlsplit(path).path
        if request_path in self.test_files:
            return str(self.test_files[request_path])
        return super().translate_path(path)

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def copyfile(self, source, outputfile) -> None:
        try:
            super().copyfile(source, outputfile)
        except (BrokenPipeError, ConnectionResetError):
            # Headless Chrome may close a completed test response while the
            # threaded static server is still flushing the last buffered bytes.
            pass

    def do_GET(self) -> None:
        self.request_paths.append(urlsplit(self.path).path)
        super().do_GET()

    def log_message(self, _format: str, *_args) -> None:
        return


def serve_web() -> None:
    server = ThreadingHTTPServer(("127.0.0.1", 8000), LocalWebHandler)
    server.daemon_threads = True
    print("TabletFX browser tester: http://127.0.0.1:8000/web/", flush=True)
    print("Cross-origin isolation is enabled for WebAssembly audio metrics.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Stopping local browser tester.")
    finally:
        server.server_close()


def make_test_audio(destination: Path, sample_rate: int = 48000,
                    duration_seconds: float = 1.0) -> int:
    frames = int(sample_rate * duration_seconds)
    samples = bytearray()
    for frame in range(frames):
        time_seconds = frame / sample_rate
        envelope = 0.58 + 0.32 * math.sin(2.0 * math.pi * 2.1 * time_seconds)
        signal = envelope * (
            0.32 * math.sin(2.0 * math.pi * 110.0 * time_seconds)
            + 0.22 * math.sin(2.0 * math.pi * 164.8138 * time_seconds)
            + 0.12 * math.sin(2.0 * math.pi * 220.0 * time_seconds)
        )
        value = max(-32768, min(32767, round(signal * 32767.0)))
        samples.extend(struct.pack("<hh", value, value))
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)
    return frames


def make_prerecorded_guitar_test_audio(destination: Path) -> str | None:
    """Prefer the locally linked stereo guitar DI fixture; keep CI fallback portable."""
    source_path = (
        ROOT / "testdata" / "reference_audio" / "guitar-techs-p3-music"
        / "P3_music" / "audio" / "directinput" / "directinput_01.wav"
    )
    if not source_path.is_file():
        return None

    sample_rate = 48000
    frame_count = sample_rate
    with wave.open(str(source_path), "rb") as source:
        if (source.getnchannels() != 2 or source.getsampwidth() != 3 or
                source.getframerate() != sample_rate):
            return None
        frames = source.readframes(frame_count)
    if len(frames) != frame_count * 6:
        return None

    stereo_16_bit = bytearray(frame_count * 4)
    for frame in range(frame_count):
        offset = frame * 6
        left = int.from_bytes(frames[offset:offset + 3], "little", signed=True)
        right = int.from_bytes(frames[offset + 3:offset + 6], "little", signed=True)
        left_16 = max(-32768, min(32767, round(left / 256.0)))
        right_16 = max(-32768, min(32767, round(right / 256.0)))
        struct.pack_into("<hh", stereo_16_bit, frame * 4, left_16, right_16)

    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(stereo_16_bit)
    return str(source_path)


def make_stereo_tape_echo_impulse(destination: Path, sample_rate: int = 8000,
                                 frames: int = 1024) -> None:
    samples = bytearray(frames * 4)
    struct.pack_into("<hh", samples, 0, 16384, 8192)
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)


def make_stereo_plex_delay_impulse(destination: Path, sample_rate: int = 8000,
                                  frames: int = 1024) -> None:
    samples = bytearray(frames * 4)
    struct.pack_into("<hh", samples, 0, 24576, 8192)
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)


def make_tuner_test_audio(destination: Path, sample_rate: int = 48000,
                          duration_seconds: float = 0.5) -> int:
    frames = int(sample_rate * duration_seconds)
    samples = bytearray()
    for frame in range(frames):
        value = max(-32768, min(32767, round(
            0.35 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate) * 32767.0
        )))
        samples.extend(struct.pack("<hh", value, value))
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)
    return frames


def make_convolution_test_ir(destination: Path, sample_rate: int = 48000) -> None:
    samples = (16384, 8192, 4096, 2048)
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(struct.pack("<4h", *samples))


def make_cabinet_ir_test_wav(destination: Path) -> None:
    """Write the exact mono PCM24/48 kHz/2,048-frame Cabinet IR test shape."""
    frames = 2048
    samples = [0] * frames
    samples[0] = 4_194_304
    samples[1] = -1_048_576
    samples[3] = 524_288
    pcm = b"".join(value.to_bytes(3, "little", signed=True) for value in samples)
    fmt = struct.pack("<HHIIHH", 1, 1, 48_000, 144_000, 3, 24)
    chunks = (
        b"fmt " + struct.pack("<I", len(fmt)) + fmt
        + b"data" + struct.pack("<I", len(pcm)) + pcm
    )
    body = b"WAVE" + chunks
    destination.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def make_long_convolution_test_ir(destination: Path, sample_rate: int = 48000) -> int:
    frames = 10 * sample_rate
    samples = bytearray(frames * 4)
    struct.pack_into("<hh", samples, 0, 16384, 16384)
    struct.pack_into("<hh", samples, len(samples) - 4, 8192, -4096)
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)
    return frames


def websocket_send(sock: socket.socket, opcode: int, payload: bytes) -> None:
    mask = os.urandom(4)
    size = len(payload)
    if size < 126:
        header = bytes((0x80 | opcode, 0x80 | size))
    elif size < 65536:
        header = bytes((0x80 | opcode, 0x80 | 126)) + struct.pack("!H", size)
    else:
        header = bytes((0x80 | opcode, 0x80 | 127)) + struct.pack("!Q", size)
    masked = bytes(value ^ mask[index & 3] for index, value in enumerate(payload))
    sock.sendall(header + mask + masked)


def websocket_receive(sock: socket.socket, stream) -> dict | None:
    first = stream.read(2)
    if len(first) != 2:
        raise RuntimeError("Chrome DevTools WebSocket closed unexpectedly")
    opcode = first[0] & 0x0F
    size = first[1] & 0x7F
    masked = bool(first[1] & 0x80)
    if size == 126:
        size = struct.unpack("!H", stream.read(2))[0]
    elif size == 127:
        size = struct.unpack("!Q", stream.read(8))[0]
    mask = stream.read(4) if masked else b""
    payload = stream.read(size)
    if len(payload) != size:
        raise RuntimeError("Incomplete Chrome DevTools WebSocket frame")
    if masked:
        payload = bytes(value ^ mask[index & 3] for index, value in enumerate(payload))
    if opcode == 8:
        return None
    if opcode == 9:
        websocket_send(sock, 10, payload)
        return None
    if opcode != 1:
        return None
    return json.loads(payload)


def cdp_call(sock: socket.socket, stream, command_id: int,
             method: str, params: dict | None = None) -> dict:
    message = {"id": command_id, "method": method}
    if params is not None:
        message["params"] = params
    websocket_send(sock, 1, json.dumps(message).encode("utf-8"))
    while True:
        response = websocket_receive(sock, stream)
        if response is None or response.get("id") != command_id:
            continue
        if "error" in response:
            raise RuntimeError(f"Chrome DevTools {method}: {response['error']}")
        return response


def run_chrome_page(chrome: str, profile: Path, url: str,
                    stderr_path: Path,
                    post_test_expression: str | None = None) -> tuple[dict, str]:
    command = [
        chrome, "--headless=new", "--no-first-run", "--no-default-browser-check",
        "--no-sandbox", "--disable-dev-shm-usage", "--autoplay-policy=no-user-gesture-required",
        "--use-fake-device-for-media-stream", "--mute-audio",
        "--disable-features=AudioServiceOutOfProcess", "--remote-debugging-port=0",
        "--user-data-dir=" + str(profile), "about:blank",
    ]
    print("+ " + " ".join(command), flush=True)
    with stderr_path.open("w", encoding="utf-8") as stderr_file:
        process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.DEVNULL,
                                   stderr=stderr_file, text=True)
        try:
            active_port = profile / "DevToolsActivePort"
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and process.poll() is None:
                if active_port.is_file():
                    break
                time.sleep(0.05)
            if not active_port.is_file():
                raise RuntimeError("Chrome did not publish its DevTools port")
            port = int(active_port.read_text(encoding="utf-8").splitlines()[0])
            targets_url = f"http://127.0.0.1:{port}/json/list"
            with urllib.request.urlopen(targets_url, timeout=5) as response:
                targets = json.load(response)
            target = next((item for item in targets if item.get("type") == "page"), None)
            if target is None:
                raise RuntimeError("Chrome DevTools did not expose a page target")
            endpoint = urlsplit(target["webSocketDebuggerUrl"])
            sock = socket.create_connection((endpoint.hostname, endpoint.port), timeout=5)
            key = base64.b64encode(os.urandom(16)).decode("ascii")
            handshake = (
                f"GET {endpoint.path} HTTP/1.1\r\n"
                f"Host: {endpoint.hostname}:{endpoint.port}\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
            )
            sock.sendall(handshake.encode("ascii"))
            stream = sock.makefile("rb")
            response_head = b""
            while not response_head.endswith(b"\r\n\r\n"):
                chunk = stream.read(1)
                if not chunk:
                    raise RuntimeError("Chrome DevTools WebSocket handshake failed")
                response_head += chunk
                if len(response_head) > 8192:
                    raise RuntimeError("Chrome DevTools WebSocket handshake was oversized")
            if b" 101 " not in response_head:
                raise RuntimeError(f"Chrome DevTools WebSocket rejected handshake: {response_head!r}")

            command_id = 1
            cdp_call(sock, stream, command_id, "Runtime.enable")
            command_id += 1
            cdp_call(sock, stream, command_id, "Page.enable")
            command_id += 1
            cdp_call(sock, stream, command_id, "Page.navigate", {"url": url})
            command_id += 1
            expression = (
                "(()=>{const n=document.querySelector('#browser-test-result');"
                "return JSON.stringify({status:n?.dataset.status,text:n?.textContent,title:document.title})})()"
            )
            deadline = time.monotonic() + 180
            result: dict = {}
            while time.monotonic() < deadline:
                response = cdp_call(sock, stream, command_id, "Runtime.evaluate", {
                    "expression": expression,
                    "returnByValue": True,
                    "awaitPromise": True,
                })
                command_id += 1
                remote = response.get("result", {}).get("result", {})
                value = remote.get("value")
                if value:
                    result = json.loads(value)
                    if result.get("status") in ("passed", "failed"):
                        break
                time.sleep(0.15)
            if result.get("status") == "passed" and post_test_expression:
                response = cdp_call(sock, stream, command_id, "Runtime.evaluate", {
                    "expression": post_test_expression,
                    "returnByValue": True,
                    "awaitPromise": True,
                    "userGesture": True,
                })
                remote = response.get("result", {}).get("result", {})
                value = remote.get("value")
                if response.get("result", {}).get("exceptionDetails"):
                    raise RuntimeError(
                        "Stereo Tape Echo browser verification failed: "
                        + json.dumps(response["result"]["exceptionDetails"])
                    )
                if not value:
                    raise RuntimeError("Stereo Tape Echo browser verification returned no result")
                stereo_report = json.loads(value)
                if not stereo_report.get("passed"):
                    raise RuntimeError(
                        "Stereo Tape Echo browser verification failed: "
                        + json.dumps(stereo_report)
                    )
                result["stereoTapeEcho"] = stereo_report
            sock.close()
            if not result:
                raise RuntimeError("Chrome page did not expose its browser-test status")
            return result, "\n".join(stderr_path.read_text(encoding="utf-8").splitlines()[-20:])
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


def browser_test(*, volume_pan_only: bool = False, stereo_filter_only: bool = False,
                 volume_pedal_only: bool = False,
                 formant_control_lfo_only: bool = False,
                 headrush_drop_tune_only: bool = False,
                 stadium_delay_trails_only: bool = False,
                 fender_stereo_flanger_phase_only: bool = False,
                 fender_flanger_tap_division_only: bool = False,
                 fender_phaser_90_tap_division_only: bool = False,
                  fractal_chorus_tempo_only: bool = False,
                 fender_tremolo_lfo_clip_only: bool = False,
                 fender_tremolo_duty_cycle_only: bool = False,
                 fractal_tzf_auto_only: bool = False,
                 stereo_panner_only: bool = False,
                 mutant_filter_only: bool = False,
                 step_tremolo_only: bool = False,
                 fractal_bias_trem_only: bool = False,
                 vibratone_only: bool = False,
                 uni_verse_vibe_only: bool = False,
                  four_osc_only: bool = False,
                  poly_detune_only: bool = False,
                  fractal_poly_pitch_only: bool = False,
                  fender_pedal_shifter_only: bool = False,
                  fender_pedal_detune_only: bool = False,
                  fractal_diatonic_custom_scale_only: bool = False,
                  fractal_chromatic_harmonizer_only: bool = False,
                  fractal_diatonic_harmonizer_only: bool = False,
                  fractal_resonator_input_gain_only: bool = False) -> None:
    compiler = emscripten_cpp()
    if compiler is None or not (WEB_BUILD_DIR / "tabletfx_wasm.js").is_file():
        raise SystemExit("Build the browser module first with `make web-build`")
    chrome = shutil.which("google-chrome")
    if chrome is None:
        raise SystemExit("Google Chrome is required for `make web-test`")
    native_renderer = ROOT / "build" / "native" / "tabletfx_offline"
    if not native_renderer.is_file():
        raise SystemExit("Build the native renderer first with `make test`")

    with tempfile.TemporaryDirectory(prefix="multifx-web-test-") as temp_name:
        temp_dir = Path(temp_name)
        input_path = temp_dir / "guitar-fixture.wav"
        reference_path = temp_dir / "native-reference.wav"
        delay_trails_dry_reference_path = temp_dir / "delay-trails-dry-reference.wav"
        stereo_crossover_reference_path = temp_dir / "stereo-crossover-reference.wav"
        stereo_tape_echo_input_path = temp_dir / "stereo-tape-echo-input.wav"
        stereo_tape_echo_reference_path = temp_dir / "stereo-tape-echo-reference.wav"
        plex_delay_input_select_input_path = temp_dir / "plex-delay-input-select-input.wav"
        plex_delay_input_select_reference_path = temp_dir / "plex-delay-input-select-reference.wav"
        tuner_path = temp_dir / "tuner-a4.wav"
        convolution_ir_path = temp_dir / "convolution-ir.wav"
        cabinet_ir_path = temp_dir / "cabinet-ir-pcm24.wav"
        long_convolution_ir_path = temp_dir / "convolution-ir-10s.wav"
        prerecorded_guitar_source = make_prerecorded_guitar_test_audio(input_path)
        if prerecorded_guitar_source is None:
            if (fractal_resonator_input_gain_only or fractal_diatonic_harmonizer_only
                    or stereo_panner_only or formant_control_lfo_only
                    or headrush_drop_tune_only or fender_stereo_flanger_phase_only
                    or fender_flanger_tap_division_only
                    or fender_phaser_90_tap_division_only
                    or fractal_chorus_tempo_only
                    or fractal_tzf_auto_only):
                raise SystemExit(
                    "Focused effect browser test requires the local prerecorded guitar DI fixture."
                )
            make_test_audio(input_path)
            print("Browser guitar input: generated deterministic fallback fixture")
        else:
            print(f"Browser guitar input: local prerecorded DI {prerecorded_guitar_source}")
        make_tuner_test_audio(tuner_path)
        make_convolution_test_ir(convolution_ir_path)
        make_cabinet_ir_test_wav(cabinet_ir_path)
        make_long_convolution_test_ir(long_convolution_ir_path)
        make_stereo_tape_echo_impulse(stereo_tape_echo_input_path)
        make_stereo_plex_delay_impulse(plex_delay_input_select_input_path)
        renderer_command = [
            str(native_renderer), "render", str(input_path), str(reference_path),
            "--gain-db", "2", "--output-gain-db", "-1",
            "--volume-position", "0.92", "--volume-min-db", "-96",
            "--volume-max-db", "0", "--volume-curve", "audio",
            "--limiter-ceiling-db", "-1", "--limiter-release-ms", "80",
            "--wah-position", "0.62", "--wah-min-hz", "350",
            "--wah-max-hz", "2200", "--wah-q", "4", "--wah-level-db", "0",
            "--touch-wah-sensitivity", "0.8", "--touch-wah-direction", "up",
            "--touch-wah-min-hz", "350", "--touch-wah-max-hz", "2200",
            "--touch-wah-q", "4", "--touch-wah-level-db", "0",
            "--touch-wah-attack-ms", "5", "--touch-wah-release-ms", "80",
            "--touch-wah-smoothing-ms", "5",
            "--overdrive-drive", "0.15", "--overdrive-level-db", "-3",
            "--overdrive-mix", "0.35",
            "--mid-overdrive-drive", "0.18", "--mid-overdrive-boost-db", "6",
            "--mid-overdrive-level-db", "-4", "--mid-overdrive-mix", "0.4",
            "--high-gain-drive", "0.75", "--high-gain-pre-low-cut-hz", "100",
            "--high-gain-pre-high-cut-hz", "9000", "--high-gain-post-tone-hz", "5200",
            "--high-gain-level-db", "-9", "--high-gain-mix", "1",
            "--round-fuzz", "0.55", "--round-fuzz-level-db", "-6",
            "--round-fuzz-mix", "0.85",
            "--tremolo-rate-hz", "4", "--tremolo-depth", "0.2",
            "--tremolo-waveform", "sine", "--tremolo-sync", "off",
            "--harmonic-tremolo-rate-hz", "4.25",
            "--harmonic-tremolo-depth", "0.8",
            "--harmonic-tremolo-crossover-hz", "950",
            "--harmonic-tremolo-waveform", "triangle",
            "--harmonic-tremolo-sync", "on",
            "--harmonic-tremolo-tempo-bpm", "96",
            "--harmonic-tremolo-beats-per-cycle", "0.5",
            "--harmonic-tremolo-stereo-alternation", "off",
            "--harmonic-tremolo-smoothing-ms", "25",
            "--auto-swell-delay", "--auto-swell-delay-time-ms", "350",
            "--auto-swell-delay-feedback", "0.25", "--auto-swell-delay-mix", "0.25",
            "--auto-swell-delay-time-mode", "milliseconds",
            "--auto-swell-delay-tap-division", "quarter",
            "--auto-swell-delay-tempo-bpm", "120",
            "--auto-swell-delay-swell-ms", "300",
            "--auto-swell-delay-mod", "0.15", "--auto-swell-delay-tone", "0.7",
            "--auto-swell-delay-sense-db", "-36", "--auto-swell-delay-taper", "normal",
            "--auto-swell-delay-dry", "off", "--auto-swell-delay-spillover", "off",
            "--chorus-rate-hz", "0.8", "--chorus-delay-ms", "15",
            "--chorus-depth-ms", "7", "--chorus-mix", "0.2",
            "--flanger-rate-hz", "0.25", "--flanger-delay-ms", "2",
            "--flanger-depth-ms", "1", "--flanger-feedback", "0.35",
             "--flanger-feedback-polarity", "negative", "--flanger-mix", "0.2",
             "--flanger-right-phase-degrees", "180",
            "--phaser-rate-hz", "0.5", "--phaser-min-hz", "250",
            "--phaser-max-hz", "1600", "--phaser-feedback", "0.15",
            "--phaser-stages", "6", "--phaser-mix", "0.2",
            "--vibrato-rate-hz", "5", "--vibrato-depth-ms", "0.35",
            "--vibrato-mix", "0.35",
            "--gate-threshold-db", "-60", "--gate-attenuation-db", "-72",
            "--gate-hysteresis-db", "3", "--gate-attack-ms", "2",
            "--gate-hold-ms", "20", "--gate-release-ms", "80",
            "--boost-db", "3",
            "--compressor-threshold-db", "-18", "--compressor-ratio", "3",
            "--compressor-knee-db", "6", "--compressor-attack-ms", "8",
            "--compressor-release-ms", "100", "--compressor-makeup-db", "0",
            "--high-pass-hz", "80", "--low-pass-hz", "12000",
            "--pedal-compressor-sensitivity", "0.45",
            "--pedal-compressor-sustain", "0.45",
            "--pedal-compressor-attack", "0.25",
            "--pedal-compressor-level-db", "0",
            "--drive", "3",
            "--eq-band1-frequency-hz", "120", "--eq-band1-gain-db", "2",
            "--eq-band1-q", "0.707",
            "--eq-band2-frequency-hz", "800", "--eq-band2-gain-db", "-1.5",
            "--eq-band2-q", "0.707",
            "--eq-band3-frequency-hz", "5000", "--eq-band3-gain-db", "1",
            "--eq-band3-q", "0.707",
            "--delay-feedback", "0.25", "--delay-mix", "0.2", "--delay-tape",
            "--delay-tape-saturation", "0.45",
            "--delay-tape-wow-depth-ms", "0.35", "--delay-tape-wow-rate-hz", "0.3",
            "--delay-tape-flutter-depth-ms", "0.08", "--delay-tape-flutter-rate-hz", "6",
            "--delay-tape-heads", "--delay-head-spacing-ms", "180",
            "--delay-heads", "all",
            "--pitch-semitones", "5",
            "--optical-compressor",
            "--optical-compressor-threshold-db", "-24",
            "--optical-compressor-ratio", "3",
            "--optical-compressor-knee-db", "6",
            "--optical-compressor-attack-ms", "10",
            "--optical-compressor-release-ms", "80",
            "--optical-compressor-program-release-ms", "600",
            "--optical-compressor-rms-window-ms", "10",
            "--optical-compressor-makeup-db", "0",
            "--fet-compressor",
            "--fet-compressor-threshold-db", "-30",
            "--fet-compressor-ratio", "5",
            "--fet-compressor-knee-db", "2",
            "--fet-compressor-attack-ms", "0.1",
            "--fet-compressor-release-ms", "50",
            "--fet-compressor-makeup-db", "0",
            "--guitar-graphic-eq", "--guitar-eq-layout", "seven",
            "--guitar-eq-band1-gain-db", "1.5",
            "--guitar-eq-band2-gain-db", "-1",
            "--guitar-eq-band3-gain-db", "0",
            "--guitar-eq-band4-gain-db", "1",
            "--guitar-eq-band5-gain-db", "0.5",
            "--guitar-eq-band6-gain-db", "-0.5",
            "--guitar-eq-band7-gain-db", "1",
            "--bass-graphic-eq", "--bass-eq-layout", "seven",
            "--bass-eq-band1-gain-db", "-1",
            "--bass-eq-band2-gain-db", "0.5",
            "--bass-eq-band3-gain-db", "2",
            "--bass-eq-band4-gain-db", "-2",
            "--bass-eq-band5-gain-db", "0.5",
            "--bass-eq-band6-gain-db", "1",
            "--bass-eq-band7-gain-db", "-0.5",
            "--shelf-tilt-eq", "--shelf-tilt-mode", "tilt",
            "--shelf-tilt-frequency-hz", "1200", "--shelf-tilt-gain-db", "2",
            "--room-reverb", "--room-size", "small", "--room-decay-s", "0.8",
            "--room-pre-delay-ms", "10", "--room-damping-hz", "6000",
            "--room-mix", "0.2", "--room-level-db", "0",
            "--hall-reverb", "--hall-decay-s", "3.2",
            "--hall-pre-delay-ms", "32", "--hall-damping-hz", "9000",
            "--hall-low-cut-hz", "100", "--hall-mix", "0.25",
            "--hall-level-db", "0",
            "--plate-reverb", "--plate-decay-s", "1.8",
            "--plate-pre-delay-ms", "0", "--plate-damping-hz", "7500",
            "--plate-diffusion", "0.82", "--plate-balance", "0.5",
            "--plate-mix", "0.2", "--plate-level-db", "0",
            "--spring-reverb", "--spring-decay-s", "2.4",
            "--spring-pre-delay-ms", "0", "--spring-damping-hz", "8000",
            "--spring-dwell", "0.35", "--spring-mix", "0.2",
            "--spring-level-db", "0",
             "--auto-swell", "--auto-swell-threshold-db", "-90",
             "--auto-swell-attack-ms", "100", "--auto-swell-release-ms", "150",
             "--auto-swell-hysteresis-db", "6", "--auto-swell-taper", "linear",
             "--auto-swell-volume", "0.9",
            "--rotary-speaker",
            "--rotary-rate-hz", "6", "--rotary-tempo-sync", "off",
            "--rotary-tempo-bpm", "120", "--rotary-beats-per-rotation", "1",
            "--rotary-speed", "slow", "--rotary-slow-speed-multiplier", "0.12",
            "--rotary-drive", "0.15", "--rotary-hi-depth", "0.75",
            "--rotary-high-level", "1", "--rotary-low-depth", "0.55",
            "--rotary-rotor-length", "0.5", "--rotary-low-rate-multi", "0.72",
            "--rotary-low-time-constant-ms", "800", "--rotary-hi-time-constant-ms", "450",
             "--rotary-low-mic-spacing", "0.4", "--rotary-high-mic-spacing", "0.75",
             "--rotary-mic-distance", "0.25",
            "--rotary-stereo-spread", "1", "--rotary-mix", "1", "--rotary-level", "1",
            "--rotary-balance", "0", "--rotary-input-select", "stereo",
            "--ring-mod", "--ring-mod-frequency-hz", "713",
            "--ring-mod-multiplier", "1.75", "--ring-mod-high-cut-hz", "9000",
            "--ring-mod-mix", "0.83",
             "--rhythmic-slicer", "--rhythmic-slicer-tempo-sync", "on",
             "--rhythmic-slicer-tempo-bpm", "120",
             "--rhythmic-slicer-division", "sixteenth",
             "--rhythmic-slicer-rate-hz", "8", "--rhythmic-slicer-depth", "0.4",
             "--rhythmic-slicer-shape", "0.15",
             "--rhythmic-slicer-attack-ms", "0.5",
             "--rhythmic-slicer-release-ms", "1",
             "--rhythmic-slicer-phase", "0",
             "--rhythmic-slicer-steps", "1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25",
              "--step-filter", "--step-filter-mode", "lowpass",
              "--step-filter-tempo-sync", "on", "--step-filter-tempo-bpm", "120",
              "--step-filter-division", "sixteenth", "--step-filter-rate-hz", "8",
              "--step-filter-min-cutoff-hz", "120", "--step-filter-max-cutoff-hz", "6000",
              "--step-filter-resonance-q", "0.707", "--step-filter-depth", "0.65",
              "--step-filter-shape", "0.4", "--step-filter-smoothing-ms", "3",
              "--step-filter-mix", "0.7", "--step-filter-phase", "0",
              "--step-filter-steps", "0,0.75,0.2,0.9,0.35,0.6,0.1,1,0.25,0.8,0.15,0.95,0.4,0.7,0.05,0.85",
              "--notch-filter", "--notch-filter-frequency-hz", "713",
              "--notch-filter-q", "6", "--notch-filter-mix", "0.72",
              "--reverse-delay", "--reverse-delay-time-ms", "350",
              "--reverse-delay-feedback", "0.25", "--reverse-delay-mix", "0.35",
              "--reverse-delay-crossfade-ms", "10",
               "--reverse-delay-time-mode", "milliseconds",
               "--reverse-delay-tempo-bpm", "120",
               "--reverse-delay-division", "quarter",
               "--reverse-delay-low-cut-hz", "0",
               "--reverse-delay-high-cut-hz", "0",
               "--reverse-delay-forward-level", "0",
               "--reverse-delay-spillover", "off",
               "--ducking-delay", "--ducking-delay-time-ms", "350",
               "--ducking-delay-feedback", "0.25", "--ducking-delay-mix", "0.25",
               "--ducking-delay-threshold-db", "-30",
               "--ducking-delay-attenuation-db", "12",
               "--ducking-delay-release-ms", "250",
               "--modulated-delay", "--modulated-delay-time-ms", "350",
               "--modulated-delay-feedback", "0.25", "--modulated-delay-mix", "0.25",
               "--modulated-delay-rate-hz", "0.8", "--modulated-delay-depth-ms", "2",
               "--modulated-delay-waveform", "sine",
               "--modulated-delay-spread-degrees", "180",
               "--modulated-delay-trigger-threshold-db", "-24",
               "--filtered-delay", "--filtered-delay-time-ms", "350",
               "--filtered-delay-feedback", "0.25", "--filtered-delay-mix", "0.25",
               "--filtered-delay-low-cut-hz", "120",
               "--filtered-delay-high-cut-hz", "6000",
              "--resonant-delay", "--resonant-delay-time-ms", "350",
              "--resonant-delay-feedback", "0.35", "--resonant-delay-mix", "0.25",
              "--resonant-delay-center-hz", "800", "--resonant-delay-q", "2",
               "--harmonic-tremolo-rate-hz", "4.25",
               "--harmonic-tremolo-depth", "0.8",
               "--harmonic-tremolo-crossover-hz", "950",
               "--harmonic-tremolo-waveform", "triangle",
               "--harmonic-tremolo-sync", "on",
               "--harmonic-tremolo-tempo-bpm", "96",
               "--harmonic-tremolo-beats-per-cycle", "0.5",
               "--harmonic-tremolo-stereo-alternation", "off",
               "--harmonic-tremolo-smoothing-ms", "25",
                "--auto-swell-delay", "--auto-swell-delay-time-ms", "350",
                "--auto-swell-delay-feedback", "0.25", "--auto-swell-delay-mix", "0.25",
                "--auto-swell-delay-time-mode", "milliseconds",
                "--auto-swell-delay-tap-division", "quarter",
                "--auto-swell-delay-tempo-bpm", "120",
                "--auto-swell-delay-swell-ms", "300",
                "--auto-swell-delay-mod", "0.15", "--auto-swell-delay-tone", "0.7",
                "--auto-swell-delay-sense-db", "-36", "--auto-swell-delay-taper", "normal",
                "--auto-swell-delay-dry", "off", "--auto-swell-delay-spillover", "off",
               "--sweep-delay", "--sweep-delay-time-ms", "350",
               "--sweep-delay-feedback", "0.25", "--sweep-delay-mix", "0.25",
               "--sweep-delay-start-hz", "250", "--sweep-delay-stop-hz", "6000",
               "--sweep-delay-resonance-q", "1.2", "--sweep-delay-rate-hz", "0.5",
               "--sweep-delay-waveform", "sine", "--sweep-delay-phase-degrees", "90",
                "--formant-filter", "--formant-filter-vowel-position", "1.5",
                "--formant-filter-q", "6", "--formant-filter-mix", "0.72",
                "--formant-filter-level-db", "-2",
                "--formant-filter-start-vowel-index", "0",
                "--formant-filter-middle-vowel-index", "2",
                "--formant-filter-end-vowel-index", "4",
                "--formant-filter-control-percent", "65",
                 "--formant-filter-control-lfo",
                 "--formant-filter-control-lfo-waveform-index", "1",
                 "--formant-filter-control-lfo-rate-hz", "1",
                 "--formant-filter-control-lfo-min-percent", "0",
                 "--formant-filter-control-lfo-max-percent", "100",
                "--formant-filter-balance-percent", "25",
                "--harmonic-resonator", "--harmonic-resonator-root-frequency-hz", "220",
                "--harmonic-resonator-decay-seconds", "0.8",
                "--harmonic-resonator-brightness", "0.7",
                "--harmonic-resonator-mix", "0.65",
                 "--multiband-compressor", "--multiband-low-mid-hz", "500",
                 "--multiband-mid-high-hz", "4000", "--multiband-slope", "db12",
                 "--multiband-low-threshold-db", "-30", "--multiband-low-ratio", "4",
                 "--multiband-low-knee-db", "0", "--multiband-low-attack-ms", "1",
                 "--multiband-low-release-ms", "30", "--multiband-low-level-db", "6",
                 "--multiband-low-detector", "peak", "--multiband-low-mute", "off",
                 "--multiband-mid-threshold-db", "-24", "--multiband-mid-ratio", "2",
                 "--multiband-mid-knee-db", "6", "--multiband-mid-attack-ms", "10",
                 "--multiband-mid-release-ms", "100", "--multiband-mid-level-db", "0",
                 "--multiband-mid-detector", "rms", "--multiband-mid-mute", "off",
                 "--multiband-high-threshold-db", "-18", "--multiband-high-ratio", "3",
                 "--multiband-high-knee-db", "3", "--multiband-high-attack-ms", "5",
                 "--multiband-high-release-ms", "250", "--multiband-high-level-db", "-2",
                 "--multiband-high-detector", "rms", "--multiband-high-mute", "off",
                 "--bit-crusher", "--bit-crusher-bit-depth", "8",
                 "--bit-crusher-sample-rate-divisor", "4", "--bit-crusher-mix", "0.65",
            "--dimension-chorus", "--dimension-chorus-version", "C",
            "--dimension-chorus-mode", "1", "--dimension-chorus-mix", "0.5",
            "--dimension-chorus-level-db", "0",
             "--triple-double-chorus", "--triple-double-chorus-preset-switch", "off",
             "--triple-double-chorus-manual-switch", "off",
             "--triple-double-chorus-rate-hz", "0.8",
             "--triple-double-chorus-left-depth-ms", "7",
             "--triple-double-chorus-center-depth-ms", "5",
             "--triple-double-chorus-right-depth-ms", "8",
             "--triple-double-chorus-feedback", "0.1",
             "--triple-double-chorus-sparkle", "0.2",
             "--triple-double-chorus-stereo-switch", "on",
             "--four-band-resonator-mode", "chord",
             "--four-band-resonator-chord", "dominant7",
             "--four-band-resonator-master-frequency", "1.1",
             "--four-band-resonator-frequencies-hz", "220,330,440,660",
             "--four-band-resonator-frequency-hz", "220",
             "--four-band-resonator-feedback", "0.65,0.55,0.45,0.35",
             "--four-band-resonator-q", "2,2.5,3,3.5",
             "--four-band-resonator-levels", "0.8,0.7,0.6,0.5",
             "--four-band-resonator-pans", "-0.75,-0.25,0.25,0.75",
             "--four-band-resonator-filter-locations", "pre,post,pre,post",
             "--four-band-resonator-input-mode", "mono",
             "--four-band-resonator-master-level", "0.9",
             "--four-band-resonator-master-pan", "-0.25",
             "--four-band-resonator-master-feedback", "0.75",
             "--four-band-resonator-master-q", "1.2",
             "--four-band-resonator-mix", "0.65",
             "--diatonic-harmonizer-mix", "0.7",
             "--diatonic-harmonizer-interval", "2",
             "--diatonic-harmonizer-key", "7",
             "--diatonic-harmonizer-scale", "major",
             "--diatonic-harmonizer-tone-hz", "8000",
             "--diatonic-harmonizer-delay-ms", "120",
             "--diatonic-harmonizer-feedback", "0.2",
             "--diatonic-harmonizer-tap-division", "free",
             "--diatonic-harmonizer-tempo-bpm", "120",
              "--pitch-arpeggiator", "--pitch-arpeggiator-run", "on",
              "--pitch-arpeggiator-trigger-threshold-dbfs", "-90",
              "--pitch-arpeggiator-mix", "0.7",
              "--pitch-arpeggiator-steps", "4",
              "--pitch-arpeggiator-division", "free",
              "--pitch-arpeggiator-step-ms", "250",
              "--pitch-arpeggiator-shifts", "0,2,4,7,0,0,0,0,0,0,0,0,0,0,0,0",
              "--mono-guitar-synth", "--mono-guitar-synth-waveform", "sawtooth",
              "--mono-guitar-synth-mix", "0.35", "--mono-guitar-synth-level", "0.5",
              "--mono-guitar-synth-gate-threshold-db", "-54",
              "--mono-guitar-synth-attack-ms", "8", "--mono-guitar-synth-release-ms", "180",
              "--mono-guitar-synth-portamento-ms", "8",
             "--grain-delay-time-ms", "350",
             "--grain-delay-feedback", "0.25", "--grain-delay-mix", "0.35",
             "--grain-delay-grain-size-ms", "80", "--grain-delay-overlap", "2",
             "--grain-delay-pitch-semitones", "7", "--grain-delay-spray-ms", "35",
             "--pitch-feedback-delay", "--pitch-feedback-delay-time-ms", "350",
             "--pitch-feedback-delay-feedback", "0.35",
             "--pitch-feedback-delay-mix", "0.25",
             "--pitch-feedback-delay-pitch-semitones", "7",
             "--feedback-generator", "--feedback-generator-type", "unison",
             "--feedback-generator-gain", "0.35",
             "--feedback-generator-attack-ms", "15",
             "--feedback-generator-release-ms", "600",
             "--feedback-generator-dry-kill", "off",
             "--feedback-generator-dry-level", "1",
             "--feedback-generator-silence-threshold-db", "-48",
              "--shimmer-reverb", "--shimmer-reverb-decay-seconds", "3.4",
              "--shimmer-reverb-damping-hz", "7200",
              "--shimmer-reverb-diffusion", "0.72",
              "--shimmer-reverb-feedback", "0.35",
              "--shimmer-reverb-high-pass-hz", "120",
              "--shimmer-reverb-pitch-semitones", "7",
              "--shimmer-reverb-intensity", "0.65",
              "--shimmer-reverb-mix", "0.3",
              "--shimmer-reverb-level-db", "-3",
              "--convolution-ir", str(convolution_ir_path),
              "--convolution-reverb-mix", "0.35",
              "--convolution-reverb-level-db", "-3",
               "--convolution-reverb-high-pass-hz", "96.25",
               "--convolution-reverb-low-pass-hz", "8650",
              "--five-band-eq-band1-frequency-hz", "100",
              "--five-band-eq-band1-gain-db", "1.5",
              "--five-band-eq-band1-q", "0.7",
              "--five-band-eq-band1-shape", "1",
              "--five-band-eq-band1-slope-db-per-octave", "12",
              "--five-band-eq-band2-frequency-hz", "350",
              "--five-band-eq-band2-gain-db", "-0.5",
              "--five-band-eq-band2-q", "1",
              "--five-band-eq-band3-frequency-hz", "1000",
              "--five-band-eq-band3-gain-db", "1",
              "--five-band-eq-band3-q", "0.7",
              "--five-band-eq-band4-frequency-hz", "3500",
              "--five-band-eq-band4-gain-db", "-0.5",
              "--five-band-eq-band4-q", "0.8",
              "--five-band-eq-band5-frequency-hz", "8000",
              "--five-band-eq-band5-gain-db", "0.5",
              "--five-band-eq-band5-q", "0.7",
              "--five-band-eq-band5-shape", "4",
              "--five-band-eq-band5-slope-db-per-octave", "24",
              "--stereo-crossover-frequency-hz", "500",
              "--stereo-crossover-frequency-multiplier", "x1",
              "--stereo-crossover-low-level-left-db", "0",
              "--stereo-crossover-low-level-right-db", "0",
              "--stereo-crossover-high-level-left-db", "0",
              "--stereo-crossover-high-level-right-db", "0",
              "--stereo-crossover-low-pan-left", "-1",
              "--stereo-crossover-high-pan-left", "-1",
              "--stereo-crossover-low-pan-right", "1",
              "--stereo-crossover-high-pan-right", "1",
              "--stereo-crossover-level-db", "0",
              "--stereo-crossover-balance", "0",
              "--double-take", "--double-take-doubles", "3",
              "--double-take-slop", "0.65", "--double-take-sensitivity", "0.55",
              "--double-take-dry-level", "0.75", "--double-take-wet-level", "0.45",
              "--double-take-source", "mono",
              "--eq5-graphic",
              "--eq5-band-1-db", "2", "--eq5-band-2-db", "-3",
              "--eq5-band-3-db", "6", "--eq5-band-4-db", "-3",
              "--eq5-band-5-db", "2", "--eq5-overall-gain-db", "0",
              "--optical-tremolo",
              "--optical-tremolo-variant", "optical-2",
              "--optical-tremolo-rate-hz", "5.2",
              "--optical-tremolo-depth", "0.65",
              "--optical-tremolo-lfo", "saw-down",
              "--optical-tremolo-tempo-sync", "off",
              "--optical-tremolo-tempo-bpm", "120",
              "--optical-tremolo-beats-per-cycle", "1",
              "--optical-tremolo-duty-percent", "35",
              "--optical-tremolo-shape-percent", "58",
              "--optical-tremolo-phase-degrees", "18",
              "--optical-tremolo-start-phase-degrees", "10",
              "--optical-tremolo-trigger", "off",
              "--optical-tremolo-trigger-threshold-dbfs", "-36",
              "--optical-tremolo-ducking-percent", "25",
              "--optical-tremolo-mix", "0.75",
              "--optical-tremolo-level-db", "-1",
              "--optical-tremolo-balance-percent", "0",
              "--fractal-bias-trem",
              "--fractal-bias-trem-rate-hz", "3.7",
              "--fractal-bias-trem-depth", "0.75",
              "--fractal-bias-trem-waveform", "triangle",
              "--fractal-bias-trem-tempo-sync", "off",
              "--fractal-bias-trem-tempo-bpm", "120",
              "--fractal-bias-trem-beats-per-cycle", "1",
              "--fractal-bias-trem-trigger-sync", "off",
              "--fractal-bias-trem-trigger-threshold-db", "-36",
              "--fractal-bias-trem-ducking", "0",
              "--fractal-bias-trem-bias-drive", "0.7",
              "--fractal-bias-trem-mix", "1",
              "--fractal-bias-trem-level-db", "0",
              "--fractal-bias-trem-balance", "0",
              "--fractal-bias-trem-bypass", "off",
              "--vibratone",
             "--fractal-diatonic-harmonizer",
        ]
        run(renderer_command, cwd=ROOT)
        if stadium_delay_trails_only:
            run([
                str(native_renderer), "render", str(input_path),
                str(delay_trails_dry_reference_path),
                "--gain-db", "2", "--output-gain-db", "-1",
                "--volume-position", "0.92", "--volume-min-db", "-96",
                "--volume-max-db", "0", "--volume-curve", "audio",
                "--gate-threshold-db", "-60", "--gate-attenuation-db", "-72",
                "--gate-hysteresis-db", "3", "--gate-attack-ms", "2",
                "--gate-hold-ms", "20", "--gate-release-ms", "80",
                "--limiter-ceiling-db", "-1", "--limiter-release-ms", "80",
            ], cwd=ROOT)
        stereo_crossover_command = [
            str(native_renderer), "render", str(input_path),
            str(stereo_crossover_reference_path),
            "--stereo-crossover-frequency-hz", "500",
            "--stereo-crossover-frequency-multiplier", "x1",
            "--stereo-crossover-low-level-left-db", "-60",
            "--stereo-crossover-low-level-right-db", "-60",
            "--stereo-crossover-high-level-left-db", "0",
            "--stereo-crossover-high-level-right-db", "0",
            "--stereo-crossover-low-pan-left", "-1",
            "--stereo-crossover-high-pan-left", "-1",
            "--stereo-crossover-low-pan-right", "1",
            "--stereo-crossover-high-pan-right", "1",
            "--stereo-crossover-level-db", "0",
            "--stereo-crossover-balance", "0",
        ]
        run(stereo_crossover_command, cwd=ROOT)
        stereo_tape_echo_command = [
            str(native_renderer), "render", str(stereo_tape_echo_input_path),
            str(stereo_tape_echo_reference_path),
            "--tape-echo-time-left-ms", "10",
            "--tape-echo-time-right-ms", "20",
            "--tape-echo-feedback", "0", "--tape-echo-mix", "1",
            "--tape-echo-record-level", "0", "--tape-echo-bias", "0",
            "--tape-echo-noise", "0", "--tape-echo-wow-flutter", "0",
        ]
        run(stereo_tape_echo_command, cwd=ROOT)
        plex_delay_input_select_command = [
            str(native_renderer), "render", str(plex_delay_input_select_input_path),
            str(plex_delay_input_select_reference_path),
            "--plex-delay", "--plex-delay-lines", "1",
            "--plex-delay-input-select", "right", "--plex-delay-mix", "1",
        ]
        run(plex_delay_input_select_command, cwd=ROOT)
        LocalWebHandler.test_files = {
            "/__test_input.wav": input_path,
            "/__test_expected.wav": reference_path,
            "/__test_stereo_crossover_expected.wav": stereo_crossover_reference_path,
            "/__test_stereo_tape_echo_input.wav": stereo_tape_echo_input_path,
            "/__test_stereo_tape_echo_expected.wav": stereo_tape_echo_reference_path,
            "/__test_plex_delay_input_select_input.wav": plex_delay_input_select_input_path,
            "/__test_plex_delay_input_select_expected.wav": plex_delay_input_select_reference_path,
            "/__test_tuner.wav": tuner_path,
            "/__test_convolution_ir.wav": convolution_ir_path,
            "/__test_cabinet_ir.wav": cabinet_ir_path,
            "/__test_convolution_ir_10s.wav": long_convolution_ir_path,
        }
        if stadium_delay_trails_only:
            LocalWebHandler.test_files["/__test_delay_trails_dry.wav"] = delay_trails_dry_reference_path
        server = ThreadingHTTPServer(("127.0.0.1", 0), LocalWebHandler)
        server.daemon_threads = True
        server_thread = threading.Thread(target=server.serve_forever, daemon=True)
        server_thread.start()
        user_data = temp_dir / "chrome-profile"
        base_url = f"http://127.0.0.1:{server.server_port}"
        for route in ("/web/audio-worklet.js", "/build/web/tabletfx_wasm.js"):
            with urllib.request.urlopen(base_url + route, timeout=5) as response:
                print(f"Served {route}: HTTP {response.status}, {response.headers.get('Content-Type')}")
        with urllib.request.urlopen(base_url + "/web/worklet-smoke.js", timeout=5) as response:
            print(f"Served /web/worklet-smoke.js: HTTP {response.status}, {response.headers.get('Content-Type')}")
        LocalWebHandler.request_paths = []
        if stadium_delay_trails_only:
            url = base_url + "/web/index.html?selftest=stadium-delay-trails"
        elif formant_control_lfo_only:
            url = base_url + "/web/index.html?selftest=formant-control-lfo"
        elif volume_pedal_only:
            url = base_url + "/web/index.html?selftest=volume-pedal"
        elif step_tremolo_only:
            url = base_url + "/web/index.html?selftest=step-tremolo"
        elif fractal_bias_trem_only:
            url = base_url + "/web/index.html?selftest=fractal-bias-trem"
        elif vibratone_only:
            url = base_url + "/web/index.html?selftest=fender-vibratone"
        elif uni_verse_vibe_only:
            url = base_url + "/web/index.html?selftest=uni-verse-vibe"
        elif four_osc_only:
            url = base_url + "/web/index.html?selftest=four-osc-generator"
        elif poly_detune_only:
            url = base_url + "/web/index.html?selftest=poly-detune"
        elif fractal_poly_pitch_only:
            url = base_url + "/web/index.html?selftest=fractal-poly-pitch-range"
        elif headrush_drop_tune_only:
            url = base_url + "/web/index.html?selftest=headrush-drop-tune"
        elif fender_stereo_flanger_phase_only:
            url = base_url + "/web/index.html?selftest=fender-stereo-flanger-phase"
        elif fender_flanger_tap_division_only:
            url = base_url + "/web/index.html?selftest=fender-flanger-tap-division"
        elif fender_phaser_90_tap_division_only:
            url = base_url + "/web/index.html?selftest=fender-phaser-90-tap-division"
        elif fractal_chorus_tempo_only:
            url = base_url + "/web/index.html?selftest=fractal-chorus-tempo"
        elif fender_tremolo_lfo_clip_only:
            url = base_url + "/web/index.html?selftest=fender-tremolo-lfo-clip"
        elif fender_tremolo_duty_cycle_only:
            url = base_url + "/web/index.html?selftest=fender-tremolo-duty-cycle"
        elif fractal_tzf_auto_only:
            url = base_url + "/web/index.html?selftest=fractal-tzf-auto"
        elif fender_pedal_shifter_only:
            url = base_url + "/web/index.html?selftest=fender-pedal-shifter"
        elif fender_pedal_detune_only:
            url = base_url + "/web/index.html?selftest=fender-pedal-detune"
        elif fractal_diatonic_custom_scale_only:
            url = base_url + "/web/index.html?selftest=fractal-diatonic-custom-scale"
        elif fractal_chromatic_harmonizer_only:
            url = base_url + "/web/index.html?selftest=fractal-chromatic-harmonizer"
        elif fractal_diatonic_harmonizer_only:
            url = base_url + "/web/index.html?selftest=fractal-diatonic-harmonizer"
        elif fractal_resonator_input_gain_only:
            url = base_url + "/web/index.html?selftest=fractal-resonator-input-gain"
        elif stereo_panner_only:
            url = base_url + "/web/index.html?selftest=stereo-panner"
        elif mutant_filter_only:
            url = base_url + "/web/index.html?selftest=mutant-filter"
        elif stereo_filter_only:
            url = base_url + "/web/index.html?selftest=stereo-filter"
        elif volume_pan_only:
            url = base_url + "/web/index.html?selftest=volume-pan"
        else:
            url = base_url + "/web/index.html?selftest=1"
        stereo_tape_echo_expression = (
            "(async()=>{"
            "const assert=(condition,message)=>{if(!condition)throw new Error(message)};"
            "const row=document.querySelector('[data-effect=\"84\"]')?.closest('.effect-row');"
            "assert(row?.dataset.category==='delay'&&row.querySelector('.effect-category-tag')?.textContent.trim()==='Delay',"
            "'Tape Echo picker entry is not categorized as Delay');"
            "const independent=document.querySelector('#tape-echo-independent-times');"
            "const shared=document.querySelector('#tape-echo-shared-time-controls');"
            "const split=document.querySelector('#tape-echo-independent-time-controls');"
            "const left=document.querySelector('#tape-echo-time-left-ms');"
            "const right=document.querySelector('#tape-echo-time-right-ms');"
            "assert(independent&&shared&&split&&left&&right,'Independent Tape Echo UI controls are missing');"
            "const posted=[],arpeggiatorPosted=[];const portPrototype=MessagePort.prototype;"
            "const originalPost=portPrototype.postMessage;"
            "portPrototype.postMessage=function(message,...transfer){"
            "if(message?.type==='pitch-arpeggiator')arpeggiatorPosted.push(message.value);"
            "if(message?.type==='tape-echo')posted.push(message.value);"
            "return originalPost.call(this,message,...transfer)};"
            "try{const arpShift=document.querySelector('#pitch-arpeggiator-step-1');"
            "assert(arpShift?.min==='-24'&&arpShift?.max==='24',"
            "'Pitch Arpeggiator UI does not expose the documented ±24-degree step range');"
            "arpShift.value='24';arpShift.dispatchEvent(new Event('input',{bubbles:true}));"
            "assert(arpeggiatorPosted.at(-1)?.[14]===24&&"
            "arpShift.parentElement.querySelector('output').value==='+24 degrees',"
            "'Pitch Arpeggiator UI did not send +24 degrees to the AudioWorklet');"
            "independent.checked=true;independent.dispatchEvent(new Event('change',{bubbles:true}));"
            "left.value='10';left.dispatchEvent(new Event('input',{bubbles:true}));"
            "right.value='20';right.dispatchEvent(new Event('input',{bubbles:true}));"
            "assert(shared.hidden&& !split.hidden,'Independent L/R controls did not become visible');"
            "assert(left.parentElement.querySelector('output').value==='10 ms'&&"
            "right.parentElement.querySelector('output').value==='20 ms','L/R control readouts are incorrect');"
            "const message=posted.at(-1);assert(message?.length===16&&message[11]===1&&"
            "message[12]===10&&message[13]===20,'UI did not send the independent L/R values to the AudioWorklet');"
            "}finally{portPrototype.postMessage=originalPost}"
            "const createModule=(await import('/build/web/tabletfx_wasm.js')).default;"
            "const module=await createModule();const sampleRate=8000;"
            "const inputResponse=await fetch('/__test_stereo_tape_echo_input.wav');"
            "const inputBytes=await inputResponse.arrayBuffer();const inputView=new DataView(inputBytes);"
            "assert(inputView.getUint16(22,true)===2&&inputView.getUint16(34,true)===16,"
            "'Stereo impulse fixture is not PCM16');const frames=(inputBytes.byteLength-44)/4;"
            "let chain=module._tf_chain_create(sampleRate,2,frames);assert(chain,'Could not create WASM chain');"
            "try{const inputOffset=module._tf_chain_input_buffer(chain)>>>2;"
            "for(let frame=0;frame<frames;frame++){for(let channel=0;channel<2;channel++){"
            "module.HEAPF32[inputOffset+frame*2+channel]=inputView.getInt16(44+(frame*2+channel)*2,true)/32768}}"
            "module._tf_chain_set_tape_echo(chain,0,350,120,4,0,1,0,0,0,0,0,1,10,20,4,4);"
            "assert(module._tf_chain_set_enabled(chain,84,1)===1,'WASM did not enable Tape Echo at index 84');"
            "assert(module._tf_chain_begin(chain,frames)===frames,'WASM chain rejected Tape Echo ramp warmup');"
            "module._tf_chain_process_effect(chain,84);module._tf_chain_reset(chain);"
            "assert(module._tf_chain_begin(chain,frames)===frames,'WASM chain rejected impulse frame count');"
            "module._tf_chain_process_effect(chain,84);module._tf_chain_finish(chain,frames);"
            "module._tf_chain_process_output_gain(chain);module._tf_chain_process_output_limiter(chain);"
            "const outputOffset=module._tf_chain_output_buffer(chain)>>>2;"
            "const actual=module.HEAPF32.slice(outputOffset,outputOffset+frames*2);"
            "assert(Math.abs(actual[80*2])>0.1&&Math.abs(actual[80*2+1])<1e-6,"
            "'WASM left channel did not echo independently at 10 ms');"
            "assert(Math.abs(actual[160*2])<1e-6&&Math.abs(actual[160*2+1])>0.05,"
            "'WASM right channel did not echo independently at 20 ms');"
            "const expectedResponse=await fetch('/__test_stereo_tape_echo_expected.wav');"
            "const expectedBytes=await expectedResponse.arrayBuffer();const expectedView=new DataView(expectedBytes);"
            "assert(expectedView.getUint16(22,true)===2&&expectedView.getUint16(34,true)===32,"
            "'Native Tape Echo reference is not stereo float32');"
            "const expected=new Float32Array(frames*2);for(let i=0;i<expected.length;i++)"
            "expected[i]=expectedView.getFloat32(44+i*4,true);let maxError=0,errorSquares=0;"
            "let firstMismatch=-1;for(let i=0;i<actual.length;i++){const error=actual[i]-expected[i];"
            "maxError=Math.max(maxError,Math.abs(error));errorSquares+=error*error;"
            "if(firstMismatch<0&&Math.abs(error)>1e-4)firstMismatch=i}"
            "const rmsError=Math.sqrt(errorSquares/actual.length);"
            "const tapSamples={wasmFrame0:[actual[0],actual[1]],nativeFrame0:[expected[0],expected[1]],"
            "wasmFrame1:[actual[2],actual[3]],nativeFrame1:[expected[2],expected[3]],"
            "wasmFrame80:[actual[160],actual[161]],nativeFrame80:[expected[160],expected[161]],"
            "wasmFrame160:[actual[320],actual[321]],nativeFrame160:[expected[320],expected[321]],firstMismatch};"
            "assert(maxError<4e-5&&rmsError<5e-6,`WASM/native Stereo Tape Echo mismatch: max ${maxError}, RMS ${rmsError}; samples ${JSON.stringify(tapSamples)}`);"
            "const plexInputSelectControl=document.querySelector('#plex-delay-input-select');"
            "assert(plexInputSelectControl?.options.length===3,'Plex Delay input selector is missing');"
            "{const previous=plexInputSelectControl.value;const messages=[];"
            "portPrototype.postMessage=function(message,...transfer){"
            "if(message?.type==='plex-delay')messages.push(message.value);"
            "return originalPost.call(this,message,...transfer)};"
            "try{plexInputSelectControl.value='2';"
            "plexInputSelectControl.dispatchEvent(new Event('change',{bubbles:true}));"
            "assert(messages.at(-1)?.inputSelect===2&&"
            "plexInputSelectControl.parentElement.querySelector('output').value==='Right',"
            "'Plex Delay input selector did not send the Right-only setting');"
            "}finally{plexInputSelectControl.value=previous;"
            "plexInputSelectControl.dispatchEvent(new Event('change',{bubbles:true}));"
            "portPrototype.postMessage=originalPost}}"
            "const plexInputResponse=await fetch('/__test_plex_delay_input_select_input.wav');"
            "const plexInputBytes=await plexInputResponse.arrayBuffer();"
            "const plexInputView=new DataView(plexInputBytes);"
            "assert(plexInputView.getUint16(22,true)===2&&plexInputView.getUint16(34,true)===16,"
            "'Plex Delay stereo impulse fixture is not PCM16');"
            "const plexFrames=(plexInputBytes.byteLength-44)/4;"
            "assert(plexFrames===frames,'Plex Delay fixture must fit the reused stereo chain');"
            "const plexChain=chain;"
            "let plexMaxError=0,plexErrorSquares=0;"
            "const plexInputOffset=module._tf_chain_input_buffer(plexChain)>>>2;"
            "for(let frame=0;frame<plexFrames;frame++){for(let channel=0;channel<2;channel++){"
            "module.HEAPF32[plexInputOffset+frame*2+channel]="
            "plexInputView.getInt16(44+(frame*2+channel)*2,true)/32768}}"
            "module._tf_chain_set_plex_delay_global(plexChain,1,120,1,1,1,3,0,20,20000,0,0,0,1,0,0,2,0,10,0);"
            "assert(module._tf_chain_set_enabled(plexChain,76,1)===1,"
            "'WASM did not enable Plex Matrix Delay at index 76');"
            "assert(module._tf_chain_begin(plexChain,plexFrames)===plexFrames,"
            "'WASM Plex Delay chain rejected stereo input');"
            "module._tf_chain_process_effect(plexChain,76);"
            "module._tf_chain_finish(plexChain,plexFrames);"
            "module._tf_chain_process_output_gain(plexChain);"
            "module._tf_chain_process_output_limiter(plexChain);"
            "const plexOutputOffset=module._tf_chain_output_buffer(plexChain)>>>2;"
            "const plexActual=module.HEAPF32.slice(plexOutputOffset,plexOutputOffset+plexFrames*2);"
            "const plexExpectedResponse=await fetch('/__test_plex_delay_input_select_expected.wav');"
            "const plexExpectedBytes=await plexExpectedResponse.arrayBuffer();"
            "const plexExpectedView=new DataView(plexExpectedBytes);"
            "assert(plexExpectedView.getUint16(22,true)===2&&plexExpectedView.getUint16(34,true)===32,"
            "'Native Plex Delay reference is not stereo float32');"
            "const plexExpected=new Float32Array(plexFrames*2);"
            "for(let i=0;i<plexExpected.length;i++){plexExpected[i]=plexExpectedView.getFloat32(44+i*4,true);"
            "const error=plexActual[i]-plexExpected[i];plexMaxError=Math.max(plexMaxError,Math.abs(error));"
            "plexErrorSquares+=error*error}"
            "const plexRmsError=Math.sqrt(plexErrorSquares/plexActual.length);"
            "assert(plexMaxError<4e-5&&plexRmsError<5e-6,"
            "`WASM/native Plex Delay Right-input mismatch: max ${plexMaxError}, RMS ${plexRmsError}`);"
            "module._tf_chain_destroy(chain);chain=0;"
            ""
            "const cverbRow=document.querySelector('[data-effect=\"58\"]')?.closest('.effect-row');"
            "assert(cverbRow?.dataset.category==='reverb'&&cverbRow.querySelector('.effect-category-tag')?.textContent.trim()==='Reverb',"
            "'Convolution Reverb is not categorized as Reverb');"
            "assert(document.querySelector('#convolution-ir-file')?.closest('.file-picker')?.textContent.includes('480,000 frames'),"
            "'Convolution Reverb UI does not advertise its 480,000-frame IR limit');"
            "const cverbWorkletSource=await (await fetch('/web/audio-worklet.js')).text();"
            "assert(cverbWorkletSource.includes('frames > 480000'),"
            "'AudioWorklet does not accept the documented 10-second IR limit');"
            "const cverbResponse=await fetch('/__test_convolution_ir_10s.wav');"
            "const cverbBytes=await cverbResponse.arrayBuffer();const cverbView=new DataView(cverbBytes);"
            "const cverbFrames=(cverbBytes.byteLength-44)/4;"
            "assert(cverbResponse.ok&&cverbView.getUint16(22,true)===2&&"
            "cverbView.getUint16(34,true)===16&&cverbView.getUint32(24,true)===48000&&"
            "cverbFrames===480000,'Long C-Verb test fixture is not 10-second stereo PCM16 at 48 kHz');"
            "const cverbChain=module._tf_chain_create(48000,2,2048);"
            "assert(cverbChain,'Could not create the 10-second C-Verb WASM chain');"
            "let cverbPointer=module._malloc(cverbFrames*2*4);"
            "assert(cverbPointer,'Could not allocate the C-Verb IR upload buffer');"
            "let cverbLeft=0,cverbRight=0,cverbWasmBytes=0;"
            "try{const cverbIrOffset=cverbPointer>>>2;"
            "for(let i=0;i<cverbFrames;i++){module.HEAPF32[cverbIrOffset+i*2]=cverbView.getInt16(44+i*4,true)/32768;"
            "module.HEAPF32[cverbIrOffset+i*2+1]=cverbView.getInt16(46+i*4,true)/32768}"
            "assert(module._tf_chain_load_convolution_reverb_ir(cverbChain,cverbPointer,cverbFrames,2,48000)===1,"
            "'WASM rejected the 10-second stereo 48 kHz C-Verb IR');"
            "module._free(cverbPointer);cverbPointer=0;"
            "module._tf_chain_set_convolution_reverb(cverbChain,1,0,0,0,0);"
            "assert(module._tf_chain_set_enabled(cverbChain,58,1)===1,"
            "'WASM did not enable Convolution Reverb at index 58');"
            "const cverbInputOffset=module._tf_chain_input_buffer(cverbChain)>>>2;"
            "module.HEAPF32[cverbInputOffset]=0.5;module.HEAPF32[cverbInputOffset+1]=0.25;"
            "assert(module._tf_chain_begin(cverbChain,2048)===2048,"
            "'WASM C-Verb chain rejected stereo input');"
            "module._tf_chain_process_effect(cverbChain,58);"
            "module._tf_chain_finish(cverbChain,2048);"
            "module._tf_chain_process_output_gain(cverbChain);"
            "module._tf_chain_process_output_limiter(cverbChain);"
            "const cverbOutputOffset=module._tf_chain_output_buffer(cverbChain)>>>2;"
            "const cverbOutput=module.HEAPF32.slice(cverbOutputOffset,cverbOutputOffset+4096);"
            "for(let i=0;i<1024*2;i++){assert(Math.abs(cverbOutput[i])<2e-5,"
            "'10-second C-Verb output started before its fixed partition latency')}"
            "cverbLeft=cverbOutput[1024*2];cverbRight=cverbOutput[1024*2+1];"
            "assert(Math.abs(cverbLeft-0.25)<3e-4&&Math.abs(cverbRight-0.125)<3e-4,"
            "`10-second C-Verb WASM output/latency mismatch: L ${cverbLeft}, R ${cverbRight}`);"
            "const replacementFrames=2048;let replacementPointer=module._malloc(replacementFrames*2*4);"
            "assert(replacementPointer,'Could not allocate short C-Verb replacement IR upload buffer');"
            "try{const replacementOffset=replacementPointer>>>2;"
            "module.HEAPF32.fill(0,replacementOffset,replacementOffset+replacementFrames*2);"
            "module.HEAPF32[replacementOffset]=0.5;module.HEAPF32[replacementOffset+1]=0.25;"
            "assert(module._tf_chain_load_convolution_reverb_ir(cverbChain,replacementPointer,replacementFrames,2,48000)===1,"
            "'WASM could not replace the loaded 10-second C-Verb IR with a short stereo test IR');"
            "}finally{module._free(replacementPointer)}"
            "cverbWasmBytes=module.HEAPF32.buffer.byteLength;"
            "assert(cverbWasmBytes<=96*1024*1024,"
            "`10-second stereo C-Verb exceeded the 96 MiB WASM heap: ${cverbWasmBytes}`);"
            "}finally{if(cverbPointer)module._free(cverbPointer);module._tf_chain_destroy(cverbChain)}"
            "return JSON.stringify({passed:true,category:'Delay',effectIndex:84,leftTimeMs:10,rightTimeMs:20,"
            "leftEcho:actual[160],rightEcho:actual[321],maxError,rmsError,frames,tapSamples,"
            "plexDelayInputSelect:{category:'Delay',effectIndex:76,selection:'right',"
            "maxError:plexMaxError,rmsError:Math.sqrt(plexErrorSquares/(plexFrames*2))},"
            "cverb:{category:'Reverb',effectIndex:58,irFrames:cverbFrames,irSeconds:cverbFrames/48000,"
            "latencyFrames:1024,outputLeft:cverbLeft,outputRight:cverbRight,wasmBytes:cverbWasmBytes}});"
            "}finally{if(chain)module._tf_chain_destroy(chain)}})()"
        )
        try:
            result, stderr = run_chrome_page(
                chrome, user_data, url, temp_dir / "chrome-stderr.log",
                None if volume_pan_only or stereo_filter_only or stereo_panner_only or
                step_tremolo_only or
                fractal_bias_trem_only or vibratone_only or uni_verse_vibe_only
                or four_osc_only or poly_detune_only or fractal_poly_pitch_only
                or fender_pedal_shifter_only or fender_pedal_detune_only
                or fractal_diatonic_custom_scale_only
                or fractal_resonator_input_gain_only
                else stereo_tape_echo_expression,
            )
        finally:
            server.shutdown()
            server.server_close()
            server_thread.join(timeout=2)
            LocalWebHandler.test_files = {}
        if result.get("status") != "passed":
            detail = result.get("text", "")
            requests = ", ".join(LocalWebHandler.request_paths)
            raise SystemExit(
                "Headless browser test failed: "
                + (detail or f"status={result.get('status', 'missing')}")
                + f"\nBrowser GET paths: {requests or '(none)'}"
                + ("\n" + stderr if stderr else "")
            )
        report = json.loads(result.get("text", "{}"))
        if volume_pan_only:
            print("Focused Volume/Pan browser/WASM Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif stereo_panner_only:
            print("Focused Stereo Panner browser/WASM prerecorded-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif stereo_filter_only:
            print("Focused Stereo Filter browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif step_tremolo_only:
            print("Focused Step Tremolo browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fractal_bias_trem_only:
            print("Focused Bias Trem browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif vibratone_only:
            print("Focused Vibratone browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif uni_verse_vibe_only:
            print("Focused Uni-Verse Vibe browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif four_osc_only:
            print("Focused Line 6 4 OSC Generator browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif poly_detune_only:
            print("Focused Line 6 Poly Detune browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fractal_poly_pitch_only:
            print("Focused Fractal-family poly-pitch ±24 browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fender_pedal_shifter_only:
            print("Focused Fender-inspired Pedal Shifter browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fender_pedal_detune_only:
            print("Focused Fender-inspired Pedal Detune browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fractal_diatonic_custom_scale_only:
            print("Focused Fractal-family custom-scale browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif volume_pedal_only:
            print("Focused Volume Pedal browser/WASM local-guitar mute and toe playback smoke passed.")
            print(json.dumps(report, indent=2))
        elif formant_control_lfo_only:
            print("Focused Formant Control LFO browser/WASM local-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        elif fractal_resonator_input_gain_only:
            print("Focused Fractal-family Resonator Input Gain browser/WASM prerecorded-guitar Play smoke passed.")
            print(json.dumps(report, indent=2))
        else:
            print("Browser/native chain comparison passed.")
            print(json.dumps(report, indent=2))
            print("Stereo Tape Echo and C-Verb browser/WASM checks passed.")
            print(json.dumps(result["stereoTapeEcho"], indent=2))


def inspect_web_build_path() -> None:
    path = ROOT / "scripts" / "web.py"
    lines = path.read_text(encoding="utf-8").splitlines()
    tokens = ("stereo_filter", "exported_functions", "source_files", "sources =", "source =")
    matched = [index for index, line in enumerate(lines)
               if any(token in line.lower() for token in tokens)]
    print(f"===== {path.relative_to(ROOT)} ({len(matched)} matching lines) =====")
    windows: list[tuple[int, int]] = []
    for index in matched:
        start, end = max(0, index - 3), min(len(lines), index + 4)
        if windows and start <= windows[-1][1]:
            windows[-1] = (windows[-1][0], max(windows[-1][1], end))
        else:
            windows.append((start, end))
    for start, end in windows[:20]:
        print(f"--- lines {start + 1}-{end} ---")
        for index in range(start, end):
            print(f"{index + 1:4}: {lines[index]}")


def inspect_web_smoke_path() -> None:
    path = ROOT / "web" / "app.js"
    lines = path.read_text(encoding="utf-8").splitlines()
    tokens = ("selftest", "runVolumePanBrowserSmokeTest", "runStereoFilterBrowserSmokeTest",
              "focusedReport", "volumePanSmoke", "stepTremoloSmoke", "__tabletFx")
    matched = [index for index, line in enumerate(lines)
               if any(token.lower() in line.lower() for token in tokens)]
    print(f"===== {path.relative_to(ROOT)} ({len(matched)} matching lines) =====")
    windows: list[tuple[int, int]] = []
    for index in matched:
        start, end = max(0, index - 4), min(len(lines), index + 5)
        if windows and start <= windows[-1][1]:
            windows[-1] = (windows[-1][0], max(windows[-1][1], end))
        else:
            windows.append((start, end))
    for start, end in windows[:30]:
        print(f"--- lines {start + 1}-{end} ---")
        for index in range(start, end):
            print(f"{index + 1:4}: {lines[index]}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=(
        "bootstrap-toolchain", "verify-toolchain", "build", "serve", "test",
        "test-volume-pan", "test-volume-pedal", "test-stereo-filter", "test-stereo-panner",
        "test-stadium-delay-trails",
        "test-formant-control-lfo",
        "test-headrush-drop-tune",
        "test-fender-stereo-flanger-phase",
        "test-fender-flanger-tap-division",
        "test-fender-phaser-90-tap-division",
        "test-fractal-chorus-tempo",
        "test-fender-tremolo-lfo-clip",
        "test-fender-tremolo-duty-cycle",
        "test-fractal-tzf-auto",
        "test-mutant-filter", "test-step-tremolo",
        "test-fractal-bias-trem",
        "test-fender-vibratone",
        "test-fender-universe-vibe",
        "test-line6-four-osc-generator",
        "test-line6-poly-detune",
        "test-fractal-poly-pitch-range",
        "test-fender-pedal-shifter",
        "test-fender-pedal-detune",
        "test-fractal-diatonic-custom-scale",
        "test-fractal-chromatic-harmonizer",
        "test-fractal-diatonic-harmonizer",
        "test-fractal-resonator-input-gain",
        "inspect-build-path", "inspect-smoke-path",
    ))
    action = parser.parse_args().action
    if action == "bootstrap-toolchain":
        bootstrap_toolchain()
    elif action == "verify-toolchain":
        verify_toolchain()
    elif action == "serve":
        serve_web()
    elif action == "test":
        browser_test()
    elif action == "test-volume-pedal":
        browser_test(volume_pedal_only=True)
    elif action == "test-stadium-delay-trails":
        browser_test(stadium_delay_trails_only=True)
    elif action == "test-formant-control-lfo":
        browser_test(formant_control_lfo_only=True)
    elif action == "test-headrush-drop-tune":
        browser_test(headrush_drop_tune_only=True)
    elif action == "test-fender-stereo-flanger-phase":
        browser_test(fender_stereo_flanger_phase_only=True)
    elif action == "test-fender-flanger-tap-division":
        browser_test(fender_flanger_tap_division_only=True)
    elif action == "test-fender-phaser-90-tap-division":
        browser_test(fender_phaser_90_tap_division_only=True)
    elif action == "test-fractal-chorus-tempo":
        browser_test(fractal_chorus_tempo_only=True)
    elif action == "test-fender-tremolo-lfo-clip":
        browser_test(fender_tremolo_lfo_clip_only=True)
    elif action == "test-fender-tremolo-duty-cycle":
        browser_test(fender_tremolo_duty_cycle_only=True)
    elif action == "test-fractal-tzf-auto":
        browser_test(fractal_tzf_auto_only=True)
    elif action == "test-volume-pan":
        browser_test(volume_pan_only=True)
    elif action == "test-stereo-filter":
        browser_test(stereo_filter_only=True)
    elif action == "test-stereo-panner":
        browser_test(stereo_panner_only=True)
    elif action == "test-mutant-filter":
        browser_test(mutant_filter_only=True)
    elif action == "test-step-tremolo":
        browser_test(step_tremolo_only=True)
    elif action == "test-fractal-bias-trem":
        browser_test(fractal_bias_trem_only=True)
    elif action == "test-fender-vibratone":
        browser_test(vibratone_only=True)
    elif action == "test-fender-universe-vibe":
        browser_test(uni_verse_vibe_only=True)
    elif action == "test-line6-four-osc-generator":
        browser_test(four_osc_only=True)
    elif action == "test-line6-poly-detune":
        browser_test(poly_detune_only=True)
    elif action == "test-fractal-poly-pitch-range":
        browser_test(fractal_poly_pitch_only=True)
    elif action == "test-fender-pedal-shifter":
        browser_test(fender_pedal_shifter_only=True)
    elif action == "test-fender-pedal-detune":
        browser_test(fender_pedal_detune_only=True)
    elif action == "test-fractal-diatonic-custom-scale":
        browser_test(fractal_diatonic_custom_scale_only=True)
    elif action == "test-fractal-chromatic-harmonizer":
        browser_test(fractal_chromatic_harmonizer_only=True)
    elif action == "test-fractal-diatonic-harmonizer":
        browser_test(fractal_diatonic_harmonizer_only=True)
    elif action == "test-fractal-resonator-input-gain":
        browser_test(fractal_resonator_input_gain_only=True)
    elif action == "inspect-build-path":
        inspect_web_build_path()
    elif action == "inspect-smoke-path":
        inspect_web_smoke_path()
    else:
        build_web()


if __name__ == "__main__":
    main()
