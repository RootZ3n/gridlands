#!/usr/bin/env bash
# Runs the P5 harness one mode (group) at a time, only on a quiet machine: waits for 1-minute load < 3
# before each run and repeats a run whose 1-minute load at its end was >= 8 (the game itself adds ~4;
# more means other sessions joined in).
cd "$(dirname "$0")/../.."
FLAG="${1:-}"; NAV="${2:-local}"   # "" local | -d dense | -b town | "-b -d" towndense
LOG=Saved/perf-quiet-$NAV.log; : > "$LOG"
load1() { cut -d' ' -f1 /proc/loadavg; }
gpu() { cat /sys/class/drm/card*/device/gpu_busy_percent 2>/dev/null | sort -n | tail -1; }
# Quiet = CPU load < 3 and the GPU idle (< 10% busy in 3 samples a second apart): other sessions and a
# browser playing video share this machine; p99 frame time moved with them, not with the build (A/B).
quiet() { for i in $(seq 1 240); do
	if awk -v l="$(load1)" 'BEGIN{exit !(l<3)}'; then
		ok=1; for k in 1 2 3; do [ "$(gpu)" -lt 10 ] || ok=0; sleep 1; done
		[ $ok -eq 1 ] && return 0
	fi
	sleep 10; done; return 0; }
rm -f Saved/Perf/$NAV-*
for group in "straight" "reversal" "sprint" "teleport resume" "roundtrips"; do
	for attempt in 1 2 3 4; do
		quiet; start="$(load1)"
		extra=""; [ "$group" = straight ] && [ "$NAV" = local ] && extra="-t"
		Tools/perf-crossing.sh $FLAG $extra $group >> "$LOG" 2>&1
		end="$(load1)"
		echo "group=[$group] attempt=$attempt load start=$start end=$end gpu-before=$(gpu)" >> "$LOG"
		awk -v l="$end" 'BEGIN{exit !(l<8)}' && break
	done
done
python3 Tools/perf/check_budgets.py Saved/Perf/$NAV-*.json >> "$LOG" 2>&1 && echo "QUIETDONE PASS" >> "$LOG" || echo "QUIETDONE FAIL" >> "$LOG"
