#!/usr/bin/env bash
# Tests the tooling itself. Needs no engine.
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"

for script in "$GRIDLANDS_ROOT"/Tools/*.sh "$GRIDLANDS_ROOT"/Tools/lib/*.sh; do
	bash -n "$script" || result FAIL "syntax error in $script"
done
if command -v shellcheck >/dev/null; then
	shellcheck -x -e SC1091 "$GRIDLANDS_ROOT"/Tools/*.sh || result FAIL "shellcheck"
fi
python3 -m unittest discover -s "$GRIDLANDS_ROOT/Tools/tests" -p 'test_*.py' -v 2>&1 || result FAIL "tooling unit tests"
result PASS "tooling self-tests"
