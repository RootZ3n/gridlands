#!/usr/bin/env bash
# Writes Data/anchor/<cell>.generated.json for every cell whose definition names a level (ADR-0018).
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"
. "$(dirname "$0")/lib/commandlet.sh"
run_commandlet GLExportAnchors
result PASS "anchors exported"
