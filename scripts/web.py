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
        "_tf_convert_channels", "_tf_channel_conversion_self_test",
        "_tf_parallel_mixer_self_test",
        "_tf_chain_create", "_tf_chain_destroy", "_tf_chain_effect_count",
        "_tf_chain_input_buffer", "_tf_chain_output_buffer", "_tf_chain_memory_bytes",
        "_tf_chain_effect_memory_bytes", "_tf_chain_set_enabled", "_tf_chain_set_mix",
        "_tf_chain_begin", "_tf_chain_process_effect", "_tf_chain_finish",
        "_tf_chain_reset", "_tf_chain_set_compressor",
        "_tf_chain_set_optical_compressor", "_tf_chain_set_fet_compressor",
        "_tf_chain_set_guitar_graphic_eq", "_tf_chain_set_bass_graphic_eq",
        "_tf_chain_set_shelf_tilt_eq", "_tf_chain_set_room_reverb",
        "_tf_chain_set_hall_reverb",
        "_tf_chain_set_plate_reverb",
        "_tf_chain_set_spring_reverb",
         "_tf_chain_set_auto_swell",
        "_tf_chain_set_drive",
        "_tf_chain_set_clean_boost",
        "_tf_chain_set_overdrive",
        "_tf_chain_set_mid_overdrive",
        "_tf_chain_set_high_gain_distortion",
        "_tf_chain_set_round_fuzz",
        "_tf_chain_set_eq", "_tf_chain_set_delay", "_tf_chain_set_delay_tempo",
        "_tf_chain_set_delay_bbd_style", "_tf_chain_set_delay_tape_echo",
        "_tf_chain_set_delay_ping_pong",
        "_tf_chain_set_delay_dual",
        "_tf_chain_set_delay_multitap", "_tf_chain_set_delay_multitap_tap",
        "_tf_chain_set_delay_tape_heads",
        "_tf_chain_set_pitch",
        "_tf_chain_set_filters",
        "_tf_chain_set_pedal_compressor", "_tf_chain_set_input_gain",
        "_tf_chain_set_output_gain", "_tf_chain_ramp_input_gain",
        "_tf_chain_ramp_output_gain", "_tf_chain_process_output_gain",
        "_tf_chain_process_output_limiter", "_tf_chain_set_volume_pedal",
        "_tf_chain_set_noise_gate",
        "_tf_chain_set_limiter", "_tf_chain_set_wah", "_tf_chain_set_tremolo",
        "_tf_chain_set_chorus",
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
        str(ROOT / "src" / "effects" / "flanger.cpp"),
        str(ROOT / "src" / "effects" / "phaser.cpp"),
        str(ROOT / "src" / "effects" / "vibrato.cpp"),
        str(ROOT / "src" / "effects" / "envelope_wah.cpp"),
        str(ROOT / "src" / "signal_controls.cpp"),
        str(ROOT / "src" / "effects" / "filters.cpp"),
        str(ROOT / "src" / "effects" / "peak_limiter.cpp"),
        str(ROOT / "src" / "effects" / "poly_pitch_shifter.cpp"),
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
        str(ROOT / "src" / "effects" / "tremolo.cpp"),
        str(ROOT / "src" / "effects" / "volume_pedal.cpp"),
        str(ROOT / "src" / "effects" / "wah.cpp"),
        str(ROOT / "src" / "level_meter.cpp"),
        str(ROOT / "src" / "web" / "wasm_chain.cpp"),
        "-o", str(WEB_BUILD_DIR / "tabletfx_wasm.js"),
        "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sSINGLE_FILE=1",
        "-sENVIRONMENT=web,worker,worklet", "-sALLOW_MEMORY_GROWTH=0",
        "-sINITIAL_MEMORY=16777216", "-sSTACK_SIZE=262144",
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
        samples.extend(struct.pack("<h", value))
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples)
    return frames


def make_tuner_test_audio(destination: Path, sample_rate: int = 48000,
                          duration_seconds: float = 0.5) -> int:
    frames = int(sample_rate * duration_seconds)
    samples = bytearray()
    for frame in range(frames):
        value = max(-32768, min(32767, round(
            0.35 * math.sin(2.0 * math.pi * 440.0 * frame / sample_rate) * 32767.0
        )))
        samples.extend(struct.pack("<h", value))
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(1)
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
                    stderr_path: Path) -> tuple[dict, str]:
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
            deadline = time.monotonic() + 35
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


def browser_test() -> None:
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
        tuner_path = temp_dir / "tuner-a4.wav"
        make_test_audio(input_path)
        make_tuner_test_audio(tuner_path)
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
            "--chorus-rate-hz", "0.8", "--chorus-delay-ms", "15",
            "--chorus-depth-ms", "7", "--chorus-mix", "0.2",
            "--flanger-rate-hz", "0.25", "--flanger-delay-ms", "2",
            "--flanger-depth-ms", "1", "--flanger-feedback", "0.35",
            "--flanger-feedback-polarity", "negative", "--flanger-mix", "0.2",
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
        ]
        run(renderer_command, cwd=ROOT)
        LocalWebHandler.test_files = {
            "/__test_input.wav": input_path,
            "/__test_expected.wav": reference_path,
            "/__test_tuner.wav": tuner_path,
        }
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
        url = base_url + "/web/index.html?selftest=1"
        try:
            result, stderr = run_chrome_page(
                chrome, user_data, url, temp_dir / "chrome-stderr.log",
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
        print("Browser/native chain comparison passed.")
        print(json.dumps(report, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=(
        "bootstrap-toolchain", "verify-toolchain", "build", "serve", "test"
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
    else:
        build_web()


if __name__ == "__main__":
    main()
