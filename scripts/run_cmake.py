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


def test(cmake: str, ctest: str) -> None:
    build(cmake)
    run([ctest, "--test-dir", str(BUILD_DIR), "-C", "Release", "--output-on-failure"])


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
    parser.add_argument("action", choices=("configure", "build", "test", "bench", "windows-build"))
    action = parser.parse_args().action
    cmake = find_cmake()
    ctest = find_ctest(cmake) if action == "test" else None
    if action == "configure":
        configure(cmake)
    elif action == "build":
        build(cmake)
    elif action == "test":
        test(cmake, ctest or "ctest")
    elif action == "bench":
        build(cmake)
        print(f"CPU: {cpu_name()}")
        executable = BUILD_DIR / ("tabletfx_offline.exe" if os.name == "nt" else "tabletfx_offline")
        run([str(executable), "--bench"])
    elif action == "windows-build":
        build_windows(cmake)


if __name__ == "__main__":
    main()
