#!/usr/bin/env python3
"""Focused safety tests for external audio fixture management."""

from __future__ import annotations

import sys
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_fixtures  # noqa: E402


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
