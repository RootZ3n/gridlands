#!/usr/bin/env bash
# P7 visual review (ADR-0032): a fresh game process with real rendering stages the style slice, runs
# day / dusk / night, a terrain edit, a structural collapse and a tree fall, and takes the review
# screenshots (gl.Style.Tour). Screenshots land in Saved/P7/<NN-name>.png, the tour log in
# Saved/P7/tour.log.txt. With --evidence, downscaled JPGs are written to Docs/Evidence/P7-visual-spike/screenshots
# (or DIR, at 1600x900). P7.1: --variant P7|A|B|C reviews a visual-direction experiment.
# Usage: Tools/p7-review.sh [--variant X] [--evidence [DIR]]
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
resolve_engine_root
UE_EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"
OUT="$GRIDLANDS_ROOT/Saved/P7"
SHOTS="$GRIDLANDS_ROOT/Saved/Screenshots/LinuxEditor"
SAVE="$GRIDLANDS_ROOT/Saved/SaveGames/Gridlands/world.json"
EVIDENCE=""; VARIANT=""; SIZE="1280,720"
while [ $# -gt 0 ]; do
	case "$1" in
		--variant) VARIANT="$2"; shift 2 ;;
		--evidence) if [ -n "${2:-}" ] && [ "${2#--}" = "$2" ]; then EVIDENCE="$2"; SIZE="1600,900"; shift 2; else EVIDENCE="$GRIDLANDS_ROOT/Docs/Evidence/P7-visual-spike/screenshots"; shift; fi ;;
		*) result FAIL "unknown argument $1" ;;
	esac
done
rm -rf "$OUT" "$SHOTS"; mkdir -p "$OUT"
rm -f "$SAVE"
timeout --signal=INT --kill-after=30 240 "$UE_EDITOR" "$GRIDLANDS_ROOT/Gridlands.uproject" -game -GLNewWorld \
	-RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 -ExecCmds="gl.Style.Tour $VARIANT" >/dev/null 2>&1
grep -E "LogGridlands: (gl\.Style|Style:|Visuals:|Structures:|Terrain:)" "$GRIDLANDS_ROOT/Saved/Logs/Gridlands.log" | cut -c31- > "$OUT/tour.log.txt"
# Shots are taken in order; the log names them in the same order.
mapfile -t NAMES < <(grep -oE "gl\.Style\.Shot [0-9]+ [^ ]+" "$OUT/tour.log.txt" | awk '{print $3}' | tr -d '\r')
mapfile -t FILES < <(ls -1tr "$SHOTS"/*.png 2>/dev/null)
if [ ${#FILES[@]} -ne ${#NAMES[@]} ] || [ ${#FILES[@]} -eq 0 ]; then
	result FAIL "tour produced ${#FILES[@]} screenshot(s) for ${#NAMES[@]} named shot(s) (see $OUT/tour.log.txt)"
fi
for i in "${!FILES[@]}"; do cp "${FILES[$i]}" "$OUT/${NAMES[$i]}.png"; done
if [ -n "$EVIDENCE" ]; then
	mkdir -p "$EVIDENCE"
	for png in "$OUT"/*.png; do
		python3 -c "import sys; from PIL import Image; w,h=map(int,sys.argv[3].split(',')); Image.open(sys.argv[1]).convert('RGB').resize((w,h), Image.LANCZOS).save(sys.argv[2], quality=90)" "$png" "$EVIDENCE/$(basename "${png%.png}").jpg" "$SIZE"
	done
	cp "$OUT/tour.log.txt" "$EVIDENCE/"
fi
result PASS "review tour${VARIANT:+ (variant $VARIANT)}: ${#FILES[@]} screenshots in Saved/P7"
