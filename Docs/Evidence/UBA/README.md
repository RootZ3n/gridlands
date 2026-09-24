# UBA network exposure: evidence (PR #2)

Unreal Build Accelerator (UBA) listened on `0.0.0.0:1345`, every interface
including Tailscale, during every build. `Tools/build.sh` now passes
`-UBAHost=127.0.0.1 -UBADisableRemote`, the supported UnrealBuildTool options
`UnrealBuildAcceleratorConfig.Host` and `bDisableRemote` in the UE 5.8.3 source.

Method: clean-clone builds (15 compile/link actions each) while `ss -ltnp
'sport = :1345'` was sampled every 0.2 s. Paths and pids are redacted.

| File | Build | Listener observed | Build |
|---|---|---|---|
| `baseline.txt` | master before the fix | `0.0.0.0:1345` (dotnet/UBT), 151 samples | PASS |
| `hostonly.txt` | `-UBAHost=127.0.0.1` only | `127.0.0.1:1345`, 151 samples | PASS |
| `loopback.txt` | both flags (this change) | **none** (0 samples) | PASS |
| `final-fresh-clone.txt` | `verify-fresh-clone.sh` on the PR head | **none** (0 samples); 7/7 tests | PASS |

The baseline row shows the sampler detects the listener, so zero samples
means no listener, not a broken probe. Local builds still use UBA's local
executor at the same speed (28.4 s vs 28.5 s).
