#!/usr/bin/env bash
# P7 dense-spawn persistence proof (ADR-0033) in the real game: with the dev-only dense authored
# fixture (-GLDenseProof), Zenny damages it through the real salvage pipeline and quits (autosave);
# a relaunch loads that save and reports. The digest of the lots' structure facts must be identical,
# and every present part must have exactly one actor, none resurrected. Logs: Saved/P7-dense/dense-*.log.txt
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P7-dense" # not Saved/P7: the review tour clears that
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
mkdir -p "$OUT"
launch() { # name, new-world flag, seconds, commands
	timeout --signal=INT --kill-after=30 "$3" "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game $2 -GLDenseProof \
		-RenderOffscreen -ResX=1280 -ResY=720 -ForceRes -nosound -unattended -NoP4 -ExecCmds="$4" >/dev/null 2>&1
	grep -E "LogGridlands: (gl\.Demo|Grid: |Load:|Save:|Placements: DEV)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- | tr -d '\r' > "$OUT/$1.log.txt"
}
rm -f "$SAVE"
launch dense-act -GLNewWorld 150 "gl.Demo.Dense act, gl.Demo.QuitIn 14"
launch dense-restart "" 120 "gl.Demo.Dense report, gl.Demo.QuitIn 6"
rm -f "$SAVE"
before="$(grep -oE "before quit: .* digest [0-9a-f]+" "$OUT/dense-act.log.txt" | grep -oE "digest [0-9a-f]+")"
after="$(grep -oE "after restart: .* digest [0-9a-f]+" "$OUT/dense-restart.log.txt" | grep -oE "digest [0-9a-f]+")"
grep -h "gl.Demo.Dense: .*|" "$OUT"/dense-*.log.txt
[ -n "$before" ] && [ "$before" = "$after" ] || result FAIL "facts differ across the restart ($before vs $after)"
grep -q "before quit: .*| PASS" "$OUT/dense-act.log.txt" && grep -q "after restart: .*| PASS" "$OUT/dense-restart.log.txt" || result FAIL "presentation invariant failed (see $OUT)"
result PASS "dense fixture: identical facts across the restart ($before), one actor per present part"
