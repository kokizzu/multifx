import hashlib
import unittest

from checkpoint_line_selectors import (
    LineSelectionError,
    PARTIAL_CHECKPOINT_LINE_SELECTORS,
    select_exact_line_operations,
)


class ExactLineSelectorTests(unittest.TestCase):
    def test_replacement_can_select_unique_lines_across_other_hunk_changes(self):
        diff = """diff --git a/example.js b/example.js
index 1234567..7654321 100644
--- a/example.js
+++ b/example.js
@@ -1,2 +1,3 @@ fixture
-const count = 123;
-const obsolete = true;
+const unrelatedFutureGuard = true;
+const count = 127;
+const anotherFutureGuard = true;
"""
        selected = select_exact_line_operations(
            diff,
            (
                {
                    "replace": {
                        "before": "-const count = 123;",
                        "current": "+const count = 127;",
                        "selected": "+const count = 124;",
                        "hunk_header": "@@ -1,2 +1,3 @@ fixture",
                        "allow_intervening_changes": True,
                    }
                },
            ),
        )
        self.assertIn("-const count = 123;", selected)
        self.assertIn("+const count = 124;", selected)
        self.assertNotIn("const count = 127;", selected)
        self.assertNotIn("obsolete", selected)
        self.assertNotIn("unrelatedFutureGuard", selected)
        self.assertNotIn("anotherFutureGuard", selected)

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

    def test_scoped_replacement_excludes_repeated_lines_and_is_idempotent(self) -> None:
        operation = {
            "replace": {
                "current": "+count = 127;",
                "selected": "+count = 125;",
                "hunk_headers": ["@@ -1,0 +1,7 @@ controls"],
                "scope_start": "+target effect starts",
                "scope_end": "+target effect ends",
                "current_occurrence": 1,
                "expected_current_count": 1,
            }
        }
        source = self.select(
            "@@ -1,0 +1,7 @@ controls\n"
            "+other effect starts\n"
            "+count = 127;\n"
            "+target effect starts\n"
            "+count = 127;\n"
            "+target effect ends\n"
            "+other effect ends\n"
            "+count = 127;\n",
            (operation,),
        )
        self.assertEqual(source.count("+count = 127;"), 0)
        self.assertEqual(source.count("+count = 125;"), 1)
        staged = self.select(
            "@@ -1,0 +1,7 @@ controls\n"
            "+other effect starts\n"
            "+count = 127;\n"
            "+target effect starts\n"
            "+count = 125;\n"
            "+target effect ends\n"
            "+other effect ends\n"
            "+count = 127;\n",
            (operation,),
        )
        self.assertEqual(staged.count("+count = 127;"), 0)
        self.assertEqual(staged.count("+count = 125;"), 1)

    def test_scoped_replacement_accepts_unique_staged_value_without_scope_markers(self) -> None:
        operation = {
            "replace": {
                "current": "+count = 127;",
                "selected": "+count = 125;",
                "scope_start": "+target effect starts",
                "scope_end": "+target effect ends",
                "allow_staged_value_without_scope": True,
            }
        }
        staged = self.select(
            "@@ -3,0 +3 @@ another staged hunk\n+count = 125;\n",
            (operation,),
        )
        self.assertEqual(staged.count("+count = 125;"), 1)

        with self.assertRaisesRegex(LineSelectionError, "exactly once"):
            self.select(
                "@@ -3,0 +3 @@ first staged hunk\n+count = 125;\n"
                "@@ -9,0 +10 @@ second staged hunk\n+count = 125;\n",
                (operation,),
            )

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


    def test_span_accepts_reviewed_start_multiplicity_alternatives(self) -> None:
        operation = {
            "span": {
                "hunk_headers": ["@@ -9,0 +9,1 @@", "@@ -10,0 +10,3 @@"],
                "start": "+    {",
                "line_count": 1,
                "start_occurrence": 1,
                "expected_start_count": [1, 3],
            }
        }
        for header, count in (
            ("@@ -9,0 +9,1 @@", 1),
            ("@@ -10,0 +10,3 @@", 3),
        ):
            diff = (
                "diff --git a/state.json b/state.json\n"
                "index 1111111..2222222 100644\n"
                "--- a/state.json\n"
                "+++ b/state.json\n"
                f"{header}\n"
                + "+    {\n" * count
            )
            selected = self.select(diff, (operation,))
            self.assertEqual(selected.count("+    {\n"), 1)

    def test_studio_app_selector_has_exact_id123_scope(self) -> None:
        app_operations = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fender-studio-delay-pan"
        ]["web/app.js"]
        studio_spans = [
            operation["span"]
            for operation in app_operations
            if isinstance(operation, dict)
            and operation.get("span", {}).get("start") in {
                "+async function runStudioPanDelayBrowserSmokeTest(",
                "+    await processorReady;",
                '+    const row = document.querySelector(`[data-effect="${effectIndex}"]`)?.closest(".effect-row");',
            }
        ]
        self.assertEqual([span["line_count"] for span in studio_spans], [9, 4, 147])
        span_lengths = {
            operation["span"]["start"]: operation["span"]["line_count"]
            for operation in app_operations
            if isinstance(operation, dict)
            and "span" in operation
            and "line_count" in operation["span"]
        }
        self.assertEqual(span_lengths["+const studioPanDelayControls = ["], 8)
        replacements = [
            operation["replace"]
            for operation in app_operations
            if isinstance(operation, dict) and "replace" in operation
        ]
        expected_ids = next(
            replacement
            for replacement in replacements
            if "expectedEffectIds" in replacement["selected"]
        )
        self.assertIn("length: 123", expected_ids["before"])
        self.assertIn("length: 127", expected_ids["current"])
        self.assertIn("length: 124", expected_ids["selected"])
        picker_error = next(
            replacement
            for replacement in replacements
            if "effect picker must list 124" in replacement["selected"]
        )
        self.assertIn("IDs 0–123", picker_error["selected"])
        self.assertIn("Studio Pan Delay", picker_error["selected"])
        self.assertNotIn("Essex Vibe", picker_error["selected"])
        self.assertNotIn("Euclidean Delay", picker_error["selected"])
        self.assertNotIn("Synth Texture Reverb", picker_error["selected"])
        exact_lines = {
            operation
            for operation in app_operations
            if isinstance(operation, str)
        }
        self.assertIn("+    parityEnabled[123] = false;", exact_lines)
        self.assertIn("+      studioPanDelay: studioPanDelaySmoke,", exact_lines)
        self.assertTrue(
            any(
                isinstance(operation, dict)
                and operation.get("span", {}).get("start")
                == '+      testResult.textContent = "Checking Studio Pan Delay with local guitar playback…";'
                and operation["span"].get("line_count") == 4
                for operation in app_operations
            )
        )

        for operation in app_operations:
            if isinstance(operation, str):
                selected = operation
            elif "span" in operation:
                selected = str(operation["span"])
            elif "replace" in operation:
                selected = operation["replace"]["selected"]
            else:
                self.fail(f"unsupported Studio app selector operation: {operation!r}")
            self.assertNotIn("Essex Vibe", selected)
            self.assertNotIn("Euclidean Delay", selected)
            self.assertNotIn("Synth Texture Reverb", selected)

    def test_studio_checklist_and_state_select_only_id123(self) -> None:
        selectors = PARTIAL_CHECKPOINT_LINE_SELECTORS["fender-studio-delay-pan"]
        checklist_line = selectors["IMPLEMENTATION_CHECKLIST.md"][0]
        self.assertIn("picker ID 123", checklist_line)
        self.assertTrue(checklist_line.endswith("physical listening remain manual."))
        state_span = selectors["research-state.json"][0]["span"]
        self.assertEqual(state_span["start"], "+    {")
        self.assertEqual(state_span["line_count"], 24)
        self.assertEqual(state_span["start_occurrence"], 1)
        self.assertEqual(state_span["expected_start_count"], [1, 3])
        self.assertIn("2026-11-28", selectors["FLAGSHIP_EFFECT_COMPARISON.md"][0]["span"]["start"])
        self.assertIn("2026-11-28", selectors["LATENCY_AND_QUALITY.md"][0]["span"]["start"])

        state_body = [
            state_span["start"],
            '+      "name": "Fender Tone Master Pro Studio Delay — project-authored Studio Pan Delay",',
        ]
        state_body.extend(f"+      \"field_{index}\": {index}," for index in range(1, 22))
        state_body.extend((
            "+    },",
            "+    {",
            '+      "name": "Fender Tone Master Pro Cirrostratus Synthverb — ID 126",',
        ))
        state_body.extend(f"+      \"future_{index}\": {index}," for index in range(1, 13))
        state_body.append("+    {")
        selected_state = self.select(
            "@@ -37,0 +49,39 @@\n"
            + "\n".join(state_body)
            + "\n@@ -100,0 +140,1 @@\n"
            + '+      "name": "Fender Tone Master Pro Cirrostratus Synthverb — ID 126",\n',
            (selectors["research-state.json"][0],),
        )
        self.assertIn("Studio Pan Delay", selected_state)
        self.assertNotIn("Cirrostratus", selected_state)

        cursor_selector = selectors["research-state.json"][1]
        cursor_replacement = cursor_selector["replace"]
        self.assertIn("Essex Vibe", cursor_replacement["selected"])
        self.assertNotIn("already-integrated", cursor_replacement["selected"])
        cursor_diff = (
            "diff --git a/research-state.json b/research-state.json\n"
            "index 1111111..2222222 100644\n"
            "--- a/research-state.json\n"
            "+++ b/research-state.json\n"
            "@@ -33,3 +44,3 @@\n"
            + cursor_replacement["before"]
            + "\n-    \"next_device\": \"Unrelated device\",\n"
            + "+    \"next_device\": \"Line 6 Helix Stadium XL\",\n"
            + cursor_replacement["current"]
            + "\n"
        )
        selected_cursor = self.select(cursor_diff, (cursor_selector,))
        self.assertIn(cursor_replacement["selected"], selected_cursor)
        self.assertNotIn("Unrelated device", selected_cursor)
        self.assertNotIn("already-integrated", selected_cursor)

    def test_studio_function_span_excludes_neighboring_future_smokes(self) -> None:
        app_operations = PARTIAL_CHECKPOINT_LINE_SELECTORS[
            "fender-studio-delay-pan"
        ]["web/app.js"]
        spans = [
            operation["span"]
            for operation in app_operations
            if isinstance(operation, dict) and "span" in operation
            and operation["span"].get("start") in {
                "+async function runStudioPanDelayBrowserSmokeTest(",
                "+    await processorReady;",
                '+    const row = document.querySelector(`[data-effect="${effectIndex}"]`)?.closest(".effect-row");',
            }
        ]
        self.assertEqual([span["line_count"] for span in spans], [9, 4, 147])
        body_span = next(span for span in spans if span["start"] == "+    await processorReady;")
        self.assertEqual(body_span["line_count"], 4)
        row_span = next(span for span in spans if span["start"].startswith("+    const row = document.querySelector("))
        self.assertEqual(row_span["start_occurrence"], 1)
        self.assertEqual(row_span["expected_start_count"], [1, 3])
        body = [spans[0]["start"]]
        body.extend(f"+  // Studio Pan Delay setup {index}" for index in range(2, 10))
        body.extend((
            "@@ -18538,2 +19495,4 @@ async function runBrowserSelfTest() {",
            spans[1]["start"],
        ))
        body.extend(f"+  // Studio Pan Delay guard {index}" for index in range(2, 5))
        body.extend((
            "@@ -18541,23 +19500,1068 @@ async function runBrowserSelfTest() {",
            spans[2]["start"],
        ))
        body.extend((spans[2]["start"], spans[2]["start"]))
        body.extend(f"+  // Studio Pan Delay test {index}" for index in range(2, 148))
        body.append("+async function runEssexVibeBrowserSmokeTest(")
        patch = self.select(
            "diff --git a/shared.txt b/shared.txt\n"
            "index 1111111..2222222 100644\n"
            "--- a/shared.txt\n"
            "+++ b/shared.txt\n"
            "@@ -18526,11 +19485,9 @@ async function runPitchRingModBrowserSmokeTest(\n"
            + "\n".join(body)
            + "\n",
            tuple({"span": span} for span in spans),
        )
        self.assertIn("+async function runStudioPanDelayBrowserSmokeTest(\n", patch)
        self.assertIn("+    await processorReady;\n", patch)
        self.assertIn(row_span["start"] + "\n", patch)
        self.assertNotIn("runEssexVibeBrowserSmokeTest", patch)


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


