# ADR-0008: Tests pass only by a parsed, non-vacuous report

- Status: Accepted
- Date: 2026-09-24
- Decider: agent (operator-approved bootstrap requirement)

## Context
Game engines can exit 0 while broken. The lab saw a Godot project with a
broken autoload exit 0, and verifier suites that "passed" having run nothing.

## Decision
`Tools/test.sh` decides pass/fail by parsing Unreal's automation report
(`index.json`), never the process exit code alone. It fails when:
- the report is missing or unparsable;
- zero tests ran;
- any test failed or did not run;
- any test listed in `Tools/required-tests.txt` is absent from the report.

Warnings do not fail a run, but every distinct warning message and its count
is printed before the `RESULT` line, and the count is part of it.

The report parser has its own tests (`Tools/tests/`), which run without the engine.
Test names follow `Gridlands.<Layer>.<System>.<Case>`.
