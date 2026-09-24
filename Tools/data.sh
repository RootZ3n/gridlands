#!/usr/bin/env bash
# Content tooling; no engine needed (ADR-0002, Docs/CONTENT-IDS-AND-TAGS.md).
#   Tools/data.sh validate   check Data/ (grammar, references, invariants)
#   Tools/data.sh generate   regenerate Config/Tags/GeneratedFromData.ini
set -uo pipefail
# No bytecode caches in the tree: a run must leave the checkout unchanged (verify-fresh-clone.sh).
export PYTHONDONTWRITEBYTECODE=1
cd "$(dirname "$0")" && exec python3 -m gldata.cli "$@"
