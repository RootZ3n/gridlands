#!/usr/bin/env python3
"""Decide pass/fail from an Unreal automation report (ADR-0008).

Usage: automation_report.py <report-dir-or-index.json> <required-tests.txt>

The process exit code of UnrealEditor-Cmd is never trusted on its own. This
parser passes only when:
  * the report exists and parses;
  * at least one test ran;
  * every test's state is Success;
  * the summary counts agree with the per-test list;
  * every required prefix matches at least its minimum number of tests.

Exit: 0 pass, 1 fail. The last stdout line is always "RESULT: PASS|FAIL <reason>".
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Verdict:
    passed: bool
    reason: str
    ran: int = 0
    lines: list[str] = field(default_factory=list)


def read_report_text(path: Path) -> str:
    raw = path.read_bytes()
    # Unreal may write UTF-16 (with BOM) or UTF-8 (with or without BOM).
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        return raw.decode("utf-16")
    return raw.decode("utf-8-sig")


def load_required(path: Path) -> list[tuple[str, int]]:
    """Lines: '<test path prefix> [min_count]'. '#' starts a comment."""
    required: list[tuple[str, int]] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) > 2:
            raise ValueError(f"{path}:{number}: expected '<prefix> [min_count]'")
        minimum = int(parts[1]) if len(parts) == 2 else 1
        if minimum < 1:
            raise ValueError(f"{path}:{number}: min_count must be >= 1")
        required.append((parts[0], minimum))
    if not required:
        raise ValueError(f"{path}: no required tests listed (a vacuous requirement list is refused)")
    return required


def evaluate(report: dict, required: list[tuple[str, int]]) -> Verdict:
    tests = report.get("tests")
    if not isinstance(tests, list):
        return Verdict(False, "report has no 'tests' list")
    if not tests:
        return Verdict(False, "zero tests ran")

    lines: list[str] = []
    failures: list[str] = []
    paths: list[str] = []
    for test in tests:
        path = test.get("fullTestPath") or test.get("testDisplayName") or "<unnamed>"
        state = test.get("state", "<missing>")
        paths.append(path)
        lines.append(f"  {state:<10} {path}")
        if state != "Success":
            failures.append(f"{path} [{state}]")

    succeeded = report.get("succeeded", 0) + report.get("succeededWithWarnings", 0)
    counted = succeeded + report.get("failed", 0) + report.get("notRun", 0) + report.get("inProcess", 0)
    verdict = Verdict(False, "", ran=len(tests), lines=lines)

    if counted != len(tests):
        verdict.reason = f"summary counts ({counted}) disagree with test list ({len(tests)})"
        return verdict
    if failures:
        verdict.reason = f"{len(failures)} of {len(tests)} tests did not succeed: " + "; ".join(failures)
        return verdict

    missing = []
    for prefix, minimum in required:
        matched = sum(1 for p in paths if p == prefix or p.startswith(prefix + "."))
        if matched < minimum:
            missing.append(f"{prefix} (need {minimum}, found {matched})")
    if missing:
        verdict.reason = "required tests absent: " + "; ".join(missing)
        return verdict

    verdict.passed = True
    verdict.reason = f"{len(tests)} tests succeeded, {len(required)} requirement(s) met"
    return verdict


def find_index(target: Path) -> Path:
    return target / "index.json" if target.is_dir() else target


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2])
        print("RESULT: FAIL usage")
        return 1
    index = find_index(Path(argv[1]))
    try:
        required = load_required(Path(argv[2]))
    except (OSError, ValueError) as error:
        print(f"RESULT: FAIL required-tests list unusable: {error}")
        return 1
    if not index.is_file():
        print(f"RESULT: FAIL no automation report at {index}")
        return 1
    try:
        report = json.loads(read_report_text(index))
    except (OSError, ValueError) as error:
        print(f"RESULT: FAIL automation report unparsable: {error}")
        return 1

    verdict = evaluate(report, required)
    for line in verdict.lines:
        print(line)
    print(f"RESULT: {'PASS' if verdict.passed else 'FAIL'} {verdict.reason}")
    return 0 if verdict.passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
