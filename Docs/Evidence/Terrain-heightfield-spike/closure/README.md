# Terrain-collision milestone closure (ADR-0035, on master)

**GREEN.** Verified on master after PR #25 merged (merge `8d817c5`), in the canonical default mode
(heightfield, no flags). The closure evidence lands through its own PR, and the milestone tag is set on
that merge.

## Automated gates
| Gate | Result |
|---|---|
| Full suite (`Saved/gate.sh`, i.e. `Tools/test.sh`) | **117/117, 42 requirements met** (`00-full-gate.out.txt`) |
| Tooling self-tests; data validation | PASS; 223 entities valid |
| Planted defects (`Tools/planted-defects/terrain_collision.py`) | **17/17 CAUGHT** (`planted-defects/`) |
| Fresh clone of `8d817c5` (build and full suite, tracked inputs and pinned engine) | **PASS, 117/117** |

**What the planted defects cover:**
- **Collision against the visible surface:** H1 visible-not-collision, H2 collision-not-visible, H3
  stale seam column, H9 the other render diagonal.
- **Persistence:** H4 collision without saved deformation.
- **Pooling:** H5 pooled chunk keeps its body, T1 pooled chunk keeps its mesh, T1b cleared-but-built,
  T2 old identity, T4 old place, T6 unbounded pool, T7b live chunk left in the pool, T8 half-reset
  after cancellation.
- **Navigation:** H6 navigation sees old terrain, H10 relevance never refreshed.
- **Stale work:** H7 older generation wins.
- **Holes:** H8 single-material holes.

## Performance and memory (quiet-run, strictly sequential; `perf/`)
- Both suites `QUIETDONE PASS`; every result is within `budgets.json`; no retries.
- **Dense reversal and dense sprint** ended at load 7.4 and 6.7 (a browser was active; the retry
  threshold is 8). Their results match the spike's heightfield runs (reversal: streaming worst 6.61 vs
  7.04 ms, p99 5.32 vs 5.34), so they are not contaminated.

| Run | Streaming worst (ms) | Frame p99 (ms) | Chunk apply mean / max (ms) | Cell complete (s) | Memory peak (MB) |
|---|---|---|---|---|---|
| local straight | 5.30 | 4.69 | 0.09 / 0.30 | 0.40 | 3604 |
| local sprint | 5.03 | 4.93 | 0.09 / 0.23 | 0.40 | 3603 |
| local reversal (512 reused chunks, 0.07 ms each) | 5.53 | 4.78 | 0.10 / 0.25 | 0.36 | 3748 |
| local teleport (cancels a load) | 5.58 | 4.71 | 0.09 / 0.27 | 0.40 | 3615 |
| local resume (save/restart) | 3.97 | (17.38)* | 0.09 / 0.21 | 0.80 | 2862 |
| dense straight | 6.16 | 5.32 | 0.09 / 0.27 | 0.52 | 3609 |
| dense sprint | 4.82 | 5.44 | 0.09 / 0.25 | 0.50 | 3601 |
| dense reversal | 6.61 | 5.32 | 0.09 / 0.24 | 0.46 | 3739 |
| dense teleport | 4.25 | 5.27 | 0.09 / 0.22 | 0.51 | 3614 |
| dense resume | 4.02 | (15.25)* | 0.09 / 0.20 | 0.90 | 2896 |
| roundtrips, local / dense (8 each way) | | | | | peak 3659 / 3657; growth 77 / 66 MB (budget 400) |

\* Resume measures until the cell completes: 0.8–0.9 s, about 26 frames. Its p99 is its single worst
frame, the first after load, and is not comparable to walking runs.

**Stale work, cancellation and persistence (every run):**
- emergency chunks 0; stale results 0;
- teleport cancels the lots' load (epoch 2) and reloads (epoch 3);
- the saved 2 m mound reads 453 cm after restart, local and dense.

**Per chunk (`perf/collisionpaths.json`, 64 chunks):**

| Path | Build | Game-thread attach or cook | Edit update | Collision memory, 256 chunks |
|---|---|---|---|---|
| **Heightfield (canonical)** | 0.042 ms on the worker (edits: 0.041 ms on the game thread) | **0.010 ms** | **0.047 ms** | **4.0 MB** |
| ADR-0034 component cook (measurement) | inside the cook | 4.92 ms | 5.04 ms | 141.1 MB |
| Worker trimesh (documented fallback) | 1.27 ms on the worker | 0.016 ms | 1.30 ms | 105.1 MB |

The render meshes of 256 chunks take 597.8 MB: **~80% of terrain memory. Recorded, not optimized.**

## Agreement (`Gridlands.Game.TerrainCollision`, in the gate)
- **Collision against the visible surface:** traces ≤ 0.01 cm, sweep contacts ≤ 0.30 cm, 0 missing, 0
  wrong overlaps.
- **Where:** untouched ground, small, steep, repeated (checked after each), neighbouring, seam and
  four-chunk-corner edits, and 30 rapid edits.
- **Restore:** a flushed and a streamed save/restore onto pooled chunks restore collision where it was.
- **Ground for structures and building:** the ground they stand on (`HeightAt`) equals the visible,
  colliding ground at every vertex (≤ 0.01 cm).
- **Inside steep non-planar quads,** HeightAt's bilinear interpolation differs from the triangle
  surface by up to ~72 cm. This is pre-existing and left to the operator (ADR-0035, "Newly exposed").

## Recorded debt (not addressed, as directed)
- **Extreme 1 m edits look harsh:** wedges, creases, hard earth/grass boundaries. This is terraforming
  presentation and brush debt. The regression evidence is
  [`../diagonal-review/side-by-side/05-*`](../diagonal-review/README.md).
- **The render mesh is ~2.33 MB per chunk,** ~80% of terrain memory. Optimize only when density,
  profiling or a budget calls for it.
- **The HeightAt interior gap** (above).
