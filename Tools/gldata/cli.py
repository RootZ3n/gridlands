"""Tools/data.sh entry point.

  validate   check Data/ against the id/tag grammar and design invariants
  generate   rewrite generated files (Config/Tags/GeneratedFromData.ini)

Last line is always "RESULT: PASS|FAIL <reason>"; exit 0 pass, 1 fail, 2 usage.
"""

from __future__ import annotations

import sys
from pathlib import Path

from . import tagfiles, validate


def main(argv: list[str]) -> int:
    repo_root = Path(__file__).resolve().parents[2]
    command = argv[1] if len(argv) > 1 else ""
    if command == "validate":
        ds = validate.run(repo_root)
        for problem in ds.problems:
            print(problem)
        counts = {}
        for entity in ds.entities.values():
            counts[entity.kind] = counts.get(entity.kind, 0) + 1
        summary = ", ".join(f"{k} {v}" for k, v in sorted(counts.items())) or "no entities"
        print(f"  entities: {len(ds.entities)} ({summary}); anchors: {len(ds.anchors)}")
        if ds.problems:
            print(f"RESULT: FAIL {len(ds.problems)} problem(s)")
            return 1
        print(f"RESULT: PASS {len(ds.entities)} entities valid")
        return 0
    if command == "generate":
        ds = validate.load(repo_root)
        path = repo_root / tagfiles.GENERATED_FILE
        path.parent.mkdir(parents=True, exist_ok=True)
        text = tagfiles.render_generated(ds)
        changed = not path.exists() or path.read_text(encoding="utf-8") != text
        path.write_text(text, encoding="utf-8")
        print(f"RESULT: PASS {tagfiles.GENERATED_FILE} {'updated' if changed else 'unchanged'}")
        return 0
    print(__doc__.strip())
    print("RESULT: FAIL usage")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
