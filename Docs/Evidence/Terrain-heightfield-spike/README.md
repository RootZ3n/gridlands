# Terrain heightfield-collision spike (decision spike; NOT canonical)

**Status: TERRAIN_COLLISION_DECISION_AWAITING_OPERATOR.**
- **Canonical collision is unchanged:** the ADR-0034 component trimesh is still the default, `-GLTerrainCollision=0`.
- **The spike adds two opt-in modes** for measurement: `-GLTerrainCollision=1` (Chaos heightfield) and `=2` (option A: trimesh
  built off the game thread).
- Decision record: [ADR-0035](../../ADR/0035-terrain-heightfield-collision-spike.md). Branch
  `terrain-heightfield-spike`; draft PR, not merged.

## 1. The engine path (what UE 5.8.3 actually allows)
Landscape does not apply: Gridlands' terrain is custom runtime chunks (ADR-0022), and Landscape's
runtime height edits are editor-only. The spike uses the Chaos layer directly, the way
`ULandscapeHeightfieldCollisionComponent` does internally:

**Creation**
- `new Chaos::FHeightField(TArray<FReal>&& heights, materials, rows, cols, scale)`.
- Pure data, so it is **safe to build on a worker**. The spike builds it inside the chunk's existing
  mesh job.

**Attach (game thread)**
- `FPhysicsInterface::CreateActor` (static), one shape (`FShapeInstanceProxy::Make`), filter data
  from the component's collision profile (`FPhysicsFilterBuilder`).
- Then `AddActorsToScene_AssumesLocked` and `AddToComponentMaps`.
- Component: `UGLTerrainCollisionComponent` (a `UPrimitiveComponent` with no render state).

**Navigation**
- `DoCustomNavigableGeometryExport` → `ExportChaosHeightField`.
- The component must refresh its cached `bNavigationRelevant` when geometry arrives. Navigation caches
  relevance at registration, when the chunk has no geometry yet (found: no navmesh at all until this
  was fixed).

**Partial / local updates**
- `FHeightField::EditHeights(heights, beginRow, beginCol, rows, cols)` exists.
- It edits **in place** (unsafe while the physics thread reads the geometry).
- It **resamples the whole field** when an edit leaves the original height range.
- Its column index is mirrored relative to storage.
- Landscape uses it only in the editor.
- The spike instead **rebuilds the chunk's 65×65 heightfield and recreates the body**; this costs
  0.05 ms in total. The partial edit costs 0.026 ms, so no meaningful saving justifies the unsafe path.

**Memory**
- Heights are quantized to uint16 over each field's own range. At the 4 m dig/raise limits the step is
  ≤ 0.02 cm.
- A low-resolution acceleration grid is added on top.
- Measured: **~16 KB per chunk** (below).

**Constraints discovered (all recorded as engine gotchas)**
1. **One material index per cell, never the single "default material" form.** With one entry,
   `GetMaterialIndex` fails its bounds check for every cell but the first and returns 255, which
   `IsHole` reads as a hole. The navigation export then skipped every cell, and overlap/sweep queries
   skip holes. Planted defect H8.
2. **A heightfield splits each cell between (x, y) and (x+1, y+1).** The canonical render mesh splits
   the other way.
   - With the wrong diagonal, collision leaves the visible surface by **up to 144 cm** on steep edits
     (measured). Planted defect H9.
   - Turning the heightfield a quarter to match (a `TImplicitObjectTransformed` wrapper) fixed the
     diagonal but **Chaos sweeps then missed contacts lying exactly on chunk seams**: a 10 cm sphere
     fell through at 4 probes; capsules did not.
   - **Chosen:** an unrotated heightfield, and **in heightfield mode the render mesh splits along the
     same diagonal**. This is a presentation-only change to which way each quad is triangulated.
     Measured: 0 seam misses, and visible = collision to 0.01 cm.
3. A perfectly flat field has range 0. Chaos then divides by zero when quantizing (NaN cast to uint16,
   which works in practice, as it must for flat Landscape components). Noted as a risk, not observed to
   fail.

