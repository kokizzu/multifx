import hashlib
import unittest

from checkpoint_line_selectors import LineSelectionError, select_exact_line_operations


class ExactLineSelectorTests(unittest.TestCase):
    def select(self, body: str, operations: tuple[str, ...]) -> str:
        return select_exact_line_operations(
            "diff --git a/shared.txt b/shared.txt\n"
            "index 1111111..2222222 100644\n"
            "--- a/shared.txt\n"
            "+++ b/shared.txt\n"
            + body,
            operations,
        )

    def test_shared_hunk_keeps_only_whitelisted_addition(self) -> None:
        patch = self.select(
            "@@ -4,0 +5,3 @@ section\n"
            "+unrelated neighboring effect\n"
            "+feature-specific line\n"
            "+another neighboring effect\n",
            ("+feature-specific line",),
        )
        self.assertIn("@@ -4,0 +5 @@ section", patch)
        self.assertIn("+feature-specific line\n", patch)
        self.assertNotIn("unrelated neighboring effect", patch)

    def test_replacement_keeps_exact_delete_and_add(self) -> None:
        patch = self.select(
            "@@ -8 +8 @@ control\n"
            "-old project-only default\n"
            "+new project-only default\n",
            ("-old project-only default", "+new project-only default"),
        )
        self.assertIn("@@ -8 +8 @@ control", patch)
        self.assertIn("-old project-only default\n", patch)
        self.assertIn("+new project-only default\n", patch)

    def test_duplicate_exact_line_is_ambiguous(self) -> None:
        with self.assertRaisesRegex(LineSelectionError, "listed multiplicity"):
            self.select(
                "@@ -0,0 +1,2 @@\n+same feature line\n+same feature line\n",
                ("+same feature line",),
            )

    def test_deletion_only_operation(self) -> None:
        patch = self.select(
            "@@ -11 +10,0 @@\n-unwanted feature line\n",
            ("-unwanted feature line",),
        )
        self.assertIn("@@ -11 +10,0 @@", patch)
        self.assertIn("-unwanted feature line\n", patch)

    def test_missing_operation_fails_closed(self) -> None:
        with self.assertRaisesRegex(LineSelectionError, "listed multiplicity"):
            self.select(
                "@@ -2,0 +3,1 @@\n+other line\n",
                ("+expected feature line",),
            )

    def test_selector_rejects_non_diff_operations(self) -> None:
        with self.assertRaisesRegex(LineSelectionError, "complete added/deleted"):
            self.select("@@ -0,0 +1,0 @@\n", ("feature line",))

    def test_exact_span_excludes_adjacent_additions(self) -> None:
        patch = self.select(
            "@@ -4,0 +5,4 @@ section\n"
            "+unrelated neighboring effect\n"
            "+feature block starts\n"
            "+feature block ends\n"
            "+another neighboring effect\n",
            ({"span": {"start": "+feature block starts", "end": "+feature block ends"}},),
        )
        self.assertIn("@@ -4,0 +5,2 @@ section", patch)
        self.assertIn("+feature block starts\n", patch)
        self.assertIn("+feature block ends\n", patch)
        self.assertNotIn("neighboring effect", patch)

    def test_ambiguous_span_marker_fails_closed(self) -> None:
        with self.assertRaisesRegex(LineSelectionError, "markers must each occur once"):
            self.select(
                "@@ -4,0 +5,3 @@ section\n"
                "+feature starts\n"
                "+feature ends\n"
                "+feature ends\n",
                ({"span": {"start": "+feature starts", "end": "+feature ends"}},),
            )


