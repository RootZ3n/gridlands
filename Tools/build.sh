#!/usr/bin/env bash
# Compiles the GridlandsEditor target (Linux, Development) with the pinned engine.
#   Tools/build.sh [extra UBT args]
set -uo pipefail
. "$(dirname "$0")/lib/common.sh"

require_pinned_engine
[ -z "$(editor_running_for_project)" ] || result FAIL "an editor has this project open; close it first (Live Coding corrupts Blueprints)"

echo "Building GridlandsEditor with Unreal $GRIDLANDS_UE_VERSION at $ENGINE_ROOT"
# Unreal Build Accelerator listens on 0.0.0.0:1345 by default, i.e. every interface
# (LAN, Tailscale). Gridlands builds locally only, so bind it to loopback and refuse
# remote helpers. Supported UBT options: UnrealBuildAcceleratorConfig.Host / bDisableRemote.
UBA_ARGS=(-UBAHost=127.0.0.1 -UBADisableRemote)
# Adaptive unity compiles recently edited files on their own, which hid unity-build name
# collisions that only a clean clone exposed. Every build now groups files like a clean build.
UNITY_ARGS=(-DisableAdaptiveUnity)
"$UE_BUILD_SH" GridlandsEditor Linux Development -Project="$GRIDLANDS_UPROJECT" -WaitMutex "${UBA_ARGS[@]}" "${UNITY_ARGS[@]}" "$@"
status=$?
[ "$status" -eq 0 ] || result FAIL "UnrealBuildTool exited $status"

for module in GridlandsCore GridlandsGame GridlandsEditor; do
	[ -f "$GRIDLANDS_ROOT/Binaries/Linux/libUnrealEditor-$module.so" ] || result FAIL "build reported success but libUnrealEditor-$module.so is missing"
done
result PASS "GridlandsEditor built"