## 2. Is Gridlands terrain heightfield-compatible? **Yes, by record and by code.**
- **The record:** [ADR-0022](../../ADR/0022-terrain-chunked-heightfield.md) was approved by the operator
  as "a chunked runtime heightfield".
  - "Player terraforming is a heightfield: dig, raise, flatten, pit, wall, ramp. **Tunnels, sewers and
    caves are authored geometry.** Player-dug caves are out of scope unless a later ADR adds a local
    voxel layer."
  - The operator decision: "Player-dug caves remain out of scope; revisiting that would be a new ADR."
  - BUILDING-SALVAGE-TERRAIN.md agrees. Dungeons and underground spaces (DESIGN-BIBLE, WORLD-AND-PROGRESSION,
    DUNGEONS-AND-LEGENDARIES) are authored interiors.
- **The code:**
  - `FGLHeightfield` stores exactly one height per vertex (`Heights[Y * VertsX + X]`).
  - The only edit operations are `Dig`, `Raise` and `Flatten` (`EGLTerrainOp`), each of which changes
    vertex heights.
  - The render mesh is a V×V vertex grid with one Z per vertex.
  - **No accepted mechanic produces caves, overhangs, tunnels or two surfaces at one X/Y.**
- **What this does constrain:** adopting heightfield collision would make "player-dug caves" or a
  voxel layer a *collision* change as well as a render change. A future ADR adding them would need a
  second collision representation for those regions. The trimesh path (option A) would not.

## 3. Gameplay semantics (every item tested in heightfield mode)
- **Full gate:** **117/117 tests pass in all three modes** (42 requirements; `00-full-gate-mode*.out.txt`).
  That covers:
  - movement and slopes, terrain traces, creature movement and pathing;
  - localized navigation (built, follows edits, crosses boundaries, unloads);
  - building placement and pieces blocking navigation, authored structures and collapse;
  - terraform up/down, digging and mounds, noise and hearing;
  - save/restart, cell unload/reload, cancellation (stale results), crossings and pooling.
- **What physics collision actually serves** (code survey):
  - character and creature movement;
  - build-mode aim and interaction traces;
  - creature line of sight;
  - navigation.
- **What it does not:** structure support, building ground, storm, scatter and noise all read the
  authoritative heights (`HeightAt`), never physics. So "structure support sees old terrain" cannot be
  caused by the collision representation. Existing data-layer defects (M4) guard that.

### The adversarial deformation proof (`Gridlands.Game.TerrainCollision`, 2 tests, required)
**What each probe compares, three ways:**
- **What the player sees:** the chunk's render mesh, read back from its component.
- **What physics answers:** a line trace, a 10 cm sphere sweep (compared to the visible surface at its
  contact point), a character capsule (34 × 88 cm) and a 34 cm sphere sweep, and overlaps straddling
  and clear of the surface.
- **The authoritative heights.**

**Where the probes are:** probes every 37 cm (off the vertex grid) plus probes on and ±0.01–1 cm
beside chunk seams.

| Step (heightfield mode) | Probes | worst \|trace − visible\| | worst \|sweep contact − visible\| | missing | overlap wrong |
|---|---|---|---|---|---|
| untouched | 2771 | 0.00 cm | 0.00 cm | 0 | 0 |
| small dig | 419 | 0.00 | 0.00 | 0 | 0 |
| steep dig (4 m in 1.2 m) | 419 | 0.00 | 0.16 | 0 | 0 |
| steep raise | 419 | 0.00 | 0.07 | 0 | 0 |
| 10 repeated digs (checked after each) | 654 | 0.00 | 0.00 | 0 | 0 |
| neighbouring vertices edited | 815 | 0.00 | 0.10 | 0 | 0 |
| edit on a seam, both sides | 654 | 0.00 | 0.01 | 0 | 0 |
| edit on a chunk corner (four chunks) | 994 | 0.01 | 0.09 | 0 | 0 |
| 30 rapid edits in one frame (centre checked after each) | 3435 | 0.01 | 0.14 | 0 | 0 |
| save/restore, flushed reload | 1339 | 0.00 | 0.30 | 0 | 0 |
| save/restore, streamed onto pooled chunks | 1339 | 0.00 | 0.30 | 0 | 0 |

