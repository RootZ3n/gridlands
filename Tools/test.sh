#!/usr/bin/env bash
# Builds, then runs the Gridlands automation tests headless, and judges them by
# the parsed report, never the exit code alone (ADR-0008).
#   Tools/test.sh [--no-build] [test filter, default "Gridlands"]
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"

BUILD=1
if [ "${1:-}" = "--no-build" ]; then BUILD=0; shift; fi
FILTER="${1:-Gridlands}"

# Engine-free checks first; they fail in seconds. The tooling self-tests (validator rules,
# report parser, architecture rules) are part of the gate: they once went stale unnoticed
# for two milestones because only a manual command ran them.
mkdir -p "$GRIDLANDS_ROOT/.test-reports"
"$GRIDLANDS_ROOT/Tools/selftest.sh" > "$GRIDLANDS_ROOT/.test-reports/selftest.log" 2>&1 \
	|| { tail -20 "$GRIDLANDS_ROOT/.test-reports/selftest.log"; result FAIL "tooling self-tests failed (log .test-reports/selftest.log)"; }
echo "  $(grep -E '^Ran [0-9]+ tests' "$GRIDLANDS_ROOT/.test-reports/selftest.log" | tail -1) (tooling)"
# Content is validated next (ADR-0002/0020).
"$GRIDLANDS_ROOT/Tools/data.sh" validate | tail -3
[ "${PIPESTATUS[0]}" -eq 0 ] || result FAIL "Data/ validation failed; run Tools/data.sh validate"

require_pinned_engine
if [ "$BUILD" = 1 ]; then
	# A stale binary once hid real failures in the lab; tests always run what was just built.
	"$GRIDLANDS_ROOT/Tools/build.sh" || result FAIL "build failed; not running tests against stale binaries"
fi

REPORT_DIR="$GRIDLANDS_ROOT/.test-reports/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$REPORT_DIR"
LOG="$REPORT_DIR/editor.log"
echo "Running automation filter '$FILTER' -> $REPORT_DIR"

timeout --kill-after=30 "${GRIDLANDS_TEST_TIMEOUT:-1800}" \
	"$UE_EDITOR_CMD" "$GRIDLANDS_UPROJECT" \
	-ExecCmds="Automation RunTests $FILTER; Quit" \
	-TestExit="Automation Test Queue Empty" \
	-ReportExportPath="$REPORT_DIR" \
	-unattended -nullrhi -nosplash -nosound -nopause -NoP4 -stdout -FullStdOutLogOutput \
	>"$LOG" 2>&1
editor_status=$?
echo "  editor exit $editor_status (informational; the report decides)"
[ "$editor_status" -eq 124 ] && result FAIL "editor timed out; log $LOG"

python3 "$GRIDLANDS_ROOT/Tools/lib/automation_report.py" "$REPORT_DIR" "$GRIDLANDS_ROOT/Tools/required-tests.txt"
verdict=$?
ln -sfn "$REPORT_DIR" "$GRIDLANDS_ROOT/.test-reports/latest"
# The parser already printed the RESULT line; mirror its exit status.
exit "$verdict"
