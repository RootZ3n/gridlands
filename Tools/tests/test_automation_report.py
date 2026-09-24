"""Tests for Tools/lib/automation_report.py. They run without Unreal."""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "lib"))

import automation_report as ar  # noqa: E402

LIFECYCLE = "Gridlands.Core.Glitch.Lifecycle"


def report(states, **summary):
    tests = [{"fullTestPath": f"{LIFECYCLE}.Case{i}", "state": s} for i, s in enumerate(states)]
    base = {
        "succeeded": sum(s == "Success" for s in states),
        "succeededWithWarnings": 0,
        "failed": sum(s == "Fail" for s in states),
        "notRun": sum(s == "NotRun" for s in states),
        "inProcess": 0,
        "tests": tests,
    }
    base.update(summary)
    return base


class EvaluateTests(unittest.TestCase):
    def test_all_success_with_requirement_met_passes(self):
        verdict = ar.evaluate(report(["Success"] * 7), [(LIFECYCLE, 7)])
        self.assertTrue(verdict.passed, verdict.reason)
        self.assertEqual(verdict.ran, 7)

    def test_zero_tests_fails(self):
        verdict = ar.evaluate(report([]), [(LIFECYCLE, 1)])
        self.assertFalse(verdict.passed)
        self.assertIn("zero tests", verdict.reason)

    def test_missing_tests_list_fails(self):
        self.assertFalse(ar.evaluate({"succeeded": 3}, [(LIFECYCLE, 1)]).passed)

    def test_any_failure_fails(self):
        verdict = ar.evaluate(report(["Success", "Fail", "Success"]), [(LIFECYCLE, 1)])
        self.assertFalse(verdict.passed)
        self.assertIn("Case1 [Fail]", verdict.reason)

    def test_not_run_fails(self):
        self.assertFalse(ar.evaluate(report(["Success", "NotRun"]), [(LIFECYCLE, 1)]).passed)

    def test_required_prefix_below_minimum_fails(self):
        verdict = ar.evaluate(report(["Success"] * 3), [(LIFECYCLE, 7)])
        self.assertFalse(verdict.passed)
        self.assertIn("need 7, found 3", verdict.reason)

    def test_required_prefix_absent_fails_even_if_others_pass(self):
        verdict = ar.evaluate(report(["Success"] * 3), [("Gridlands.Game.Pehlichi", 1)])
        self.assertFalse(verdict.passed)

    def test_prefix_matches_on_segment_boundary_only(self):
        # "Gridlands.Core.Glitch.Life" must not match "...Lifecycle.Case0".
        self.assertFalse(ar.evaluate(report(["Success"]), [("Gridlands.Core.Glitch.Life", 1)]).passed)

    def test_warnings_pass_but_are_reported(self):
        data = report(["Success", "Success"], succeeded=0, succeededWithWarnings=2)
        noise = {"event": {"type": "Warning", "message": "engine noise"}}
        for test in data["tests"]:
            test["entries"] = [noise, {"event": {"type": "Info", "message": "fine"}}]
        verdict = ar.evaluate(data, [(LIFECYCLE, 2)])
        self.assertTrue(verdict.passed, verdict.reason)
        self.assertEqual(verdict.warnings, 2)
        self.assertIn("2 warning(s)", verdict.reason)
        self.assertIn("  warning x2: engine noise", verdict.lines)

    def test_summary_disagreeing_with_list_fails(self):
        verdict = ar.evaluate(report(["Success"], succeeded=5), [(LIFECYCLE, 1)])
        self.assertFalse(verdict.passed)
        self.assertIn("disagree", verdict.reason)


class FileTests(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.required = self.dir / "required.txt"
        self.required.write_text(f"# comment\n{LIFECYCLE} 2\n", encoding="utf-8")

    def run_main(self, target):
        with contextlib.redirect_stdout(io.StringIO()):
            return ar.main(["automation_report.py", str(target), str(self.required)])

    def write_index(self, data, encoding):
        text = json.dumps(data)
        (self.dir / "index.json").write_bytes(text.encode(encoding))

    def test_reads_utf16_with_bom(self):
        self.write_index(report(["Success", "Success"]), "utf-16")
        self.assertEqual(self.run_main(self.dir), 0)

    def test_reads_utf8_with_bom(self):
        self.write_index(report(["Success", "Success"]), "utf-8-sig")
        self.assertEqual(self.run_main(self.dir / "index.json"), 0)

    def test_missing_report_fails(self):
        self.assertEqual(self.run_main(self.dir), 1)

    def test_garbage_report_fails(self):
        (self.dir / "index.json").write_text("{not json", encoding="utf-8")
        self.assertEqual(self.run_main(self.dir), 1)

    def test_empty_required_list_is_refused(self):
        self.required.write_text("# nothing\n", encoding="utf-8")
        self.write_index(report(["Success"]), "utf-8")
        self.assertEqual(self.run_main(self.dir), 1)

    def test_repo_required_list_is_well_formed(self):
        repo_list = Path(__file__).resolve().parents[1] / "required-tests.txt"
        self.assertTrue(ar.load_required(repo_list))


if __name__ == "__main__":
    unittest.main()
