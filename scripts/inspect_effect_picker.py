#!/usr/bin/env python3
"""Audit the categorized browser effect picker from its rendered HTML rows."""

from collections import Counter
from html import unescape
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
HTML_PATH = ROOT / "web" / "index.html"
MATRIX_PATH = ROOT / "EFFECTS_MATRIX.md"
html = HTML_PATH.read_text(encoding="utf-8")
row_pattern = re.compile(
    r'<label\b(?P<attrs>[^>]*)class="effect-row"(?P<tail>[^>]*)>'
    r"(?P<body>.*?)</label>",
    re.DOTALL,
)

rows: list[tuple[int, str, str]] = []
problems: list[str] = []
for match in row_pattern.finditer(html):
    row_attrs = match.group("attrs") + match.group("tail")
    index_match = re.search(r'data-effect="(\d+)"', match.group("body"))
    category_match = re.search(r'data-category="([^"]+)"', row_attrs)
    name_match = re.search(r'<span class="effect-name">(.*?)</span>', match.group("body"), re.DOTALL)
    if not index_match or not category_match or not name_match:
        problems.append(f"Malformed effect picker row: {match.group(0)[:100]}")
        continue
    rows.append((int(index_match.group(1)), category_match.group(1), unescape(name_match.group(1)).strip()))

if not rows:
    problems.append("No effect rows were found in web/index.html")
else:
    indices = [row[0] for row in rows]
    if len(indices) != len(set(indices)):
        problems.append("Effect picker has duplicate data-effect indices")
    expected = list(range(min(indices), max(indices) + 1))
    missing = sorted(set(expected) - set(indices))
    if missing:
        problems.append(f"Effect picker has missing data-effect indices: {missing}")

filter_match = re.search(
    r'<select\b[^>]*id="effect-category-filter"[^>]*>(.*?)</select>',
    html,
    re.DOTALL,
)
filter_options = (
    re.findall(r'<option\b[^>]*value="([^"]+)"[^>]*>(.*?)</option>', filter_match.group(1), re.DOTALL)
    if filter_match
    else []
)
filter_labels = [unescape(label).strip() for _, label in filter_options]
filter_categories = {value for value, _ in filter_options} - {"all"}
if not filter_match:
    problems.append("Missing effect-category-filter in web/index.html")
else:
    unmapped = sorted({category for _, category, _ in rows} - filter_categories)
    if unmapped:
        problems.append(f"Picker row categories missing from category filter: {unmapped}")

categories = Counter(category for _, category, _ in rows)
print(f"Categorized picker rows: {len(rows)}")
print(f"Category filter choices: {', '.join(filter_labels)}")
for category in sorted(categories):
    print(f"\n{category} ({categories[category]}):")
    for index, row_category, name in rows:
        if row_category == category:
            print(f"  {index:02d} · {name}")

matrix_lines = MATRIX_PATH.read_text(encoding="utf-8").splitlines()
print("\nEffect matrix checkpoints:")
for line_number, line in enumerate(matrix_lines, 1):
    if "categorized browser picker" in line or "`tape-echo`" in line:
        print(f"EFFECTS_MATRIX.md:{line_number}: {line}")

if problems:
    for problem in problems:
        print(f"ERROR: {problem}", file=sys.stderr)
    raise SystemExit(1)
