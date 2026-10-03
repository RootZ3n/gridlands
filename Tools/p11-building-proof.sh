#!/usr/bin/env bash
# P11 WINCHESTER / REAL HOUSE-0 proof in the real game (ADR-0039): build (a fresh world: frame, finish, screenshots),
# restart (the house is back exactly; streamed away and back; the porch collapse decided; quit mid-fall) and resume
# (the fall resumes and lands once; salvage). Results: Saved/P11/building-<mode>.json, <mode>.log.txt and screenshots.
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P11"
SHOTS="$GRIDLANDS_ROOT/Saved/Screenshots/LinuxEditor"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
mkdir -p "$OUT/screenshots"
launch() { # mode, new-world flag
	rm -f "$OUT/building-$1.json"
	rm -rf "$SHOTS"
	timeout --signal=INT --kill-after=30 400 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $2 \
		-RenderOffscreen -ResX=1600 -ResY=900 -ForceRes -nosound -unattended -NoP4 -ExecCmds="gl.Building.Proof $1" >/dev/null 2>&1
	grep -E "LogGridlands: (gl\.Building|Load:|Save:|Building:|Structures:)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- | tr -d '\r' > "$OUT/$1.log.txt"
	# Screenshots in the order the run logged them, renamed after what they show.
	mapfile -t NAMES < <(grep -o "screenshot [0-9]* [a-z-]*" "$OUT/$1.log.txt" | awk '{print $3}')
	mapfile -t FILES < <(ls -1tr "$SHOTS"/*.png 2>/dev/null)
	for i in "${!FILES[@]}"; do [ -n "${NAMES[$i]:-}" ] && cp "${FILES[$i]}" "$OUT/screenshots/$1-$i-${NAMES[$i]}.png"; done
	if [ -s "$OUT/building-$1.json" ]; then echo "  $1: done ($(python3 -c "import json;print('PASS' if json.load(open('$OUT/building-$1.json'))['pass'] else 'FAIL')"))"; else echo "  $1: NO RESULT"; fi
}
rm -f "$SAVE"
launch build -GLNewWorld
launch restart ""
launch resume ""
rm -f "$SAVE"
FAIL=0
for m in build restart resume; do python3 -c "import json,sys;sys.exit(0 if json.load(open('$OUT/building-$m.json'))['pass'] else 1)" 2>/dev/null || FAIL=1; done
[ $FAIL -eq 0 ] || result FAIL "a WINCHESTER check failed (Saved/P11)"
result PASS "WINCHESTER proof: build, restart (streamed), resume all pass (Saved/P11)"
