# P5 evidence: canonical 1 km Grid, seamless streaming, localized navigation

- Engine: Unreal 5.8.3, Linux, Vulkan.
- Hardware: RX 6800, i7-11700K, 32 GB.
- Real-game runs: 1920×1080 offscreen, vsync off.
- Decisions: [ADR-0027](../../ADR/0027-canonical-grid-scale.md) (scale),
  [ADR-0028](../../ADR/0028-seamless-grid-streaming.md) (streaming),
  [ADR-0029](../../ADR/0029-localized-navigation.md) (navigation).
- Configuration: two 1024 m cells (`cell.home.origin`, `cell.outer.diner_lots`); 1 m terrain
  spacing; 64 m chunks; 256 chunks per cell.
- Reproduce:
  - `Tools/perf-crossing.sh -t` for localized navigation;
  - `Tools/perf-crossing.sh -w -t straight reversal sprint` for whole-cell navigation;
  - `Tools/test.sh`.

## Files

| File | What it is |
|---|---|
| `perf/local-*.json`, `perf/local-*.log.txt` | Real-game runs with localized navigation (the shipped configuration) and their Grid logs |
| `perf/whole-*.json` | The same runs with whole-cell navigation (invokers off), for comparison |
| `perf/unload-hitch-bisect.*` | The investigation of the unload-frame hitch (see D) |
| `10-*`, `11-*` | Planted streaming race defects, both caught |
| `20-*` … `23-*` | Planted navigation defects, all caught |
| `24-diagnostic-invoker-strip-leftover.index.json` | Diagnostic run behind a known limit (ADR-0029, Limits) |
| `30-full-gate.index.json` | Full automation gate: 91/91, 36 requirements |
| `40-real-game-torture-walk.log-excerpt.txt` | Real game: build at the boundary, repair in the lots, 7 crossings, save in the lots |
| `41-session2-restore-in-second-cell.log-excerpt.txt` | Real game: relaunch from that save and walk home |

## C. Crossing 1 km cells (real game, localized navigation)
Zenny walks along y = −300 m from the origin centre to 900 m east (400 m into the lots) at a set
speed. Every frame is recorded, and nothing is teleported except in the teleport run.

| Run | Speed | Worst frame | Mean | p99 | Streaming game-thread worst / mean | Emergency chunks |
|---|---|---|---|---|---|---|
| straight | 6 m/s | 22.9 ms | 3.55 ms | 4.6 ms | 9.5 / 0.05 ms | 0 |
| sprint | 18 m/s | 18.0 ms | 3.61 ms | 8.5 ms | 8.5 / 0.12 ms | 0 |
| reversal | 12 m/s | 34.1 ms | 3.57 ms | 8.2 ms | 9.2 / 0.12 ms | 0 |
| teleport | 12 m/s | 26.5 ms (the jump frame) | 3.52 ms | 5.2 ms | 8.5 / 0.06 ms | 0 |
| resume (start from a save in the lots) | — | 12.6 ms | 9.0 ms (startup) | 12.0 ms | 6.5 / 5.6 ms | 0 |

**Readiness** (per cell load; seconds after the load began):

| Load | Ground usable | Runtime layer in | Every chunk built |
|---|---|---|---|
| origin (start) | 0.03 | 0.48 | 2.75 |
| lots (each walking crossing) | 0.09–0.10 | 0.10–0.11 | 2.23–2.39 |
| lots after restart (resume) | 0.11 | 0.54 | 2.80 |

- **The lots are complete before Zenny arrives, in every run.** Loading starts 256 m before the
  boundary, which gives 43 s of walking or 14 s of sprinting against 2.4 s to build.
- **0 emergency chunks.** The synchronous "ground under Zenny now" path never had to run.
- **The cells repeat identically.** In the reversal run the lots loaded three times (epochs 2, 3
  and 4), each in 2.2–2.4 s, reusing 512 pooled chunk actors.

**Rapid reversal and cancellation mid-load.**
- **Walking cannot cancel a load.** Loading completes in ~2.4 s, and the unload margin is 128 m
  further out than the load margin.
- **The teleport run forces a cancellation.** It steps 10 cm into the load margin and jumps home
  at once: lots epoch 2 is **cancelled mid-load**. Walking back, epoch 3 loads cleanly and
  arrives complete.
- **The automated test does the same deterministically:** `RapidReversalMidLoadKeepsEverything`.

**Save/restart while standing in the second cell.**
1. **The teleport run ends in the lots.** It raises a 2 m mound 5 m ahead (ground 253 → 453 cm)
   and quits, which autosaves.
2. **`resume` relaunches from that save:**
   - Zenny starts in the lots;
   - the ground is usable at 0.11 s, the runtime at 0.54 s, and everything at 2.80 s;
   - the ground at the mark reads **453 cm**.
3. **Real-game session two** (`41-*`):
   - the relaunch keeps "2 cell records (0 loaded now)";
   - in the lots, the jukebox is Repaired;
   - walking home, the origin streams in with its 2 pieces, the 78 cm mound, 53 level actors and 1 creature.

## D. Worst hitch, and where it comes from
Each frame slower than 16.7 ms is recorded with its streaming, thread, GC and level state. The
CSV profiler and a bisect then attributed each one.

**The worst frames:**

