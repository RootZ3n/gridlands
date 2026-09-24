# Runs a Gridlands editor commandlet headless and judges it by its exit status and log.
# shellcheck shell=bash
run_commandlet() {
	local name="$1"; shift
	require_pinned_engine
	[ -z "$(editor_running_for_project)" ] || result FAIL "an editor has this project open; close it first"
	local log="$GRIDLANDS_ROOT/.test-reports/commandlet-$name.log"
	mkdir -p "$GRIDLANDS_ROOT/.test-reports"
	"$UE_EDITOR_CMD" "$GRIDLANDS_UPROJECT" -run="$name" -unattended -nullrhi -nosplash -nosound -nopause -NoP4 -stdout "$@" >"$log" 2>&1
	local status=$?
	grep -E "LogGridlandsEditor: (Display|Error|Warning)" "$log" | sed 's/^.*LogGridlandsEditor: /  /'
	[ "$status" -eq 0 ] || result FAIL "$name exited $status (log $log)"
	! grep -q "LogGridlandsEditor: Error" "$log" || result FAIL "$name logged errors (log $log)"
}