class BoundedSpanSelectorTests(unittest.TestCase):
    def effect_count_replacement(self):
        return ({
            "replace": {
                "before": "-        return effects_.size() + 27;",
                "current": "+    return effects_.size() + 33;",
                "selected": "+    return effects_.size() + 28;",
                "hunk_header": "@@ -460 +474,3 @@ public:",
                "allow_intervening_added_lines": True,
                "allow_cross_hunk_staged": True,
            }
        },)

    def test_hunk_bounded_span_selects_repeated_end_by_occurrence(self) -> None:
        diff = (
            "diff --git a/a.cpp b/a.cpp\n"
            "--- a/a.cpp\n"
            "+++ b/a.cpp\n"
            "@@ -4,0 +4,6 @@\n"
            "+unrelated begin\n"
            "+}\n"
            "+feature begin\n"
            "+feature body\n"
            "+}\n"
            "+unrelated end\n"
        )
        operations = ({
            "span": {
                "hunk_header": "@@ -4,0 +4,6 @@",
                "start": "+feature begin",
                "end": "+}",
                "end_occurrence": 2,
                "expected_line_count": 3,
            },},)
        selected = select_exact_line_operations(diff, operations)
        self.assertIn("+feature begin\n+feature body\n+}\n", selected)
        self.assertNotIn("+unrelated", selected)

    def test_hunk_bounded_span_rejects_changed_length(self) -> None:
        diff = (
            "diff --git a/a.cpp b/a.cpp\n"
            "--- a/a.cpp\n"
            "+++ b/a.cpp\n"
            "@@ -4,0 +4,3 @@\n"
            "+feature begin\n"
            "+feature body\n"
            "+}\n"
        )
        operations = ({
            "span": {
                "hunk_header": "@@ -4,0 +4,3 @@",
                "start": "+feature begin",
                "end": "+}",
                "expected_line_count": 4,
            },},)
        with self.assertRaises(LineSelectionError):
            select_exact_line_operations(diff, operations)

    def test_hashed_range_selects_exact_change_lines(self) -> None:
        selected_lines = "+feature begin\n+feature body\n"
        diff = (
            "diff --git a/a.cpp b/a.cpp\n"
            "--- a/a.cpp\n"
            "+++ b/a.cpp\n"
            "@@ -4,0 +4,3 @@\n"
            "+feature begin\n"
            "+feature body\n"
            "+unrelated\n"
        )
        operations = ({
            "range": {
                "hunk_header": "@@ -4,0 +4,3 @@",
                "start_index": 0,
                "end_index": 2,
                "sha256": hashlib.sha256(selected_lines.encode()).hexdigest(),
            },},)
        selected = select_exact_line_operations(diff, operations)
        self.assertIn(selected_lines, selected)
        self.assertNotIn("+unrelated", selected)

    def test_hashed_range_rejects_changed_line(self) -> None:
        diff = (
            "diff --git a/a.cpp b/a.cpp\n"
            "--- a/a.cpp\n"
            "+++ b/a.cpp\n"
            "@@ -4,0 +4,1 @@\n"
            "+changed\n"
        )
        operations = ({
            "range": {
                "hunk_header": "@@ -4,0 +4,1 @@",
                "start_index": 0,
                "end_index": 1,
                "sha256": hashlib.sha256(b"+original\n").hexdigest(),
            },},)
        with self.assertRaisesRegex(LineSelectionError, "SHA-256 whitelist"):
            select_exact_line_operations(diff, operations)

    def test_start_relative_line_count_selects_only_reviewed_operations(self) -> None:
        diff = (
            "diff --git a/FLAGSHIP_EFFECT_COMPARISON.md b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "--- a/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "+++ b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "@@ -4,0 +5,7 @@\n"
            "+feature heading\n"
            "+\n"
            "+feature evidence\n"
            "+\n"
            "+feature summary\n"
            "+\n"
            "+later effect heading\n"
        )
        selected = select_exact_line_operations(
            diff,
            ({"span": {"start": "+feature heading", "line_count": 6}},),
        )
        self.assertIn("+feature summary\n+\n", selected)
        self.assertNotIn("later effect heading", selected)

    def test_start_relative_line_count_rejects_truncated_hunk(self) -> None:
        diff = (
            "diff --git a/FLAGSHIP_EFFECT_COMPARISON.md b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "--- a/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "+++ b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "@@ -4,0 +5,2 @@\n"
            "+feature heading\n"
            "+feature evidence\n"
        )
        with self.assertRaisesRegex(LineSelectionError, "extends beyond"):
            select_exact_line_operations(
                diff,
                ({"span": {"start": "+feature heading", "line_count": 3}},),
            )

    def test_start_relative_line_count_accepts_exact_hunk_header_variants(self):
        diff = (
            "diff --git a/wasm.cpp b/wasm.cpp\n"
            "--- a/wasm.cpp\n"
            "+++ b/wasm.cpp\n"
            "@@ -10,0 +12,2 @@ staged insertion\n"
            "+if (index == 122U) {\n"
            "+  resonator_only();\n"
            "@@ -20,0 +24,2 @@ unrelated insertion\n"
            "+if (index == 122U) {\n"
            "+  unrelated_effect();\n"
        )
        selected = select_exact_line_operations(
            diff,
            ({
                "span": {
                    "start": "+if (index == 122U) {",
                    "line_count": 2,
                    "hunk_headers": [
                        "@@ -10,0 +12,2 @@ staged insertion",
                        "@@ -11,0 +13,3 @@ worktree insertion",
                    ],
                }
            },),
        )
        self.assertIn("+  resonator_only();", selected)
        self.assertNotIn("unrelated_effect", selected)

    def test_staged_replacement_accepts_unique_values_across_adjacent_hunks(self):
        staged = (
            "diff --git a/src/web/wasm_chain.cpp b/src/web/wasm_chain.cpp\n"
            "--- a/src/web/wasm_chain.cpp\n"
            "+++ b/src/web/wasm_chain.cpp\n"
            "@@ -460 +462,0 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "@@ -461,0 +464 @@ public:\n"
            "+    return effects_.size() + 28;\n"
        )
        selected = select_exact_line_operations(staged, self.effect_count_replacement())
        self.assertIn("-        return effects_.size() + 27;", selected)
        self.assertIn("+    return effects_.size() + 28;", selected)
        self.assertEqual(
            select_exact_line_operations(selected, self.effect_count_replacement()),
            selected,
        )

    def test_unstaged_replacement_keeps_original_hunk_scope(self):
        worktree = (
            "diff --git a/src/web/wasm_chain.cpp b/src/web/wasm_chain.cpp\n"
            "--- a/src/web/wasm_chain.cpp\n"
            "+++ b/src/web/wasm_chain.cpp\n"
            "@@ -460 +474,3 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "+    // adjacent selector context\n"
            "+    return effects_.size() + 33;\n"
        )
        selected = select_exact_line_operations(
            worktree, self.effect_count_replacement()
        )
        self.assertIn("+    return effects_.size() + 28;", selected)
        self.assertNotIn("+33;", selected)

    def test_cross_hunk_staged_replacement_rejects_duplicate_selected_value(self):
        staged = (
            "diff --git a/src/web/wasm_chain.cpp b/src/web/wasm_chain.cpp\n"
            "--- a/src/web/wasm_chain.cpp\n"
            "+++ b/src/web/wasm_chain.cpp\n"
            "@@ -460 +462,0 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "@@ -461,0 +464 @@ public:\n"
            "+    return effects_.size() + 28;\n"
            "@@ -700,0 +704 @@ public:\n"
            "+    return effects_.size() + 28;\n"
        )
        with self.assertRaisesRegex(
            LineSelectionError,
            "staged replacement value must occur exactly once",
        ):
            select_exact_line_operations(staged, self.effect_count_replacement())

    def test_cross_hunk_staged_replacement_rejects_duplicate_deleted_value(self):
        staged = (
            "diff --git a/src/web/wasm_chain.cpp b/src/web/wasm_chain.cpp\n"
            "--- a/src/web/wasm_chain.cpp\n"
            "+++ b/src/web/wasm_chain.cpp\n"
            "@@ -460 +462,0 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "@@ -500 +502,0 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "@@ -461,0 +464 @@ public:\n"
            "+    return effects_.size() + 28;\n"
        )
        with self.assertRaisesRegex(
            LineSelectionError,
            "replacement deleted line must occur exactly once",
        ):
            select_exact_line_operations(staged, self.effect_count_replacement())


