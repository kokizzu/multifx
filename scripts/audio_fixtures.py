#!/usr/bin/env python3
"""Fetch and link licensed, prerecorded guitar fixtures outside the source tree."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tempfile
import urllib.request
import zipfile


ASSETS = (
    {
        "key": "egfxset-clean",
        "filename": "EGFxSet-Clean.zip",
        "url": "https://zenodo.org/records/7044411/files/Clean.zip?download=1",
        "md5": "cdb1b401960f56becc8640387910e78a",
        "compressed_bytes": 431_000_000,
        "folder": "egfxset-clean",
    },
    {
        "key": "guitar-techs-p3-music",
        "filename": "Guitar-TECHS-P3-music.zip",
        "url": "https://zenodo.org/records/14963133/files/P3_music.zip?download=1",
        "md5": "071ba80aecf00f4a31fbd167b3f22198",
        "compressed_bytes": 129_500_000,
        "folder": "guitar-techs-p3-music",
    },
)


def safe_archive_member(name: str) -> bool:
    """Reject absolute, parent-traversing, drive-like, and backslash ZIP paths."""
    if not name or "\\" in name or ":" in name:
        return False
    member = PurePosixPath(name)
    return not member.is_absolute() and all(part not in ("", ".", "..") for part in member.parts)


def md5_file(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolved_link_path(repo_root: Path, link_relative: str) -> Path:
    relative = Path(link_relative)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("Fixture link must be a safe path relative to the repository")
    repo_root = repo_root.resolve()
    link_parent = (repo_root / relative).parent.resolve(strict=False)
    if link_parent != repo_root and repo_root not in link_parent.parents:
        raise ValueError("Fixture link parent must remain inside the repository")
    return link_parent / (repo_root / relative).name


def resolve_data_dir(raw_path: str, repo_root: Path, link_relative: str) -> tuple[Path, str]:
    if raw_path != "auto":
        return Path(raw_path).expanduser().resolve(strict=False), "explicit path"
    configured = os.environ.get("MULTIFX_AUDIO_DATA")
    if configured:
        return Path(configured).expanduser().resolve(strict=False), "MULTIFX_AUDIO_DATA setting"
    existing_link = resolved_link_path(repo_root.resolve(), link_relative)
    if existing_link.is_symlink():
        target = existing_link.resolve(strict=False)
        if target != repo_root.resolve() and repo_root.resolve() not in target.parents:
            return target, "existing local fixture symlink"

    candidates: list[tuple[int, Path]] = []
    media_root = Path("/media")
    try:
        for user_directory in media_root.iterdir():
            if not user_directory.is_dir() or user_directory.is_symlink():
                continue
            for mountpoint in user_directory.iterdir():
                if not mountpoint.is_dir() or mountpoint.is_symlink() or not os.path.ismount(mountpoint):
                    continue
                try:
                    stats = os.statvfs(mountpoint)
                except OSError:
                    continue
                candidates.append((stats.f_bavail * stats.f_frsize, mountpoint))
    except OSError:
        pass
    resumable: list[tuple[int, Path]] = []
    for free_bytes, mountpoint in candidates:
        fixture_dir = mountpoint / "multifx-test-audio"
        if any(fixture_dir.glob("EGFxSet-Clean.zip.*.part")) or (fixture_dir / "EGFxSet-Clean.zip").exists():
            resumable.append((free_bytes, fixture_dir))
    if len(resumable) == 1:
        free_bytes, fixture_dir = resumable[0]
        return fixture_dir.resolve(strict=False), (
            f"resume existing external fixture data ({free_bytes / 1024**3:.1f} GiB available)"
        )
    if len(resumable) > 1:
        _, fixture_dir = max(resumable, key=lambda item: item[0])
        return fixture_dir.resolve(strict=False), "choose among existing external fixture directories by available space"
    if candidates:
        free_bytes, mountpoint = max(candidates, key=lambda item: item[0])
        return (mountpoint / "multifx-test-audio").resolve(strict=False), (
            f"mounted /media volume with most free space ({free_bytes / 1024**3:.1f} GiB available)"
        )
    fallback = Path.home() / ".local" / "share" / "multifx-test-audio"
    return fallback.resolve(strict=False), "home data directory fallback (no mounted /media volume found)"


def ensure_external_link(repo_root: Path, data_dir: Path, link_relative: str) -> Path:
    repo_root = repo_root.resolve()
    data_dir = data_dir.resolve(strict=False)
    link = resolved_link_path(repo_root, link_relative)
    if link == repo_root or repo_root in link.parents:
        pass
    else:
        raise ValueError("Fixture link must remain inside the repository")
    if data_dir == repo_root or repo_root in data_dir.parents:
        raise ValueError("Audio data directory must remain outside the repository")

    link.parent.mkdir(parents=True, exist_ok=True)
    if os.path.lexists(link):
        if link.is_symlink() and link.resolve(strict=False) == data_dir:
            return link
        raise FileExistsError(f"Refusing to replace existing fixture path: {link}")
    link.symlink_to(data_dir, target_is_directory=True)
    return link


def plan(repo_root: Path, data_dir: Path, link_relative: str, selection: str) -> None:
    total = sum(asset["compressed_bytes"] for asset in ASSETS)
    link = resolved_link_path(repo_root.resolve(), link_relative)
    print("Offline reference audio fixture plan (no files will be changed):")
    print(f"Storage selection:       {selection}")
    print(f"External data directory: {data_dir.resolve(strict=False)}")
    print(f"Repository symlink:      {link} -> {data_dir.resolve(strict=False)}")
    for asset in ASSETS:
        archive_path = data_dir / asset["filename"]
        partials = sorted(data_dir.glob(asset["filename"] + ".*.part"))
        print(
            f"- {asset['key']}: {asset['filename']} (~{asset['compressed_bytes'] / 1_000_000:.1f} MB), "
            f"MD5 {asset['md5']}, {asset['url']}"
        )
        if archive_path.exists():
            print(f"  Existing archive: {archive_path.stat().st_size} bytes; fetch will verify its checksum before reuse.")
        for partial in partials:
            print(f"  Resumable partial: {partial} ({partial.stat().st_size} bytes)")
    print(f"Total compressed download: ~{total / 1_000_000:.1f} MB; extracted audio needs additional space.")
    print("The fixture link is local-only and ignored by Git; source archives and extracted audio stay external.")


def download(asset: dict[str, object], archive_path: Path) -> None:
    if archive_path.exists():
        actual = md5_file(archive_path)
        if actual != asset["md5"]:
            raise ValueError(f"Existing archive checksum mismatch; leaving it untouched: {archive_path}")
        print(f"Verified existing archive: {archive_path}")
        return

    partials = sorted(archive_path.parent.glob(archive_path.name + ".*.part"))
    if len(partials) > 1:
        raise RuntimeError(f"Multiple interrupted downloads need review before resuming: {partials}")
    if partials:
        temporary_path = partials[0]
    else:
        fd, temporary_name = tempfile.mkstemp(prefix=archive_path.name + ".", suffix=".part", dir=archive_path.parent)
        os.close(fd)
        temporary_path = Path(temporary_name)

    try:
        expected_total: int | None = None
        while expected_total is None or temporary_path.stat().st_size < expected_total:
            offset = temporary_path.stat().st_size
            headers = {"User-Agent": "TabletFX offline test fixture fetcher/1.0"}
            if offset:
                headers["Range"] = f"bytes={offset}-"
            request = urllib.request.Request(str(asset["url"]), headers=headers)
            with urllib.request.urlopen(request, timeout=300) as response:
                status = getattr(response, "status", response.getcode())
                content_range = response.headers.get("Content-Range", "")
                if status == 206:
                    match = re.fullmatch(r"bytes (\d+)-(\d+)/(\d+)", content_range)
                    if not match:
                        raise RuntimeError(f"Unexpected partial response range: {content_range!r}")
                    range_start, range_end, range_total = map(int, match.groups())
                    if range_start != offset or range_end < range_start or range_end >= range_total:
                        raise RuntimeError(
                            f"Unexpected partial response range {content_range!r} for offset {offset}"
                        )
                    if expected_total is not None and expected_total != range_total:
                        raise RuntimeError(
                            f"Fixture size changed during download: {expected_total} vs {range_total} bytes"
                        )
                    expected_total = range_total
                    expected_chunk = range_end - range_start + 1
                    content_length = response.headers.get("Content-Length")
                    if content_length is not None and int(content_length) != expected_chunk:
                        raise RuntimeError(
                            f"Content-Length {content_length} disagrees with Content-Range {content_range!r}"
                        )
                    mode = "ab" if offset else "wb"
                elif status == 200 and offset == 0:
                    content_length = response.headers.get("Content-Length")
                    expected_total = int(content_length) if content_length is not None else None
                    expected_chunk = expected_total
                    mode = "wb"
                elif status == 200:
                    preserved = temporary_path.with_suffix(".range-unsupported")
                    temporary_path.rename(preserved)
                    fd, replacement_name = tempfile.mkstemp(
                        prefix=archive_path.name + ".restart.", suffix=".part", dir=archive_path.parent
                    )
                    os.close(fd)
                    temporary_path = Path(replacement_name)
                    expected_total = int(response.headers["Content-Length"])
                    expected_chunk = expected_total
                    mode = "wb"
                else:
                    raise RuntimeError(f"Unexpected HTTP status while downloading fixture: {status}")

                transferred = temporary_path.stat().st_size if mode == "ab" else 0
                next_report = transferred + 1024 * 1024
                received = 0
                with temporary_path.open(mode) as output:
                    for chunk in iter(lambda: response.read(1024 * 1024), b""):
                        output.write(chunk)
                        transferred += len(chunk)
                        received += len(chunk)
                        if transferred >= next_report:
                            print(
                                f"Download progress {asset['key']}: {transferred / 1_000_000:.1f} MB",
                                flush=True,
                            )
                            next_report = transferred + 1024 * 1024
                    output.flush()
                    os.fsync(output.fileno())
                if expected_chunk is not None and received != expected_chunk:
                    raise RuntimeError(
                        f"Incomplete HTTP response for {asset['key']}: received {received} of "
                        f"{expected_chunk} bytes"
                    )
            if expected_total is not None and temporary_path.stat().st_size > expected_total:
                raise RuntimeError(
                    f"Downloaded fixture exceeds advertised size {expected_total}: "
                    f"{temporary_path.stat().st_size} bytes"
                )
    except Exception:
        print(f"Download stopped; partial file preserved for inspection: {temporary_path}", file=sys.stderr)
        raise
    actual = md5_file(temporary_path)
    if actual != asset["md5"]:
        raise ValueError(
            f"Downloaded archive checksum mismatch for {temporary_path}: expected {asset['md5']}, got {actual}. "
            "The file is preserved for inspection and will not be extracted."
        )
    try:
        os.link(temporary_path, archive_path)
    except FileExistsError:
        raise FileExistsError(f"A file appeared during download; leaving both files untouched: {archive_path}")
    temporary_path.unlink()
    print(f"Downloaded and verified: {archive_path}")


def extract_archive(archive_path: Path, destination: Path) -> None:
    marker = destination / ".tabletfx-fixture-verified"
    if marker.is_file():
        print(f"Verified extracted fixture already exists: {destination}")
        return
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite incomplete or unverified fixture directory: {destination}")

    staging = destination.with_name(destination.name + ".extracting")
    if staging.exists():
        raise FileExistsError(f"Refusing to reuse existing extraction staging directory: {staging}")
    staging.mkdir(parents=True)
    try:
        with zipfile.ZipFile(archive_path) as archive:
            members = archive.infolist()
            for info in members:
                if not safe_archive_member(info.filename):
                    raise ValueError(f"Unsafe archive path: {info.filename!r}")
                if (info.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError(f"Symbolic links are not accepted in the data archive: {info.filename!r}")
            for info in members:
                if info.is_dir():
                    (staging / PurePosixPath(info.filename)).mkdir(parents=True, exist_ok=True)
                    continue
                target = staging / PurePosixPath(info.filename)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output, length=1024 * 1024)
        wave_count = sum(1 for item in staging.rglob("*") if item.is_file() and item.suffix.lower() in {".wav", ".flac"})
        if wave_count == 0:
            raise ValueError(f"No WAV or FLAC audio found after extracting {archive_path}")
        marker.write_text(f"archive={archive_path.name}\naudio_files={wave_count}\n", encoding="utf-8")
        staging.rename(destination)
        print(f"Extracted {wave_count} audio files: {destination}")
    except Exception:
        print(f"Extraction stopped; review preserved staging directory: {staging}", file=sys.stderr)
        raise


def fetch(repo_root: Path, data_dir: Path, link_relative: str, selection: str) -> None:
    repo_root = repo_root.resolve()
    data_dir = data_dir.resolve(strict=False)
    if repo_root == data_dir or repo_root in data_dir.parents:
        raise ValueError("Audio data directory must be outside the repository")
    data_dir.mkdir(parents=True, exist_ok=True)
    print(f"Storage selection: {selection}")
    for asset in ASSETS:
        archive_path = data_dir / asset["filename"]
        download(asset, archive_path)
        extract_archive(archive_path, data_dir / str(asset["folder"]))
    link = ensure_external_link(repo_root, data_dir, link_relative)
    print(f"Local fixture symlink ready: {link} -> {data_dir}")


def verify(repo_root: Path, data_dir: Path, link_relative: str) -> None:
    repo_root = repo_root.resolve()
    data_dir = data_dir.resolve(strict=False)
    link = resolved_link_path(repo_root, link_relative)
    if not link.is_symlink() or link.resolve(strict=False) != data_dir:
        raise SystemExit(f"Expected fixture symlink {link} -> {data_dir}; it is missing or points elsewhere")
    for asset in ASSETS:
        archive = data_dir / asset["filename"]
        if not archive.is_file() or md5_file(archive) != asset["md5"]:
            raise SystemExit(f"Missing or invalid archive: {archive}")
        extracted = data_dir / str(asset["folder"])
        if not (extracted / ".tabletfx-fixture-verified").is_file():
            raise SystemExit(f"Missing extraction verification marker: {extracted}")
        count = sum(1 for item in extracted.rglob("*") if item.is_file() and item.suffix.lower() in {".wav", ".flac"})
        if count == 0:
            raise SystemExit(f"No audio files found in {extracted}")
        print(f"OK {asset['key']}: archive checksum valid; {count} audio files available")
    print(f"OK local-only Git-ignored symlink: {link} -> {data_dir}")


def inspect(data_dir: Path) -> None:
    print(f"External audio fixtures: {data_dir.resolve(strict=False)}")
    found_any = False
    for asset in ASSETS:
        extracted = data_dir / str(asset["folder"])
        if not extracted.is_dir():
            print(f"- {asset['key']}: not extracted yet")
            continue
        files = sorted(
            item for item in extracted.rglob("*")
            if item.is_file() and item.suffix.lower() in {".wav", ".flac"}
        )
        found_any = found_any or bool(files)
        print(f"- {asset['key']}: {len(files)} audio files")
        for item in files[:12]:
            print(f"  {item.relative_to(data_dir)}")
        if len(files) > 12:
            print(f"  ... and {len(files) - 12} more")
    if not found_any:
        raise SystemExit("No extracted WAV or FLAC files found; run make audio-fixtures-fetch first")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("plan", "fetch", "verify", "inspect"))
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--data-dir", default="auto")
    parser.add_argument("--link", required=True)
    args = parser.parse_args()
    data_dir, selection = resolve_data_dir(args.data_dir, args.repo_root, args.link)
    if args.mode == "plan":
        plan(args.repo_root, data_dir, args.link, selection)
    elif args.mode == "fetch":
        fetch(args.repo_root, data_dir, args.link, selection)
    elif args.mode == "verify":
        verify(args.repo_root, data_dir, args.link)
    else:
        inspect(data_dir)


if __name__ == "__main__":
    main()
