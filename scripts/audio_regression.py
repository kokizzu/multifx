#!/usr/bin/env python3
"""Exercise the offline DSP chain with licensed, prerecorded DI guitar audio."""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import re
import subprocess
import sys
import tempfile

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


def run_input(renderer: Path, source: Path, output: Path,
              output_channels: str | None = None) -> str:
    command = [
        str(renderer), "render", str(source), str(output),
        "--gain-db", "2", "--output-gain-db", "-1",
        "--volume-position", "0.92", "--volume-min-db", "-96",
        "--volume-max-db", "0", "--volume-curve", "audio",
        "--limiter-ceiling-db", "-1", "--limiter-release-ms", "80",
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
        "--tremolo-rate-hz", "4", "--tremolo-depth", "0.2",
        "--tremolo-waveform", "sine", "--tremolo-sync", "off",
        "--chorus-rate-hz", "0.8", "--chorus-delay-ms", "15",
        "--chorus-depth-ms", "7", "--chorus-mix", "0.2",
        "--flanger-rate-hz", "0.25", "--flanger-delay-ms", "2",
        "--flanger-depth-ms", "1", "--flanger-feedback", "0.35",
        "--flanger-feedback-polarity", "positive", "--flanger-mix", "0.2",
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
        "--drive", "3", "--delay-ms", "125", "--delay-feedback", "0.2", "--delay-mix", "0.1",
        "--pitch-semitones", "5",
        "--eq-band1-frequency-hz", "120", "--eq-band1-gain-db", "1.5", "--eq-band1-q", "0.7",
        "--eq-band2-frequency-hz", "900", "--eq-band2-gain-db", "-1", "--eq-band2-q", "1",
        "--eq-band3-frequency-hz", "5000", "--eq-band3-gain-db", "0.5", "--eq-band3-q", "0.7",
    ]
    if output_channels is not None:
        command.extend(["--output-channels", output_channels])
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Offline rendering failed for {source}: {result.stderr.strip()}")
    report = result.stdout.strip()
    match = re.search(r"rendered frames=(\d+).*?processors=(\d+).*?output_peak=([0-9.eE+-]+).*?output_rms=([0-9.eE+-]+)", report)
    if not match or int(match.group(1)) <= 0 or int(match.group(2)) != 24:
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
        for label in PICKUP_ORDER:
            source = pickups[label]
            report = run_input(renderer, source, output_root / f"{label.replace('-', '_')}.wav")
            print(f"OK EGFxSet note {note}, {label}: {source.relative_to(clean_root)}; {report}")
        source = music_files[0]
        report = run_input(renderer, source, output_root / "guitar-techs-music.wav")
        print(f"OK Guitar-TECHS DI performance: {source.relative_to(music_root)}; {report}")
        report = run_input(renderer, source, output_root / "guitar-techs-mono.wav", "mono")
        print(f"OK Guitar-TECHS DI stereo-to-mono (average): {source.relative_to(music_root)}; {report}")
    print("All prerecorded clean guitar inputs rendered through the 24-effect offline chain.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"audio_regression: {error}", file=sys.stderr)
        raise SystemExit(1) from error
