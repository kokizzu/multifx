#!/usr/bin/env python3
"""Exercise the offline DSP chain with licensed, prerecorded DI guitar audio."""

from __future__ import annotations

import argparse
from collections import defaultdict
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import wave

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_fixtures  # noqa: E402


PICKUP_ORDER = ("bridge", "bridge-middle", "middle", "middle-neck", "neck")
PICKUP_ALIASES = {
    "bridge-middle": ("bridgemiddle", "bridge_mid", "bridge-mid"),
    "middle-neck": ("middleneck", "middle_neck", "mid-neck"),
    "bridge": ("bridge",),
    "middle": ("middle",),
    "neck": ("neck",),
}
PICKUP_SUFFIXES = {
    "1": "bridge-middle",
    "2": "middle",
    "3": "middle-neck",
    "4": "neck",
}


def pickup_for(path: Path, root: Path) -> str | None:
    parent_text = " ".join(part.casefold() for part in path.relative_to(root).parts[:-1])
    compact = re.sub(r"[^a-z0-9]", "", parent_text)
    for label in ("bridge-middle", "middle-neck", "bridge", "middle", "neck"):
        if any(re.sub(r"[^a-z0-9]", "", alias.casefold()) in compact for alias in PICKUP_ALIASES[label]):
            return label

    suffix = re.search(r"_([1-4])$", path.stem)
    if suffix:
        return PICKUP_SUFFIXES[suffix.group(1)]
    if re.search(r"(?:^|[_-])bridge(?:$|[_-])", path.stem, re.IGNORECASE):
        return "bridge"
    return None


def matching_pickup_set(root: Path) -> tuple[str, dict[str, Path]]:
    groups: dict[str, dict[str, Path]] = defaultdict(dict)
    for path in sorted(root.rglob("*.wav")):
        label = pickup_for(path, root)
        if label is None:
            continue
        stem = re.sub(r"_[1-4]$", "", path.stem)
        groups[stem].setdefault(label, path)

    candidates = [(stem, values) for stem, values in groups.items() if all(label in values for label in PICKUP_ORDER)]
    if not candidates:
        counts = {label: sum(label in values for values in groups.values()) for label in PICKUP_ORDER}
        raise RuntimeError(f"Could not find the same recorded note for all five pickup settings; candidates={counts}")
    stem, values = sorted(candidates, key=lambda item: item[0])[0]
    return stem, values


def is_direct_input(path: Path, root: Path) -> bool:
    parts = [part.casefold() for part in path.relative_to(root).parts]
    if any(re.sub(r"[^a-z0-9]", "", part) in {"di", "directinput", "directin"} for part in parts):
        return True
    return bool(re.search(r"(?:^|[_-])di(?:$|[_-])", path.stem, re.IGNORECASE))


def write_regression_ir(path: Path) -> None:
    """Create a small deterministic mono room-like IR for local regression."""
    sample_rate = 48_000
    frames = 4096
    pcm = bytearray()
    for frame in range(frames):
        if frame == 0:
            sample = 0.55
        else:
            envelope = math.exp(-6.0 * frame / (frames - 1))
            sample = 0.08 * envelope * math.sin(2.0 * math.pi * frame / 47.0)
        pcm.extend(struct.pack("<h", round(max(-1.0, min(1.0, sample)) * 32767)))
    with wave.open(str(path), "wb") as impulse:
        impulse.setnchannels(1)
        impulse.setsampwidth(2)
        impulse.setframerate(sample_rate)
        impulse.writeframes(pcm)


