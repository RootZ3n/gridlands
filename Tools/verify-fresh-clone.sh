#!/usr/bin/env bash
# Proves a build + test works from repository-tracked inputs plus the pinned engine only.
#   Tools/verify-fresh-clone.sh [ref, default HEAD]
# Clones the ref into a temporary directory with no local engine override
# (GRIDLANDS_UE_ROOT unset, no .engine-root), checks the clone is pristine,
# runs Tools/test.sh (which builds first), and then checks that the run changed no
# tracked file and created only ignored build outputs.
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"

REF="${1:-HEAD}"
COMMIT="$(git -C "$GRIDLANDS_ROOT" rev-parse --verify "$REF^{commit}")" || result FAIL "unknown ref $REF"
mkdir -p "$GRIDLANDS_ROOT/.test-reports/fresh-clones"
WORK="$(mktemp -d "$GRIDLANDS_ROOT/.test-reports/fresh-clones/$(date +%Y%m%d-%H%M%S).XXXX")"
CLONE="$WORK/gridlands"
echo "Fresh clone of $COMMIT -> $CLONE"

git clone -q --no-local "$GRIDLANDS_ROOT" "$CLONE" || result FAIL "clone failed"
git -C "$CLONE" -c advice.detachedHead=false checkout -q "$COMMIT" || result FAIL "checkout failed"
git -C "$CLONE" lfs pull >/dev/null 2>&1 || result FAIL "git lfs pull failed"

[ -z "$(git -C "$CLONE" status --porcelain --ignored)" ] || result FAIL "clone is not pristine before the run"
[ ! -e "$CLONE/.engine-root" ] || result FAIL "clone contains .engine-root"

# Only the pinned default engine location is allowed.
env -u GRIDLANDS_UE_ROOT "$CLONE/Tools/test.sh"
status=$?

modified="$(git -C "$CLONE" status --porcelain --untracked-files=all)"
[ -z "$modified" ] || { echo "$modified"; result FAIL "the run modified tracked files or left untracked, non-ignored files"; }
unexpected="$(git -C "$CLONE" status --porcelain --ignored | awk '$1=="!!" {print $2}' \
	| grep -v -E '^(Binaries|Intermediate|Saved|DerivedDataCache|\.test-reports)/$' || true)"
[ -z "$unexpected" ] || { echo "$unexpected"; result FAIL "unexpected ignored outputs"; }

echo "  clone kept at $CLONE (report: $CLONE/.test-reports/latest)"
[ "$status" -ne 2 ] || result ENV "pinned engine unavailable to the fresh clone"
[ "$status" -eq 0 ] || result FAIL "Tools/test.sh failed in the fresh clone (exit $status)"
result PASS "fresh clone of $COMMIT built and tested from tracked inputs + pinned engine"
