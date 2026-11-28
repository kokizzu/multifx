#!/usr/bin/env python3
"""Validate the compact continuation ledger used by the effects audit."""

from __future__ import annotations

import json
from pathlib import Path
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]
STATE_PATH = ROOT / "research-state.json"
REQUIRED_KEYS = {
    "schema_version",
    "updated",
    "repository",
    "primary_branch",
    "workflow",
    "previous_checkpoint_commit",
    "latest_audit",
    "continuation_cursor",
    "completed_checkpoints",
    "previous_checkpoint",
    "current_checkpoint",
    "next_research",
    "active_goal_checkpoints",
    "constraints",
}


def fail(message: str) -> None:
    raise SystemExit(f"Research state validation failed: {message}")


try:
    state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError) as error:
    fail(str(error))

if not isinstance(state, dict):
    fail("top-level value must be an object")
missing = REQUIRED_KEYS - state.keys()
if missing:
    fail(f"missing top-level keys: {', '.join(sorted(missing))}")
if state["repository"] != "https://github.com/kokizzu/multifx":
    fail(f"unexpected repository: {state['repository']!r}")
if state["primary_branch"] != "main":
    fail(f"unexpected primary branch: {state['primary_branch']!r}")

checkpoints = state["completed_checkpoints"]
if not isinstance(checkpoints, list):
    fail("completed_checkpoints must be a list")
names: list[str] = []
for index, checkpoint in enumerate(checkpoints):
    if not isinstance(checkpoint, dict):
        fail(f"completed_checkpoints[{index}] must be an object")
    name = checkpoint.get("name")
    if not isinstance(name, str) or not name.strip():
        fail(f"completed_checkpoints[{index}] has no name")
    names.append(name)
    source = checkpoint.get("official_source", {})
    # Older history entries used different source shapes. Validate URL fields
    # when present, without rewriting or rejecting those preserved records.
    urls = source.get("urls", []) if isinstance(source, dict) else []
    if not isinstance(urls, list):
        fail(f"completed_checkpoints[{index}].official_source.urls must be a list")
    for url in urls:
        parsed = urlparse(url) if isinstance(url, str) else None
        if parsed is None or parsed.scheme not in {"http", "https"} or not parsed.netloc:
            fail(f"completed_checkpoints[{index}] has an invalid source URL")

current = state["current_checkpoint"]
latest = state["latest_audit"]
if not isinstance(current, dict) or not isinstance(latest, dict):
    fail("current_checkpoint and latest_audit must be objects")
if current.get("name") != latest.get("name"):
    fail("current_checkpoint and latest_audit do not refer to the same item")
if current.get("name") not in names:
    fail("current checkpoint is absent from completed_checkpoints")

cursor = state["continuation_cursor"]
if not isinstance(cursor, dict) or not isinstance(cursor.get("next_feature"), str):
    fail("continuation_cursor.next_feature must be text")
next_research = state["next_research"]
if not isinstance(next_research, list):
    fail("next_research must be a list")
for index, item in enumerate(next_research):
    if not isinstance(item, (dict, str)):
        fail(f"next_research[{index}] must be an object or a text item")

print(
    f"Research state verified: {len(checkpoints)} completed checkpoints, "
    f"{len(next_research)} queued research items; current: {current['name']}."
)
