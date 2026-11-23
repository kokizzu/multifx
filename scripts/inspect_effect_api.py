#!/usr/bin/env python3
"""Print bounded source examples for implementing an effect in this project."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = (
    ("include/tabletfx/effect.hpp", 180),
    ("include/tabletfx/effects/tremolo.hpp", 180),
    ("src/effects/tremolo.cpp", 220),
    ("tests/test_tremolo.cpp", 220),
)


def main() -> None:
    for relative, limit in EXAMPLES:
        path = ROOT / relative
        print(f"===== {relative} =====")
        lines = path.read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines[:limit], start=1):
            print(f"{number:4}: {line}")
        if len(lines) > limit:
            print(f"... truncated after {limit} of {len(lines)} lines")


if __name__ == "__main__":
    main()

# The Stereo Filter checkpoint is intentionally included in the shared bounded
# API examples so integration code can map its project-authored controls.
from pathlib import Path as _Path

_stereo_filter_header = _Path(__file__).resolve().parents[1] / "include/tabletfx/effects/stereo_filter.hpp"
if _stereo_filter_header.is_file():
    print("===== include/tabletfx/effects/stereo_filter.hpp (bounded API example) =====")
    for _line_number, _line in enumerate(_stereo_filter_header.read_text().splitlines()[:180], 1):
        print(f"{_line_number:4}: {_line}")

_stereo_filter_source = _Path(__file__).resolve().parents[1] / "src/effects/stereo_filter.cpp"
if _stereo_filter_source.is_file():
    _source_lines = _stereo_filter_source.read_text().splitlines()
    _selected = set()
    for _index, _line in enumerate(_source_lines):
        if any(_term in _line for _term in ("parameters.", "kMinimum", "kMaximum", "bounded(")):
            _selected.update(range(max(0, _index - 1), min(len(_source_lines), _index + 2)))
    print("===== src/effects/stereo_filter.cpp (parameter bounds) =====")
    for _index in sorted(_selected):
        print(f"{_index + 1:4}: {_source_lines[_index]}")

_stereo_filter_test = _Path(__file__).resolve().parents[1] / "tests/test_stereo_filter.cpp"
if _stereo_filter_test.is_file():
    _test_lines = _stereo_filter_test.read_text().splitlines()
    print("===== tests/test_stereo_filter.cpp (test names) =====")
    for _line_number, _line in enumerate(_test_lines, 1):
        if "void test_" in _line:
            print(f"{_line_number:4}: {_line}")

_wasm_source = _Path(__file__).resolve().parents[1] / "src/web/wasm_chain.cpp"
if _wasm_source.is_file():
    _wasm_lines = _wasm_source.read_text().splitlines()
    for _label, _needle in (
        ("enable routing", "if (index == 96U)"),
        ("effect processing", "void process_effect(std::uint32_t index)"),
        ("Stereo Filter configuration", "void configure_stereo_filter("),
        ("C ABI enable export", "tf_chain_set_enabled(void* opaque"),
        ("C ABI Stereo Filter export", "tf_chain_set_stereo_filter("),
        ("effect count", "std::size_t effect_count() const noexcept"),
    ):
        _index = next((i for i, _line in enumerate(_wasm_lines) if _needle in _line), -1)
        if _index >= 0:
            print(f"===== src/web/wasm_chain.cpp ({_label}) =====")
            for _line_index in range(max(0, _index - 4), min(len(_wasm_lines), _index + 30)):
                print(f"{_line_index + 1:4}: {_wasm_lines[_line_index]}")

_worklet_source = _Path(__file__).resolve().parents[1] / "web/audio-worklet.js"
if _worklet_source.is_file():
    _worklet_lines = _worklet_source.read_text().splitlines()
    _index = next((i for i, _line in enumerate(_worklet_lines)
                   if "_tf_chain_process_effect(this.chain, effect)" in _line), -1)
    if _index >= 0:
        print("===== web/audio-worklet.js (filter/output processing order) =====")
        for _line_index in range(max(0, _index - 4), min(len(_worklet_lines), _index + 24)):
            print(f"{_line_index + 1:4}: {_worklet_lines[_line_index]}")
