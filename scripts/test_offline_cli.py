#!/usr/bin/env python3
"""Exercise the real offline render CLI with a deterministic local WAV fixture."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def write_pcm16(path: Path, sample_rate: int, samples: list[int]) -> None:
    payload = struct.pack("<" + "h" * len(samples), *samples)
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + len(payload), b"WAVE", b"fmt ", 16,
        1, 1, sample_rate, sample_rate * 2, 2, 16, b"data", len(payload),
    )
    path.write_bytes(header + payload)


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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    check_render(args.executable.resolve())
    check_compressor(args.executable.resolve())
    check_noise_gate(args.executable.resolve())
    check_clean_boost(args.executable.resolve())
    check_pcm24_padding(args.executable.resolve())
    check_malformed_pcm24_rejected(args.executable.resolve())
    print("Offline render CLI tests passed: PCM16 gain/compressor/gate/clean boost, padded PCM24 input, and malformed alignment rejection.")


if __name__ == "__main__":
    main()
