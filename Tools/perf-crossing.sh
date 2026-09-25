#!/usr/bin/env bash
# Seamless-streaming harness (P5): one fresh game process per run, real rendering (Vulkan,
# 1920x1080 offscreen, vsync off), Zenny walks across the canonical 1 km Grid (gl.Perf.Crossing).
# Results: Saved/Perf/<nav>-crossing-<mode>.json, plus <nav>-terrain-1024m.json with -t.
# Usage: Tools/perf-crossing.sh [-t] [-w] [mode...]
#   modes: straight reversal sprint teleport resume (default: straight reversal sprint teleport resume)
#   -w: whole-cell navigation (invokers off) instead of localized navigation (ADR-0029)
#   -t: also run the 1 km terrain harness (gl.Perf.Terrain 1024)
#   "resume" launches from the save that "teleport" leaves in the second cell (quit autosaves).
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
NAV=local; EXTRA=(); TERRAIN=0
while getopts "wt" OPT; do
	case $OPT in
		w) NAV=whole; EXTRA=(-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bGenerateNavigationOnlyAroundNavigationInvokers=False) ;;
		t) TERRAIN=1 ;;
		*) exit 2 ;;
	esac
done
shift $((OPTIND - 1))
MODES=("$@"); [ ${#MODES[@]} -eq 0 ] && MODES=(straight reversal sprint teleport resume)
PERF="$GRIDLANDS_ROOT/Saved/Perf"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
mkdir -p "$PERF"

run() { # command, result file, output name, new-world flag
	rm -f "$PERF/$2"
	timeout --signal=INT --kill-after=30 1500 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $4 \
		-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 "${EXTRA[@]}" \
		-ExecCmds="r.SetRes 1920x1080, r.VSync 0, t.MaxFPS 0, $1" >/dev/null 2>&1
	if [ -s "$PERF/$2" ]; then
		mv "$PERF/$2" "$PERF/$3"
		grep -E "LogGridlands: (Grid|Load|Save|gl.Demo.GridReport)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- > "$PERF/$3.log.txt"
		echo "  $3: done"
	else
		echo "  $3: NO RESULT (timeout or crash)"
	fi
}

[ $TERRAIN -eq 1 ] && { run "gl.Perf.Terrain 1024" terrain-1024m.json "$NAV-terrain-1024m.json" -GLNewWorld; rm -f "$SAVE"; }
for MODE in "${MODES[@]}"; do
	if [ "$MODE" = resume ]; then
		[ -f "$SAVE" ] || { echo "  resume: no save (run teleport first)"; continue; }
		run "gl.Perf.Crossing resume" crossing-resume.json "$NAV-crossing-resume.json" ""
		rm -f "$SAVE"
	else
		rm -f "$SAVE"
		run "gl.Perf.Crossing $MODE" "crossing-$MODE.json" "$NAV-crossing-$MODE.json" -GLNewWorld
		[ "$MODE" = teleport ] || rm -f "$SAVE"
	fi
done
rm -f "$SAVE"
result PASS "crossing runs finished (Saved/Perf/$NAV-*)"