- **Visible versus authoritative heights:** 0.00 cm in every step.
- **Canonical (mode 0) and worker trimesh (mode 2):** the same test passes with the same agreement.
- **Restored collision equals pre-unload collision** at 40 marked points (flushed and streamed).

## 4. Performance (same machine, same session, quiet-run, strictly sequential; `perf/`)

### Per chunk (`perf/collisionpaths.json`: 64 chunks of uneven ground, one process)

| Path | Geometry build | Game-thread cook/attach | Navigation notify | Edit update (game thread) |
|---|---|---|---|---|
| Canonical component cook | inside the cook | **4.93 ms** (max 6.49) | 0.011 ms | 5.01 ms |
| **Heightfield** | 0.042 ms (worker) | **0.010 ms** (max 0.059) | 0.002 ms | **0.049 ms** (build 0.043 + attach 0.006) |
| Option A: direct trimesh | 1.27 ms (worker) | 0.014 ms | 0.002 ms | 1.28 ms (built on the game thread for edits) |
| Heightfield partial `EditHeights` 9×9 (reference) | | 0.026 ms (in place, unsafe) | | |

### Memory

| | Canonical | Heightfield | Option A |
|---|---|---|---|
| Collision, 256 chunks (one cell) | 142.5 MB (0.56 MB/chunk) | **4.2 MB (16 KB/chunk)** | 105.7 MB (0.41 MB/chunk) |
| Render meshes, 256 chunks (all modes) | 597.3 MB (2.33 MB/chunk) | same | same |
| **Terrain per loaded cell (render + collision)** | ~740 MB | **~601 MB (−19%)** | ~703 MB (−5%) |
| Crossing memory peak (10 runs) | 3221–4406 MB | **2858–3719 MB (−330…−690)** | 3097–4325 MB |
| Roundtrip growth, local / dense (budget 400) | 131 / 123 MB | **70 / 78 MB** | 151 / 154 MB |

**The render mesh, not collision, is ~80% of terrain memory.** It is a newly exposed target (below).

### Real game (`perf/mode0|1|2/`, local and dense, every result within `budgets.json`, no retries)
In each cell, s = streaming game-thread worst (ms), p = frame p99 (ms), a = chunk apply mean / max
(ms), c = cell complete (s).

| Run | Canonical (s, p, a, c) | **Heightfield** | Option A |
|---|---|---|---|
| local straight | 7.00, 5.07, 5.35/10.27, 2.29 | **4.55, 4.72, 0.10/0.31, 0.41** | 4.68, 4.68, 0.09/0.50, 0.53 |
| local sprint | 6.75, 8.24, 5.37/9.91, 2.29 | **4.31, 4.93, 0.10/0.31, 0.40** | 4.85, 5.06, 0.08/0.27, 0.53 |
| local reversal | 8.65, 7.95, 5.37/8.38, 2.14 | **4.99, 4.78, 0.10/0.33, 0.35** | 4.97, 4.84, 0.08/0.19, 0.45 |
| local teleport (cancels a load) | 6.33, 5.67, 5.34/10.35, 2.28 | **4.75, 4.79, 0.10/0.76, 0.41** | 6.89, 4.81, 0.08/0.32, 0.53 |
| local resume (save/restart) | 8.80, (12.81)*, 5.18/8.34, 2.72 | **4.27**, (18.43)*, 0.09/0.22, **0.80** | 3.53, (18.78)*, 0.08/0.22, 0.90 |
| dense straight | 6.48, 5.53, 5.38/10.67, 2.39 | **5.03, 5.32, 0.09/0.51, 0.50** | 5.69, 5.30, 0.08/0.39, 0.62 |
| dense sprint | 6.81, 8.68, 5.35/10.43, 2.37 | **4.42, 5.51, 0.09/0.26, 0.51** | 7.01, 5.49, 0.08/0.23, 0.63 |
| dense reversal | 7.20, 8.47, 5.37/10.48, 2.22 | **7.04, 5.34, 0.09/0.38, 0.47** | 6.82, 5.35, 0.08/0.28, 0.56 |
| dense teleport | 6.67, 6.04, 5.41/13.53, 2.38 | **4.50, 5.24, 0.09/0.21, 0.50** | 5.77, 5.29, 0.08/0.36, 0.64 |
| dense resume | 6.80, (12.59)*, 5.17/6.78, 2.81 | **3.78**, (13.36)*, 0.09/0.25, **0.88** | 3.63, (19.69)*, 0.08/0.24, 1.02 |