class AppCheckpointSelectorTests(unittest.TestCase):
    def test_stage_resume_skips_only_preverified_line_selected_paths(self):
        from checkpoint_line_selectors import PARTIAL_CHECKPOINT_LINE_SELECTORS
        from publish_checkpoint import already_verified_line_selected_path

        line_selected = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]
        staged = {"CMakeLists.txt", "apps/resonator_bench.cpp"}
        self.assertTrue(
            already_verified_line_selected_path(
                "CMakeLists.txt", staged, line_selected
            )
        )
        self.assertFalse(
            already_verified_line_selected_path(
                "FLAGSHIP_EFFECT_COMPARISON.md", staged, line_selected
            )
        )
        self.assertFalse(
            already_verified_line_selected_path(
                "apps/resonator_bench.cpp", staged, line_selected
            )
        )

    def test_single_line_hunk_header_matches_git_omission(self):
        diff = (
            "diff --git a/CMakeLists.txt b/CMakeLists.txt\n"
            "--- a/CMakeLists.txt\n"
            "+++ b/CMakeLists.txt\n"
            "@@ -21,0 +22 @@ add_library(tabletfx_core\n"
            "+    src/effects/resonator.cpp\n"
        )
        selected = select_exact_line_operations(
            diff, ("+    src/effects/resonator.cpp",)
        )
        self.assertIn("@@ -21,0 +22 @@", selected)
        self.assertNotIn("@@ -21,0 +22,1 @@", selected)

    def test_resonator_checklist_selector_excludes_trailing_blank_line(self):
        from checkpoint_line_selectors import (
            PARTIAL_CHECKPOINT_LINE_SELECTORS,
            select_checkpoint_line_patch,
        )

        span = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["IMPLEMENTATION_CHECKLIST.md"][0]["span"]
        changed = [span["start"]]
        changed.extend(f"+  checklist item {index}" for index in range(9))
        changed.append(span["end"])
        changed.append("+")
        diff = (
            "diff --git a/IMPLEMENTATION_CHECKLIST.md b/IMPLEMENTATION_CHECKLIST.md\n"
            "--- a/IMPLEMENTATION_CHECKLIST.md\n"
            "+++ b/IMPLEMENTATION_CHECKLIST.md\n"
            f"@@ -1,0 +1,{len(changed)} @@\n"
            + "".join(line + "\n" for line in changed)
        )
        selected = select_checkpoint_line_patch(
            "fractal-resonator-effect", "IMPLEMENTATION_CHECKLIST.md", diff
        )
        self.assertIn(span["end"], selected)
        self.assertNotIn("+\n", selected)

    def test_resonator_flagship_selector_uses_six_line_start_span(self):
        from checkpoint_line_selectors import (
            PARTIAL_CHECKPOINT_LINE_SELECTORS,
            select_checkpoint_line_patch,
        )

        span = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["FLAGSHIP_EFFECT_COMPARISON.md"][0]["span"]
        changed = [span["start"]]
        changed.extend(f"+  Resonator detail {index}" for index in range(4))
        changed.append("+")
        changed.append("+## Unstaged later effect")
        diff = (
            "diff --git a/FLAGSHIP_EFFECT_COMPARISON.md b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "--- a/FLAGSHIP_EFFECT_COMPARISON.md\n"
            "+++ b/FLAGSHIP_EFFECT_COMPARISON.md\n"
            f"@@ -4,0 +5,{len(changed)} @@\n"
            + "".join(line + "\n" for line in changed)
        )
        selected = select_checkpoint_line_patch(
            "fractal-resonator-effect", "FLAGSHIP_EFFECT_COMPARISON.md", diff
        )
        self.assertEqual(
            sum(line.startswith("+") and not line.startswith("+++") for line in selected.splitlines()),
            6,
        )
        self.assertNotIn("Unstaged later effect", selected)

    def test_resonator_worklet_spans_match_staged_and_worktree_hunks(self):
        from checkpoint_line_selectors import PARTIAL_CHECKPOINT_LINE_SELECTORS

        worklet_operations = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["web/audio-worklet.js"]
        spans = [
            operation["span"]
            for operation in worklet_operations
            if isinstance(operation, dict) and "span" in operation
        ]
        self.assertEqual([span["line_count"] for span in spans], [11, 16, 7])
        for span_index, span in enumerate(spans):
            for phase, new_start in (("staged", 20), ("worktree", 200)):
                changed = [span["start"]]
                changed.extend(
                    f"+  Resonator block {span_index} line {line_index}"
                    for line_index in range(span["line_count"] - 1)
                )
                changed.append("+following unrelated effect")
                deleted_count = sum(line.startswith("-") for line in changed)
                added_count = sum(line.startswith("+") for line in changed)
                diff = (
                    "diff --git a/web/audio-worklet.js b/web/audio-worklet.js\n"
                    "--- a/web/audio-worklet.js\n"
                    "+++ b/web/audio-worklet.js\n"
                    f"@@ -10,{deleted_count} +{new_start},{added_count} @@ {phase}\n"
                    + "".join(line + "\n" for line in changed)
                )
                selected = select_exact_line_operations(diff, ({"span": span},))
                selected_changes = [
                    line for line in selected.splitlines()
                    if line.startswith(("+", "-"))
                    and not line.startswith(("+++", "---"))
                ]
                self.assertEqual(len(selected_changes), span["line_count"])
                self.assertNotIn("following unrelated effect", selected)

    def test_resonator_index_controls_span_matches_staged_hunk(self):
        from checkpoint_line_selectors import (
            PARTIAL_CHECKPOINT_LINE_SELECTORS,
            select_checkpoint_line_patch,
        )

        operations = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["web/index.html"]
        row = next(operation for operation in operations if isinstance(operation, str))
        span = next(operation["span"] for operation in operations if isinstance(operation, dict))
        changed = [row, span["start"]]
        changed.extend(f"+      control line {index}" for index in range(span["line_count"] - 1))
        changed.append('+      <div aria-label="later effect controls">')
        diff = (
            "diff --git a/web/index.html b/web/index.html\n"
            "--- a/web/index.html\n"
            "+++ b/web/index.html\n"
            f"@@ -672,0 +674,{len(changed)} @@\n"
            + "".join(line + "\n" for line in changed)
        )
        selected = select_checkpoint_line_patch(
            "fractal-resonator-effect", "web/index.html", diff
        )
        selected_changes = [
            line for line in selected.splitlines()
            if line.startswith(("+", "-")) and not line.startswith(("+++", "---"))
        ]
        self.assertEqual(len(selected_changes), 61)
        self.assertNotIn("later effect controls", selected)

    def test_resonator_worklet_span_rejects_truncation(self):
        from checkpoint_line_selectors import PARTIAL_CHECKPOINT_LINE_SELECTORS

        span = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["web/audio-worklet.js"][1]["span"]
        changed = [span["start"]]
        changed.extend(
            f"+  partial line {index}"
            for index in range(span["line_count"] - 2)
        )
        diff = (
            "diff --git a/web/audio-worklet.js b/web/audio-worklet.js\n"
            "--- a/web/audio-worklet.js\n"
            "+++ b/web/audio-worklet.js\n"
            f"@@ -1,0 +1,{len(changed)} @@\n"
            + "".join(line + "\n" for line in changed)
        )
        with self.assertRaisesRegex(LineSelectionError, "extends beyond"):
            select_exact_line_operations(diff, ({"span": span},))

    def test_scoped_line_rejects_duplicate_calls_in_target_function(self):
        diff = (
            "diff --git a/web/app.js b/web/app.js\n"
            "--- a/web/app.js\n"
            "+++ b/web/app.js\n"
            "@@ -10,0 +11,2 @@ async function runMercatorDistortionBrowserSmokeTest(reference)\n"
            "+    await stopBrowserSmokePlaybackAndWait();\n"
            "+    await stopBrowserSmokePlaybackAndWait();\n"
        )
        with self.assertRaisesRegex(LineSelectionError, "scoped line multiplicity changed"):
            select_exact_line_operations(
                diff,
                ({
                    "line": {
                        "operation": "+    await stopBrowserSmokePlaybackAndWait();",
                        "hunk_context": "async function runMercatorDistortionBrowserSmokeTest",
                        "expected_count": 1,
                    }
                },),
            )

    def test_start_relative_span_selects_the_first_closing_operation(self):
        diff = (
            "diff --git a/web/app.js b/web/app.js\n"
            "--- a/web/app.js\n"
            "+++ b/web/app.js\n"
            "@@ -10,0 +11,5 @@\n"
            "+}\n"
            "+function updateResonator() {\n"
            "+  call();\n"
            "+}\n"
            "+}\n"
        )
        selected = select_exact_line_operations(
            diff,
            ({
                "span": {
                    "start": "+function updateResonator() {",
                    "end": "+}",
                    "end_after_start": True,
                    "expected_line_count": 3,
                }
            },),
        )
        self.assertIn("+function updateResonator() {", selected)
        self.assertIn("+  call();", selected)
        self.assertEqual(selected.count("+}\n"), 1)
        self.assertNotIn("+}\n+function", selected)

    def test_replacement_selector_accepts_its_staged_value(self):
        operations = ({
            "replace": {
                "before": "-  const rows = 117;",
                "current": "+  const rows = 127;",
                "selected": "+  const rows = 123;",
            }
        },)
        diff = (
            "diff --git a/web/app.js b/web/app.js\n"
            "--- a/web/app.js\n"
            "+++ b/web/app.js\n"
            "@@ -10,1 +10,1 @@\n"
            "-  const rows = 117;\n"
            "+  const rows = 127;\n"
        )
        planned = select_exact_line_operations(diff, operations)
        self.assertIn("+  const rows = 123;", planned)
        self.assertEqual(select_exact_line_operations(planned, operations), planned)

    def test_resonator_app_selector_excludes_future_rows_and_smokes(self):
        from checkpoint_line_selectors import (
            PARTIAL_CHECKPOINT_LINE_SELECTORS,
            select_checkpoint_line_patch,
        )

        operations = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fractal-resonator-effect"
        ]["web/app.js"]
        changed_lines = []
        for index, operation in enumerate(operations):
            if isinstance(operation, str):
                changed_lines.append(operation)
            elif "span" in operation:
                span = operation["span"]
                count = span["expected_line_count"]
                changed_lines.append(span["start"])
                changed_lines.extend(
                    f"+/* span filler {index}:{line_index} */"
                    for line_index in range(count - 2)
                )
                changed_lines.append(span["end"])
            elif "line" in operation:
                changed_lines.append(operation["line"]["operation"])
            elif "replace" in operation:
                replacement = operation["replace"]
                if "before" in replacement:
                    changed_lines.append(replacement["before"])
                changed_lines.append(replacement["current"])
            else:
                self.fail(f"unsupported synthetic selector operation: {operation}")

        excluded = [
            '+          const laterRow = document.querySelector(\'[data-effect="123"]\');',
            "+          await runEuclideanDelayBrowserSmokeTest(reference);",
            "+          if (EFFECT_NAMES.length !== 126) return;",
            '+  "Studio Pan Delay · project-authored",',
            '+  "Euclidean Delay · project-authored controls",',
            '+  "Synth Texture Reverb · project-authored controls",',
        ]
        changed_lines.extend(excluded)
        deleted_count = sum(line.startswith("-") for line in changed_lines)
        added_count = sum(line.startswith("+") for line in changed_lines)
        synthetic_diff = (
            "diff --git a/web/app.js b/web/app.js\n"
            "--- a/web/app.js\n"
            "+++ b/web/app.js\n"
            f"@@ -1,{deleted_count} +1,{added_count} @@ async function runMercatorDistortionBrowserSmokeTest\n"
            + "".join(line + "\n" for line in changed_lines)
        )
        selected = select_checkpoint_line_patch(
            "fractal-resonator-effect", "web/app.js", synthetic_diff
        )
        self.assertIn('"Fractal Resonator · project-authored', selected)
        self.assertIn("length !== 123", selected)
        self.assertIn("effectRows[122]", selected)
        for forbidden in (
            'data-effect="123"',
            "runEuclideanDelayBrowserSmokeTest",
            "EFFECT_NAMES.length !== 126",
            "Studio Pan Delay",
            "Euclidean Delay",
            "Synth Texture Reverb",
        ):
            self.assertNotIn(forbidden, selected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
