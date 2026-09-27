# Terrain chunk collision and pool hardening (after P7.1, before P8)

**Status: engineering GREEN; the headroom goal is NOT met by pooling, and the decision that would
meet it is the operator's** ([ADR-0034](../../ADR/0034-terrain-chunk-collision-and-pool.md), "Open
decision").
- All P5/P6/P7/P7.1 gates hold, and no budget was changed.
- **Tests:** 115/115, 41 requirements met.
- **Planted defects:** 13 caught; 2 equivalent mutants are explained below.

## The finding, in one line
**A 64 m chunk's activation cost is the Chaos trimesh collision cook (~5 ms), not the actor.** A
reused, pooled chunk pays exactly what a fresh one pays (real game, 512 reuses: 5.20–5.26 ms against
first builds of 5.36–5.39 ms). A warm pool of actors cannot create headroom, so the directive's stop
applies: the remedies that would (worker-thread cook, heightfield collision, coarser collision,
smaller chunks) are architectural and have **not** been started.

## What changed (within the existing architecture)
1. **One cook instead of two.**
   - Before: `SetMesh` cooked collision synchronously, and streaming then asked for a second,
     asynchronous cook.
   - Now: the chunk component defers collision, and `ApplyMesh` cooks once, synchronously.
2. **The P5 pool is hardened and bounded.**
   - Chunk state (`Live` / `Retiring` / `Pooled`) and owner cell.
   - Refusal of a pooled chunk that is not `Pooled`.
   - A full clear: empty mesh, no body, hidden, no collision, out of navigation.
   - `PoolLimit` = 512, overflow destroyed.
   - `CheckChunkIntegrity`.
3. **Instrumentation.**
   - First-build vs reused apply cost in every crossing JSON.
   - Pool rejections and overflow counts.
   - The slow-frame log splits the worst apply into mesh + collision + navigation.
   - `gl.Perf.ChunkApply` gives the before/after cost of the chunk paths and chunk memory.

## Why pooling is not the remedy (measured)
`perf/chunkapply.json` (64 chunks each, real-size 65×65-vertex mesh, quiet machine):

| Path | Mean / max (ms) |
|---|---|
| `SetMesh` with the P5–P7.1 component (cooks inside): **before** | 4.92 / 5.28 |
| `SetMesh`, deferred collision: fresh, pooled reuse, tiny-warmed, full-warmed | 0.03–0.05 |
| The cook alone, synchronous | 5.17 / 5.64 |
| The cook alone, not navigation-relevant | 4.91 / 5.37 |
| Whole `ApplyMesh` now (mesh + one cook + navigation octree), fresh actor | 6.64 / 7.60 |
| Whole `ApplyMesh` now, pooled and reused | 6.35 / 7.77 |

In streaming, every slow apply frame is the cook. Examples from `perf/*.log.txt`:
- "worst 8.71 = mesh 0.05 + collision 8.63 + nav 0.02";
- "worst 10.31 = mesh 0.08 + collision 10.16 + nav 0.07".

**Asynchronous cooking was tried first and rejected.**
- The apply fell to ~0.2 ms, but each async completion cost ~7.5 ms of game thread.
- Completions bunched: worst frames 60–62 ms, and 23–25 ms with a completion cap.
- Automation worlds never complete an async cook.

## Before / after (quiet-run, strictly sequential)
- **Before** is the P7.1 closure's final quiet run (`../P7.1-visual-refinement/perf-final/`).
- **After** is `perf/`, run with the same harness under the same gating (1-minute load < 3, GPU idle):
  - both suites `QUIETDONE PASS`, all 13 results within `budgets.json`;
  - no run needed a retry (end load < 8).
- First-build apply includes the synchronous start-of-run cell load, so its max is not a streaming
  frame.

**Local**

| Run | Streaming worst (ms) before → after | Frame p99 (ms) | Worst frame (ms) | Memory peak (MB) | First-build apply mean / max (ms) | Reused applies, mean (ms) |
|---|---|---|---|---|---|---|
| straight | 6.72 → **6.56** | 4.94 → 5.00 | 23.4 → 23.9 | 4482 → 4242 | 5.38 / 11.19 | 0 |
| sprint | 6.41 → **6.56** | 8.66 → 8.29 | 18.0 → 17.6 | 4507 → 4241 | 5.36 / 8.14 | 0 |
| reversal | 7.01 → **6.90** | 8.39 → 7.93 | 35.1 → 31.7 | 4572 → 4378 | 5.36 / 8.80 | 512, 5.20 |
| teleport | 6.76 → **8.97** | 5.73 → 5.71 | 24.1 → 24.4 | 4355 → 4275 | 5.40 / 10.31 | 0 |
| resume | 7.22 → **7.05** | 13.43 → 13.06 | 15.3 → 13.8 | 3332 → 3223 | 5.17 / 7.75 | 0 |
| roundtrips (8 each way) | | | | 4351 → 4092; growth 67 → 119 (budget 400) | | |