\* **Resume "p99" is not comparable.** The run measures until the cell completes. With heightfields it
completes in 0.8–0.9 s (26 frames), not 2.7–2.8 s (248 frames). The p99 of 26 frames is simply its
single worst frame, the first frame after load.

**The control:**
- Mode 0 reproduces the PR #23 evidence within run-to-run noise (streaming worst ±2 ms between
  identical builds), so the spike's code does not disturb the canonical path.
- The ~90–110 ms frames in resume and roundtrips are the deliberate synchronous whole-cell loads
  (restart, jumps), outside the streaming budget. They exist in every mode (canonical ~105 ms,
  heightfield ~90 ms) and are dominated by field generation.

**Cancellation, stale work and save/restart in the real game** (every mode):
- teleport cancels the lots' load (epoch 2) and reloads (epoch 3);
- stale results 0, emergency chunks 0;
- the saved 2 m mound reads 453 cm after restart.

## 5. Planted defects (`mutations/`, heightfield mode, each built and run in isolation)

| Defect | Result |
|---|---|
| H1 an edit updates the visible mesh but not collision | **CAUGHT** (agreement, navigation, terrain tests) |
| H2 an edit updates collision but not the visible mesh | **CAUGHT** (only the visible-vs-heights check sees it) |
| H3 a chunk's seam column of collision copies its neighbour's (stale boundary) | **CAUGHT** (agreement, restore, pool streaming) |
| H4 collision built without the saved deformation (restores visuals, not collision) | **CAUGHT** |
| H5 a pooled chunk keeps its collision body (stale after reassignment) | **CAUGHT** (pool, navigation-octree tests) |
| H6 navigation is never told about the new terrain | **CAUGHT** (navigation follows edits, AI ridge) |
| H7 an older generation's collision wins (no generation check) | **CAUGHT** (stale-work test) |
| H8 the single "default material" form (holes everywhere) | **CAUGHT** |
| H9 the render diagonal differs from the heightfield's | **CAUGHT** (agreement: up to 144 cm) |
| H10 collision relevance never refreshed for navigation | **CAUGHT** |
| "Structure support sees old terrain" | **Not expressible in the collision layer:** support reads `HeightAt`, never physics (§3); M4 guards the data layer |

The existing planted-defect coverage (ADR-0034's T1–T9d, M4, M6b) is unchanged; those tests pass in
every mode.

## 6. Option A (direct trimesh, attach on the game thread): is it clearly preferable? **No.**
- It removes the game-thread spike just as well: apply 0.08 ms, cell complete 0.45–1.02 s.
- It costs **30× the worker CPU** (1.27 vs 0.042 ms per chunk) and **26× the edit cost** on the game
  thread (1.28 vs 0.049 ms; edits must collide at once).
- It keeps **25× the collision memory** (0.41 MB vs 16 KB per chunk). Its crossing memory peaks are
  near canonical, and its roundtrip growth is the highest measured (151–154 MB).
- **Its one real advantage:** a trimesh can represent anything, so a future cave or voxel ADR would
  not need a second collision representation.

## 7. Limitations and open points
- **Measured on one machine** (RX 6800, Linux).
- **The render-diagonal change** exists only in heightfield mode. If adopted, the flip of each quad's
  triangulation must pass an operator visual check. Normals are smooth, so it is expected to be
  invisible, but it has not been reviewed.
- **The canonical 64 m chunk size, collision resolution (1 m, the same as the visible mesh) and all
  budgets are unchanged.** No coarse collision was used.
- **Newly exposed, not in scope:** the render mesh (2.33 MB per chunk, ~0.6 GB per cell) is now the
  dominant terrain memory. The remaining synchronous whole-cell load (~90 ms, field generation) is
  unchanged.
