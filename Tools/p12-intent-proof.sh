#!/usr/bin/env bash
# P12 playable-building proof in the real game (ADR-0040): every step goes through the build mode's public intents with
# the real camera; camera aim == gameplay aim is checked at every placement. build (a fresh world: browser, variants,
# finishes, stair climbed, removal and salvage, build camera, interaction counts, screenshots) and restart (everything is
# back exactly; the profile's favorites and recents survive). Results: Saved/P12/intent-<mode>.json, <mode>.log.txt and
# Saved/P12/screenshots.
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P12"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
mkdir -p "$OUT"
launch() { # mode, new-world flag
	rm -f "$OUT/intent-$1.json"
	rm -f "$OUT"/screenshots/"$1"-*.png
	timeout --signal=INT --kill-after=30 600 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $2 \
		-RenderOffscreen -ResX=1600 -ResY=900 -ForceRes -nosound -unattended -NoP4 -ExecCmds="gl.Building.IntentProof $1" >/dev/null 2>&1
	grep -E "LogGridlands: (gl\.Building|Load:|Save:|Building:)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- | tr -d '\r' > "$OUT/$1.log.txt"
	if [ -s "$OUT/intent-$1.json" ]; then echo "  $1: done ($(python3 -c "import json;print('PASS' if json.load(open('$OUT/intent-$1.json'))['pass'] else 'FAIL')"))"; else echo "  $1: NO RESULT"; fi
}
# The operator's own world (a daily-driver session) is set aside and put back; the proof never consumes it.
KEEP=""
if [ -f "$SAVE" ]; then KEEP="$OUT/world.operator-backup.json"; cp -p "$SAVE" "$KEEP"; fi
restore() { rm -f "$SAVE"; if [ -n "$KEEP" ]; then mv "$KEEP" "$SAVE"; fi; }
trap restore EXIT
rm -f "$SAVE"
launch build -GLNewWorld
launch restart ""
FAIL=0
for m in build restart; do python3 -c "import json,sys;sys.exit(0 if json.load(open('$OUT/intent-$m.json'))['pass'] else 1)" 2>/dev/null || FAIL=1; done
[ $FAIL -eq 0 ] || result FAIL "a playable-building intent check failed (Saved/P12)"
result PASS "P12 intent proof: build and restart pass through the public intents (Saved/P12)"