class EssexVibeSelectorTests(unittest.TestCase):
    def test_essex_picker_panel_excludes_adjacent_effects(self) -> None:
        from checkpoint_line_selectors import select_checkpoint_line_patch

        essex_row = '+        <label class="effect-row" data-category="modulation"><input type="checkbox" data-effect="124"><span class="effect-number">125</span><span class="effect-name">Essex Vibe · project-authored controls</span><span class="effect-default">0.8 Hz · 2 stages · disabled by default</span><span class="effect-category-tag">Modulation</span></label>'
        diff = "\n".join((
            "diff --git a/web/index.html b/web/index.html",
            "--- a/web/index.html",
            "+++ b/web/index.html",
            "@@ -233,2 +233,5 @@",
            essex_row,
            '+        <label class="effect-row" data-category="delay"><input type="checkbox" data-effect="125"><span class="effect-number">126</span><span class="effect-name">Euclidean Delay · project-authored controls</span><span class="effect-default">125 ms · 8 steps · disabled by default</span><span class="effect-category-tag">Delay</span></label>',
            '+        <label class="effect-row" data-category="reverb"><input type="checkbox" data-effect="126"><span class="effect-number">127</span><span class="effect-name">Synth Texture Reverb · project-authored</span><span class="effect-default">25% mix · disabled by default</span><span class="effect-category-tag">Reverb</span></label>',
            "@@ -675,2 +686,43 @@",
            '+      <div class="effect-parameter-group" aria-label="Essex Vibe controls">',
            "+        <h3>Essex Vibe · project-authored control mapping</h3>",
            "+        <div class=\"gain-controls\">",
            "+          <label>Rate</label>",
            "+          <label>Depth</label>",
            "+          <label>Trem</label>",
            "+          <label>Stages</label>",
            "+          <label>Stereo Spread</label>",
            "+          <label>Level</label>",
            "+        </div>",
            "+        <p class=\"hint\">Project-authored control bounds and no parity claim.</p>",
            "+      </div>",
            '+      <div class="effect-parameter-group" aria-label="Euclidean Delay controls">',
            "+        <h3>Euclidean Delay</h3>",
            '+      <div class="effect-parameter-group" aria-label="Synth Texture Reverb controls">',
            "+        <h3>Synth Texture Reverb</h3>",
        )) + "\n"

        selected = select_checkpoint_line_patch("essex-vibe", "web/index.html", diff)

        self.assertIsNotNone(selected)
        self.assertIn(essex_row, selected)
        self.assertIn('aria-label="Essex Vibe controls"', selected)
        self.assertNotIn("Euclidean Delay", selected)
        self.assertNotIn("Synth Texture Reverb", selected)
        self.assertNotIn('data-effect="125"', selected)
        self.assertNotIn('data-effect="126"', selected)

    def test_essex_comparison_selects_refreshed_record_only(self):
        from checkpoint_line_selectors import select_checkpoint_line_patch

        diff = """diff --git a/FLAGSHIP_EFFECT_COMPARISON.md b/FLAGSHIP_EFFECT_COMPARISON.md
--- a/FLAGSHIP_EFFECT_COMPARISON.md
+++ b/FLAGSHIP_EFFECT_COMPARISON.md
@@ -1,0 +1,7 @@
+## Line 6 Helix Stadium XL — Essex Vibe implementation checkpoint (2026-11-27)
+
+Official source paragraph with current Essex Vibe documentation.
+
+Refreshed TabletFX browser result; target-device timing remain manual.
+## Fender Orbit
+Adjacent comparison content.
"""
        selected = select_checkpoint_line_patch(
            "essex-vibe", "FLAGSHIP_EFFECT_COMPARISON.md", diff
        )
        self.assertIn(
            "+Refreshed TabletFX browser result; target-device timing remain manual.",
            selected,
        )
        self.assertNotIn("+## Fender Orbit", selected)
        self.assertEqual(sum(line.startswith("+") and not line.startswith("+++")
                             for line in selected.splitlines()), 5)

    def test_essex_checklist_selects_all_entries_only(self):
        from checkpoint_line_selectors import select_checkpoint_line_patch

        diff = """diff --git a/IMPLEMENTATION_CHECKLIST.md b/IMPLEMENTATION_CHECKLIST.md
--- a/IMPLEMENTATION_CHECKLIST.md
+++ b/IMPLEMENTATION_CHECKLIST.md
@@ -1,0 +1,10 @@
+## Line 6 Helix Stadium XL — Essex Vibe (2026-11-27)
+
+- [x] Verify the exact model controls.
+- [x] Keep project choices labeled.
+- [x] Register the native processor.
+- [x] Add categorized browser controls.
+- [x] Run native and browser checks.
+- [x] No Line 6 circuit, algorithm, parameter, or sound parity is claimed.
+## Fender Orbit
+Adjacent checklist content.
"""
        selected = select_checkpoint_line_patch(
            "essex-vibe", "IMPLEMENTATION_CHECKLIST.md", diff
        )
        self.assertIn(
            "+- [x] No Line 6 circuit, algorithm, parameter, or sound parity is claimed.",
            selected,
        )
        self.assertNotIn("+## Fender Orbit", selected)
        self.assertEqual(sum(line.startswith("+") and not line.startswith("+++")
                             for line in selected.splitlines()), 8)


