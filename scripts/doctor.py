#!/usr/bin/env python3
"""Report build/audio toolchain availability without installing anything."""

from __future__ import annotations

import os
import shutil
import subprocess


def probe(label: str, args: list[str], timeout: int = 8,
          fallback_executable: str | None = None) -> None:
    executable = shutil.which(args[0]) or fallback_executable
    if executable is None:
        print(f"{label}: not found")
        return
    try:
        result = subprocess.run([executable, *args[1:]], capture_output=True,
                                text=True, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(f"{label}: unavailable ({exc})")
        return
    detail = (result.stdout + result.stderr).strip().splitlines()
    summary = " | ".join(detail[:4]) if detail else "available"
    print(f"{label}: {summary} (exit {result.returncode})")


def main() -> None:
    for label, args in (
        ("CMake", ["cmake", "--version"]),
        ("System CMake", ["/usr/bin/cmake", "--version"]),
        ("Ninja", ["ninja", "--version"]),
        ("C++ compiler", ["c++", "--version"]),
        ("Clang", ["clang++", "--version"]),
        ("Node.js", ["node", "--version"]),
        ("Playwright CLI", ["playwright", "--version"]),
        ("Chromium", ["chromium", "--version"]),
        ("Google Chrome", ["google-chrome", "--version"]),
        ("Qt 6 Core", ["pkg-config", "--modversion", "Qt6Core"]),
        ("Qt 6 Quick", ["pkg-config", "--modversion", "Qt6Quick"]),
        ("JACK", ["pkg-config", "--modversion", "jack"]),
        ("MinGW-w64", ["x86_64-w64-mingw32-g++", "--version"]),
        ("Clang-cl", ["clang-cl", "--version"]),
        ("Java", ["java", "-version"]),
    ):
        probe(label, args)
    emsdk_root = os.environ.get("TABLETFX_EMSDK_ROOT", "/tmp/multifx-emsdk")
    emscripten = os.path.join(emsdk_root, "upstream", "emscripten", "em++")
    probe("Emscripten C++", ["em++", "--version"], fallback_executable=emscripten)
    print(f"ANDROID_HOME: {os.environ.get('ANDROID_HOME', 'unset')}")
    print(f"ANDROID_SDK_ROOT: {os.environ.get('ANDROID_SDK_ROOT', 'unset')}")
    print(f"JAVA_HOME: {os.environ.get('JAVA_HOME', 'unset')}")
    if shutil.which("adb"):
        probe("ADB devices", ["adb", "devices", "-l"])
        for label, args in (
            ("Device model", ["adb", "shell", "getprop", "ro.product.model"]),
            ("Device Android release", ["adb", "shell", "getprop", "ro.build.version.release"]),
            ("Device ABIs", ["adb", "shell", "getprop", "ro.product.cpu.abilist"]),
            ("Device memory", ["adb", "shell", "cat", "/proc/meminfo"]),
        ):
            probe(label, args, timeout=5)
    else:
        print("ADB: not found; Android hardware/ABI not probed")


if __name__ == "__main__":
    main()