def run_input(renderer: Path, source: Path, output: Path,
              convolution_ir: Path,
              output_channels: str | None = None,
              delay_character: str = "digital",
              delay_tape_heads: bool = False,
              three_stage_distortion: bool = False) -> str:
    command = [
        str(renderer), "render", str(source), str(output),
        "--gain-db", "2", "--output-gain-db", "-1",
        "--volume-position", "0.92", "--volume-min-db", "-96",
        "--volume-max-db", "0", "--volume-curve", "audio",
        "--limiter-ceiling-db", "-1", "--limiter-release-ms", "80",
        "--convolution-ir", str(convolution_ir),
        "--wah-position", "0.62", "--wah-min-hz", "350", "--wah-max-hz", "2200",
        "--wah-q", "4", "--wah-level-db", "0",
        "--touch-wah-sensitivity", "0.8", "--touch-wah-direction", "up",
        "--touch-wah-min-hz", "350", "--touch-wah-max-hz", "2200",
        "--touch-wah-q", "4", "--touch-wah-level-db", "0",
        "--touch-wah-attack-ms", "5", "--touch-wah-release-ms", "80",
        "--touch-wah-smoothing-ms", "5",
        "--boost-db", "3",
        "--overdrive-drive", "0.15", "--overdrive-level-db", "-3",
        "--overdrive-mix", "0.35",
        "--mid-overdrive-drive", "0.18", "--mid-overdrive-boost-db", "6",
        "--mid-overdrive-level-db", "-4", "--mid-overdrive-mix", "0.4",
        "--high-gain-drive", "0.72", "--high-gain-pre-low-cut-hz", "100",
        "--high-gain-pre-high-cut-hz", "9000", "--high-gain-post-tone-hz", "5200",
        "--high-gain-level-db", "-9", "--high-gain-mix", "0.8",
        "--round-fuzz", "0.55", "--round-fuzz-level-db", "-6",
        "--round-fuzz-mix", "0.85",
        "--tremolo-rate-hz", "4", "--tremolo-depth", "0.2",
        "--tremolo-waveform", "sine", "--tremolo-sync", "off",
        "--harmonic-tremolo-rate-hz", "3", "--harmonic-tremolo-depth", "0.65",
        "--harmonic-tremolo-crossover-hz", "700",
        "--harmonic-tremolo-waveform", "sine",
        "--harmonic-tremolo-sync", "off",
        "--harmonic-tremolo-stereo-alternation", "on",
        "--harmonic-tremolo-smoothing-ms", "10",
        "--auto-swell-delay",
        "--chorus-rate-hz", "0.8", "--chorus-delay-ms", "15",
        "--chorus-depth-ms", "7", "--chorus-mix", "0.2",
        "--dimension-chorus-version", "C", "--dimension-chorus-mode", "1",
        "--dimension-chorus-mix", "0.5", "--dimension-chorus-level-db", "0",
        "--triple-double-chorus", "--triple-double-chorus-preset-switch", "off",
        "--triple-double-chorus-manual-switch", "off",
        "--triple-double-chorus-rate-hz", "0.8",
        "--triple-double-chorus-left-depth-ms", "7",
        "--triple-double-chorus-center-depth-ms", "5",
        "--triple-double-chorus-right-depth-ms", "8",
        "--triple-double-chorus-feedback", "0.1",
        "--triple-double-chorus-sparkle", "0.2",
        "--triple-double-chorus-stereo-switch", "on",
        "--four-band-resonator",
        "--diatonic-harmonizer-mix", "0.35",
        "--diatonic-harmonizer-interval", "2",
        "--diatonic-harmonizer-key", "0",
        "--diatonic-harmonizer-scale", "major",
        "--diatonic-harmonizer-tone-hz", "8000",
        "--diatonic-harmonizer-delay-ms", "0",
        "--diatonic-harmonizer-feedback", "0.2",
        "--diatonic-harmonizer-tap-division", "free",
        "--diatonic-harmonizer-tempo-bpm", "120",
        "--pitch-arpeggiator", "--pitch-arpeggiator-run", "on",
        "--pitch-arpeggiator-trigger-threshold-dbfs", "-90",
        "--pitch-arpeggiator-steps", "4",
        "--pitch-arpeggiator-division", "free",
        "--pitch-arpeggiator-step-ms", "250",
        "--pitch-arpeggiator-shifts", "0,2,4,7,0,0,0,0,0,0,0,0,0,0,0,0",
        "--mono-guitar-synth", "--mono-guitar-synth-waveform", "sawtooth",
        "--mono-guitar-synth-mix", "0.35", "--mono-guitar-synth-level", "0.5",
        "--mono-guitar-synth-gate-threshold-db", "-54",
        "--mono-guitar-synth-attack-ms", "8", "--mono-guitar-synth-release-ms", "180",
        "--mono-guitar-synth-portamento-ms", "8",
        "--grain-delay-time-ms", "350", "--grain-delay-feedback", "0.2",
        "--grain-delay-mix", "0.15", "--grain-delay-grain-size-ms", "80",
        "--grain-delay-overlap", "2", "--grain-delay-pitch-semitones", "5",
        "--grain-delay-spray-ms", "20",
        "--plex-delay", "--plex-delay-lines", "4",
        "--plex-delay-decay-seconds", "2.5", "--plex-delay-diffusion", "0.65",
        "--plex-delay-pre-delay-ms", "30", "--plex-delay-mix", "0.12",
        "--pitch-feedback-delay", "--pitch-feedback-delay-time-ms", "350",
        "--pitch-feedback-delay-feedback", "0.2",
        "--pitch-feedback-delay-mix", "0.15",
        "--pitch-feedback-delay-pitch-semitones", "5",
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
        "--celestial-reverb", "--celestial-reverb-mix", "0.18",
        "--celestial-reverb-decay-seconds", "2.5",
        "--celestial-reverb-pitch-1-semitones", "12",
        "--celestial-reverb-pitch-2-semitones", "7",
        "--celestial-reverb-pitch-decay-seconds", "1.8",
        "--celestial-reverb-high-cut-hz", "9000",
        "--celestial-reverb-blend", "0.5",
        "--celestial-reverb-spillover", "on",
        "--flanger-rate-hz", "0.25", "--flanger-delay-ms", "2",
        "--flanger-depth-ms", "1", "--flanger-feedback", "0.35",
        "--flanger-feedback-polarity", "positive", "--flanger-mix", "0.2",
        "--manual-thru-zero-flanger",
        "--manual-thru-zero-flanger-minimum-time-ms", "2",
        "--manual-thru-zero-flanger-maximum-time-ms", "8",
        "--manual-thru-zero-flanger-manual", "2.5",
        "--manual-thru-zero-flanger-dry-delay-position", "0.65",
        "--manual-thru-zero-flanger-feedback-percent", "12",
        "--manual-thru-zero-flanger-phase-reverse", "both",
        "--manual-thru-zero-flanger-mix", "0.2",
        "--manual-thru-zero-flanger-smoothing-ms", "10",
        "--phaser-rate-hz", "0.5", "--phaser-min-hz", "250",
        "--phaser-max-hz", "1600", "--phaser-feedback", "0.15",
        "--phaser-stages", "6", "--phaser-mix", "0.2",
        "--vibrato-rate-hz", "5", "--vibrato-depth-ms", "0.35",
        "--vibrato-mix", "0.35",
        "--high-pass-hz", "80", "--low-pass-hz", "12000",
        "--gate-threshold-db", "-72", "--gate-attenuation-db", "-72",
        "--gate-hysteresis-db", "3", "--gate-attack-ms", "2",
        "--gate-hold-ms", "20", "--gate-release-ms", "80",
        "--pedal-compressor-sensitivity", "0.45",
        "--pedal-compressor-sustain", "0.45",
        "--pedal-compressor-attack", "0.25",
        "--pedal-compressor-level-db", "0",
        "--compressor-threshold-db", "-24", "--compressor-ratio", "3",
        "--compressor-knee-db", "6", "--compressor-attack-ms", "8",
        "--compressor-release-ms", "100",
        "--optical-compressor", "--optical-compressor-threshold-db", "-30",
        "--optical-compressor-ratio", "2.5", "--optical-compressor-knee-db", "6",
        "--fet-compressor", "--fet-compressor-threshold-db", "-30",
        "--fet-compressor-ratio", "2.5", "--fet-compressor-knee-db", "4",
        "--drive", "3", "--delay-ms", "125", "--delay-feedback", "0.2", "--delay-mix", "0.1",
        "--pitch-semitones", "5",
        "--eq-band1-frequency-hz", "120", "--eq-band1-gain-db", "1.5", "--eq-band1-q", "0.7",
        "--eq-band2-frequency-hz", "900", "--eq-band2-gain-db", "-1", "--eq-band2-q", "1",
        "--eq-band3-frequency-hz", "5000", "--eq-band3-gain-db", "0.5", "--eq-band3-q", "0.7",
        "--five-band-eq-band1-frequency-hz", "100", "--five-band-eq-band1-gain-db", "1.5", "--five-band-eq-band1-q", "0.7",
        "--five-band-eq-band2-frequency-hz", "350", "--five-band-eq-band2-gain-db", "-0.5", "--five-band-eq-band2-q", "1",
        "--five-band-eq-band3-frequency-hz", "1000", "--five-band-eq-band3-gain-db", "1", "--five-band-eq-band3-q", "0.7",
        "--five-band-eq-band4-frequency-hz", "3500", "--five-band-eq-band4-gain-db", "-0.5", "--five-band-eq-band4-q", "0.8",
        "--five-band-eq-band5-frequency-hz", "8000", "--five-band-eq-band5-gain-db", "0.5", "--five-band-eq-band5-q", "0.7",
        "--guitar-graphic-eq", "--guitar-eq-layout", "seven",
        "--guitar-eq-band3-gain-db", "1.5", "--guitar-eq-band5-gain-db", "-1",
        "--bass-graphic-eq", "--bass-eq-layout", "seven",
        "--bass-eq-band1-gain-db", "-1.5",
        "--shelf-tilt-eq", "--shelf-tilt-mode", "tilt",
        "--shelf-tilt-frequency-hz", "1200", "--shelf-tilt-gain-db", "1.5",
        "--room-reverb", "--room-size", "small",
        "--room-decay-s", "0.8", "--room-pre-delay-ms", "10",
        "--room-damping-hz", "6000", "--room-mix", "0.2",
        "--room-level-db", "0",
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
        "--auto-swell", "--auto-swell-threshold-db", "-36",
        "--auto-swell-attack-ms", "250", "--auto-swell-release-ms", "250",
        "--auto-swell-hysteresis-db", "6", "--auto-swell-taper", "s-taper",
        "--auto-swell-volume", "1",
        "--rotary-speaker", "--rotary-rate-hz", "6",
        "--rotary-tempo-sync", "off", "--rotary-tempo-bpm", "120",
        "--rotary-beats-per-rotation", "1", "--rotary-speed", "slow",
        "--rotary-slow-speed-multiplier", "0.12", "--rotary-drive", "0.15",
        "--rotary-hi-depth", "0.75", "--rotary-high-level", "1",
        "--rotary-low-depth", "0.55", "--rotary-rotor-length", "0.5",
        "--rotary-low-rate-multi", "0.72", "--rotary-low-time-constant-ms", "800",
        "--rotary-hi-time-constant-ms", "450",
        "--rotary-low-mic-spacing", "0.4", "--rotary-high-mic-spacing", "0.75",
        "--rotary-mic-distance", "0.25", "--rotary-stereo-spread", "1",
        "--rotary-mix", "1", "--rotary-level", "1", "--rotary-balance", "0",
        "--rotary-input-select", "stereo",
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
        "--rhythmic-slicer-steps",
        "1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25,1,0.25",
        "--step-filter", "--step-filter-mode", "lowpass",
        "--step-filter-tempo-sync", "on", "--step-filter-tempo-bpm", "120",
        "--step-filter-division", "sixteenth", "--step-filter-rate-hz", "8",
        "--step-filter-min-cutoff-hz", "120", "--step-filter-max-cutoff-hz", "6000",
        "--step-filter-resonance-q", "0.707", "--step-filter-depth", "0.65",
        "--step-filter-shape", "0.4", "--step-filter-smoothing-ms", "3",
        "--step-filter-mix", "0.7", "--step-filter-phase", "0",
        "--step-filter-steps",
        "0,0.75,0.2,0.9,0.35,0.6,0.1,1,0.25,0.8,0.15,0.95,0.4,0.7,0.05,0.85",
        "--notch-filter", "--notch-filter-frequency-hz", "713",
        "--notch-filter-q", "4", "--notch-filter-mix", "0.35",
        "--reverse-delay", "--reverse-delay-time-ms", "350",
        "--reverse-delay-feedback", "0.25", "--reverse-delay-mix", "0.35",
        "--reverse-delay-crossfade-ms", "10",
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
        "--sweep-delay", "--sweep-delay-time-ms", "350",
        "--sweep-delay-feedback", "0.25", "--sweep-delay-mix", "0.25",
        "--sweep-delay-start-hz", "250", "--sweep-delay-stop-hz", "6000",
        "--sweep-delay-resonance-q", "1.2", "--sweep-delay-rate-hz", "0.5",
        "--sweep-delay-waveform", "sine", "--sweep-delay-phase-degrees", "90",
        "--formant-filter", "--formant-filter-vowel-position", "1.5",
        "--formant-filter-q", "6", "--formant-filter-mix", "0.72",
        "--formant-filter-level-db", "-2",
        "--harmonic-resonator", "--harmonic-resonator-root-frequency-hz", "110",
        "--harmonic-resonator-decay-seconds", "1.2",
        "--harmonic-resonator-brightness", "0.55",
        "--harmonic-resonator-mix", "0.5",
        "--multiband-compressor", "--multiband-low-mid-hz", "250",
        "--multiband-mid-high-hz", "2500", "--multiband-slope", "db24",
        "--multiband-low-threshold-db", "-24", "--multiband-low-ratio", "2",
        "--multiband-low-knee-db", "6", "--multiband-low-attack-ms", "10",
        "--multiband-low-release-ms", "100", "--multiband-low-level-db", "0",
        "--multiband-low-detector", "rms", "--multiband-low-mute", "off",
        "--multiband-mid-threshold-db", "-24", "--multiband-mid-ratio", "2",
        "--multiband-mid-knee-db", "6", "--multiband-mid-attack-ms", "10",
        "--multiband-mid-release-ms", "100", "--multiband-mid-level-db", "0",
        "--multiband-mid-detector", "rms", "--multiband-mid-mute", "off",
        "--multiband-high-threshold-db", "-24", "--multiband-high-ratio", "2",
        "--multiband-high-knee-db", "6", "--multiband-high-attack-ms", "10",
        "--multiband-high-release-ms", "100", "--multiband-high-level-db", "0",
        "--multiband-high-detector", "rms", "--multiband-high-mute", "off",
        "--bit-crusher", "--bit-crusher-bit-depth", "8",
        "--bit-crusher-sample-rate-divisor", "4", "--bit-crusher-mix", "0.65",
    ]
    if delay_character == "tape":
        command.extend([
            "--delay-tape", "--delay-tape-saturation", "0.45",
            "--delay-tape-wow-depth-ms", "0.35", "--delay-tape-wow-rate-hz", "0.3",
            "--delay-tape-flutter-depth-ms", "0.08", "--delay-tape-flutter-rate-hz", "6",
        ])
    elif delay_character != "digital":
        raise ValueError(f"Unsupported delay regression character: {delay_character}")
    if delay_tape_heads:
        command.extend([
            "--delay-tape-heads",
            "--delay-head-spacing-ms", "110",
            "--delay-heads", "all",
        ])
    if three_stage_distortion:
        command.append("--three-stage-distortion")
    if output_channels is not None:
        command.extend(["--output-channels", output_channels])
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Offline rendering failed for {source}: {result.stderr.strip()}")
    report = result.stdout.strip()
    match = re.search(r"rendered frames=(\d+).*?processors=(\d+).*?output_peak=([0-9.eE+-]+).*?output_rms=([0-9.eE+-]+)", report)
    expected_processors = 67 + int(three_stage_distortion)
    if not match or int(match.group(1)) <= 0 or int(match.group(2)) != expected_processors:
        raise RuntimeError(f"Unexpected renderer report for {source}: {report}")
    if output_channels is not None:
        layout = re.search(r"input_channels=(\d+) channels=(\d+)", report)
        if not layout or int(layout.group(1)) != 2 or int(layout.group(2)) != 1:
            raise RuntimeError(f"Stereo-to-mono render did not report 2→1 channels: {report}")
    if float(match.group(3)) > 0.8914:
        raise RuntimeError(f"Peak limiter exceeded its -1 dBFS ceiling for {source}: {report}")
    if not output.is_file() or output.stat().st_size <= 44:
        raise RuntimeError(f"Renderer did not write audio output for {source}: {output}")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--data-dir", default="auto")
    parser.add_argument("--link", default="testdata/reference_audio")
    parser.add_argument("--renderer", type=Path, required=True)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    data_dir, selection = audio_fixtures.resolve_data_dir(args.data_dir, repo_root, args.link)
    audio_fixtures.verify(repo_root, data_dir, args.link)
    print(f"Audio regression storage: {data_dir} ({selection})")

    clean_root = data_dir / "egfxset-clean"
    music_root = data_dir / "guitar-techs-p3-music"
    note, pickups = matching_pickup_set(clean_root)
    music_files = sorted(path for path in music_root.rglob("*.wav") if is_direct_input(path, music_root))
    if not music_files:
        raise RuntimeError(f"No Guitar-TECHS P3 music file is clearly labeled as direct input under {music_root}")

    renderer = args.renderer.resolve()
    if not renderer.is_file():
        raise FileNotFoundError(f"Offline renderer not found: {renderer}")
    with tempfile.TemporaryDirectory(prefix="tabletfx-audio-regression-", dir=data_dir) as temp_dir:
        output_root = Path(temp_dir)
        convolution_ir = output_root / "local-regression-impulse.wav"
        write_regression_ir(convolution_ir)
        for label in PICKUP_ORDER:
            source = pickups[label]
            report = run_input(renderer, source, output_root / f"{label.replace('-', '_')}.wav",
                               convolution_ir)
            print(f"OK EGFxSet note {note}, {label}: {source.relative_to(clean_root)}; {report}")
            tape_report = run_input(
                renderer, source, output_root / f"{label.replace('-', '_')}-tape.wav",
                convolution_ir,
                delay_character="tape",
            )
            print(f"OK EGFxSet Tape Echo note {note}, {label}: {tape_report}")
            heads_report = run_input(
                renderer, source, output_root / f"{label.replace('-', '_')}-multi-head.wav",
                convolution_ir,
                delay_tape_heads=True,
            )
            print(f"OK EGFxSet Multi-head Echo note {note}, {label}: {heads_report}")
        source = music_files[0]
        report = run_input(renderer, source, output_root / "guitar-techs-music.wav",
                           convolution_ir)
        print(f"OK Guitar-TECHS DI performance: {source.relative_to(music_root)}; {report}")
        tape_report = run_input(
            renderer, source, output_root / "guitar-techs-music-tape.wav",
            convolution_ir,
            delay_character="tape",
        )
        print(f"OK Guitar-TECHS DI Tape Echo performance: {tape_report}")
        heads_report = run_input(
            renderer, source, output_root / "guitar-techs-music-multi-head.wav",
            convolution_ir,
            delay_tape_heads=True,
        )
        print(f"OK Guitar-TECHS DI Multi-head Echo performance: {heads_report}")
        report = run_input(renderer, source, output_root / "guitar-techs-mono.wav",
                           convolution_ir, "mono")
        print(f"OK Guitar-TECHS DI stereo-to-mono (average): {source.relative_to(music_root)}; {report}")
        source = pickups["bridge"]
        report = run_input(
            renderer, source, output_root / "mercator-bridge.wav", convolution_ir,
            three_stage_distortion=True,
        )
        print(f"OK Three-Stage Distortion on prerecorded EGFxSet bridge pickup: {report}")
    print("All prerecorded clean guitar inputs rendered through the 66-stage chain with a deterministic local convolution IR, including the project-authored Harmonic Tremolo, Manual Thru-Zero Flanger, Fender-guide-named Auto-Swell Delay control set, five-band parametric EQ, monophonic oscillator synth, Feedback Generator, Shimmer Reverb, Convolution Reverb, Pitch Arpeggiator and Diatonic Harmonizer, Bit Crusher, Dimension Chorus, Triple-Double Chorus, four-band Resonator, Grain Delay, pitch-feedback delay, Plex Matrix Delay, Room, Hall, Plate, Spring Reverb, Auto Swell, Rotary Speaker, Ring Modulator, Rhythmic Slicer, Step Filter, Notch Filter, Formant Filter, Harmonic Resonator, three-band compression, Reverse Delay, Ducking Delay, Modulated Delay, Filtered Delay, Resonant Delay, and Swept Delay, five pickup positions, and DI music with Tape Echo and multi-head echo. A separate 68-stage bridge-pickup render enables the project-authored Three-Stage Distortion through the offline CLI.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"audio_regression: {error}", file=sys.stderr)
        raise SystemExit(1) from error
