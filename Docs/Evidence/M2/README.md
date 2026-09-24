# M2 evidence: data pipeline v1

Verified commit: `21c9dde1024d913ea4a3a12980034cb0b1e020e4` (branch `m2-data-pipeline`).
Engine: Unreal 5.8.3, CL 58210709. Reports are sanitized: host details removed,
test paths, states, counts and entries unchanged.

| File | What it proves | Verdict |
|---|---|---|
| `02-data-validate.log` | `Tools/data.sh validate`: 57 entities (10 eras, 6 bands, 10 yield categories, ...) pass every rule | PASS |
| `04-tooling-tests.log` | 55 tooling tests. Each validator rule (ID-1..10, TAG-2/3, GEN-1, ERA-1, NC-2, E-1, P-3, DLG-1, PLC-1..3, SCHEMA) fires on a targeted break of the real data, and clean data gives zero problems | PASS |
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-21c9dde.index.json` | fresh clone: validate, build, 15/15 automation tests, 0 warnings, tree unchanged afterwards | PASS |
| `03-mutation.diff` + `03-mutation-cxx-drops-weight.index.json` | deleting one C++ field (`FGLItemDef::Weight`) is caught by 5 tests, including `SchemaKeysMatchValidator` | rejected, 5/15 fail |

The first hermeticity run failed on `Tools/gldata/__pycache__`. The fix was to
stop the tools writing bytecode, not to widen the allowed outputs.
