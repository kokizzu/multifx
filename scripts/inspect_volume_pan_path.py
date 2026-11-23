#!/usr/bin/env python3
"""Verify and report the Volume/Pan browser-to-WASM slot mapping."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def string_array(source: str, name: str) -> list[str]:
    match = re.search(rf"const\s+{re.escape(name)}\s*=\s*\[(.*?)\];", source, re.S)
    if match is None:
        raise SystemExit(f"Could not find {name} array")
    return re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', match.group(1))


app = read("web/app.js")
worklet = read("web/audio-worklet.js")
wasm = read("src/web/wasm_chain.cpp")
app_effects = string_array(app, "EFFECT_NAMES")
worklet_effects = string_array(worklet, "EFFECTS")

for anchor, insertion in re.findall(
    r'EFFECTS\.splice\(EFFECTS\.indexOf\("([^"]+)"\),\s*0,\s*"([^"]+)"\);',
    worklet,
):
    if anchor not in worklet_effects:
        raise SystemExit(f"Worklet insertion anchor not found: {anchor}")
    worklet_effects.insert(worklet_effects.index(anchor), insertion)

app_label = "Volume/Pan · project-authored Fractal FM9-family controls"
expected = (
    len(app_effects) == 96
    and app_effects[93] == app_label
    and app_effects[94] == "Output level"
    and app_effects[95] == "Peak limiter"
    and len(worklet_effects) == 96
    and worklet_effects[93:96] == ["volume-pan", "output-level", "peak-limiter"]
)
wasm_has_volume_pan_slot = (
    "std::make_unique<tabletfx::VolumePan>()" in wasm
    and re.search(
        r"static_cast<tabletfx::VolumePan\*>\(\s*effects_\[93\]\.effect\.get\(\)\)",
        wasm,
    ) is not None
)
worklet_handles_settings = (
    'message.type === "volume-pan" && this.ready' in worklet
    and "this.configureVolumePan(message.value)" in worklet
    and "_tf_chain_set_volume_pan" in worklet
    and 'type: "volume-pan", value: values' in app
)
if not expected or not wasm_has_volume_pan_slot or not worklet_handles_settings:
    raise SystemExit(
        "Volume/Pan browser/WASM slot mismatch: "
        f"app={len(app_effects)} rows, Worklet={len(worklet_effects)} slots, "
        f"WASM slot configured={wasm_has_volume_pan_slot}, "
        f"settings routed={worklet_handles_settings}"
    )

print(f"App picker ID 93: {app_effects[93]} (IDs 0–95; {len(app_effects)} total)")
print(f"AudioWorklet ID 93: {worklet_effects[93]} ({len(worklet_effects)} total)")
print("WASM chain: VolumePan is appended in the matching effect slot and configured at effects_[93].")
print("Browser controls: app -> volume-pan message -> AudioWorklet -> tf_chain_set_volume_pan.")
