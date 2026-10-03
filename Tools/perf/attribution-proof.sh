#!/usr/bin/env bash
# Proves the perf harness attributes a slow frame's GC, game-, render- and RHI-thread time to that frame
# (pre-P11 fix): injects a known 60 ms stall on each thread in the real game (gl.Perf.AttributionProof).
# Result: Saved/Perf/attribution-proof.json. Exit 0 only when every injected stall lands where it belongs.
set -uo pipefail
. "$(dirname "$0")/../lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/Perf/attribution-proof.json"
rm -f "$OUT"
timeout --signal=INT --kill-after=30 600 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game -GLNewWorld \
	-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 \
	-ExecCmds="r.SetRes 1920x1080, r.VSync 0, t.MaxFPS 0, gl.Perf.AttributionProof" >/dev/null 2>&1
grep "gl.Perf.AttributionProof" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31-
[ -s "$OUT" ] || result FAIL "no proof result (timeout or crash)"
python3 -c "import json,sys; sys.exit(0 if json.load(open('$OUT'))['pass'] else 1)" || result FAIL "a stall was attributed to the wrong frame or metric (see $OUT)"
result PASS "every injected stall is attributed to the frame it made slow, on its own metric"
