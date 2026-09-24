#!/usr/bin/env bash
# Checks this machine against the project's pinned toolchain.
#   Tools/doctor.sh               report
#   Tools/doctor.sh --record-pin  also write the installed changelist into engine-pin.env
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"

RECORD_PIN=0
[ "${1:-}" = "--record-pin" ] && RECORD_PIN=1

fails=0; env_fails=0
ok()   { echo "  ok    $*"; }
warn() { echo "  warn  $*"; }
bad()  { echo "  FAIL  $*"; fails=$((fails + 1)); }
envbad() { echo "  ENV   $*"; env_fails=$((env_fails + 1)); }

echo "Gridlands doctor (pin: Unreal $GRIDLANDS_UE_VERSION${GRIDLANDS_UE_CHANGELIST:+ CL $GRIDLANDS_UE_CHANGELIST})"

echo "Repository"
command -v git >/dev/null && ok "git $(git --version | awk '{print $3}')" || envbad "git not installed"
if command -v git-lfs >/dev/null; then
	ok "$(git lfs version | awk '{print $1}')"
	if grep -qs 'git lfs' "$GRIDLANDS_ROOT/.git/hooks/pre-push" 2>/dev/null \
		|| git -C "$GRIDLANDS_ROOT" config --get filter.lfs.process >/dev/null; then
		ok "git lfs filters configured"
	else
		bad "git lfs not initialised for this user: run 'git lfs install'"
	fi
else
	envbad "git-lfs not installed (Fedora: sudo dnf install git-lfs)"
fi
if command -v python3 >/dev/null && python3 -c 'import sys; sys.exit(sys.version_info < (3, 9))'; then
	ok "python $(python3 -c 'import platform; print(platform.python_version())')"
else
	envbad "python3 >= 3.9 required (tooling)"
fi
association="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['EngineAssociation'])" "$GRIDLANDS_UPROJECT" 2>/dev/null)"
[ "$association" = "$GRIDLANDS_UE_ASSOCIATION" ] \
	&& ok "Gridlands.uproject EngineAssociation $association" \
	|| bad "Gridlands.uproject EngineAssociation '$association' != pin '$GRIDLANDS_UE_ASSOCIATION'"

echo "Engine"
resolve_engine_root
echo "  info  engine root $ENGINE_ROOT (from $ENGINE_ROOT_SOURCE)"
if [ ! -f "$UE_BUILD_VERSION" ]; then
	envbad "Unreal $GRIDLANDS_UE_VERSION not found. Download Linux_Unreal_Engine_$GRIDLANDS_UE_VERSION.zip from"
	echo "        https://www.unrealengine.com/linux (Epic login) and unzip so that"
	echo "        $ENGINE_ROOT/Engine/Build/Build.version exists, or set GRIDLANDS_UE_ROOT / .engine-root."
else
	found="$(engine_version)"; version="${found% *}"; changelist="${found#* }"
	if [ "$version" = "$GRIDLANDS_UE_VERSION" ]; then
		ok "engine version $version (CL $changelist)"
	else
		envbad "engine version $version != pin $GRIDLANDS_UE_VERSION"
	fi
	if [ -z "$GRIDLANDS_UE_CHANGELIST" ]; then
		if [ "$RECORD_PIN" = 1 ] && [ "$version" = "$GRIDLANDS_UE_VERSION" ]; then
			sed -i "s/^GRIDLANDS_UE_CHANGELIST=.*/GRIDLANDS_UE_CHANGELIST=$changelist/" "$GRIDLANDS_PIN_FILE"
			ok "recorded changelist $changelist in Tools/engine-pin.env (commit it)"
		else
			bad "pin incomplete: changelist not recorded; run 'Tools/doctor.sh --record-pin'"
		fi
	elif [ "$changelist" != "$GRIDLANDS_UE_CHANGELIST" ]; then
		envbad "engine changelist $changelist != pin $GRIDLANDS_UE_CHANGELIST"
	fi
	[ -x "$UE_EDITOR_CMD" ] && ok "UnrealEditor-Cmd present" || envbad "missing $UE_EDITOR_CMD"
	[ -x "$UE_BUILD_SH" ] && ok "Build.sh present" || envbad "missing $UE_BUILD_SH"
fi

echo "Host"
mem_gb=$(awk '/MemTotal/ {printf "%d", $2/1048576}' /proc/meminfo)
[ "$mem_gb" -ge 32 ] && ok "RAM ${mem_gb} GB" || warn "RAM ${mem_gb} GB (Epic minimum 32 GB; watch for OOM while compiling)"
free_gb=$(df -Pk "$GRIDLANDS_ROOT" | awk 'NR==2 {printf "%d", $4/1048576}')
[ "$free_gb" -ge 50 ] && ok "disk free ${free_gb} GB" || warn "disk free ${free_gb} GB (< 50 GB)"
if [ -r /etc/os-release ]; then
	. /etc/os-release
	case "$ID" in ubuntu|rocky|rhel) ok "distro $PRETTY_NAME" ;; *) warn "distro $PRETTY_NAME is not Epic-recommended (Ubuntu 22.04 / Rocky 8)" ;; esac
fi
running="$(editor_running_for_project)"
[ -z "$running" ] && ok "no editor open on this project" || warn "an editor has this project open; close it before Tools/build.sh"

[ "$env_fails" -gt 0 ] && result ENV "$env_fails environment problem(s), $fails check failure(s)"
[ "$fails" -gt 0 ] && result FAIL "$fails check failure(s)"
result PASS "environment matches the pin"
