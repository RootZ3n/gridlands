#!/usr/bin/env bash
# P12 build-mode measurement (ADR-0040), quiet machine only: beside the P11 309-piece player base (-GLPlayerDense), the
# game-thread cost of each build-mode state (gl.Perf.BuildMode). Repeats a run whose 1-minute load at its end was >= 8.
# The <= 2 ms incremental game-thread figure is a TARGET; no budget changes. Result: Saved/Perf/buildmode.json.
set -uo pipefail
cd "$(dirname "$0")/../.."
. Tools/lib/common.sh
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
SAVE=Saved/SaveGames/Gridlands/world.json
LOG=Saved/perf-buildmode.log; : > "$LOG"
load1() { cut -d' ' -f1 /proc/loadavg; }
gpu() { cat /sys/class/drm/card*/device/gpu_busy_percent 2>/dev/null | sort -n | tail -1; }
quiet() { for i in $(seq 1 240); do
	if awk -v l="$(load1)" 'BEGIN{exit !(l<3)}'; then
		ok=1; for k in 1 2 3; do [ "$(gpu)" -lt 10 ] || ok=0; sleep 1; done
		[ $ok -eq 1 ] && return 0
	fi
	sleep 10; done; return 0; }
KEEP=""
if [ -f "$SAVE" ]; then KEEP=Saved/Perf/world.operator-backup.json; mkdir -p Saved/Perf; cp -p "$SAVE" "$KEEP"; fi
restore() { rm -f "$SAVE"; if [ -n "$KEEP" ]; then mv "$KEEP" "$SAVE"; fi; }
trap restore EXIT
for attempt in 1 2 3; do
	quiet; start="$(load1)"
	rm -f Saved/Perf/buildmode.json "$SAVE"
	timeout --signal=INT --kill-after=30 400 "$UE_EDITOR" "$PWD/Gridlands.uproject" -game -GLNewWorld -GLPlayerDense \
		-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 -ExecCmds="gl.Perf.BuildMode" >/dev/null 2>&1
	end="$(load1)"
	grep -E "gl\.Perf\.BuildMode" Saved/Logs/Gridlands.log | cut -c31- | tr -d '\r' >> "$LOG"
	echo "attempt=$attempt load start=$start end=$end" >> "$LOG"
	awk -v l="$end" 'BEGIN{exit !(l<8)}' && break
done
[ -s Saved/Perf/buildmode.json ] || result FAIL "no build-mode result (see $LOG)"
result PASS "build-mode measurement written (Saved/Perf/buildmode.json; $LOG)"
