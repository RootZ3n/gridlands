#!/usr/bin/env bash
# Terrain scaling harness (P2): one fresh game process per cell size, real rendering (Vulkan,
# 1920x1080 offscreen, vsync off). Results: Saved/Perf/terrain-<m>m.json (gl.Perf.Terrain).
# Usage: Tools/perf-terrain.sh [metres[:spacing:chunk]...] (default 256 512 1024 2048)
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
SIZES=("$@"); [ ${#SIZES[@]} -eq 0 ] && SIZES=(256 512 1024 2048)
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
mkdir -p "$GRIDLANDS_ROOT/Saved/Perf"
for SPEC in "${SIZES[@]}"; do
	IFS=: read -r M SPACING CHUNK <<<"$SPEC"
	ARGS="$M ${SPACING:-1} ${CHUNK:-64}"
	OUT="terrain-${M}m.json"; [ -n "${SPACING:-}" ] && OUT="terrain-${M}m-s${SPACING}-c${CHUNK}.json"
	rm -f "$GRIDLANDS_ROOT/Saved/Perf/$OUT"
	timeout --signal=INT --kill-after=30 1500 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game -GLNewWorld \
		-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 \
		-ExecCmds="r.SetRes 1920x1080, r.VSync 0, t.MaxFPS 0, gl.Perf.Terrain $ARGS" >/dev/null 2>&1
	[ -s "$GRIDLANDS_ROOT/Saved/Perf/$OUT" ] && echo "  $SPEC: done" || echo "  $SPEC: NO RESULT (timeout or crash)"
done
rm -f "$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
result PASS "terrain scaling runs finished (Saved/Perf)"
