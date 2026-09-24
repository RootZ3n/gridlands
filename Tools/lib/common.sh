# Shared helpers for Tools/*.sh. Source it; do not execute it.
# shellcheck shell=bash

GRIDLANDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GRIDLANDS_UPROJECT="$GRIDLANDS_ROOT/Gridlands.uproject"
GRIDLANDS_PIN_FILE="$GRIDLANDS_ROOT/Tools/engine-pin.env"

# shellcheck source=../engine-pin.env
. "$GRIDLANDS_PIN_FILE"

# Final line of every tool: RESULT: PASS|FAIL|ENV <reason>. Exit 0 / 1 / 2.
result() {
	local status="$1"; shift
	echo "RESULT: $status $*"
	case "$status" in
		PASS) exit 0 ;;
		ENV) exit 2 ;;
		*) exit 1 ;;
	esac
}

# Engine root: $GRIDLANDS_UE_ROOT, then .engine-root, then the pinned default.
resolve_engine_root() {
	if [ -n "${GRIDLANDS_UE_ROOT:-}" ]; then
		ENGINE_ROOT_SOURCE="GRIDLANDS_UE_ROOT"
		ENGINE_ROOT="$GRIDLANDS_UE_ROOT"
	elif [ -f "$GRIDLANDS_ROOT/.engine-root" ]; then
		ENGINE_ROOT_SOURCE=".engine-root"
		ENGINE_ROOT="$(head -n1 "$GRIDLANDS_ROOT/.engine-root")"
	else
		ENGINE_ROOT_SOURCE="default"
		ENGINE_ROOT="$GRIDLANDS_UE_DEFAULT_ROOT"
	fi
	ENGINE_ROOT="${ENGINE_ROOT%/}"
	UE_BUILD_VERSION="$ENGINE_ROOT/Engine/Build/Build.version"
	UE_EDITOR_CMD="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
	UE_BUILD_SH="$ENGINE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
}

# Prints "Major.Minor.Patch Changelist" from Build.version, or fails.
engine_version() {
	python3 - "$UE_BUILD_VERSION" <<'PY'
import json, sys
v = json.load(open(sys.argv[1], encoding="utf-8-sig"))
print(f"{v['MajorVersion']}.{v['MinorVersion']}.{v['PatchVersion']} {v.get('Changelist', '')}")
PY
}

# Refuses to continue unless the engine exists and matches the pin. Exit 2 otherwise.
require_pinned_engine() {
	resolve_engine_root
	[ -f "$UE_BUILD_VERSION" ] || result ENV "no Unreal engine at $ENGINE_ROOT (from $ENGINE_ROOT_SOURCE); run Tools/doctor.sh"
	local found version changelist
	found="$(engine_version)" || result ENV "cannot read $UE_BUILD_VERSION"
	version="${found% *}"; changelist="${found#* }"
	[ "$version" = "$GRIDLANDS_UE_VERSION" ] || result ENV "engine is $version, pin is $GRIDLANDS_UE_VERSION"
	if [ -n "$GRIDLANDS_UE_CHANGELIST" ] && [ "$changelist" != "$GRIDLANDS_UE_CHANGELIST" ]; then
		result ENV "engine changelist $changelist, pin is $GRIDLANDS_UE_CHANGELIST"
	fi
}

# The editor holding this project's binaries makes builds and tests unreliable.
editor_running_for_project() {
	pgrep -af 'UnrealEditor' 2>/dev/null | grep -F -- "$GRIDLANDS_UPROJECT" | grep -v -- '-Cmd' || true
}
