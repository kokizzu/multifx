#!/usr/bin/env python3
"""Exercise the real offline render CLI with a deterministic local WAV fixture."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def write_pcm16_channels(path: Path, sample_rate: int, channels: int, samples: list[int]) -> None:
    payload = struct.pack("<" + "h" * len(samples), *samples)
    block_align = channels * 2
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + len(payload), b"WAVE", b"fmt ", 16,
        1, channels, sample_rate, sample_rate * block_align, block_align, 16,
        b"data", len(payload),
    )
    path.write_bytes(header + payload)


def write_pcm16(path: Path, sample_rate: int, samples: list[int]) -> None:
    write_pcm16_channels(path, sample_rate, 1, samples)


def write_pcm24(path: Path, sample_rate: int, payload: bytes) -> None:
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + len(payload), b"WAVE", b"fmt ", 16,
        1, 1, sample_rate, sample_rate * 3, 3, 24, b"data", len(payload),
    )
    path.write_bytes(header + payload)


def check_render(executable: Path) -> None:
    sample_rate = 48000
    input_samples = [-32768, -16384, 0, 16384, 32767]
    gain = 10.0 ** (-6.0 / 20.0)
    with tempfile.TemporaryDirectory(prefix="tabletfx-cli-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, sample_rate, input_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output), "--gain-db", "-6"],
            capture_output=True,
            text=True,
            check=True,
        )
        if "frames=5" not in result.stdout or "processors=1" not in result.stdout:
            raise AssertionError(f"Unexpected render summary: {result.stdout}")
        data = output.read_bytes()
        if len(data) != 44 + len(input_samples) * 4:
            raise AssertionError(f"Unexpected output WAV size: {len(data)}")
        header = struct.unpack("<4sI4s4sIHHIIHH4sI", data[:44])
        if header[5:8] != (3, 1, sample_rate) or header[10] != 32:
            raise AssertionError(f"Unexpected output WAV format: {header}")
        values = struct.unpack("<" + "f" * len(input_samples), data[44:])
        for index, (actual, source_sample) in enumerate(zip(values, input_samples, strict=True)):
            expected = source_sample / 32768.0 * gain
            if not math.isfinite(actual) or abs(actual - expected) > 1.0e-5:
                raise AssertionError(f"Sample {index}: expected {expected}, got {actual}")


def check_room_reverb(executable: Path) -> None:
    sample_rate = 8000
    frame_count = 4096
    impulse = [26214] + [0] * (frame_count - 1)
    guitar_like = [
        round(11000 * math.exp(-frame / 1400.0) * (
            math.sin(2.0 * math.pi * 110.0 * frame / sample_rate)
            + 0.32 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate)
            + 0.16 * math.sin(2.0 * math.pi * 330.0 * frame / sample_rate)
        ))
        for frame in range(frame_count)
    ]
    parameters = [
        "--room-size", "large",
        "--room-decay-s", "1.6",
        "--room-pre-delay-ms", "3",
        "--room-damping-hz", "1200",
        "--room-mix", "1",
        "--room-level-db", "-3",
    ]

    with tempfile.TemporaryDirectory(prefix="tabletfx-room-reverb-cli-test-") as directory:
        root = Path(directory)
        for label, source_samples in (("impulse", impulse), ("guitar", guitar_like)):
            source = root / f"{label}.wav"
            dry_output = root / f"{label}-dry.wav"
            wet_output = root / f"{label}-room.wav"
            write_pcm16(source, sample_rate, source_samples)

            subprocess.run(
                [str(executable), "render", str(source), str(dry_output)],
                capture_output=True, text=True, check=True,
            )
            result = subprocess.run(
                [str(executable), "render", str(source), str(wet_output), *parameters],
                capture_output=True, text=True, check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"Room reverb parameters did not enable one processor: {result.stdout}")

            dry_data = dry_output.read_bytes()
            wet_data = wet_output.read_bytes()
            dry = struct.unpack("<" + "f" * frame_count, dry_data[44:])
            wet = struct.unpack("<" + "f" * frame_count, wet_data[44:])
            if not all(math.isfinite(value) for value in wet):
                raise AssertionError(f"Room reverb produced non-finite {label} audio")
            difference = max(abs(a - b) for a, b in zip(dry, wet, strict=True))
            if difference < 1.0e-3:
                raise AssertionError(f"Room reverb did not change {label} audio")
            if label == "impulse" and not any(abs(value) > 1.0e-5 for value in wet[100:]):
                raise AssertionError("Room reverb impulse response did not produce an audible tail")

        switch_output = root / "room-switch.wav"
        switch_result = subprocess.run(
            [str(executable), "render", str(root / "impulse.wav"), str(switch_output),
             "--room-reverb"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in switch_result.stdout:
            raise AssertionError(f"--room-reverb did not enable one processor: {switch_result.stdout}")
        switch_values = struct.unpack("<" + "f" * frame_count, switch_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in switch_values):
            raise AssertionError("Room reverb enable switch produced non-finite audio")
        dry_values = struct.unpack("<" + "f" * frame_count, (root / "impulse-dry.wav").read_bytes()[44:])
        if max(abs(a - b) for a, b in zip(dry_values, switch_values, strict=True)) < 1.0e-3:
            raise AssertionError("--room-reverb did not change rendered impulse audio")


def check_hall_reverb(executable: Path) -> None:
    sample_rate = 8000
    frame_count = 4096
    impulse = [26214] + [0] * (frame_count - 1)
    with tempfile.TemporaryDirectory(prefix="tabletfx-hall-reverb-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        dry_output = root / "impulse-dry.wav"
        wet_output = root / "impulse-hall.wav"
        write_pcm16(source, sample_rate, impulse)
        subprocess.run(
            [str(executable), "render", str(source), str(dry_output)],
            capture_output=True, text=True, check=True,
        )
        result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output), "--hall-reverb",
             "--hall-decay-s", "4", "--hall-pre-delay-ms", "2",
             "--hall-damping-hz", "2200", "--hall-low-cut-hz", "80",
             "--hall-mix", "1", "--hall-level-db", "-3"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Hall reverb controls did not enable one processor: {result.stdout}")
        dry = struct.unpack("<" + "f" * frame_count, dry_output.read_bytes()[44:])
        wet = struct.unpack("<" + "f" * frame_count, wet_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in wet):
            raise AssertionError("Hall reverb produced non-finite output")
        if max(abs(a - b) for a, b in zip(dry, wet, strict=True)) < 1.0e-3:
            raise AssertionError("Hall reverb parameters did not change the rendered impulse")
        if not any(abs(value) > 1.0e-5 for value in wet[100:]):
            raise AssertionError("Hall reverb impulse response did not produce an audible tail")


def check_plate_reverb(executable: Path) -> None:
    sample_rate = 8000
    frame_count = 4096
    impulse = [26214] + [0] * (frame_count - 1)
    with tempfile.TemporaryDirectory(prefix="tabletfx-plate-reverb-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        dry_output = root / "impulse-dry.wav"
        wet_output = root / "impulse-plate.wav"
        write_pcm16(source, sample_rate, impulse)
        subprocess.run(
            [str(executable), "render", str(source), str(dry_output)],
            capture_output=True, text=True, check=True,
        )
        result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output), "--plate-reverb",
             "--plate-decay-s", "4", "--plate-pre-delay-ms", "2",
             "--plate-damping-hz", "2200", "--plate-diffusion", "0.7",
             "--plate-balance", "0.8", "--plate-mix", "1", "--plate-level-db", "-3"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Plate reverb controls did not enable one processor: {result.stdout}")
        dry = struct.unpack("<" + "f" * frame_count, dry_output.read_bytes()[44:])
        wet = struct.unpack("<" + "f" * frame_count, wet_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in wet):
            raise AssertionError("Plate reverb produced non-finite output")
        if max(abs(a - b) for a, b in zip(dry, wet, strict=True)) < 1.0e-3:
            raise AssertionError("Plate reverb parameters did not change the rendered impulse")
        if not any(abs(value) > 1.0e-5 for value in wet[100:]):
            raise AssertionError("Plate reverb impulse response did not produce an audible tail")


def check_spring_reverb(executable: Path) -> None:
    sample_rate = 8000
    frame_count = 4096
    impulse = [26214] + [0] * (frame_count - 1)
    with tempfile.TemporaryDirectory(prefix="tabletfx-spring-reverb-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        dry_output = root / "impulse-dry.wav"
        wet_output = root / "impulse-spring.wav"
        write_pcm16(source, sample_rate, impulse)
        subprocess.run(
            [str(executable), "render", str(source), str(dry_output)],
            capture_output=True, text=True, check=True,
        )
        result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output), "--spring-reverb",
             "--spring-decay-s", "4", "--spring-pre-delay-ms", "2",
             "--spring-damping-hz", "2200", "--spring-dwell", "0.7",
             "--spring-mix", "1", "--spring-level-db", "-3"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Spring reverb controls did not enable one processor: {result.stdout}")
        dry = struct.unpack("<" + "f" * frame_count, dry_output.read_bytes()[44:])
        wet = struct.unpack("<" + "f" * frame_count, wet_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in wet):
            raise AssertionError("Spring reverb produced non-finite output")
        if max(abs(a - b) for a, b in zip(dry, wet, strict=True)) < 1.0e-3:
            raise AssertionError("Spring reverb parameters did not change the rendered impulse")
        if not any(abs(value) > 1.0e-5 for value in wet[100:]):
            raise AssertionError("Spring reverb impulse response did not produce an audible tail")


def check_auto_swell(executable: Path) -> None:
    sample_rate = 8000
    frames_below_threshold = 128
    frames_above_threshold = 4096
    frames_release = 4096
    source_samples = (
        [655] * frames_below_threshold
        + [16384] * frames_above_threshold
        + [0] * frames_release
    )
    with tempfile.TemporaryDirectory(prefix="tabletfx-auto-swell-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-onset.wav"
        output = root / "guitar-swell.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output), "--auto-swell",
             "--auto-swell-threshold-db", "-30", "--auto-swell-attack-ms", "100",
             "--auto-swell-release-ms", "100", "--auto-swell-hysteresis-db", "6",
             "--auto-swell-taper", "linear", "--auto-swell-volume", "0.8"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Auto Swell controls did not enable one processor: {result.stdout}")
        values = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Auto Swell produced non-finite output")
        onset = values[frames_below_threshold]
        peak = max(abs(value) for value in values[
            frames_below_threshold:frames_below_threshold + frames_above_threshold])
        tail = values[-1]
        if abs(values[0]) > 1.0e-8 or not 0.0 < abs(onset) < 0.05:
            raise AssertionError("Auto Swell did not suppress sub-threshold input and the note onset")
        if not 0.38 <= peak <= 0.41:
            raise AssertionError(f"Auto Swell attack/volume controls produced unexpected peak {peak}")
        if abs(tail) > 1.0e-6:
            raise AssertionError(f"Auto Swell did not return to silence after release: {tail}")


def check_rotary_speaker(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
        for frame in range(2400)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-rotary-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar.wav"
        output = root / "rotary.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--rotary-speaker", "--rotary-rate-hz", "2.5",
             "--rotary-tempo-sync", "on", "--rotary-tempo-bpm", "90",
             "--rotary-beats-per-rotation", "0.5", "--rotary-speed", "fast",
             "--rotary-slow-speed-multiplier", "0.2", "--rotary-drive", "0.15",
             "--rotary-hi-depth", "0.8", "--rotary-high-level", "0.9",
             "--rotary-low-depth", "0.6", "--rotary-rotor-length", "0.6",
             "--rotary-low-rate-multi", "0.7", "--rotary-low-time-constant-ms", "700",
              "--rotary-hi-time-constant-ms", "400",
              "--rotary-low-mic-spacing", "0.65",
              "--rotary-high-mic-spacing", "0.8",
             "--rotary-mic-distance", "0.3", "--rotary-stereo-spread", "1.2",
             "--rotary-mix", "0.85", "--rotary-level", "0.95",
             "--rotary-balance", "0.1", "--rotary-input-select", "stereo"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Rotary controls did not enable one processor: {result.stdout}")
        values = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Rotary Speaker produced non-finite output")
        source_float = [value / 32768.0 for value in source_samples]
        if max(abs(value) for value in values) < 0.05:
            raise AssertionError("Rotary Speaker unexpectedly silenced the test tone")
        if max(abs(actual - expected) for actual, expected in zip(values, source_float, strict=True)) < 0.02:
            raise AssertionError("Rotary controls did not change the test tone")


def check_ring_modulator(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
        for frame in range(2400)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-ring-mod-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar.wav"
        output = root / "ring-mod.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output), "--ring-mod",
             "--ring-mod-frequency-hz", "700", "--ring-mod-multiplier", "1.5",
             "--ring-mod-high-cut-hz", "3000", "--ring-mod-mix", "0.75"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Ring Mod controls did not enable one processor: {result.stdout}")
        values = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        source_float = [value / 32768.0 for value in source_samples]
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Ring Mod produced non-finite output")
        if max(abs(actual - original) for actual, original in
               zip(values, source_float, strict=True)) < 0.05:
            raise AssertionError("Ring Mod controls did not modulate the test tone")


def check_rhythmic_slicer(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
        for frame in range(8000)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-rhythmic-slicer-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar.wav"
        output = root / "sliced.wav"
        write_pcm16(source, sample_rate, source_samples)
        pattern = "1,0," + ",".join(["1", "0"] * 7)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--rhythmic-slicer", "--rhythmic-slicer-tempo-sync", "on",
             "--rhythmic-slicer-tempo-bpm", "120",
             "--rhythmic-slicer-division", "sixteenth",
             "--rhythmic-slicer-depth", "1", "--rhythmic-slicer-shape", "0",
             "--rhythmic-slicer-attack-ms", "0", "--rhythmic-slicer-release-ms", "0",
             "--rhythmic-slicer-phase", "0", "--rhythmic-slicer-steps", pattern],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Rhythmic Slicer controls did not enable one processor: {result.stdout}")
        values = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Rhythmic Slicer produced non-finite output")
        first_step = values[100:900]
        second_step = values[1100:1900]
        if max(abs(value) for value in first_step) < 0.25:
            raise AssertionError("Rhythmic Slicer did not pass the on-step audio")
        if max(abs(value) for value in second_step) > 1.0e-6:
            raise AssertionError("Rhythmic Slicer did not mute the off-step audio")


def check_step_filter(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
        for frame in range(sample_rate)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-step-filter-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar.wav"
        output = root / "filtered.wav"
        write_pcm16(source, sample_rate, source_samples)
        pattern = "0,1," + ",".join(["0", "1"] * 7)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--step-filter", "--step-filter-mode", "lowpass",
             "--step-filter-tempo-sync", "on", "--step-filter-tempo-bpm", "120",
             "--step-filter-division", "sixteenth", "--step-filter-depth", "1",
             "--step-filter-min-cutoff-hz", "100",
             "--step-filter-max-cutoff-hz", "2000",
             "--step-filter-resonance-q", "0.707", "--step-filter-shape", "0",
             "--step-filter-smoothing-ms", "0", "--step-filter-mix", "1",
             "--step-filter-phase", "0", "--step-filter-steps", pattern],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Step Filter options did not enable one processor: {result.stdout}")
        values = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Step Filter produced non-finite output")
        low_step_rms = math.sqrt(sum(value * value for value in values[100:900]) / 800)
        high_step_rms = math.sqrt(sum(value * value for value in values[1100:1900]) / 800)
        if high_step_rms < low_step_rms * 2.0:
            raise AssertionError(
                f"Step Filter did not sweep the cutoff: low={low_step_rms} high={high_step_rms}"
            )


def check_notch_filter(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 1000.0 * frame / sample_rate))
        for frame in range(sample_rate)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-notch-filter-cli-test-") as directory:
        root = Path(directory)
        source = root / "tone.wav"
        wet_output = root / "notched.wav"
        dry_output = root / "dry.wav"
        write_pcm16(source, sample_rate, source_samples)
        common = ["--notch-filter", "--notch-filter-frequency-hz", "1000",
                  "--notch-filter-q", "4"]
        wet_result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output),
             *common, "--notch-filter-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        dry_result = subprocess.run(
            [str(executable), "render", str(source), str(dry_output),
             *common, "--notch-filter-mix", "0"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in wet_result.stdout or "processors=1" not in dry_result.stdout:
            raise AssertionError("Notch Filter CLI controls did not enable one processor")
        wet = struct.unpack("<" + "f" * len(source_samples), wet_output.read_bytes()[44:])
        dry = struct.unpack("<" + "f" * len(source_samples), dry_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in wet):
            raise AssertionError("Notch Filter produced non-finite output")
        wet_rms = math.sqrt(sum(value * value for value in wet[12000:]) / (len(wet) - 12000))
        dry_rms = math.sqrt(sum(value * value for value in dry[12000:]) / (len(dry) - 12000))
        if wet_rms >= 0.01 or abs(dry_rms - 12000 / 32768 / math.sqrt(2)) > 0.002:
            raise AssertionError(f"Notch Filter response or dry mix is wrong: wet={wet_rms} dry={dry_rms}")


def check_reverse_delay(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [((frame * 713) % 20000) - 10000 for frame in range(96)]
    with tempfile.TemporaryDirectory(prefix="tabletfx-reverse-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "sequence.wav"
        wet_output = root / "reversed.wav"
        muted_output = root / "muted.wav"
        sync_source = root / "sync-sequence.wav"
        sync_output = root / "sync-reversed.wav"
        write_pcm16(source, sample_rate, source_samples)
        sync_source_samples = [((frame * 997) % 24000) - 12000 for frame in range(4800)]
        write_pcm16(sync_source, sample_rate, sync_source_samples)
        options = ["--reverse-delay", "--reverse-delay-time-ms", "1",
                   "--reverse-delay-feedback", "0", "--reverse-delay-mix", "1",
                   "--reverse-delay-crossfade-ms", "0",
                   "--reverse-delay-time-mode", "milliseconds",
                   "--reverse-delay-tempo-bpm", "120",
                   "--reverse-delay-division", "quarter",
                   "--reverse-delay-low-cut-hz", "0",
                   "--reverse-delay-high-cut-hz", "0",
                   "--reverse-delay-forward-level", "0",
                   "--reverse-delay-spillover", "off"]
        wet_result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output), *options],
            capture_output=True, text=True, check=True,
        )
        muted_result = subprocess.run(
            [str(executable), "render", str(source), str(muted_output), *options,
             "--reverse-delay-run", "off"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in wet_result.stdout or "processors=1" not in muted_result.stdout:
            raise AssertionError("Reverse Delay CLI controls did not enable one processor")
        wet = struct.unpack("<" + "f" * len(source_samples), wet_output.read_bytes()[44:])
        muted = struct.unpack("<" + "f" * len(source_samples), muted_output.read_bytes()[44:])
        expected = [sample / 32768.0 for sample in reversed(source_samples[:48])]
        if any(abs(value) > 1e-6 for value in wet[:48]):
            raise AssertionError("Reverse Delay must wait for the first captured snippet")
        if any(abs(actual - target) > 1 / 32768 for actual, target in zip(wet[48:96], expected)):
            raise AssertionError("Reverse Delay did not play its captured segment backward")
        if any(abs(value) > 1e-6 for value in muted):
            raise AssertionError("Reverse Delay Run off must mute wet-only output")
        sync_result = subprocess.run(
            [str(executable), "render", str(sync_source), str(sync_output),
             "--reverse-delay", "--reverse-delay-time-ms", "1",
             "--reverse-delay-time-mode", "tempo-sync",
             "--reverse-delay-tempo-bpm", "300",
             "--reverse-delay-division", "sixteenth-triplet",
             "--reverse-delay-feedback", "0", "--reverse-delay-mix", "1",
             "--reverse-delay-crossfade-ms", "0"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in sync_result.stdout:
            raise AssertionError("Tempo-synced Reverse Delay did not enable one processor")
        synced = struct.unpack(
            "<" + "f" * len(sync_source_samples), sync_output.read_bytes()[44:],
        )
        sync_delay_frames = 1600
        if any(abs(value) > 1e-6 for value in synced[:sync_delay_frames]):
            raise AssertionError("Tempo-synced Reverse Delay started before its division")
        expected_sync = [sample / 32768.0
                         for sample in reversed(sync_source_samples[:sync_delay_frames])]
        if any(abs(actual - target) > 1 / 32768
               for actual, target in zip(
                   synced[sync_delay_frames:sync_delay_frames * 2], expected_sync)):
            raise AssertionError("Reverse Delay tempo division did not set the capture window")


def check_ducking_delay(executable: Path) -> None:
    sample_rate = 48000
    delay_frames = 96
    source_samples = [0] * 384
    source_samples[0] = 26214
    source_samples[delay_frames] = 26214
    with tempfile.TemporaryDirectory(prefix="tabletfx-ducking-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "sequence.wav"
        output = root / "ducked.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--ducking-delay", "--ducking-delay-time-ms", "2",
             "--ducking-delay-feedback", "0.95", "--ducking-delay-mix", "1",
             "--ducking-delay-threshold-db", "-6",
             "--ducking-delay-attenuation-db", "24",
             "--ducking-delay-release-ms", "2"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Ducking Delay CLI controls did not enable one processor")
        rendered = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if rendered[delay_frames] >= 0.2:
            raise AssertionError("a loud input above threshold must duck the simultaneous wet repeat")
        if rendered[delay_frames * 3] <= rendered[delay_frames] * 2.0:
            raise AssertionError("the delayed signal must recover after the input falls below threshold")


def check_pitch_shifter(executable: Path) -> None:
    sample_rate = 48000
    frame_count = sample_rate
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate))
        for frame in range(frame_count)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-pitch-cli-test-") as directory:
        root = Path(directory)
        source = root / "tone.wav"
        output = root / "shifted.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--pitch-semitones", "7"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Pitch option did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        header = struct.unpack("<4sI4s4sIHHIIHH4sI", data[:44])
        if header[5:8] != (3, 1, sample_rate) or header[10] != 32:
            raise AssertionError(f"Pitch render has unexpected WAV format: {header}")
        samples = struct.unpack("<" + "f" * ((len(data) - 44) // 4), data[44:])
        start = 18000
        length = 12000

        def amplitude_at(frequency: float) -> float:
            cosine = 0.0
            sine = 0.0
            window_sum = 0.0
            for offset in range(length):
                window = 0.5 - 0.5 * math.cos(2.0 * math.pi * offset / (length - 1))
                angle = 2.0 * math.pi * frequency * offset / sample_rate
                value = samples[start + offset] * window
                cosine += value * math.cos(angle)
                sine += value * math.sin(angle)
                window_sum += window
            return 2.0 * math.hypot(cosine, sine) / window_sum

        expected = 220.0 * 2.0 ** (7.0 / 12.0)
        candidates = [expected * 0.94 + index * 0.5 for index in range(49)]
        frequency = max(candidates, key=amplitude_at)
        amplitude = amplitude_at(frequency)
        cents = 1200.0 * math.log2(frequency / expected)
        if abs(cents) > 40.0 or amplitude < 0.03:
            raise AssertionError(
                f"CLI pitch result {frequency:.2f} Hz ({cents:.1f} cents), amplitude {amplitude:.3f}"
            )


def check_chorus(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [32767] + [0] * 79
    with tempfile.TemporaryDirectory(prefix="tabletfx-chorus-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "chorus.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--chorus-rate-hz", "0.8", "--chorus-delay-ms", "8",
             "--chorus-depth-ms", "0", "--chorus-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Chorus options did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<80f", data[44:44 + 80 * 4])
        expected = 32767 / 32768.0
        if abs(samples[0]) > 1.0e-7 or abs(samples[64] - expected) > 1.0e-6:
            raise AssertionError(
                f"Chorus impulse should be delayed 64 frames at 8 kHz; got frame 0={samples[0]}, frame 64={samples[64]}"
            )


def check_dimension_chorus(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [
        round(9000 * math.sin(2.0 * math.pi * 330.0 * frame / sample_rate))
        for frame in range(256)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-dimension-chorus-cli-test-") as directory:
        root = Path(directory)
        source = root / "sine.wav"
        output = root / "dimension-chorus.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--dimension-chorus", "--dimension-chorus-version", "D",
             "--dimension-chorus-mode", "3+4", "--dimension-chorus-mix", "0.65",
             "--dimension-chorus-level-db", "-3"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Dimension Chorus options did not instantiate one processor")
        rendered = struct.unpack("<256f", output.read_bytes()[44:44 + 256 * 4])
        if not all(math.isfinite(sample) for sample in rendered) or max(map(abs, rendered)) < 0.1:
            raise AssertionError("Dimension Chorus returned invalid or silent audio")

        for selector, value in (("--dimension-chorus-version", "W"),
                                ("--dimension-chorus-mode", "2+4+1")):
            invalid = subprocess.run(
                [str(executable), "render", str(source), str(root / "invalid.wav"),
                 selector, value],
                capture_output=True, text=True, check=False,
            )
            if invalid.returncode == 0:
                raise AssertionError(f"Dimension Chorus accepted invalid {selector} {value}")


def check_triple_double_chorus(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate))
        for frame in range(2048)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-triple-double-chorus-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-like.wav"
        output = root / "triple-double-chorus.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--triple-double-chorus", "--triple-double-chorus-preset-switch", "off",
             "--triple-double-chorus-manual-switch", "on",
             "--triple-double-chorus-rate-hz", "1.25",
             "--triple-double-chorus-left-depth-ms", "10",
             "--triple-double-chorus-center-depth-ms", "6",
             "--triple-double-chorus-right-depth-ms", "14",
             "--triple-double-chorus-feedback", "-0.2",
             "--triple-double-chorus-sparkle", "0.65",
             "--triple-double-chorus-stereo-switch", "on"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Triple-Double Chorus options did not instantiate one processor")
        rendered = struct.unpack("<2048f", output.read_bytes()[44:44 + 2048 * 4])
        if not all(math.isfinite(sample) for sample in rendered) or max(map(abs, rendered)) < 0.1:
            raise AssertionError("Triple-Double Chorus returned invalid or silent audio")

        invalid = subprocess.run(
            [str(executable), "render", str(source), str(root / "invalid.wav"),
             "--triple-double-chorus-stereo-switch", "sometimes"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0:
            raise AssertionError("Triple-Double Chorus accepted an invalid switch value")


def check_four_band_resonator(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate))
        for frame in range(4096)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-four-band-resonator-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-like.wav"
        output = root / "four-band-resonator.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--four-band-resonator-mode", "chord",
             "--four-band-resonator-chord", "dominant7",
             "--four-band-resonator-frequency-hz", "220",
             "--four-band-resonator-feedback", "0.65,0.55,0.45,0.35",
             "--four-band-resonator-q", "2,2.5,3,3.5",
             "--four-band-resonator-filter-locations", "pre,post,pre,post",
             "--four-band-resonator-input-mode", "mono"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Four-band resonator options did not instantiate one processor")
        rendered = struct.unpack("<4096f", output.read_bytes()[44:44 + 4096 * 4])
        if not all(math.isfinite(sample) for sample in rendered) or max(map(abs, rendered)) < 0.01:
            raise AssertionError("Four-band resonator returned invalid or silent audio")

        invalid = subprocess.run(
            [str(executable), "render", str(source), str(root / "invalid.wav"),
             "--four-band-resonator-frequencies-hz", "110,220"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0:
            raise AssertionError("Four-band resonator accepted an incomplete frequency list")


def check_diatonic_harmonizer(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate))
        for frame in range(8192)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-diatonic-harmonizer-cli-test-") as directory:
        root = Path(directory)
        source = root / "single-note.wav"
        output = root / "harmonized.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--diatonic-harmonizer-mix", "1",
             "--diatonic-harmonizer-interval", "2",
             "--diatonic-harmonizer-key", "0",
             "--diatonic-harmonizer-scale", "major",
             "--diatonic-harmonizer-tone-hz", "8000",
             "--diatonic-harmonizer-delay-ms", "0",
             "--diatonic-harmonizer-feedback", "0",
             "--diatonic-harmonizer-tap-division", "free",
             "--diatonic-harmonizer-tempo-bpm", "120"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Diatonic Harmonizer options did not instantiate one processor")
        rendered = struct.unpack("<8192f", output.read_bytes()[44:44 + 8192 * 4])
        if not all(math.isfinite(sample) for sample in rendered) or max(map(abs, rendered)) < 0.01:
            raise AssertionError("Diatonic Harmonizer returned invalid or silent audio")

        invalid = subprocess.run(
            [str(executable), "render", str(source), str(root / "invalid.wav"),
             "--diatonic-harmonizer-scale", "not-a-scale"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0:
            raise AssertionError("Diatonic Harmonizer accepted an unknown scale")


def check_pitch_arpeggiator(executable: Path) -> None:
    sample_rate = 48000
    frame_count = 32768
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 220.0 * frame / sample_rate))
        for frame in range(frame_count)
    ]
    shifts = "0,7," + ",".join(["0"] * 14)
    with tempfile.TemporaryDirectory(prefix="tabletfx-pitch-arpeggiator-cli-test-") as directory:
        root = Path(directory)
        source = root / "single-note.wav"
        output = root / "arpeggiated.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--pitch-arpeggiator", "--pitch-arpeggiator-mix", "1",
             "--pitch-arpeggiator-run", "off",
             "--pitch-arpeggiator-trigger-threshold-dbfs", "-20",
             "--pitch-arpeggiator-steps", "2", "--pitch-arpeggiator-repeats", "0",
             "--pitch-arpeggiator-division", "free", "--pitch-arpeggiator-step-ms", "100",
             "--pitch-arpeggiator-key", "0", "--pitch-arpeggiator-scale", "major",
             "--pitch-arpeggiator-shifts", shifts,
             "--pitch-arpeggiator-amplitude-shape", "ramp-up",
             "--pitch-arpeggiator-amplitude-alpha", "0.25",
             "--pitch-arpeggiator-pan-shape", "ping-pong",
             "--pitch-arpeggiator-pan-alpha", "0.5"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Pitch Arpeggiator options did not instantiate one processor")
        rendered = struct.unpack(f"<{frame_count}f", output.read_bytes()[44:])
        dry = [sample / 32768.0 for sample in source_samples]
        if not all(math.isfinite(sample) for sample in rendered) or \
                max(abs(a - b) for a, b in zip(dry, rendered, strict=True)) < 0.05:
            raise AssertionError("Pitch Arpeggiator did not render a finite pitch/level pattern")

        invalid = subprocess.run(
            [str(executable), "render", str(source), str(root / "invalid.wav"),
             "--pitch-arpeggiator-division", "not-a-division"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0:
            raise AssertionError("Pitch Arpeggiator accepted an unknown step division")


def check_grain_delay(executable: Path) -> None:
    sample_rate = 8000
    frame_count = 4096
    source_samples = [
        round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
        for frame in range(frame_count)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-grain-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-tone.wav"
        output = root / "grain-delay.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--grain-delay-time-ms", "40", "--grain-delay-feedback", "0.2",
             "--grain-delay-mix", "1", "--grain-delay-grain-size-ms", "80",
             "--grain-delay-overlap", "2", "--grain-delay-pitch-semitones", "12",
             "--grain-delay-spray-ms", "10"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Grain Delay controls did not instantiate one processor")
        rendered = struct.unpack(f"<{frame_count}f", output.read_bytes()[44:])
        if not all(math.isfinite(sample) for sample in rendered):
            raise AssertionError("Grain Delay returned non-finite output")
        dry = [sample / 32768.0 for sample in source_samples]
        if max(abs(a - b) for a, b in zip(dry, rendered, strict=True)) < 0.05:
            raise AssertionError("Grain Delay controls did not change rendered guitar audio")


def check_flanger(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [32767] + [0] * 159
    with tempfile.TemporaryDirectory(prefix="tabletfx-flanger-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "flanger.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--flanger-rate-hz", "0.25", "--flanger-delay-ms", "8",
             "--flanger-depth-ms", "0", "--flanger-feedback", "0.5",
             "--flanger-feedback-polarity", "negative", "--flanger-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Flanger options did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<160f", data[44:44 + 160 * 4])
        expected = 32767 / 32768.0
        if abs(samples[64] - expected) > 1.0e-6 or abs(samples[128] + expected * 0.5) > 1.0e-6:
            raise AssertionError(
                f"Flanger CLI should render negative feedback at frames 64/128; got {samples[64]}/{samples[128]}"
            )


def check_delay_time_modes(executable: Path) -> None:
    sample_rate = 8000
    source_samples = [32767] + [0] * 2049
    with tempfile.TemporaryDirectory(prefix="tabletfx-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        write_pcm16(source, sample_rate, source_samples)
        cases = (
            ("free", ["--delay-ms", "8"], 64),
            ("tempo", ["--delay-sync-bpm", "120", "--delay-division", "eighth"], 2000),
        )
        for label, time_args, expected_frame in cases:
            output = root / f"{label}.wav"
            result = subprocess.run(
                [str(executable), "render", str(source), str(output), *time_args,
                 "--delay-feedback", "0", "--delay-mix", "1"],
                capture_output=True, text=True, check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"{label} delay option did not instantiate one processor: {result.stdout}")
            data = output.read_bytes()
            samples = struct.unpack("<2050f", data[44:44 + 2050 * 4])
            expected = 32767 / 32768.0
            if abs(samples[0]) > 1.0e-7 or abs(samples[expected_frame] - expected) > 1.0e-6:
                raise AssertionError(
                    f"{label} delay impulse should appear at frame {expected_frame}; got {samples[expected_frame]}"
                )

        slapback_output = root / "slapback.wav"
        result = subprocess.run(
            [str(executable), "render", str(source), str(slapback_output),
             "--delay-preset", "slapback"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"slapback preset did not instantiate one processor: {result.stdout}")
        data = slapback_output.read_bytes()
        samples = struct.unpack("<2050f", data[44:44 + 2050 * 4])
        input_peak = 32767 / 32768.0
        expected = (input_peak * 0.75, input_peak * 0.25, input_peak * 0.03)
        actual = (samples[0], samples[880], samples[1760])
        if any(abs(got - want) > 1.0e-6 for got, want in zip(actual, expected)):
            raise AssertionError(
                "slapback preset should use 110 ms, 12% feedback, and 25% wet; "
                f"got dry/first echo/second echo {actual}, expected {expected}"
            )


def check_bbd_style_delay(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [0] * 700
    source_samples[480] = 32767
    with tempfile.TemporaryDirectory(prefix="tabletfx-bbd-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "warmed-impulse.wav"
        output = root / "bbd-delay.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--delay-preset", "bbd", "--delay-ms", "1",
             "--delay-feedback", "0", "--delay-mix", "1", "--delay-bbd"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"BBD-style delay did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<700f", data[44:44 + 700 * 4])
        first_echo = samples[528]
        next_echo_sample = samples[529]
        if abs(samples[527]) > 1.0e-7:
            raise AssertionError("BBD-style delay moved its first echo before the selected delay time")
        if not 0.40 < first_echo < 0.46:
            raise AssertionError(f"BBD-style first echo was not bandwidth-limited: {first_echo}")
        if not 0.20 < next_echo_sample < first_echo:
            raise AssertionError("BBD-style first echo did not have a decaying filtered tail")
        if not all(math.isfinite(sample) for sample in samples):
            raise AssertionError("BBD-style delay produced a non-finite sample")


def check_tape_echo_delay(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [0] * 700
    source_samples[480] = 32767
    with tempfile.TemporaryDirectory(prefix="tabletfx-tape-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "warmed-impulse.wav"
        output = root / "tape-delay.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--delay-preset", "tape", "--delay-ms", "1",
             "--delay-feedback", "0", "--delay-mix", "1", "--delay-tape",
             "--delay-tape-saturation", "0.45", "--delay-tape-wow-depth-ms", "0",
             "--delay-tape-flutter-depth-ms", "0"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Tape delay did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<700f", data[44:44 + 700 * 4])
        if abs(samples[527]) > 1.0e-7:
            raise AssertionError("Tape delay moved its first echo before the selected delay time")
        if not 0.76 < samples[528] < 0.79:
            raise AssertionError(f"Tape saturation control produced an unexpected echo: {samples[528]}")
        if not all(math.isfinite(sample) for sample in samples):
            raise AssertionError("Tape delay produced a non-finite sample")


def check_ping_pong_delay(executable: Path) -> None:
    sample_rate = 8000
    frames = 80
    source_samples = [32767, 0] + [0] * (frames * 2 - 2)
    with tempfile.TemporaryDirectory(prefix="tabletfx-ping-pong-cli-test-") as directory:
        root = Path(directory)
        source = root / "stereo-impulse.wav"
        output = root / "stereo-ping-pong.wav"
        write_pcm16_channels(source, sample_rate, 2, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output), "--delay-ms", "4",
             "--delay-ping-pong", "--delay-feedback", "0.5", "--delay-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"ping-pong CLI did not instantiate one delay: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<" + "f" * (frames * 2), data[44:44 + frames * 8])
        peak = 32767 / 32768.0
        expected = ((0, 0.0), (32 * 2 + 1, peak), (64 * 2, peak * 0.5))
        for index, value in expected:
            if abs(samples[index] - value) > 1.0e-6:
                raise AssertionError(
                    f"ping-pong echo mismatch at interleaved sample {index}: "
                    f"got {samples[index]}, expected {value}"
                )
        if abs(samples[32 * 2]) > 1.0e-7 or abs(samples[64 * 2 + 1]) > 1.0e-7:
            raise AssertionError("ping-pong echoes did not alternate between stereo channels")


def check_independent_dual_delay(executable: Path) -> None:
    sample_rate = 8000
    frames = 110
    source_samples = [0] * (frames * 2)
    source_samples[0] = 32767
    source_samples[3] = 16384
    with tempfile.TemporaryDirectory(prefix="tabletfx-dual-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "stereo-impulses.wav"
        output = root / "dual-delay.wav"
        write_pcm16_channels(source, sample_rate, 2, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--delay-dual-a-ms", "4", "--delay-dual-a-feedback", "0.5",
             "--delay-dual-a-mix", "1", "--delay-dual-b-ms", "6",
             "--delay-dual-b-feedback", "0.25", "--delay-dual-b-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"dual delay CLI did not instantiate one delay: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<" + "f" * (frames * 2), data[44:44 + frames * 8])
        peak = 32767 / 32768.0
        expected = (
            (32 * 2, peak),
            (49 * 2 + 1, 0.5),
            (64 * 2, peak * 0.5),
            (97 * 2 + 1, 0.125),
        )
        for index, value in expected:
            if abs(samples[index] - value) > 1.0e-6:
                raise AssertionError(
                    f"independent dual delay mismatch at sample {index}: "
                    f"got {samples[index]}, expected {value}"
                )
        if abs(samples[32 * 2 + 1]) > 1.0e-7 or abs(samples[49 * 2]) > 1.0e-7:
            raise AssertionError("independent dual delay leaked echoes between channels")


def check_multitap_delay(executable: Path) -> None:
    sample_rate = 8000
    frames = 96
    source_samples = [0] * (frames * 2)
    source_samples[0] = 32767
    with tempfile.TemporaryDirectory(prefix="tabletfx-multitap-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "stereo-impulse.wav"
        output = root / "multitap-delay.wav"
        write_pcm16_channels(source, sample_rate, 2, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--delay-feedback", "0", "--delay-mix", "1",
             "--delay-tap", "4", "1", "-1",
             "--delay-tap", "6", "0.5", "1",
             "--delay-tap", "8", "0", "0",
             "--delay-tap", "10", "0", "0", "--delay-multitap"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"multi-tap delay CLI did not instantiate one delay: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<" + "f" * (frames * 2), data[44:44 + frames * 8])
        peak = 32767 / 32768.0
        if abs(samples[32 * 2] - peak * 0.5) > 1.0e-6 or abs(samples[32 * 2 + 1]) > 1.0e-7:
            raise AssertionError("hard-left first tap has the wrong time, level, or pan")
        if abs(samples[48 * 2]) > 1.0e-7 or abs(samples[48 * 2 + 1] - peak * 0.25) > 1.0e-6:
            raise AssertionError("hard-right second tap has the wrong time, level, or pan")


def check_tape_heads_delay(executable: Path) -> None:
    sample_rate = 8000
    frames = 40
    source_samples = [0] * (frames * 2)
    source_samples[0] = 32767
    with tempfile.TemporaryDirectory(prefix="tabletfx-tape-heads-cli-test-") as directory:
        root = Path(directory)
        source = root / "stereo-impulse.wav"
        output = root / "tape-heads.wav"
        write_pcm16_channels(source, sample_rate, 2, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--delay-head-spacing-ms", "1", "--delay-heads", "1,2",
             "--delay-feedback", "0", "--delay-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"tape-head delay CLI did not instantiate one delay: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<" + "f" * (frames * 2), data[44:44 + frames * 8])
        if abs(samples[8 * 2]) <= abs(samples[8 * 2 + 1]):
            raise AssertionError("first tape head was not placed left of the second channel")
        if abs(samples[16 * 2]) >= abs(samples[16 * 2 + 1]):
            raise AssertionError("second tape head was not placed right of the first channel")
        if any(abs(sample) > 1.0e-7 for sample in samples[24 * 2:]):
            raise AssertionError("unselected third/fourth tape heads produced echoes")


def check_phaser(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [32767] + [0] * 1023
    with tempfile.TemporaryDirectory(prefix="tabletfx-phaser-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "phaser.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--phaser-rate-hz", "0.5", "--phaser-min-hz", "250",
             "--phaser-max-hz", "1600", "--phaser-feedback", "0.2",
             "--phaser-stages", "8", "--phaser-mix", "0.5"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Phaser options did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<1024f", data[44:44 + 1024 * 4])
        if not all(math.isfinite(sample) for sample in samples):
            raise AssertionError("Phaser CLI produced a non-finite sample")
        if not 0.1 < samples[0] < 0.99:
            raise AssertionError(f"Phaser CLI did not apply the selected wet chain: {samples[0]}")


def check_vibrato(executable: Path) -> None:
    sample_rate = 48000
    source_samples = [32767] + [0] * 1023
    with tempfile.TemporaryDirectory(prefix="tabletfx-vibrato-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "vibrato.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--vibrato-rate-hz", "5", "--vibrato-depth-ms", "0",
             "--vibrato-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Vibrato options did not instantiate one processor: {result.stdout}")
        data = output.read_bytes()
        samples = struct.unpack("<1024f", data[44:44 + 1024 * 4])
        expected = 32767 / 32768.0
        if any(abs(sample) > 1.0e-7 for sample in samples[:96]) or \
                abs(samples[96] - expected) > 1.0e-5:
            raise AssertionError(
                f"Vibrato CLI should output its center-delay impulse at frame 96; got {samples[96]}"
            )


def check_compressor(executable: Path) -> None:
    sample_rate = 48000
    input_samples = [16384] * sample_rate
    with tempfile.TemporaryDirectory(prefix="tabletfx-compressor-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, sample_rate, input_samples)
        result = subprocess.run(
            [
                str(executable), "render", str(source), str(output),
                "--compressor-threshold-db", "-24",
                "--compressor-ratio", "4",
                "--compressor-knee-db", "0",
                "--compressor-attack-ms", "0.1",
                "--compressor-release-ms", "50",
                "--eq-band2-frequency-hz", "1000",
                "--eq-band2-gain-db", "3",
                "--eq-band2-q", "0.7",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        if "processors=2" not in result.stdout:
            raise AssertionError(f"Compressor chain was not constructed: {result.stdout}")
        data = output.read_bytes()
        values = struct.unpack("<" + "f" * sample_rate, data[44:])
        input_level_db = 20.0 * math.log10(0.5)
        expected_level_db = -24.0 + (input_level_db + 24.0) / 4.0
        expected_sample = 10.0 ** (expected_level_db / 20.0)
        if not math.isfinite(values[-1]) or abs(values[-1] - expected_sample) > 2.0e-3:
            raise AssertionError(
                f"Compressor output expected {expected_sample}, got {values[-1]}"
            )
        if not abs(values[-1]) < 0.5:
            raise AssertionError("Compressor did not reduce a signal above threshold")


def check_new_effect_modules(executable: Path) -> None:
    sample_rate = 48000
    frame_count = 4096
    cases = (
        ("optical compressor", 440.0, [
            "--optical-compressor", "--optical-compressor-threshold-db", "-30",
            "--optical-compressor-ratio", "4", "--optical-compressor-knee-db", "0",
        ]),
        ("FET compressor", 440.0, [
            "--fet-compressor", "--fet-compressor-threshold-db", "-30",
            "--fet-compressor-ratio", "8", "--fet-compressor-knee-db", "0",
        ]),
        ("guitar graphic EQ", 800.0, [
            "--guitar-graphic-eq", "--guitar-eq-layout", "seven",
            "--guitar-eq-band4-gain-db", "6",
        ]),
        ("bass graphic EQ", 160.0, [
            "--bass-graphic-eq", "--bass-eq-layout", "seven",
            "--bass-eq-band3-gain-db", "6",
        ]),
        ("shelf/tilt EQ", 1000.0, [
            "--shelf-tilt-eq", "--shelf-tilt-mode", "tilt",
            "--shelf-tilt-frequency-hz", "1000", "--shelf-tilt-gain-db", "6",
        ]),
    )
    with tempfile.TemporaryDirectory(prefix="tabletfx-new-effects-cli-test-") as directory:
        root = Path(directory)
        source = root / "tone.wav"
        source_samples = [
            round(12000 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
            for frame in range(frame_count)
        ]
        write_pcm16(source, sample_rate, source_samples)
        for index, (label, frequency, options) in enumerate(cases):
            source_samples = [
                round(12000 * math.sin(2.0 * math.pi * frequency * frame / sample_rate))
                for frame in range(frame_count)
            ]
            write_pcm16(source, sample_rate, source_samples)
            output = root / f"effect-{index}.wav"
            result = subprocess.run(
                [str(executable), "render", str(source), str(output), *options],
                capture_output=True, text=True, check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"{label} CLI did not instantiate one processor: {result.stdout}")
            data = output.read_bytes()
            values = struct.unpack("<" + "f" * frame_count, data[44:])
            reference = [sample / 32768.0 for sample in source_samples]
            if not all(math.isfinite(value) for value in values):
                raise AssertionError(f"{label} CLI produced a non-finite sample")
            if max(abs(actual - expected) for actual, expected in zip(values, reference, strict=True)) < 1.0e-5:
                raise AssertionError(f"{label} CLI did not affect its input tone")

        invalid_output = root / "invalid-layout.wav"
        invalid = subprocess.run(
            [str(executable), "render", str(source), str(invalid_output),
             "--guitar-graphic-eq", "--guitar-eq-layout", "five",
             "--guitar-eq-band6-gain-db", "6"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0 or "does not include band 6" not in invalid.stderr:
            raise AssertionError(
                "Five-band guitar EQ should reject a nonzero sixth-band gain; "
                f"status={invalid.returncode}, stderr={invalid.stderr!r}"
            )


def check_noise_gate(executable: Path) -> None:
    sample_rate = 48000
    input_samples = [1638, 16384, 2621, 1966, 16384]
    with tempfile.TemporaryDirectory(prefix="tabletfx-gate-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, sample_rate, input_samples)
        result = subprocess.run(
            [
                str(executable), "render", str(source), str(output),
                "--gate-threshold-db", "-20",
                "--gate-attenuation-db", "-60",
                "--gate-hysteresis-db", "3",
                "--gate-attack-ms", "0",
                "--gate-hold-ms", "0",
                "--gate-release-ms", "0",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Noise gate was not constructed: {result.stdout}")
        data = output.read_bytes()
        values = struct.unpack("<" + "f" * len(input_samples), data[44:])
        expected = [input_samples[0] / 32768.0 * 0.001,
                    input_samples[1] / 32768.0,
                    input_samples[2] / 32768.0,
                    input_samples[3] / 32768.0 * 0.001,
                    input_samples[4] / 32768.0]
        for index, (actual, wanted) in enumerate(zip(values, expected, strict=True)):
            if not math.isfinite(actual) or abs(actual - wanted) > 1.0e-6:
                raise AssertionError(
                    f"Gate sample {index}: expected {wanted}, got {actual}"
                )


def check_clean_boost(executable: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="tabletfx-clean-boost-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, 48000, [8192, -8192])
        result = subprocess.run(
            [str(executable), "render", str(source), str(output), "--boost-db", "6"],
            capture_output=True,
            text=True,
            check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Clean boost was not constructed: {result.stdout}")
        values = struct.unpack("<2f", output.read_bytes()[44:52])
        expected = 0.25 * 10.0 ** (6.0 / 20.0)
        if any(not math.isfinite(value) for value in values) or any(
            abs(actual - wanted) > 1.0e-6
            for actual, wanted in zip(values, (expected, -expected), strict=True)
        ):
            raise AssertionError(f"Clean boost did not apply transparent level gain: {values}")


def check_overdrive(executable: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="tabletfx-overdrive-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, 48000, [16384] * 2400)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--overdrive-drive", "0.5", "--overdrive-level-db", "0",
             "--overdrive-mix", "1"],
            capture_output=True,
            text=True,
            check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Overdrive was not constructed: {result.stdout}")
        values = struct.unpack("<" + "f" * 2400, output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in values):
            raise AssertionError("Overdrive CLI produced non-finite audio")
        if not 0.97 < values[-1] < 1.0:
            raise AssertionError(f"Overdrive CLI did not apply its bounded soft clip: {values[-1]}")


def check_mid_overdrive(executable: Path) -> None:
    sample_rate = 48000
    with tempfile.TemporaryDirectory(prefix="tabletfx-mid-overdrive-test-") as directory:
        root = Path(directory)
        rms_values = []
        for label, frequency in (("bass", 90.0), ("mids", 800.0)):
            source = root / f"{label}.wav"
            output = root / f"{label}-processed.wav"
            frames = sample_rate
            values = [round(8192 * math.sin(2.0 * math.pi * frequency * frame / sample_rate))
                      for frame in range(frames)]
            write_pcm16(source, sample_rate, values)
            result = subprocess.run(
                [str(executable), "render", str(source), str(output),
                 "--mid-overdrive-drive", "0",
                 "--mid-overdrive-boost-db", "12",
                 "--mid-overdrive-level-db", "0",
                 "--mid-overdrive-mix", "1"],
                capture_output=True, text=True, check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"Mid-focused overdrive was not constructed: {result.stdout}")
            data = output.read_bytes()
            samples = struct.unpack("<" + "f" * frames, data[44:])
            if not all(math.isfinite(sample) for sample in samples):
                raise AssertionError(f"Mid-focused overdrive produced non-finite {label} audio")
            tail = samples[sample_rate // 2:]
            rms_values.append(math.sqrt(sum(sample * sample for sample in tail) / len(tail)))
        if rms_values[1] <= rms_values[0] * 1.25:
            raise AssertionError(
                f"Mid-focused CLI should emphasize 800 Hz over 90 Hz: bass={rms_values[0]}, mids={rms_values[1]}"
            )


def check_high_gain_distortion(executable: Path) -> None:
    sample_rate = 48000
    with tempfile.TemporaryDirectory(prefix="tabletfx-high-gain-distortion-test-") as directory:
        root = Path(directory)
        rms_values = []
        for label, frequency in (("bass", 90.0), ("mids", 1000.0)):
            source = root / f"{label}.wav"
            output = root / f"{label}-processed.wav"
            frames = sample_rate
            values = [round(512 * math.sin(2.0 * math.pi * frequency * frame / sample_rate))
                      for frame in range(frames)]
            write_pcm16(source, sample_rate, values)
            result = subprocess.run(
                [str(executable), "render", str(source), str(output),
                 "--high-gain-drive", "0.8",
                 "--high-gain-pre-low-cut-hz", "300",
                 "--high-gain-pre-high-cut-hz", "12000",
                 "--high-gain-post-tone-hz", "12000",
                 "--high-gain-level-db", "0",
                 "--high-gain-mix", "1"],
                capture_output=True, text=True, check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"High-gain distortion was not constructed: {result.stdout}")
            data = output.read_bytes()
            samples = struct.unpack("<" + "f" * frames, data[44:])
            if not all(math.isfinite(sample) for sample in samples):
                raise AssertionError(f"High-gain distortion produced non-finite {label} audio")
            tail = samples[sample_rate // 2:]
            rms_values.append(math.sqrt(sum(sample * sample for sample in tail) / len(tail)))
        if rms_values[0] >= rms_values[1] * 0.75:
            raise AssertionError(
                f"High-gain pre low-cut should reduce 90 Hz relative to 1 kHz: bass={rms_values[0]}, mids={rms_values[1]}"
            )


def check_round_fuzz(executable: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="tabletfx-round-fuzz-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        output = root / "output.wav"
        write_pcm16(source, 48000, [16384] * 2400)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--round-fuzz", "1", "--round-fuzz-level-db", "0", "--round-fuzz-mix", "1"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError(f"Round fuzz was not constructed: {result.stdout}")
        samples = struct.unpack("<" + "f" * 2400, output.read_bytes()[44:])
        if not all(math.isfinite(sample) for sample in samples):
            raise AssertionError("Round fuzz CLI produced non-finite audio")
        if not 0.99 < samples[-1] <= 1.0:
            raise AssertionError(f"Round fuzz did not reach bounded saturation: {samples[-1]}")


def check_envelope_wah(executable: Path) -> None:
    sample_rate = 48000
    samples = [round(16384 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate))
               for frame in range(4800)]
    with tempfile.TemporaryDirectory(prefix="tabletfx-touch-wah-test-") as directory:
        root = Path(directory)
        source = root / "input.wav"
        write_pcm16(source, sample_rate, samples)
        outputs = []
        for direction in ("up", "down"):
            output = root / f"{direction}.wav"
            result = subprocess.run(
                [
                    str(executable), "render", str(source), str(output),
                    "--touch-wah-sensitivity", "1",
                    "--touch-wah-direction", direction,
                    "--touch-wah-attack-ms", "0",
                    "--touch-wah-release-ms", "0",
                    "--touch-wah-smoothing-ms", "0",
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            if "processors=1" not in result.stdout:
                raise AssertionError(f"Envelope wah was not constructed: {result.stdout}")
            values = struct.unpack("<" + "f" * len(samples), output.read_bytes()[44:])
            if any(not math.isfinite(value) for value in values):
                raise AssertionError(f"Envelope wah produced non-finite audio ({direction})")
            rms = math.sqrt(sum(value * value for value in values) / len(values))
            if rms < 1.0e-4:
                raise AssertionError(f"Envelope wah produced unexpectedly silent audio ({direction})")
            outputs.append(values)
        difference = max(abs(up - down) for up, down in zip(*outputs, strict=True))
        if difference < 1.0e-3:
            raise AssertionError("Touch-wah up/down directions produced indistinguishable output")


def check_pcm24_padding(executable: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="tabletfx-pcm24-test-") as directory:
        root = Path(directory)
        source = root / "input-padded.wav"
        output = root / "output.wav"
        write_pcm24(source, 48000, bytes((0x00, 0x00, 0x40, 0x00)))
        result = subprocess.run(
            [str(executable), "render", str(source), str(output)],
            capture_output=True,
            text=True,
            check=True,
        )
        if "frames=1" not in result.stdout:
            raise AssertionError(f"Unexpected 24-bit padded WAV render summary: {result.stdout}")
        data = output.read_bytes()
        (sample,) = struct.unpack("<f", data[44:48])
        if abs(sample - 0.5) > 1.0e-6:
            raise AssertionError(f"Expected padded PCM24 sample 0.5, got {sample}")


def check_malformed_pcm24_rejected(executable: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="tabletfx-invalid-pcm24-test-") as directory:
        root = Path(directory)
        for index, payload in enumerate((bytes((0x00, 0x00, 0x40, 0x01)), bytes((0x00, 0x00, 0x40, 0x00, 0x00)))):
            source = root / f"invalid-{index}.wav"
            output = root / f"invalid-{index}-output.wav"
            write_pcm24(source, 48000, payload)
            result = subprocess.run(
                [str(executable), "render", str(source), str(output)],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0 or "Invalid WAV block alignment" not in result.stderr:
                raise AssertionError(f"Malformed PCM24 alignment was not rejected: {result}")


def check_channel_conversion(executable: Path) -> None:
    sample_rate = 48000
    stereo_samples = [8192, 16384, -8192, 8192]
    with tempfile.TemporaryDirectory(prefix="tabletfx-channel-conversion-test-") as directory:
        root = Path(directory)
        source = root / "stereo.wav"
        average_output = root / "mono-average.wav"
        equal_power_output = root / "mono-equal-power.wav"
        write_pcm16_channels(source, sample_rate, 2, stereo_samples)

        average_result = subprocess.run(
            [str(executable), "render", str(source), str(average_output),
             "--output-channels", "mono"],
            capture_output=True, text=True, check=True,
        )
        if "input_channels=2 channels=1" not in average_result.stdout:
            raise AssertionError(f"Stereo-to-mono summary is wrong: {average_result.stdout}")
        average_data = average_output.read_bytes()
        average_header = struct.unpack("<4sI4s4sIHHIIHH4sI", average_data[:44])
        if average_header[6] != 1:
            raise AssertionError(f"Stereo-to-mono output has {average_header[6]} channels")
        average = struct.unpack("<2f", average_data[44:52])
        expected_average = (0.375, 0.0)
        if any(abs(actual - expected) > 1.0e-6
               for actual, expected in zip(average, expected_average, strict=True)):
            raise AssertionError(f"Average downmix expected {expected_average}, got {average}")

        subprocess.run(
            [str(executable), "render", str(source), str(equal_power_output),
             "--output-channels", "mono", "--mono-sum", "equal-power"],
            capture_output=True, text=True, check=True,
        )
        equal_power_data = equal_power_output.read_bytes()
        equal_power = struct.unpack("<2f", equal_power_data[44:52])
        expected_equal_power = (0.375 * math.sqrt(2.0), 0.0)
        if any(abs(actual - expected) > 1.0e-6
               for actual, expected in zip(equal_power, expected_equal_power, strict=True)):
            raise AssertionError(
                f"Equal-power downmix expected {expected_equal_power}, got {equal_power}"
            )

        mono_source = root / "mono.wav"
        stereo_output = root / "stereo-upmix.wav"
        mono_samples = [8192, -16384, 0]
        write_pcm16(mono_source, sample_rate, mono_samples)
        subprocess.run(
            [str(executable), "render", str(mono_source), str(stereo_output),
             "--output-channels", "stereo"],
            capture_output=True, text=True, check=True,
        )
        stereo_data = stereo_output.read_bytes()
        stereo_header = struct.unpack("<4sI4s4sIHHIIHH4sI", stereo_data[:44])
        if stereo_header[6] != 2:
            raise AssertionError(f"Mono-to-stereo output has {stereo_header[6]} channels")
        upmixed = struct.unpack("<6f", stereo_data[44:68])
        expected_upmixed = tuple(value for sample in mono_samples
                                 for value in (sample / 32768.0, sample / 32768.0))
        if any(abs(actual - expected) > 1.0e-6
               for actual, expected in zip(upmixed, expected_upmixed, strict=True)):
            raise AssertionError(f"Mono upmix expected {expected_upmixed}, got {upmixed}")


def check_modulated_delay(executable: Path) -> None:
    sample_rate = 48000
    delay_frames = 96
    source_samples = [0] * 384
    source_samples[0] = 26214
    with tempfile.TemporaryDirectory(prefix="tabletfx-modulated-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "delayed.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--modulated-delay", "--modulated-delay-time-ms", "2",
             "--modulated-delay-feedback", "0", "--modulated-delay-mix", "1",
             "--modulated-delay-rate-hz", "0.8", "--modulated-delay-depth-ms", "0",
             "--modulated-delay-waveform", "sine",
             "--modulated-delay-spread-degrees", "0",
             "--modulated-delay-trigger-threshold-db", "-24"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Modulated Delay CLI controls did not enable one processor")
        rendered = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if abs(rendered[delay_frames] - (26214 / 32768)) > 0.002:
            raise AssertionError("zero-depth Modulated Delay must produce the 2 ms wet repeat")
        if any(abs(rendered[index]) > 0.002 for index in range(delay_frames + 1, len(rendered))):
            raise AssertionError("zero-feedback Modulated Delay must not produce later repeats")


def check_filtered_delay(executable: Path) -> None:
    sample_rate = 48000
    delay_frames = 96
    source_samples = [0] * 384
    source_samples[0] = 26214
    with tempfile.TemporaryDirectory(prefix="tabletfx-filtered-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "filtered.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--filtered-delay", "--filtered-delay-time-ms", "2",
             "--filtered-delay-feedback", "0", "--filtered-delay-mix", "1",
             "--filtered-delay-low-cut-hz", "120",
             "--filtered-delay-high-cut-hz", "6000"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Filtered Delay CLI controls did not enable one processor")
        rendered = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if any(abs(rendered[index]) > 0.002 for index in range(delay_frames)):
            raise AssertionError("fully-wet Filtered Delay must not leak dry input")
        if not any(abs(rendered[index]) > 0.01 for index in range(delay_frames, delay_frames + 24)):
            raise AssertionError("Filtered Delay must produce its first filtered repeat")
        if any(abs(rendered[index]) > 0.002 for index in range(delay_frames + 128, len(rendered))):
            raise AssertionError("zero-feedback Filtered Delay must not produce later repeats")


def check_sweep_delay(executable: Path) -> None:
    sample_rate = 48000
    delay_frames = 96
    source_samples = [0] * 768
    source_samples[0] = 26214
    with tempfile.TemporaryDirectory(prefix="tabletfx-sweep-delay-cli-test-") as directory:
        root = Path(directory)
        source = root / "impulse.wav"
        output = root / "swept.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--sweep-delay", "--sweep-delay-time-ms", "2",
             "--sweep-delay-feedback", "0", "--sweep-delay-mix", "1",
             "--sweep-delay-start-hz", "1000", "--sweep-delay-stop-hz", "1000",
             "--sweep-delay-resonance-q", "1.2", "--sweep-delay-rate-hz", "0.5",
             "--sweep-delay-waveform", "sine", "--sweep-delay-phase-degrees", "90"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Sweep Delay CLI controls did not enable one processor")
        rendered = struct.unpack("<" + "f" * len(source_samples), output.read_bytes()[44:])
        if any(abs(rendered[index]) > 0.002 for index in range(delay_frames)):
            raise AssertionError("fully-wet Sweep Delay must not leak dry input")
        if not any(abs(rendered[index]) > 0.01 for index in range(delay_frames, delay_frames + 48)):
            raise AssertionError("Sweep Delay must produce its band-pass-filtered repeat")
        if any(abs(rendered[index]) > 0.01 for index in range(delay_frames + 384, len(rendered))):
            raise AssertionError("zero-feedback Sweep Delay must not produce a second repeat")


def check_formant_filter(executable: Path) -> None:
    sample_rate = 48000
    frame_count = 4800
    source_samples = [
        round(10000 * math.sin(2.0 * math.pi * 430.0 * frame / sample_rate))
        for frame in range(frame_count)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-formant-filter-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-tone.wav"
        dry_output = root / "dry.wav"
        wet_output = root / "formant.wav"
        write_pcm16(source, sample_rate, source_samples)
        subprocess.run(
            [str(executable), "render", str(source), str(dry_output)],
            capture_output=True, text=True, check=True,
        )
        result = subprocess.run(
            [str(executable), "render", str(source), str(wet_output),
             "--formant-filter", "--formant-filter-vowel-position", "2.5",
             "--formant-filter-q", "6", "--formant-filter-mix", "0.72",
             "--formant-filter-level-db", "-2"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Formant Filter options did not enable one processor")
        dry = struct.unpack("<" + "f" * frame_count, dry_output.read_bytes()[44:])
        wet = struct.unpack("<" + "f" * frame_count, wet_output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in wet):
            raise AssertionError("Formant Filter produced non-finite output")
        if max(abs(a - b) for a, b in zip(dry, wet, strict=True)) < 1.0e-3:
            raise AssertionError("Formant Filter controls did not change the rendered tone")


def check_harmonic_resonator(executable: Path) -> None:
    sample_rate = 48000
    frame_count = 9600
    source_samples = [
        round(10000 * (math.sin(2.0 * math.pi * 110.0 * frame / sample_rate) +
                       0.35 * math.sin(2.0 * math.pi * 173.0 * frame / sample_rate)))
        for frame in range(frame_count)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-harmonic-resonator-cli-test-") as directory:
        root = Path(directory)
        source = root / "guitar-tone.wav"
        output = root / "resonated.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--harmonic-resonator", "--harmonic-resonator-root-frequency-hz", "110",
             "--harmonic-resonator-decay-seconds", "0.8",
             "--harmonic-resonator-brightness", "0.7",
             "--harmonic-resonator-mix", "0.65"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Harmonic Resonator options did not enable one processor")
        rendered = struct.unpack("<" + "f" * frame_count, output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in rendered):
            raise AssertionError("Harmonic Resonator produced non-finite output")
        dry = [sample / 32768.0 for sample in source_samples]
        if max(abs(a - b) for a, b in zip(dry, rendered, strict=True)) < 1.0e-3:
            raise AssertionError("Harmonic Resonator did not change the rendered signal")


def check_bit_crusher(executable: Path) -> None:
    sample_rate = 48000
    source_values = [6554, 19661, -6554, -19661]
    expected_held = [0.25, 0.25, -0.25, -0.25]
    with tempfile.TemporaryDirectory(prefix="tabletfx-bit-crusher-cli-test-") as directory:
        root = Path(directory)
        source = root / "quantizer-input.wav"
        output = root / "crushed.wav"
        write_pcm16(source, sample_rate, source_values)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--bit-crusher", "--bit-crusher-bit-depth", "3",
             "--bit-crusher-sample-rate-divisor", "2",
             "--bit-crusher-mix", "0.5"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Bit Crusher options did not enable one processor")

        rendered = struct.unpack("<" + "f" * len(source_values),
                                 output.read_bytes()[44:])
        dry = [sample / 32768.0 for sample in source_values]
        expected = [sample + (held - sample) * 0.5
                    for sample, held in zip(dry, expected_held, strict=True)]
        if not all(math.isfinite(sample) for sample in rendered):
            raise AssertionError("Bit Crusher produced non-finite output")
        if any(abs(actual - target) > 1.0e-4
               for actual, target in zip(rendered, expected, strict=True)):
            raise AssertionError(
                f"Bit Crusher depth/divisor/mix settings expected {expected}, got {rendered}")

        invalid = subprocess.run(
            [str(executable), "render", str(source), str(root / "invalid.wav"),
             "--bit-crusher-bit-depth", "3.5"],
            capture_output=True, text=True, check=False,
        )
        if invalid.returncode == 0 or "Invalid integer argument" not in invalid.stderr:
            raise AssertionError("Bit Crusher accepted a fractional bit-depth value")


def check_multiband_compressor(executable: Path) -> None:
    sample_rate = 48000
    frame_count = 12000
    source_samples = [
        round(15000 * (
            math.sin(2.0 * math.pi * 90.0 * frame / sample_rate) +
            0.5 * math.sin(2.0 * math.pi * 1200.0 * frame / sample_rate) +
            0.25 * math.sin(2.0 * math.pi * 7000.0 * frame / sample_rate)))
        for frame in range(frame_count)
    ]
    with tempfile.TemporaryDirectory(prefix="tabletfx-multiband-compressor-cli-test-") as directory:
        root = Path(directory)
        source = root / "three-band-tone.wav"
        output = root / "compressed.wav"
        write_pcm16(source, sample_rate, source_samples)
        result = subprocess.run(
            [str(executable), "render", str(source), str(output),
             "--multiband-compressor", "--multiband-slope", "db12",
             "--multiband-low-mid-hz", "500", "--multiband-mid-high-hz", "4000",
             "--multiband-low-threshold-db", "-36", "--multiband-low-ratio", "6",
             "--multiband-low-knee-db", "0", "--multiband-low-attack-ms", "0.1",
             "--multiband-low-release-ms", "30", "--multiband-low-level-db", "2",
             "--multiband-low-detector", "peak", "--multiband-low-mute", "off"],
            capture_output=True, text=True, check=True,
        )
        if "processors=1" not in result.stdout:
            raise AssertionError("Multiband Compressor options did not enable one processor")
        rendered = struct.unpack("<" + "f" * frame_count, output.read_bytes()[44:])
        if not all(math.isfinite(value) for value in rendered):
            raise AssertionError("Multiband Compressor produced non-finite output")
        dry = [sample / 32768.0 for sample in source_samples]
        if max(abs(a - b) for a, b in zip(dry, rendered, strict=True)) < 1.0e-3:
            raise AssertionError("Multiband Compressor controls did not change rendered audio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    check_render(args.executable.resolve())
    check_room_reverb(args.executable.resolve())
    check_hall_reverb(args.executable.resolve())
    check_plate_reverb(args.executable.resolve())
    check_spring_reverb(args.executable.resolve())
    check_auto_swell(args.executable.resolve())
    check_rotary_speaker(args.executable.resolve())
    check_ring_modulator(args.executable.resolve())
    check_rhythmic_slicer(args.executable.resolve())
    check_step_filter(args.executable.resolve())
    check_notch_filter(args.executable.resolve())
    check_reverse_delay(args.executable.resolve())
    check_ducking_delay(args.executable.resolve())
    check_modulated_delay(args.executable.resolve())
    check_filtered_delay(args.executable.resolve())
    check_sweep_delay(args.executable.resolve())
    check_formant_filter(args.executable.resolve())
    check_harmonic_resonator(args.executable.resolve())
    check_bit_crusher(args.executable.resolve())
    check_multiband_compressor(args.executable.resolve())
    check_pitch_shifter(args.executable.resolve())
    check_delay_time_modes(args.executable.resolve())
    check_bbd_style_delay(args.executable.resolve())
    check_tape_echo_delay(args.executable.resolve())
    check_ping_pong_delay(args.executable.resolve())
    check_independent_dual_delay(args.executable.resolve())
    check_multitap_delay(args.executable.resolve())
    check_tape_heads_delay(args.executable.resolve())
    check_chorus(args.executable.resolve())
    check_dimension_chorus(args.executable.resolve())
    check_triple_double_chorus(args.executable.resolve())
    check_four_band_resonator(args.executable.resolve())
    check_diatonic_harmonizer(args.executable.resolve())
    check_pitch_arpeggiator(args.executable.resolve())
    check_grain_delay(args.executable.resolve())
    check_flanger(args.executable.resolve())
    check_phaser(args.executable.resolve())
    check_vibrato(args.executable.resolve())
    check_compressor(args.executable.resolve())
    check_new_effect_modules(args.executable.resolve())
    check_noise_gate(args.executable.resolve())
    check_clean_boost(args.executable.resolve())
    check_overdrive(args.executable.resolve())
    check_mid_overdrive(args.executable.resolve())
    check_high_gain_distortion(args.executable.resolve())
    check_round_fuzz(args.executable.resolve())
    check_envelope_wah(args.executable.resolve())
    check_pcm24_padding(args.executable.resolve())
    check_malformed_pcm24_rejected(args.executable.resolve())
    check_channel_conversion(args.executable.resolve())
    print("Offline render CLI tests passed: PCM16 gain/compressors/gate/clean boost/transparent, mid-focused/high-gain/round fuzz/envelope wah, optical/FET and multiband compressors, guitar/bass graphic and shelf/tilt EQs, Room/Hall/Plate/Spring reverb, Auto Swell, Rotary Speaker, Ring Modulator, Rhythmic Slicer, Step Filter, static Notch and Formant Filters, reverse/ducking/modulated delay, free/tempo-synced/ping-pong/independent dual/multi-tap/tape-head delay, chorus/Dimension Chorus/flanger/phaser/vibrato/pitch shifting, Diatonic Harmonizer, and Pitch Arpeggiator, mono/stereo conversion, padded PCM24 input, and malformed alignment rejection.")


if __name__ == "__main__":
    main()
