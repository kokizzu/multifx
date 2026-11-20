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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    check_render(args.executable.resolve())
    print("Offline render CLI test passed: PCM16 input -> gain -> float32 WAV output.")


if __name__ == "__main__":
    main()
