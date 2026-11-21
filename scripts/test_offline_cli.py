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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    check_render(args.executable.resolve())
    check_pitch_shifter(args.executable.resolve())
    check_chorus(args.executable.resolve())
    check_flanger(args.executable.resolve())
    check_compressor(args.executable.resolve())
    check_noise_gate(args.executable.resolve())
    check_clean_boost(args.executable.resolve())
    check_envelope_wah(args.executable.resolve())
    check_pcm24_padding(args.executable.resolve())
    check_malformed_pcm24_rejected(args.executable.resolve())
    check_channel_conversion(args.executable.resolve())
    print("Offline render CLI tests passed: PCM16 gain/compressor/gate/clean boost/envelope wah/chorus/flanger/pitch shifting, mono/stereo conversion, padded PCM24 input, and malformed alignment rejection.")


if __name__ == "__main__":
    main()
