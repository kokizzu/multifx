#!/usr/bin/env python3
"""Synthetic tests for fail-closed intermediate checkpoint replacements."""

from __future__ import annotations

import unittest

from checkpoint_line_selectors import LineSelectionError, select_exact_line_operations


HEADER = (
    "diff --git a/settings.txt b/settings.txt\n"
    "index 1111111..2222222 100644\n"
    "--- a/settings.txt\n"
    "+++ b/settings.txt\n"
)


class CheckpointReplacementTests(unittest.TestCase):
    def test_stages_intermediate_value_without_changing_worktree_diff(self) -> None:
        diff = HEADER + "@@ -1 +1 @@\n-count = 117\n+count = 127\n"
        operations = ({"replace": {
            "before": "-count = 117",
            "current": "+count = 127",
            "selected": "+count = 123",
        }},)

        selected = select_exact_line_operations(diff, operations)

        self.assertIn("-count = 117\n+count = 123\n", selected)
        self.assertNotIn("+count = 127", selected)
        self.assertIn("+count = 127", diff)

    def test_replacement_can_select_its_lines_from_a_shared_hunk_only(self) -> None:
        diff = HEADER + (
            "@@ -1,2 +1,3 @@\n"
            "-count = 117\n"
            "+count = 127\n"
            "+unrelated = true\n"
        )
        operations = ({"replace": {
            "before": "-count = 117",
            "current": "+count = 127",
            "selected": "+count = 123",
        }},)

        selected = select_exact_line_operations(diff, operations)

        self.assertIn("-count = 117\n+count = 123\n", selected)
        self.assertNotIn("unrelated", selected)

    def test_rewrites_a_new_registry_line_for_an_intermediate_checkpoint(self) -> None:
        diff = HEADER + "@@ -0,0 +1 @@\n+expected_rows = 127\n"
        operations = ({"replace": {
            "current": "+expected_rows = 127",
            "selected": "+expected_rows = 123",
        }},)

        selected = select_exact_line_operations(diff, operations)

        self.assertIn("+expected_rows = 123\n", selected)
        self.assertNotIn("+expected_rows = 127", selected)
        self.assertIn("+expected_rows = 127", diff)

    def test_ambiguous_replacement_source_fails_closed(self) -> None:
        diff = HEADER + (
            "@@ -1 +1,2 @@\n-count = 117\n"
            "+count = 127\n+count = 127\n"
        )
        operations = ({"replace": {
            "before": "-count = 117",
            "current": "+count = 127",
            "selected": "+count = 123",
        }},)

        with self.assertRaisesRegex(LineSelectionError, "exactly once"):
            select_exact_line_operations(diff, operations)

    def test_deleted_line_can_be_selected_without_neighboring_addition(self) -> None:
        diff = HEADER + (
            "@@ -1,2 +1 @@\n"
            "-obsolete = true\n"
            "+current = true\n"
        )

        selected = select_exact_line_operations(diff, ("-obsolete = true",))

        self.assertIn("-obsolete = true\n", selected)
        self.assertNotIn("+current = true", selected)

    def test_missing_replacement_source_fails_closed(self) -> None:
        diff = HEADER + "@@ -1 +1 @@\n-count = 117\n+count = 127\n"
        operations = ({"replace": {
            "before": "-count = 116",
            "current": "+count = 127",
            "selected": "+count = 123",
        }},)

        with self.assertRaisesRegex(LineSelectionError, "exactly once"):
            select_exact_line_operations(diff, operations)

    def test_non_adjacent_replacement_fails_closed(self) -> None:
        diff = HEADER + (
            "@@ -1,2 +1,3 @@\n"
            "-count = 117\n"
            "+unrelated = true\n"
            "+count = 127\n"
        )
        operations = ({"replace": {
            "before": "-count = 117",
            "current": "+count = 127",
            "selected": "+count = 123",
        }},)

        with self.assertRaisesRegex(LineSelectionError, "adjacent"):
            select_exact_line_operations(diff, operations)

    def test_hunk_scoped_intermediate_replacement_skips_added_comments(self) -> None:
        diff = HEADER + (
            "@@ -460 +474,3 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "+    // Effect 126 is handled by a later checkpoint.\n"
            "+    // Keep this intermediate chain count independent.\n"
            "+    return effects_.size() + 33;\n"
        )
        operations = ({"replace": {
            "before": "-        return effects_.size() + 27;",
            "current": "+    return effects_.size() + 33;",
            "selected": "+    return effects_.size() + 28;",
            "hunk_header": "@@ -460 +474,3 @@ public:",
            "allow_intervening_added_lines": True,
        }},)

        selected = select_exact_line_operations(diff, operations)

        self.assertIn("-        return effects_.size() + 27;", selected)
        self.assertIn("+    return effects_.size() + 28;", selected)
        self.assertNotIn("+    return effects_.size() + 33;", selected)
        self.assertNotIn("Effect 126", selected)
        self.assertIn("+    return effects_.size() + 33;", diff)

    def test_intermediate_replacement_rejects_intervening_deletions(self) -> None:
        diff = HEADER + (
            "@@ -460,2 +474,3 @@ public:\n"
            "-        return effects_.size() + 27;\n"
            "-    effects_.clear();\n"
            "+    // intervening addition\n"
            "+    return effects_.size() + 33;\n"
        )
        operations = ({"replace": {
            "before": "-        return effects_.size() + 27;",
            "current": "+    return effects_.size() + 33;",
            "selected": "+    return effects_.size() + 28;",
            "hunk_header": "@@ -460,2 +474,3 @@ public:",
            "allow_intervening_added_lines": True,
        }},)

        with self.assertRaisesRegex(LineSelectionError, "intervening replacement lines"):
            select_exact_line_operations(diff, operations)

    def test_hunk_header_scopes_repeated_replacement(self):
        diff = (
            "diff --git a/file b/file\n"
            "@@ -1,1 +1,1 @@ first\n"
            "- 127\n"
            "+ 128\n"
            "@@ -2,1 +2,1 @@ second\n"
            "- 127\n"
            "+ 128\n"
        )
        selected = select_exact_line_operations(
            diff,
            ({
                "replace": {
                    "before": "- 127",
                    "current": "+ 128",
                    "selected": "+ 123",
                    "hunk_header": "@@ -1,1 +1,1 @@ first",
                },},),
        )
        self.assertIn("+ 123\n", selected)
        self.assertNotIn("@@ -2,1 +2,1 @@ second", selected)


    def test_current_occurrence_selects_repeated_line_fail_closed(self):
        diff = (
            "diff --git a/file b/file\n"
            "@@ -1,0 +1,2 @@ additions\n"
            "+ 128\n"
            "+ 128\n"
        )
        selected = select_exact_line_operations(
            diff,
            ({
                "replace": {
                    "current": "+ 128",
                    "selected": "+ 123",
                    "current_occurrence": 2,
                    "expected_current_count": 2,
                },},),
        )
        self.assertEqual(selected.splitlines()[-1], "+ 123")
        with self.assertRaisesRegex(LineSelectionError, "multiplicity"):
            select_exact_line_operations(
                diff.replace("+ 128\n+ 128", "+ 128"),
                ({
                    "replace": {
                        "current": "+ 128",
                        "selected": "+ 123",
                        "current_occurrence": 2,
                        "expected_current_count": 2,
                    },},),
            )


if __name__ == "__main__":
    unittest.main()