**Dense town (the 12× ADR-0033 fixture, `-d`)**

| Run | Streaming worst (ms) before → after | Frame p99 (ms) | Worst frame (ms) | Memory peak (MB) | First-build apply mean / max (ms) | Reused applies, mean (ms) |
|---|---|---|---|---|---|---|
| straight | 6.88 → **7.80** | 5.52 → 5.58 | 23.2 → 24.0 | 4482 → 4262 | 5.39 / 10.25 | 0 |
| sprint | 6.64 → **6.44** | 8.95 → 8.68 | 23.4 → 17.8 | 4528 → 4270 | 5.37 / 9.85 | 0 |
| reversal | 7.47 → **7.29** | 8.77 → 8.46 | 38.4 → 37.7 | 4548 → 4364 | 5.39 / 12.19 | 512, 5.26 |
| teleport | 10.43 → **7.64** | 6.41 → 6.15 | 25.4 → 24.5 | 4371 → 4270 | 5.39 / 10.75 | 0 |
| resume | 6.82 → **7.13** | 12.59 → 12.80 | 13.3 → 13.4 | 3333 → 3232 | 5.18 / 6.71 | 0 |
| roundtrips (8 each way) | | | | 4369 → 4072; growth 120 → 113 (budget 400) | | |

The P7.1 dense sprint once measured 15.25 ms (a chunk's `SetMesh` at 14.4 ms under load); its two
reruns, recorded above as "before", gave 6.84 and 6.64 ms.

**Reading it honestly:**
- **Memory peak is lower in all 12 runs, by 80–297 MB:** the redundant second collision body is gone.
- **Frame p99 is lower in 7 of 10 crossings**, by up to 0.46 ms.
- **Streaming worst is unchanged within run-to-run noise** (±1–2 ms here): 6.4–9.0 ms against
  6.4–10.4 ms. The single cook still lands whole on one frame. **This change does not create the
  requested headroom.**
- Local roundtrip growth moved from 67 to 119 MB. Dense moved from 120 to 113 MB, and both are far
  inside the 400 MB budget. The samples level off: 4050 → 4092 MB over the last 12 moves.

## Pool bound and memory
- **The bound is derived from the geometry and the measurements.** A cell retires 256 chunks, and at
  most two cells retire at once, so `PoolLimit` = 512. In every run the pool peaked at 256
  (`chunksPooledAtEnd`), with 0 overflow destroyed and 0 rejected.
- **A pooled chunk costs what a never-built actor costs:** 256 empty chunks take 17.5 MB, so the
  full bound of 512 is **~35 MB**.
  - A live, built chunk takes **~2.9 MB** (256 take 741 MB).
  - **Terrain chunks are therefore ~0.75 GB per loaded 1 km cell,** mostly mesh, render and
    collision data. This bears on the open decision (heightfield collision would shrink it).
- Resident memory does not fall when chunks are cleared: the allocator keeps the pages for the next
  cell, and roundtrip growth stays flat.

## Cancellation, reversal, stale work, save/restart
- **Teleport** cancels the lots' load mid-flight (epoch 2, `cancelledMidLoad`) and reloads it (epoch
  3). Stale results 0 and emergency chunks 0, local and dense.
- **Reversal** turns back while the lots' chunks are building, goes home, and crosses again. It
  reuses 512 pooled chunks, with stale results 0 and emergency chunks 0.
- **Save/restart:** the teleport run leaves a 2 m mound in the lots, and after quit and resume the
  ground there reads 453 cm, as before quitting (local and dense).

## Tests (`Gridlands.Game.TerrainPool`, 6, required)

