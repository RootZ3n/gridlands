#!/usr/bin/env bash
# Terrain visual regression scenes (ADR-0035): the render-diagonal review's scenes with the same cameras,
# lighting and settings, captured twice (the second run is the determinism control). Compare "current"
# with Docs/Evidence/Terrain-heightfield-spike/diagonal-review (its "after" side is the approved state; the
# aggressive-terraforming pair 05-* is the regression case future presentation work must improve).
# Frame-counted with a fixed timestep. Screenshots land in Saved/TerrainDiagonal/<run>/.
# Usage: Tools/terrain-diagonal-review.sh
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/TerrainDiagonal"
SHOTS="$GRIDLANDS_ROOT/Saved/Screenshots/LinuxEditor"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
rm -rf "$OUT"; mkdir -p "$OUT"
capture() { # run name, collision mode
	rm -rf "$SHOTS"; rm -f "$SAVE"; mkdir -p "$OUT/$1"
	timeout --signal=INT --kill-after=30 600 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game -GLNewWorld \
		-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 -benchmark -fps=30 \
		-GLTerrainCollision="$2" -ExecCmds="gl.Terrain.DiagonalReview" >/dev/null 2>&1
	grep -E "LogGridlands: gl\.Terrain\.Diagonal" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- | tr -d '\r' > "$OUT/$1/review.log.txt"
	mapfile -t NAMES < <(grep -oE "gl\.Terrain\.DiagonalShot [0-9]+ [^ ]+" "$OUT/$1/review.log.txt" | awk '{print $3}')
	mapfile -t FILES < <(ls -1tr "$SHOTS"/*.png 2>/dev/null)
	if [ ${#FILES[@]} -ne ${#NAMES[@]} ] || [ ${#FILES[@]} -eq 0 ]; then
		result FAIL "$1: ${#FILES[@]} screenshot(s) for ${#NAMES[@]} named shot(s) (see $OUT/$1/review.log.txt)"
	fi
	for i in "${!FILES[@]}"; do cp "${FILES[$i]}" "$OUT/$1/${NAMES[$i]}.png"; done
	echo "  $1: ${#FILES[@]} screenshots"
}
capture current 1
capture current-control 1
result PASS "terrain regression scenes captured in Saved/TerrainDiagonal (current, current-control)"
