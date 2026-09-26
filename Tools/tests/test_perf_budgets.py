"""The P5 regression budgets hold for the accepted evidence, and can fail (ADR-0028, ADR-0029)."""

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "perf"))
import check_budgets  # noqa: E402

EVIDENCE = ROOT / "Docs" / "Evidence" / "P5-seamless-grid" / "perf"


class PerfBudgets(unittest.TestCase):
    def setUp(self):
        self.budgets = json.loads(check_budgets.BUDGETS.read_text(encoding="utf-8"))

    def test_accepted_p5_evidence_is_within_budget(self):
        files = sorted(EVIDENCE.glob("local-*.json"))
        self.assertGreaterEqual(len(files), 6)
        for path in files:
            self.assertEqual(check_budgets.breaches(path.name, json.loads(path.read_text()), self.budgets), [], path.name)

    def test_whole_cell_navigation_breaches(self):
        for path in sorted(EVIDENCE.glob("whole-*.json")):
            self.assertNotEqual(check_budgets.breaches(path.name, json.loads(path.read_text()), self.budgets), [], path.name)

    def test_a_worse_unload_hitch_breaches(self):
        result = json.loads((EVIDENCE / "local-crossing-reversal.json").read_text())
        result["worstFrameMs"] = 45.0
        self.assertTrue(any("worstFrameMs" in b for b in check_budgets.breaches("local-crossing-reversal.json", result, self.budgets)))

    def test_a_round_trip_leak_breaches(self):
        levelled = {"roundTrips": 8, "memGrowthMb": 102.0, "memPeakMb": 4383.0}
        self.assertEqual(check_budgets.breaches("local-roundtrips.json", levelled, self.budgets), [])
        leaking = dict(levelled, memGrowthMb=540.0 * 7)  # the editor automation world's per-round-trip growth
        self.assertTrue(any("memGrowthMb" in b for b in check_budgets.breaches("local-roundtrips.json", leaking, self.budgets)))


class WholeCellNavigationIsProhibited(unittest.TestCase):
    """ADR-0029 (operator decision 2026-09-25): localized navigation is canonical."""

    def test_config_generates_only_around_invokers(self):
        ini = (ROOT / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8")
        section = ini.split("[/Script/NavigationSystem.NavigationSystemV1]", 1)
        self.assertEqual(len(section), 2, "the navigation system section is missing")
        body = section[1].split("\n[", 1)[0]
        self.assertIn("bGenerateNavigationOnlyAroundNavigationInvokers=True", body)
        self.assertNotIn("bGenerateNavigationOnlyAroundNavigationInvokers=False", ini)


if __name__ == "__main__":
    unittest.main()
