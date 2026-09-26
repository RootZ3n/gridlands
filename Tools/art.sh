#!/usr/bin/env bash
# P7 art pipeline: Blender recipes (Art/Source) -> FBX + manifest -> Unreal import + master materials
# + validation (GLImportArt). Rebuilds every style-proof mesh from source; nothing is hand-fixed.
# Usage: Tools/art.sh [--preview]
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
. "$(dirname "$0")/lib/commandlet.sh"
OUT="$GRIDLANDS_ROOT/Saved/Art/Export"
rm -rf "$OUT"
command -v blender >/dev/null || result FAIL "blender is not installed (Art/Source recipes need it)"
blender -b --factory-startup --python "$GRIDLANDS_ROOT/Art/Source/build_assets.py" -- "$OUT" "$@" 2>&1 | grep -E "^GLART|Traceback|Error:" | grep -v OpenColorIO
[ -s "$OUT/manifest.json" ] || result FAIL "Blender wrote no manifest"
run_commandlet GLImportArt "-Manifest=$OUT/manifest.json"
result PASS "art built from source, imported and validated (report: $OUT/import-report.json)"
