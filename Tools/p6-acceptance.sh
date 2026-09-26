#!/usr/bin/env bash
# P6 real-game acceptance (structural salvage, collapse, trees, noise): fresh game processes with real
# rendering; each run's Grid/structure/noise log lines land in Saved/P6/<run>.log.txt and its
# screenshots in Saved/P6/<run>-*.png. Persistence runs quit (autosave) and relaunch without -GLNewWorld.
# Usage: Tools/p6-acceptance.sh [run...]   runs: collapse kill creature tree edge noise (default: all)
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P6"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
SHOTS="$GRIDLANDS_ROOT/Saved/Screenshots/LinuxEditor"
mkdir -p "$OUT"
RUNS=("$@"); [ ${#RUNS[@]} -eq 0 ] && RUNS=(collapse kill creature tree edge noise)

launch() { # name, new-world flag, seconds, commands
	rm -rf "$SHOTS"
	timeout --signal=INT --kill-after=30 "$3" "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $2 \
		-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 -ExecCmds="$4" >/dev/null 2>&1
	grep -E "LogGridlands: (gl\.Demo|Grid: (loading|unloaded|entered)|Load:|Save:|Structures:|Placements: DEV)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- > "$OUT/$1.log.txt"
	local i=0
	for shot in "$SHOTS"/*.png; do
		[ -f "$shot" ] || continue
		i=$((i + 1)); cp "$shot" "$OUT/$1-$i.png"
	done
	echo "  $1: $(grep -cE 'PASS' "$OUT/$1.log.txt") PASS, $(grep -cE 'FAIL' "$OUT/$1.log.txt") FAIL line(s)"
}

for RUN in "${RUNS[@]}"; do
	rm -f "$SAVE"
	case $RUN in
		collapse) launch collapse -GLNewWorld 120 "gl.Demo.Collapse, gl.Demo.QuitIn 12"
		          launch collapse-restart "" 90 "gl.Demo.StructureReport, gl.Demo.QuitIn 6" ;;
		kill)     launch kill -GLNewWorld 120 "gl.Demo.Collapse kill, gl.Demo.QuitIn 12" ;;
		creature) launch creature -GLNewWorld 120 "gl.Demo.CollapseCreature, gl.Demo.QuitIn 10" ;;
		tree)     launch tree -GLNewWorld 120 "gl.Demo.FellTree, gl.Demo.QuitIn 12"
		          launch tree-restart "" 90 "gl.Demo.StructureReport, gl.Demo.QuitIn 6" ;;
		edge)     launch edge -GLNewWorld 120 "gl.Demo.StructureEdge, gl.Demo.QuitIn 16"
		          launch edge-restart "" 90 "gl.Demo.GridMove 48600 1500, gl.Demo.StructureReport, gl.Demo.QuitIn 6" ;;
		noise)    launch noise -GLNewWorld 150 "gl.Demo.Noise, gl.Demo.QuitIn 56" ;;
		*) echo "unknown run $RUN"; exit 2 ;;
	esac
done
rm -f "$SAVE"
FAILS=$(cat "$OUT"/*.log.txt | grep -c "FAIL" || true)
[ "$FAILS" -eq 0 ] && result PASS "P6 acceptance runs (Saved/P6)" || result FAIL "$FAILS FAIL line(s) in Saved/P6"
