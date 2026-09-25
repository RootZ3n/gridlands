"""Mechanical architecture rules that are cheaper to check as text than to remember."""

import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "Source"


class DialogueIsEventDriven(unittest.TestCase):
    """D-4 (ADR-0015): gameplay systems emit events; only dialogue code, UI and tests touch the director."""

    # Save/ may include it to persist dialogue HISTORY (ADR-0019); it never makes anyone speak.
    ALLOWED = re.compile(r"/(Dialogue|Tests|UI|Save)/")

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


class OnlyPehlichiRepairs(unittest.TestCase):
    """P-1 (ADR-0005), in the source: glitch state changes need a passkey only Pehlichi's systems can make."""

    HEADER = SOURCE / "GridlandsGame/Public/Glitch/GLGlitchComponent.h"

    def test_every_state_mutator_takes_an_authority(self):
        text = self.HEADER.read_text()
        # Parse the component class itself, not the passkey structs above it.
        body = text[text.index("class GRIDLANDSGAME_API UGLGlitchComponent"):]
        public = body.split("public:", 1)[1].split("private:", 1)[0]
        mutators = re.findall(r"^\s*(?:bool|void)\s+(Reveal|SetRequirementsMet|BeginRepair|AddRepairProgress|Interrupt|MarkItemsDelivered|RestoreFromSave)\((.*?)\)", public, re.M)
        self.assertGreaterEqual(len(mutators), 7)
        for name, params in mutators:
            self.assertRegex(params, r"FGL(Scan|Repair|World|Restore)Authority", f"{name} must require an authority passkey")

    def test_passkeys_are_private_and_befriend_one_class(self):
        text = self.HEADER.read_text()
        for key, friend in (("FGLScanAuthority", "UGLScanComponent"), ("FGLRepairAuthority", "UGLRepairComponent"),
                            ("FGLWorldAuthority", "UGLGlitchSubsystem"), ("FGLRestoreAuthority", "UGLSaveSubsystem")):
            block = re.search(r"struct " + key + r"\s*\{(.*?)\};", text, re.S).group(1)
            self.assertIn("private:", block, f"{key} constructor must be private")
            self.assertEqual(re.findall(r"friend class (\w+);", block), [friend], f"{key} may only befriend {friend}")

    def test_there_is_no_player_authority(self):
        self.assertNotRegex(self.HEADER.read_text(), r"FGLPlayerAuthority")


class DerivedValuesAreNeverStored(unittest.TestCase):
    """S-1 (ADR-0013): stability, interference and NICE composure are computed, never kept as state."""

    def test_the_rule_can_fail(self):
        sample = "UPROPERTY() double CachedInterference = 0.0;"
        self.assertTrue(re.search(r"(Interference|Composure|Stability)", re.search(r"UPROPERTY\([^)]*\)\s*[\w<>:, ]+\s+(\w+)", sample).group(1)))

    def test_no_uproperty_stores_a_derived_value(self):
        offenders = []
        for path in SOURCE.rglob("*.h"):
            # Content definitions hold authored INPUTS (a band's baseline, a glitch's weight), not derived state.
            if path.name == "GLContentDefinitions.h":
                continue
            for match in re.finditer(r"UPROPERTY\([^)]*\)\s*[\w<>:, ]+\s+(\w+)", path.read_text(encoding="utf-8", errors="replace")):
                if re.search(r"(Interference|Composure|Stability)", match.group(1)):
                    offenders.append(f"{path.relative_to(SOURCE).as_posix()}: {match.group(1)}")
        self.assertEqual(offenders, [], "derived values must be computed from glitch states, not stored (S-1)")


class DialogueNeverSolvesPuzzles(unittest.TestCase):
    """ADR-0023: NICE and Pehlichi may discuss a puzzle; only gameplay completes it."""

    def test_no_dialogue_code_calls_solve(self):
        offenders = [p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*.*")
                     if p.suffix in (".h", ".cpp") and "/Dialogue/" in p.as_posix()
                     and re.search(r"\bSolve\s*\(", p.read_text(encoding="utf-8", errors="replace"))]
        self.assertEqual(offenders, [])

    def test_only_puzzle_sites_and_tests_call_solve(self):
        callers = [p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*.cpp")
                   if re.search(r"->Solve\s*\(", p.read_text(encoding="utf-8", errors="replace"))
                   and "/Tests/" not in p.as_posix()]
        self.assertEqual(callers, ["GridlandsGame/Private/Puzzle/GLPuzzleSite.cpp"])


class ThreatComesFromPlaces(unittest.TestCase):
    """ADR-0014: creatures enter the world only through placements, never because of an activity."""

    def test_only_the_placement_subsystem_spawns_creatures(self):
        spawners = sorted(p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*.cpp")
                          if re.search(r"SpawnActor(Deferred)?\s*<\s*AGLCreature\s*>", p.read_text(encoding="utf-8", errors="replace"))
                          and "/Tests/" not in p.as_posix())
        self.assertEqual(spawners, ["GridlandsGame/Private/World/GLPlacementSubsystem.cpp"])

    def test_glitch_storms_are_harmless(self):
        offenders = [p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*.*")
                     if p.suffix in (".h", ".cpp") and "/Storm/" in p.as_posix()
                     and re.search(r"\b(ApplyDamage|TakeDamage)\b", p.read_text(encoding="utf-8", errors="replace"))]
        self.assertEqual(offenders, [])


class PehlichiDealsNoDamage(unittest.TestCase):
    """P-3 (ADR-0017): no Pehlichi source applies damage."""

    def test_no_damage_calls_in_pehlichi_code(self):
        offenders = []
        for path in SOURCE.rglob("*.*"):
            if path.suffix in (".h", ".cpp") and ("Pehlichi" in path.name or "/Pehlichi/" in path.as_posix()):
                text = path.read_text(encoding="utf-8", errors="replace")
                if re.search(r"\b(ApplyDamage|TakeDamage|ApplyPointDamage|ApplyRadialDamage)\b", text):
                    offenders.append(path.relative_to(SOURCE).as_posix())
        self.assertEqual(offenders, [], "Pehlichi deals zero direct damage (ADR-0017)")


if __name__ == "__main__":
    unittest.main()