| Test | Proves |
|---|---|
| `ReusedChunksAreRebornCleanInAnotherCell` | The origin (with a dug pit) is unloaded into the pool, and the lots are built entirely from it: 0 spawned. Every chunk sits at its new slot, and its mesh bounds match the new field. Collision at the pit's relative place is the lots' ground, and nothing is left at the origin. |
| `StaleMeshWorkNeverLandsOnAReassignedChunk` | Finished, unapplied mesh jobs of one load meet a reload with a raised patch, in the same Pump and at equal slot versions, so **only the load generation** separates them. 64 are dropped, and every probe collides at the new ground. |
| `PoolStaysInsideItsBound` | With `PoolLimit` 100 over three cell cycles, the pool never exceeds 100; overflow is destroyed (≥ 156) and chunk actors stay ≤ 100. |
| `StreamedReusedChunksCollideAsTheirNewCell` | Frame-streamed (not flushed) onto reused chunks: 48 probes, none stale or missing. |
| `CancellationNeverHandsOutAHalfResetChunk` | Retirement is interrupted after two pumps and the lots load at once. No half-reset chunk is handed out (0 rejected), integrity holds, and every chunk is rebound. |
| `PooledChunksLeaveNavigation` | In a navigation world, after streaming deep into the lots: 256 pooled chunks, **0 in the navigation octree**; 256 live chunks, 0 missing from it. |

`GLDenseSpawnTests`' cancel test now flushes before counting objects: automation worlds never finish
asynchronous work, and the count is of settled state.

## Planted defects (`mutations/`, each built and run in isolation)

| Defect | Operator's case | Result |
|---|---|---|
| T1 `ClearForPool` keeps the mesh | reused chunk retains previous heights | **CAUGHT** (3 tests; integrity: pooled chunk not empty) |
| T1b cleared chunk still marked built | incompletely reset chunk | **CAUGHT** (3) |
| T2 reused chunk keeps its previous owner cell | previous cell identity | **CAUGHT** (4) |
| T3 no load-generation check | stale async mesh work after reassignment | **CAUGHT** (the generation-isolating stale test) |
| T4 reused chunk not moved to its new slot | previous edits/ground at the old place | **CAUGHT** (4) |
| T5 reuse skips the collision cook | collision survives reassignment | **CAUGHT** (3) |
| T6 no pool bound | pool grows past its bound | **CAUGHT** |
| T7 `Pool.Last()` without popping | pooled chunk reused while live | **CAUGHT**: the state guard refused it ("a pooled chunk was not in the Pooled state"), then the mutant looped until the test runner's timeout (no report; `T7-*.no-report.txt`) |
| T7b a live chunk left in the pool | pooled chunk reused while live | **CAUGHT** (5) |
| T8 retirement pools without `ClearForPool` | cancellation returns a half-reset chunk | **CAUGHT** (5) |
| T9b reused chunk never re-registers with navigation | navigation survives reassignment | **CAUGHT** (Navigation 2) |
| T9d retired/pooled chunks stay collidable and in navigation (both places) | collision/navigation survive | **CAUGHT** (4, including `PooledChunksLeaveNavigation`) |
| M6b unload leaves vegetation behind (updated for ADR-0033's retiring scatter) | vegetation survives | **CAUGHT** (`StreamingNeverLosesDuplicatesOrReplays`) |
| M4 restore skips the saved ground (rerun) | edits / persistence | **CAUGHT** (`TerraformConservesProtectsAndPersists`) |
| T9 `ClearForPool` skips the navigation update | | **Equivalent**: disabling the actor's collision already unregisters it (engine). The property is tested by `PooledChunksLeaveNavigation`. |
| T9c `ClearForPool` skips collision-off and navigation | | **Equivalent**: `RemoveCell` already turned collision off when retiring. T9d removes both and is caught. |

**Vegetation is not chunk state.** Scatter patches are per-cell placement actors that read the
terrain subsystem, not chunks. That is why M6b guards the vegetation case, not a pool test.

## Remaining debt (all kept)
- **The terrain collision cook (~5 ms per 64 m chunk, 9–12 ms under load)** remains the largest
  single streaming cost. The remedy is the open decision in ADR-0034.
- **LODs are required before substantial production content density** (operator, P7.1). None were
  built here.
- **Terrain memory:** ~2.9 MB per live chunk, ~0.75 GB per loaded cell.
- **A GPU-bound frame at x = 352 m on the straight route** (22–27 ms since P5), uninvestigated.
- **Actor-state placements** (creatures, glitches, salvage nodes) are still made in the
  authoritative frame; they need a model/actor split before density (ADR-0033).
- **The mid-fall save is still GAMEPLAY CONSISTENCY DEBT** (P6).
- **The ~34 ms authored-level unload hitch** (engine-side; accepted, ADR-0028).
- **Characters remain unapproved proxies;** there is no artist-facing authoring; vertex colour is the
  only colour source; outline categories are unevaluated for future systems (P7/P7.1 debt,
  unchanged).

## Fresh clone
*(filled in after verification)*
