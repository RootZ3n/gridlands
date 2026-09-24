#!/usr/bin/env bash
# Regenerates the origin cell's blockout map (Content/Gridlands/Maps/L_Origin.umap) from
# Source/GridlandsEditor/Private/Commandlets/GLBuildBlockoutCommandlet.cpp, then exports its anchors.
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
. "$(dirname "$0")/lib/commandlet.sh"
run_commandlet GLBuildBlockout
run_commandlet GLExportAnchors
result PASS "blockout map rebuilt and anchors exported"
