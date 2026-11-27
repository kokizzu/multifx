#!/usr/bin/env python3
"""Configure, build, test, and benchmark the native TabletFX core."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "native"
WINDOWS_BUILD_DIR = ROOT / "build" / "windows-mingw"
WINDOWS_TOOLCHAIN = ROOT / "cmake" / "toolchains" / "mingw-w64.cmake"


def find_cmake() -> str:
    candidates = [Path("/usr/bin/cmake"), Path("/usr/local/bin/cmake")]
    found = shutil.which("cmake")
    if found:
        candidates.append(Path(found))
    for candidate in candidates:
        if not candidate.is_file():
            continue
        try:
            result = subprocess.run([str(candidate), "--version"], capture_output=True, text=True, check=False)
        except OSError:
            continue
        if result.returncode == 0 and "cmake version" in result.stdout.lower():
            print(f"Using CMake: {candidate} ({result.stdout.splitlines()[0]})")
            return str(candidate)
        print(f"Skipping unusable CMake candidate {candidate}: {result.stderr.strip() or result.stdout.strip()}")
    raise SystemExit("No working CMake executable was found")


def find_ctest(cmake: str) -> str:
    candidates = [Path(cmake).with_name("ctest")]
    found = shutil.which("ctest")
    if found:
        candidates.append(Path(found))
    for candidate in candidates:
        if not candidate.is_file():
            continue
        try:
            result = subprocess.run([str(candidate), "--version"], capture_output=True, text=True, check=False)
        except OSError:
            continue
        if result.returncode == 0 and "ctest version" in result.stdout.lower():
            print(f"Using CTest: {candidate} ({result.stdout.splitlines()[0]})")
            return str(candidate)
        print(f"Skipping unusable CTest candidate {candidate}: {result.stderr.strip() or result.stdout.strip()}")
    raise SystemExit("No working CTest executable was found")


def run(command: list[str]) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def configure(cmake: str) -> None:
    command = [cmake, "-S", str(ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=Release"]
    if shutil.which("ninja"):
        command.extend(["-G", "Ninja"])
    run(command)


def build(cmake: str) -> None:
    configure(cmake)
    run([cmake, "--build", str(BUILD_DIR), "--parallel", str(min(4, os.cpu_count() or 1))])


def test(cmake: str, ctest: str, test_filter: str = "",
         verbose: bool = False) -> None:
    build(cmake)
    command = [ctest, "--test-dir", str(BUILD_DIR), "-C", "Release",
               "--output-on-failure"]
    test_filter = test_filter.strip()
    if test_filter:
        command.extend(["-R", test_filter])
    if verbose:
        command.append("-V")
    run(command)


def build_windows(cmake: str) -> None:
    compiler = shutil.which("x86_64-w64-mingw32-g++")
    if not compiler:
        raise SystemExit("MinGW-w64 x86_64 C++ compiler is required for windows-cross-build")
    if not WINDOWS_TOOLCHAIN.is_file():
        raise SystemExit(f"Windows CMake toolchain file is missing: {WINDOWS_TOOLCHAIN}")
    command = [
        cmake, "-S", str(ROOT), "-B", str(WINDOWS_BUILD_DIR),
        "-DCMAKE_TOOLCHAIN_FILE=" + str(WINDOWS_TOOLCHAIN),
        "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF",
    ]
    if shutil.which("ninja"):
        command.extend(["-G", "Ninja"])
    run(command)
    run([cmake, "--build", str(WINDOWS_BUILD_DIR), "--parallel",
         str(min(4, os.cpu_count() or 1))])


def bench_fractal_diatonic_harmonizer(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_fractal_diatonic_harmonizer_bench.exe"
        if os.name == "nt"
        else "tabletfx_fractal_diatonic_harmonizer_bench"
    )
    run([str(executable)])


def bench_feedback_generator(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_feedback_generator_bench.exe"
        if os.name == "nt"
        else "tabletfx_feedback_generator_bench"
    )
    run([str(executable)])


def bench_shuffling_looper(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_shuffling_looper_bench.exe"
        if os.name == "nt"
        else "tabletfx_shuffling_looper_bench"
    )
    run([str(executable)])


def bench_fractal_chorus_tempo(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_fractal_chorus_tempo_bench.exe"
        if os.name == "nt"
        else "tabletfx_fractal_chorus_tempo_bench"
    )
    run([str(executable)])


def bench_headrush_phaser_stereo(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_headrush_phaser_stereo_bench.exe"
        if os.name == "nt"
        else "tabletfx_headrush_phaser_stereo_bench"
    )
    run([str(executable)])


def bench_stadium_dynamics_sidechain(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_stadium_dynamics_sidechain_bench.exe"
        if os.name == "nt"
        else "tabletfx_stadium_dynamics_sidechain_bench"
    )
    run([str(executable)])


def bench_headrush_phase_invert(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_headrush_phase_invert_bench.exe"
        if os.name == "nt"
        else "tabletfx_headrush_phase_invert_bench"
    )
    run([str(executable)])


def bench_fractal_flanger_vco(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_fractal_flanger_vco_bench.exe"
        if os.name == "nt"
        else "tabletfx_fractal_flanger_vco_bench"
    )
    run([str(executable)])


def bench_fender_harmonic_tremolo_mode(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_fender_harmonic_tremolo_mode_bench.exe"
        if os.name == "nt"
        else "tabletfx_fender_harmonic_tremolo_mode_bench"
    )
    run([str(executable)])


def bench_fender_tremolo_duty_cycle(cmake: str) -> None:
    build(cmake)
    print(f"CPU: {cpu_name()}")
    executable = BUILD_DIR / (
        "tabletfx_fender_tremolo_duty_cycle_bench.exe"
        if os.name == "nt"
        else "tabletfx_fender_tremolo_duty_cycle_bench"
    )
    run([str(executable)])


def cpu_name() -> str:
    if os.name == "nt":
        return os.environ.get("PROCESSOR_IDENTIFIER", platform.processor() or "unknown")
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8", errors="replace").splitlines():
            if line.lower().startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine() or "unknown"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=("configure", "build", "test", "bench",
                 "bench-fractal-diatonic-harmonizer",
                  "bench-feedback-generator", "bench-shuffling-looper",
                   "bench-fractal-chorus-tempo", "bench-fender-tremolo-duty-cycle",
                  "bench-pitch-ring-mod", "bench-headrush-phaser-stereo",
                  "bench-headrush-phase-invert",
                   "bench-fractal-flanger-vco",
                   "bench-fender-harmonic-tremolo-mode",
                 "windows-build"),
    )
    parser.add_argument("--test-filter", default="")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    action = args.action
    cmake = find_cmake()
    ctest = find_ctest(cmake) if action == "test" else None
    if action == "configure":
        configure(cmake)
    elif action == "build":
        build(cmake)
    elif action == "test":
        test(cmake, ctest or "ctest", args.test_filter, args.verbose)
    elif action == "bench":
        build(cmake)
        print(f"CPU: {cpu_name()}")
        executable = BUILD_DIR / ("tabletfx_offline.exe" if os.name == "nt" else "tabletfx_offline")
        run([str(executable), "--bench"])
        xr1_benchmark = BUILD_DIR / (
            "tabletfx_xr1_global_eq_bench.exe"
                  "bench-fractal-global-mix-flanger",
            if os.name == "nt"
            else "tabletfx_xr1_global_eq_bench"
        )
        run([str(xr1_benchmark)])
        pitch_mix_benchmark = BUILD_DIR / (
            "tabletfx_fender_virtual_capo_mix_bench.exe"
            if os.name == "nt"
            else "tabletfx_fender_virtual_capo_mix_bench"
        )
        run([str(pitch_mix_benchmark)])
        panner_benchmark = BUILD_DIR / (
            "tabletfx_stereo_panner_bench.exe"
            if os.name == "nt"
            else "tabletfx_stereo_panner_bench"
        )
        run([str(panner_benchmark)])
        tone_master_3d_benchmark = BUILD_DIR / (
            "tabletfx_tone_master_pro_3d_chorus_bench.exe"
            if os.name == "nt"
            else "tabletfx_tone_master_pro_3d_chorus_bench"
        )
        run([str(tone_master_3d_benchmark)])
        fractal_surround_delay_benchmark = BUILD_DIR / (
            "tabletfx_fractal_surround_delay_bench.exe"
            if os.name == "nt"
            else "tabletfx_fractal_surround_delay_bench"
        )
        run([str(fractal_surround_delay_benchmark)])
        fractal_plex_shift_benchmark = BUILD_DIR / (
            "tabletfx_fractal_plex_shift_bench.exe"
            if os.name == "nt"
            else "tabletfx_fractal_plex_shift_bench"
        )
        run([str(fractal_plex_shift_benchmark)])
        fender_memory_delay_benchmark = BUILD_DIR / (
            "tabletfx_fender_memory_delay_bench.exe"
            if os.name == "nt"
            else "tabletfx_fender_memory_delay_bench"
        )
        run([str(fender_memory_delay_benchmark)])
        tone_master_small_hall_benchmark = BUILD_DIR / (
            "tabletfx_tone_master_small_hall_bench.exe"
            if os.name == "nt"
            else "tabletfx_tone_master_small_hall_bench"
        )
        run([str(tone_master_small_hall_benchmark)])
        tone_master_modulated_large_hall_benchmark = BUILD_DIR / (
            "tabletfx_tone_master_modulated_large_hall_bench.exe"
            if os.name == "nt"
            else "tabletfx_tone_master_modulated_large_hall_bench"
        )
        run([str(tone_master_modulated_large_hall_benchmark)])
    elif action == "bench-fractal-diatonic-harmonizer":
        bench_fractal_diatonic_harmonizer(cmake)
    elif action == "bench-feedback-generator":
        bench_feedback_generator(cmake)
    elif action == "bench-shuffling-looper":
        bench_shuffling_looper(cmake)
    elif action == "bench-fractal-chorus-tempo":
        bench_fractal_chorus_tempo(cmake)
    elif action == "bench-fender-tremolo-duty-cycle":
        bench_fender_tremolo_duty_cycle(cmake)
    elif action == "bench-pitch-ring-mod":
        bench_pitch_ring_mod(cmake)
    elif action == "bench-headrush-phaser-stereo":
        bench_headrush_phaser_stereo(cmake)
    elif action == "bench-stadium-dynamics-sidechain":
        bench_stadium_dynamics_sidechain(cmake)
    elif action == "bench-headrush-phase-invert":
        bench_headrush_phase_invert(cmake)
    elif action == "bench-fractal-flanger-vco":
        bench_fractal_flanger_vco(cmake)
    elif action == "bench-fender-harmonic-tremolo-mode":
        bench_fender_harmonic_tremolo_mode(cmake)
    elif action == "windows-build":
        build_windows(cmake)


if __name__ == "__main__":
    main()
    elif action == "bench-fractal-global-mix-flanger":
        bench_fractal_global_mix_flanger(cmake)