| Hitch | When | Whole-cell nav | Localized nav | Cause |
|---|---|---|---|---|
| Load start | Zenny 256 m from the boundary: level request, field worker, nav bounds registration | 27–29 ms | **17–18 ms** | Whole-cell: navigation registering 1 km bounds took 19 ms of game thread (CSV `NavigationBuild`). Localized: ~5.5 ms of streaming work remains. |
| Unload | The frame after a cell unloads | 43–47 ms | **28–34 ms** | Bisected (`perf/unload-hitch-bisect.*`): it disappears only when the **authored level is not removed**. Skipping terrain removal lowers it to ~23 ms; placements and boundary posts do not matter. Engine time-slicing CVars (`s.UnregisterComponentsTimeLimit`, `s.LevelStreamingComponentsUnregistrationGranularity`) do not change it. Keeping the level loaded but hidden is worse. It is engine level removal (`RemoveFromWorld`), not Gridlands work. |
| Isolated | e.g. 352 m, 705 m, 719 m | ~23 ms | ~22–24 ms | No streaming work, game thread ~2 ms, no GC, no level streaming pending. Not attributable to streaming. |

**The worst walking hitch is 34 ms**, once per cell unload (roughly once per kilometre walked).
Straight and sprint crossings stay under 23 ms, and every p99 is ≤ 8.6 ms.

## E. Peak memory with two cells overlapping

| | Start (1 cell) | Peak (2 cells) |
|---|---|---|
| Localized navigation | 3,280 MB | **4,445 MB** (straight), 4,553 MB (reversal) |
| Whole-cell navigation | 3,420 MB | 4,691 MB (straight), 4,710 MB (reversal) |

- **Bounded across repeated crossings.** Pooling keeps chunk geometry from waiting for GC. The
  automated torture test also bounds growth over its rounds (< 1.5 GB).

## F. Localized navigation vs the whole-cell baseline
Same build and harness; the whole-cell runs use an `-ini` override that switches invokers off.

| | Whole-cell | Localized |
|---|---|---|
| Initial navigation, 1 km cell (`perf/*-terrain-1024m.json`) | 7.64 s | **0.53 s** |
| Active navmesh tiles | 20,581 | **324** |
| Detour tile memory | 11.2 MB | **0.18 MB** |
| Process memory growth over the navigation phase | +1,662 MB | **+509 MB** |
| Navigation update after an edit near Zenny (mean / max) | 42 / 47 ms | 40 / 43 ms |
| Tile tasks pending while crossing (peak) | 21,235–21,548 | **8–51** |
| Tile tasks still pending at the end of a crossing | 5,956–8,486 (never caught up) | **0** |
| Worst crossing frame (straight / reversal / sprint) | 30.3 / 47.1 / 28.2 ms | **22.9 / 34.1 / 18.0 ms** |
| p99 (straight / reversal / sprint) | 6.1 / 11.6 / 12.3 ms | **4.6 / 8.2 / 8.5 ms** |

**Path correctness is kept in the real game:**
- `gl.Demo.NavProof` passes: the AI walker detours the raised ridge through the gap and arrives.
- `gl.Demo.DrainProof` passes: the gremlin chases on the drain navmesh, carried by its own
  invoker, then follows Pehlichi's distraction.

**The edit cost in this harness (~20 ms per dig, in both modes) is not navigation.** In the
Grid map, `gl.Perf.Terrain` builds its ground on top of the origin cell's own ground, so every
edit applies twice. It is a harness artifact and is identical in both columns.

## G. Tests and planted defects
Automation: **91/91** (`30-full-gate.index.json`), 36 requirements met, 0 warnings.
- `Gridlands.Game.Grid`, 5 tests: torture, save in the second cell, depth/era, rapid reversal
  mid-load, stale async work.
- `Gridlands.Game.Navigation`, 3 tests: built only around invokers, creatures far from the
  player, crossing boundaries and unloading cleanly.
- The M10 terrain navigation tests, now with an invoker.

| Planted defect | Caught by |
|---|---|
| `10` stow a half-loaded cell as if it were live (a realistic race) | `RapidReversalMidLoadKeepsEverything`: the jukebox's kept repair is overwritten |
| `11` apply a mesh built before an edit (no version check) | `StaleAsyncWorkNeverLandsAndZennyNeverFalls`: trace −103 vs field 47 |
| `20` Zenny's invoker never activates | `BuiltOnlyAround…` and `CrossesBoundaries…`: no navigation near Zenny, no path across the boundary |
| `21` creatures carry no invoker | `CreaturesPathFarFromThePlayer`: no navigation or path at the creature. `BuiltOnlyAround…` also fails, through the known limit in ADR-0029 (Limits). |
| `22` an unloading cell leaves its nav bounds | `CrossesBoundaries…`: 2 then 3 bounds actors; the origin stays navigable |
| `23` whole-cell navigation (config) | all 3: 10,816 tiles; navigation 300 m away and between creature and player |

**A real test-harness defect was found and fixed while checking planted defect 21.** `Settle`
treated navigation as idle while tile tasks were still running. It now waits for
`GetNumRemainingBuildTasks() == 0` too.

## H. Fresh clone
PASS: a fresh clone of `fe32e05` built and passed **91/91** tests (36 requirements, 0 warnings) and the data validation (147 entities) from tracked inputs plus the pinned engine (`00-*`).
