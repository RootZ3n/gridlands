"""Mechanical architecture rules that are cheaper to check as text than to remember."""

import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "Source"


class DialogueIsEventDriven(unittest.TestCase):
    """D-4 (ADR-0015): gameplay systems emit events; only dialogue code, UI and tests touch the director."""

    ALLOWED = re.compile(r"/(Dialogue|Tests|UI)/")

    def test_no_gameplay_code_includes_the_director(self):
        offenders = []
        for path in SOURCE.rglob("*.*"):
            if path.suffix not in (".h", ".cpp") or self.ALLOWED.search(path.as_posix()):
                continue
            if "Dialogue/GLDialogueDirector.h" in path.read_text(encoding="utf-8", errors="replace"):
                offenders.append(path.relative_to(SOURCE).as_posix())
        self.assertEqual(offenders, [], "gameplay code must emit events instead of calling the dialogue director")

    def test_the_rule_can_fail(self):
        self.assertIsNone(self.ALLOWED.search("GridlandsGame/Private/Salvage/GLSalvageableComponent.cpp"))


class PehlichiDealsNoDamage(unittest.TestCase):
    """P-3 (ADR-0017): no Pehlichi source applies damage."""

    def test_no_damage_calls_in_pehlichi_code(self):
        offenders = []
        for path in SOURCE.rglob("*.*"):
            if path.suffix in (".h", ".cpp") and "Pehlichi" in path.name:
                text = path.read_text(encoding="utf-8", errors="replace")
                if re.search(r"\b(ApplyDamage|TakeDamage|ApplyPointDamage|ApplyRadialDamage)\b", text):
                    offenders.append(path.relative_to(SOURCE).as_posix())
        self.assertEqual(offenders, [], "Pehlichi deals zero direct damage (ADR-0017)")


if __name__ == "__main__":
    unittest.main()
