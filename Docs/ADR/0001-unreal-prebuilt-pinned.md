# ADR-0001: Unreal Engine 5, Epic's prebuilt Linux binary, exact version pinned

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Context
The host is Fedora 44, 31 GB RAM, an RX 6800-class AMD GPU (Vulkan/RADV). Building
Unreal from source needs Epic GitHub access, far more RAM, and hours per build.

## Decision
- Use Epic's **prebuilt Linux binary** (`Linux_Unreal_Engine_<ver>.zip`).
- Use the current stable 5.x at bootstrap (**5.8.3**, hotfix of 2026-09-22) and
  pin the exact version in `Tools/engine-pin.env`. The pin includes the build
  changelist once the engine is installed, read from `Engine/Build/Build.version`.
- `Tools/doctor.sh` refuses a mismatched engine.
- **No source build of the engine** without explicit operator authorization.
- The engine is located via `GRIDLANDS_UE_ROOT`, then an untracked
  `.engine-root` file, then the default `/pehverse/engines/UE_<ver>`.

## Consequences
- Fedora is not an Epic-recommended distro (Ubuntu 22.04 / Rocky 8 are). Distro
  problems are reported, not worked around silently.
- Epic lists 32 GB as minimum RAM; the host has 31 GB. Doctor warns, and we watch
  for OOM while compiling.
- The installed build compiles project C++ with its bundled clang toolchain;
  no system compiler is required.

## Reversal cost
Low. Moving to a source build or a newer version is a pin change plus a rebuild.
