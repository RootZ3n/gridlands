#!/usr/bin/env bash
# P9 dual-route proof in the real game (ADR-0037), with the dev proof room (-GLDungeonProof): each route in a
# fresh world, then a restart from its autosave that reports what persisted (nothing may replay); and the
# navigation-scale measurement (the room's creatures, then 1 and 16 extra active ones in the same region).
# Results: Saved/P9/dungeon-<run>.json and <run>.log.txt.
# Usage: Tools/p9-dungeon-proof.sh [direct] [environmental] [navscale]   (default: all three)
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P9"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
mkdir -p "$OUT"
launch() { # run name, new-world flag, mode
	local MODE="${3%% *}"
	rm -f "$OUT/dungeon-$MODE.json"
	timeout --signal=INT --kill-after=30 400 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $2 -GLDungeonProof \
		-RenderOffscreen -ResX=1280 -ResY=720 -ForceRes -nosound -unattended -NoP4 -ExecCmds="gl.Dungeon.Proof $3" >/dev/null 2>&1
	grep -E "LogGridlands: (gl\.Dungeon|Mechanism|Load:|Save:|Placements: DEV)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- | tr -d '\r' > "$OUT/$1.log.txt"
	if [ -s "$OUT/dungeon-$MODE.json" ]; then [ "$MODE" = "$1" ] || mv "$OUT/dungeon-$MODE.json" "$OUT/dungeon-$1.json"; echo "  $1: done"; else echo "  $1: NO RESULT"; fi
}
ROUTES=("$@"); [ ${#ROUTES[@]} -eq 0 ] && ROUTES=(direct environmental navscale)
NAVSCALE_ONLY_INSIDE=0; [ "${ROUTES[*]}" = "navscale-inside" ] && NAVSCALE_ONLY_INSIDE=1 && ROUTES=(navscale)
for route in "${ROUTES[@]}"; do
	[ "$route" = navscale ] && continue
	rm -f "$SAVE"
	launch "$route" -GLNewWorld "$route"
	launch "$route-restart" "" report
done
rm -f "$SAVE"
if printf '%s\n' "${ROUTES[@]}" | grep -qx navscale; then
	[ $NAVSCALE_ONLY_INSIDE -eq 1 ] || { launch navscale -GLNewWorld navscale; rm -f "$SAVE"; }   # Zenny 60 m away: the region on demand only
	launch navscale-inside -GLNewWorld "navscale inside"; rm -f "$SAVE"   # Zenny inside the room
fi
result PASS "P9 dungeon proof runs finished (Saved/P9)"
