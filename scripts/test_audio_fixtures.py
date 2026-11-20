#!/usr/bin/env python3
"""Focused safety tests for external audio fixture management."""

from __future__ import annotations

import sys
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_fixtures  # noqa: E402
import audio_regression  # noqa: E402


class AudioFixtureSafetyTests(unittest.TestCase):
    def test_rejects_zip_traversal_and_absolute_paths(self) -> None:
        for name in ("../outside.wav", "/absolute.wav", "C:/outside.wav", "folder\\outside.wav"):
            with self.subTest(name=name):
                self.assertFalse(audio_fixtures.safe_archive_member(name))

    def test_accepts_regular_nested_zip_path(self) -> None:
        self.assertTrue(audio_fixtures.safe_archive_member("P3_music/DI/song.wav"))

    def test_creates_external_symlink_and_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            external = root / "external-audio"
            repo.mkdir()
            external.mkdir()
            link = audio_fixtures.ensure_external_link(repo, external, "testdata/reference_audio")
            self.assertTrue(link.is_symlink())
            self.assertEqual(link.resolve(), external.resolve())
            self.assertEqual(audio_fixtures.ensure_external_link(repo, external, "testdata/reference_audio"), link)

    def test_refuses_to_replace_existing_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            external = root / "external-audio"
            repo.mkdir()
            external.mkdir()
            target = repo / "testdata" / "reference_audio"
            target.parent.mkdir(parents=True)
            target.mkdir()
            with self.assertRaises(FileExistsError):
                audio_fixtures.ensure_external_link(repo, external, "testdata/reference_audio")

    def test_refuses_data_directory_inside_repo(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp) / "repo"
            repo.mkdir()
            with self.assertRaises(ValueError):
                audio_fixtures.ensure_external_link(repo, repo / "large-data", "testdata/reference_audio")

    def test_resumes_download_using_http_range(self) -> None:
        payload = b"fixture audio payload" * 10000
        offset = 1234

        class RangeHandler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                requested = self.headers.get("Range", "")
                start = int(requested.removeprefix("bytes=").removesuffix("-")) if requested else 0
                end = min(start + 4095, len(payload) - 1)
                body = payload[start : end + 1]
                self.send_response(206 if requested else 200)
                self.send_header("Content-Length", str(len(body)))
                if requested:
                    self.send_header("Content-Range", f"bytes {start}-{end}/{len(payload)}")
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, format: str, *args: object) -> None:
                return

        server = ThreadingHTTPServer(("127.0.0.1", 0), RangeHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as temp:
                archive_path = Path(temp) / "fixture.zip"
                partial = Path(temp) / "fixture.zip.resume.part"
                partial.write_bytes(payload[:offset])
                asset = {
                    "key": "local-range-test",
                    "url": f"http://127.0.0.1:{server.server_port}/fixture.zip",
                    "md5": hashlib.md5(payload).hexdigest(),
                }
                audio_fixtures.download(asset, archive_path)
                self.assertEqual(archive_path.read_bytes(), payload)
                self.assertFalse(partial.exists())
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_retries_after_server_closes_a_range_early(self) -> None:
        payload = b"longer fixture payload" * 20000
        offset = 1024
        first_range = True

        class EarlyCloseHandler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                nonlocal first_range
                requested = self.headers.get("Range", "")
                start = int(requested.removeprefix("bytes=").removesuffix("-"))
                body = payload[start:]
                self.send_response(206)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Content-Range", f"bytes {start}-{len(payload) - 1}/{len(payload)}")
                self.end_headers()
                if first_range:
                    first_range = False
                    self.wfile.write(body[:4096])
                    self.wfile.flush()
                    self.close_connection = True
                else:
                    self.wfile.write(body)

            def log_message(self, format: str, *args: object) -> None:
                return

        server = ThreadingHTTPServer(("127.0.0.1", 0), EarlyCloseHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as temp:
                archive_path = Path(temp) / "fixture.zip"
                partial = Path(temp) / "fixture.zip.resume.part"
                partial.write_bytes(payload[:offset])
                asset = {
                    "key": "local-short-range-test",
                    "url": f"http://127.0.0.1:{server.server_port}/fixture.zip",
                    "md5": hashlib.md5(payload).hexdigest(),
                }
                audio_fixtures.download(asset, archive_path)
                self.assertEqual(archive_path.read_bytes(), payload)
                self.assertFalse(partial.exists())
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_matches_same_egfx_note_across_all_pickup_settings(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "egfxset-clean"
            folder_names = (
                "Clean_Bridge",
                "Clean_Bridge-Middle",
                "Clean_Middle",
                "Clean_Middle-Neck",
                "Clean_Neck",
            )
            for folder in folder_names:
                directory = root / folder
                directory.mkdir(parents=True)
                (directory / "1-0.wav").write_bytes(b"")
            note, paths = audio_regression.matching_pickup_set(root)
            self.assertEqual(note, "1-0")
            self.assertEqual(set(paths), set(audio_regression.PICKUP_ORDER))

    def test_direct_input_detection_excludes_amp_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "music"
            di_file = root / "Player3" / "DI" / "excerpt.wav"
            amp_file = root / "Player3" / "Amp" / "excerpt.wav"
            di_file.parent.mkdir(parents=True)
            amp_file.parent.mkdir(parents=True)
            di_file.touch()
            amp_file.touch()
            self.assertTrue(audio_regression.is_direct_input(di_file, root))
            self.assertFalse(audio_regression.is_direct_input(amp_file, root))

    def test_extracts_archive_and_places_marker_inside_promoted_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive_path = root / "audio.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("Clean/Bridge/1-0.wav", b"wav fixture")
            destination = root / "extracted"
            audio_fixtures.extract_archive(archive_path, destination)
            self.assertEqual((destination / "Clean/Bridge/1-0.wav").read_bytes(), b"wav fixture")
            self.assertTrue((destination / ".tabletfx-fixture-verified").is_file())
            self.assertFalse(destination.with_name("extracted.extracting").exists())

    def test_recovers_staging_only_after_matching_archive_contents(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive_path = root / "audio.zip"
            payload = b"verified wav fixture"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("Clean/Bridge/1-0.wav", payload)
            destination = root / "extracted"
            staging = destination.with_name("extracted.extracting")
            staged_file = staging / "Clean/Bridge/1-0.wav"
            staged_file.parent.mkdir(parents=True)
            staged_file.write_bytes(payload)
            audio_fixtures.extract_archive(archive_path, destination)
            self.assertEqual((destination / "Clean/Bridge/1-0.wav").read_bytes(), payload)
            self.assertTrue((destination / ".tabletfx-fixture-verified").is_file())
            self.assertFalse(staging.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
