"""P5 regression budgets: checks gl.Perf result files against Tools/perf/budgets.json.

Usage: python3 Tools/perf/check_budgets.py <result.json>...   (exit 1 on any breach)
A file's kind comes from its name: *terrain-1024m.json, *roundtrips.json, or *crossing-<mode>.json.
"""

import json
import re
import sys
from pathlib import Path

BUDGETS = Path(__file__).with_name("budgets.json")


def rules_for(name, budgets):
    """The (key, rule) pairs that apply to a result file, by its name."""
    if name.endswith("terrain-1024m.json"):
        return list(budgets["terrain"].items())
    if name.endswith("roundtrips.json"):
        return list(budgets["roundtrips"].items())
    match = re.search(r"crossing-([a-z]+)\.json$", name)
    if not match:
        return []
    mode = budgets["modes"].get(match.group(1), {})
    return list(budgets["all"].items()) + list(mode.items())


def breaches(name, result, budgets):
    """Human-readable breaches of the budgets by one result (empty when within budget)."""
    out = []
    for key, rule in rules_for(name, budgets):
        if key not in result:
            out.append(f"{name}: {key} missing from the result")
            continue
        value = result[key]
        if "equals" in rule and value != rule["equals"]:
            out.append(f"{name}: {key} = {value}, must be {rule['equals']}")
        if "max" in rule and value > rule["max"]:
            out.append(f"{name}: {key} = {value}, budget {rule['max']}")
    return out


def main(paths):
    budgets = json.loads(BUDGETS.read_text(encoding="utf-8"))
    problems, checked = [], 0
    for path in map(Path, paths):
        if not rules_for(path.name, budgets):
            continue
        checked += 1
        problems += breaches(path.name, json.loads(path.read_text(encoding="utf-8")), budgets)
    for problem in problems:
        print(f"  BUDGET BREACH {problem}")
    print(f"  {checked} result(s) checked against {BUDGETS.name}: {'within budget' if not problems else f'{len(problems)} breach(es)'}")
    return 1 if problems or not checked else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