class EuclideanDelaySelectorTests(unittest.TestCase):
    def test_stable_hunk_context_survives_staged_count_changes(self):
        operations = (
            {
                "span": {
                    "hunk_context": "@@ -20,",
                    "start": "+shared block starts",
                    "end": "+Euclidean-only body",
                }
            },
        )
        before = (
            "diff --git a/shared.cpp b/shared.cpp\n"
            "--- a/shared.cpp\n+++ b/shared.cpp\n"
            "@@ -10,0 +10,3 @@ first\n"
            "+shared block starts\n+wrong block\n+Euclidean-only body\n"
            "@@ -20,0 +20,4 @@ second\n"
            "+Synth Texture Reverb\n+shared block starts\n"
            "+Euclidean-only body\n+adjacent future code\n"
        )
        selected = select_exact_line_operations(before, operations)
        self.assertIn("+Euclidean-only body", selected)
        self.assertNotIn("Synth Texture Reverb", selected)
        self.assertNotIn("wrong block", selected)

        staged = before.replace("@@ -20,0 +20,4 @@", "@@ -20,0 +20,2 @@")
        staged = staged.replace(
            "+Synth Texture Reverb\n+shared block starts\n"
            "+Euclidean-only body\n+adjacent future code\n",
            "+Synth Texture Reverb\n+shared block starts\n"
            "+Euclidean-only body\n",
        )
        selected_staged = select_exact_line_operations(staged, operations)
        self.assertIn("+Euclidean-only body", selected_staged)
        self.assertNotIn("Synth Texture Reverb", selected_staged)

    def test_euclidean_wasm_selector_excludes_adjacent_synth(self):
        from checkpoint_line_selectors import select_checkpoint_line_patch

        hunks = {
            "@@ -3,2 +3,4 @@": (
                '+#include "tabletfx/effects/euclidean_delay.hpp"',
                '+#include "tabletfx/effects/synth_texture_reverb.hpp"',
            ),
            "@@ -646,2 +652,54 @@ public:": (
                "+        if (index == 125U) {",
                "+            if (!enabled) {",
                "+                euclidean_delay_enabled_ = false;",
                "+            }",
                "+            return true;",
                "+        }",
                "+        if (index == 126U) {",
                "+            synth_texture_reverb_enabled_ = true;",
                "+        }",
            ),
            "@@ -1023,2 +1081,25 @@ public:": (
                "+        if (index == 125U) {",
                "+            euclidean_delay_->process(",
                "+            return;",
                "+        }",
                "+        if (index == 126U) {",
                "+            synth_texture_reverb_->process(",
                "+        }",
            ),
            "@@ -1099,2 +1180,3 @@ public:": (
                "+        if (euclidean_delay_) euclidean_delay_->reset();",
            ),
            "@@ -3991,2 +4105,85 @@ public:": (
                "+    void configure_euclidean_delay_filters(float low_cut_hz,",
                "+        euclidean_delay_parameters_ = parameters;",
                "+    }",
                "+    void configure_synth_texture_reverb(",
                "+        synth_texture_reverb_parameters_ = parameters;",
                "+    }",
                "+    void configure_euclidean_delay(float step_time_ms, float steps,",
                "+        euclidean_delay_parameters_ = parameters;",
                "+    }",
            ),
            "@@ -4224,2 +4421,3 @@ public:": (
                "+        if (euclidean_delay_) bytes += euclidean_delay_->memory_bytes();",
            ),
            "@@ -4421,2 +4619,17 @@ public:": (
                "+        if (index == 125U) {",
                "+            if (!euclidean_delay_) return 0U;",
                "+        }",
                "+        if (index == 126U) {",
                "+            if (!synth_texture_reverb_) return 0U;",
                "+        }",
            ),
            "@@ -4471,2 +4684,4 @@ private:": (
                "+        case 125U: return euclidean_delay_enabled_ && euclidean_delay_ != nullptr;",
                "+        case 126U: return synth_texture_reverb_enabled_ && synth_texture_reverb_ != nullptr;",
            ),
            "@@ -4521,2 +4736,7 @@ private:": (
                "+    std::unique_ptr<tabletfx::EuclideanDelay> euclidean_delay_;",
                "+    tabletfx::EuclideanDelayParameters euclidean_delay_parameters_{};",
                "+    std::unique_ptr<tabletfx::effects::SynthTextureReverb> synth_texture_reverb_;",
            ),
            "@@ -4557,2 +4777,3 @@ private:": (
                "+    bool euclidean_delay_enabled_ = false;",
            ),
            "@@ -6478,2 +6729,26 @@ TABLETFX_WASM_EXPORT void tf_chain_set_essex_vibe(": (
                "+TABLETFX_WASM_EXPORT void tf_chain_set_euclidean_delay(",
                "+        chain->configure_euclidean_delay(",
                "+}",
                "+TABLETFX_WASM_EXPORT void tf_chain_set_synth_texture_reverb(",
            ),
        }
        diff = [
            "diff --git a/src/web/wasm_chain.cpp b/src/web/wasm_chain.cpp",
            "--- a/src/web/wasm_chain.cpp",
            "+++ b/src/web/wasm_chain.cpp",
        ]
        for header, lines in hunks.items():
            diff.append(header)
            diff.extend(lines)
        selected = select_checkpoint_line_patch(
            "euclidean-delay", "src/web/wasm_chain.cpp", "\n".join(diff) + "\n"
        )
        self.assertIn('+#include "tabletfx/effects/euclidean_delay.hpp"', selected)
        self.assertIn("+        case 125U:", selected)
        self.assertIn("+TABLETFX_WASM_EXPORT void tf_chain_set_euclidean_delay(", selected)
        self.assertNotIn("synth_texture_reverb", selected)

    def test_euclidean_research_selector_keeps_cursor_and_delay_only(self):
        from checkpoint_line_selectors import select_checkpoint_line_patch

        diff = "\n".join((
            "diff --git a/research-state.json b/research-state.json",
            "--- a/research-state.json",
            "+++ b/research-state.json",
            "@@ -7 +7 @@",
            '+  "previous_checkpoint_commit": "0d5804262e69d43a8a191dc353488cd1dfe71549",',
            "@@ -9,2 +9 @@",
            '-    "name": "Fractal FM9-family Global Mix — Phaser subset",',
            '-    "status": "previous status",',
            '+    "name": "Line 6 Helix Stadium XL — Euclidean Delay",',
            "@@ -12 +10,0 @@",
            '-    "picker_index": 68,',
            "@@ -13,0 +12 @@",
            '+    "picker_index": 125,',
            "@@ -15,5 +14,9 @@",
            '-      "title": "Fractal Audio Systems Blocks Guide and FM9 downloads",',
            '-      "revision": "old revision",',
            '-      "url": "https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf",',
            '-      "firmware_url": "https://www.fractalaudio.com/fm9-downloads/",',
            '-      "qualification": "old qualification",',
            '+      "title": "Helix Stadium model catalogue, Effects manual, Blocks manual, and 1.4 release notes",',
            '+      "revision": "reviewed revision",',
            '+      "urls": [',
            '+        "https://line6.com/helix-stadium-models/",',
            '+        "https://manuals.line6.com/en/helix-stadium/live/effect-blocks",',
            '+        "https://manuals.line6.com/en/helix-stadium/live/the-blocks",',
            '+        "https://kb.line6.com/helix-stadium-1-4-release-notes"',
            '+      ],',
            '+      "qualification": "The catalogue identifies a Mono/Stereo Line 6 Original Euclidean Delay, first appearing in 1.0. The accessible manual provides common Delay controls but no model-specific parameter table, ranges, defaults, topology, CPU cost, or latency. No settings or parity are inferred.",',
            "@@ -21,2 +24,3 @@",
            '-    "documented_controls": "Global Mix is an opt-in to the shared Effects Mix offset, up to ±50%. Megatap also lists Mix, Level, Balance, Bypass/Bypass Mode, Input Select/Gain, Kill Dry, and spillover. The guide does not state how Global Mix combines with Megatap Mix.",',
            '-    "project_controls": "Old controls",',
            '+    "documented_controls": "Mono/Stereo listing and category identity only; exact model-specific controls and numeric values are not published in the checked manual.",',
            '+    "project_controls": "Project controls",',
            '+    "status": "Implemented as default-disabled Delay picker ID 125/display number 126. Project-authored functional coverage only; no Line 6 settings, DSP, or sound parity.",',
            "@@ -24,6 +28,13 @@",
            '-      "native": "make test-fractal-megatap-global-mix passes focused defaults, disabled identity, offset, clamps, storage, and zero-process-allocation coverage; make test passes all 123 CTest cases.",',
            '+      "native": "make test-euclidean-delay passed focused defaults and bounds.",',
            '+    }',
            "@@ -33,6 +44,6 @@",
            '-    "next_device": "Fractal FM9 Mark II Turbo",',
            '-    "qualification": "Preserve exact-model and firmware qualifications, source URLs, categorized picker placement, project-authored control semantics, and no-parity claims. Keep browser/WASM and Windows cross-build checks separate from native Windows UI/runtime; hardware listening and target-device timing remain open. Do not touch expression-volume, footswitch-display, or general-research cursors.",',
            '+    "next_device": "HeadRush Prime",',
            '+    "next_feature": "Integrate and validate the project-authored Prime Drum Machine prototype under Loop & performance, then continue the ordered source-backed Prime, Fender, Line 6 Stadium, Fractal, Melofty, and XRecer gaps. Complete, test, and publish one feature checkpoint at a time.",',
            '+    "qualification": "Maintain exact-model and firmware qualifications, source URLs, categorized placement, project-authored control labels, CPU/memory/latency measurements, and manual-test remainder. The full browser suite has historical intermittent Global Mix capture failures; a successful run does not eliminate them. Keep expression-volume, footswitch-display, and general-research cursors untouched.",',
            "@@ -36 +47,0 @@",
            '-    "next_feature": "After this Studio Pan Delay ID 123 checkpoint reaches main, publish the local Essex Vibe at ID 124 as the next standalone checkpoint; continue stable picker order only after each commit is on main.",',
            "@@ -1713,0 +1727,8 @@",
            '+      "name": "unrelated prior audit",',
            '+      "name": "unrelated prior audit tail",',
            '+      "name": "unrelated future audit",',
            '+      "name": "Line 6 Helix Stadium XL — Euclidean Delay",',
            '+      "date": "2026-11-27",',
            '+      "qualification": "no parity",',
            '+      "verification": {',
            '+        "smoke": true',
            '+      }',
            '+    }',
            "@@ -1740,2 +1868 @@",
            '-    "name": "Fractal FM9-family Global Mix — Phaser subset",',
            '+    "name": "Line 6 Helix Stadium XL — Euclidean Delay",',
            "@@ -1743 +1869,0 @@",
            '-    "picker_index": 68,',
            "@@ -1744,0 +1871 @@",
            '+    "picker_index": 125,',
            "@@ -1746,5 +1873,9 @@",
            '-      "title": "Fractal Audio Systems Blocks Guide and FM9 downloads",',
            '-      "revision": "old revision",',
            '-      "url": "https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf",',
            '-      "firmware_url": "https://www.fractalaudio.com/fm9-downloads/",',
            '-      "qualification": "old qualification",',
            '+      "title": "Helix Stadium model catalogue, Effects manual, Blocks manual, and 1.4 release notes",',
            '+      "revision": "reviewed revision",',
            '+      "urls": [',
            '+        "https://line6.com/helix-stadium-models/",',
            '+        "https://manuals.line6.com/en/helix-stadium/live/effect-blocks",',
            '+        "https://manuals.line6.com/en/helix-stadium/live/the-blocks",',
            '+        "https://kb.line6.com/helix-stadium-1-4-release-notes"',
            '+      ],',
            '+      "qualification": "The catalogue identifies a Mono/Stereo Line 6 Original Euclidean Delay, first appearing in 1.0. The accessible manual provides common Delay controls but no model-specific parameter table, ranges, defaults, topology, CPU cost, or latency. No settings or parity are inferred.",',
            "@@ -1752,2 +1883,3 @@",
            '-    "documented_controls": "Global Mix is an opt-in to the shared Effects Mix offset, up to ±50%. Megatap also lists Mix, Level, Balance, Bypass/Bypass Mode, Input Select/Gain, Kill Dry, and spillover. The guide does not state how Global Mix combines with Megatap Mix.",',
            '-    "project_controls": "Old controls",',
            '+    "documented_controls": "Mono/Stereo listing and category identity only; exact model-specific controls and numeric values are not published in the checked manual.",',
            '+    "project_controls": "Project controls",',
            '+    "status": "Implemented as default-disabled Delay picker ID 125/display number 126. Project-authored functional coverage only; no Line 6 settings, DSP, or sound parity.",',
            "@@ -1755,7 +1887,13 @@",
            '-      "native": "make test-fractal-megatap-global-mix passes focused defaults, disabled identity, offset, clamps, storage, and zero-process-allocation coverage; make test passes all 123 CTest cases.",',
            '+      "native": "make test-euclidean-delay passed focused defaults and bounds.",',
            '+    }',
            "@@ -1987,2 +2129,23 @@",
            '+      "name": "HeadRush Prime current catalog and controls audit",',
        )) + "\n"
        selected = select_checkpoint_line_patch(
            "euclidean-delay", "research-state.json", diff
        )
        self.assertIn('"name": "Line 6 Helix Stadium XL — Euclidean Delay"', selected)
        self.assertIn('"next_device": "HeadRush Prime"', selected)
        self.assertIn('-    "next_device": "Fractal FM9 Mark II Turbo",', selected)
        self.assertNotIn("unrelated prior audit", selected)
        self.assertNotIn("unrelated future audit", selected)
        self.assertNotIn("HeadRush Prime current catalog", selected)
if __name__ == "__main__":
    unittest.main(verbosity=2)
