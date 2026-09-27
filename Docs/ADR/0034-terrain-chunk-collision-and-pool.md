# ADR-0034: Terrain chunk collision cook and pool hardening (after P7.1)

- Status: **Superseded for collision by [ADR-0035](0035-terrain-heightfield-collision-spike.md)** (heightfield
  collision, operator 2026-09-27). The pool hardening, bounds and integrity checks here remain canonical. The
  component cook remains only as a measurement mode (`-GLTerrainCollision=0`).
- Earlier status: **Accepted as an engineering checkpoint** (operator, 2026-09-26: merge the safe improvements; do
  not claim the streaming problem solved; next, a bounded heightfield-collision spike). Originally **Proposed**. The engineering cleanup between P7.1 and P8 (operator, 2026-09-26: "warm
  terrain chunk pool"). **The warm pool was measured and is not the remedy** (below); the operator's
  directive was to stop and report before any materially different architecture, and this ADR
  stops there. The decision it leaves open is under "Open decision".
- Date: 2026-09-26
- Builds on: [ADR-0027](0027-canonical-grid-scale.md) (1 km cells, 64 m chunks),
  [ADR-0028](0028-seamless-grid-streaming.md) (the terrain pump, the P5 pool),
  [ADR-0033](0033-multi-frame-cell-presentation.md) (the presentation ceiling).
- Evidence: [Docs/Evidence/Terrain-chunk-collision](../Evidence/Terrain-chunk-collision/README.md).

## Context
The largest remaining streaming cost was the P5 terrain's first build of a 64 m chunk: ~5 ms, 9–15 ms
under load. The dense P7 proof reached 11.87 ms against the 12 ms budget, and the final P7.1 quiet
run once breached it (dense sprint, 15.25 ms, a chunk's first `SetMesh` at 14.4 ms). A warmed,
reusable chunk pool was the expected remedy.

P5 already had a pool: `UGLTerrainSubsystem` retires an unloaded cell's chunk actors over frames
(`RetireOne`, 16 a frame) into `Pool`, and `AcquireChunk` takes from it before spawning.

## What was measured
**1. The cost is not the actor; it is the collision cook.** `gl.Perf.ChunkApply` times `SetMesh` of
a real-size 64 m chunk on a fresh actor, on a pooled one, on actors warmed with a tiny mesh and with a
full one. All cost the same (4.9–5.3 ms, P7.1 build). The component's defaults explain it:
- `UDynamicMeshComponent::SetMesh` cooks complex collision **synchronously** unless collision updates
  are deferred. That is ~5 ms for a 65×65-vertex trimesh; the mesh itself is ~0.05 ms.
- `ApplyMesh` then asked for **a second cook**, asynchronous (streaming passed `bAsyncCollision`).
  Its result replaced the first; the first was wasted.
- Navigation is not a factor (a cook without navigation relevance: 5.05 vs 5.14 ms).

**2. In the real game, a reused chunk costs what a fresh one costs.** The reversal crossing reuses
512 pooled chunks: reused apply mean 5.2 ms, first-build apply mean 5.36 ms. Pre-warming more actors
cannot remove a per-chunk cook of new ground.

**3. Asynchronous cooking is worse on the game thread here, not better.** With only the async cook,
the apply fell to ~0.2 ms, but each completion (`FinishPhysicsAsyncCook`) cost ~7.5 ms of game thread,
and completions bunched: worst frames 60–62 ms, 23–25 ms with a completion cap. Automation worlds
never complete an async cook at all (no frame ticks), so tests would also lose their collision.

## Decision (made, within the existing architecture)
1. **One collision cook per chunk mesh, synchronous.** The chunk component defers collision updates
   (`SetDeferredCollisionUpdatesEnabled(true)`); `ApplyMesh` cooks once, synchronously, for streaming,
   edits and restores alike. The asynchronous-cook parameter was removed, so no path can request one. There is no window in which new ground lacks collision.
2. **The pool is hardened, bounded, and checked**, because a reused chunk must never show another
   cell's ground (operator):
   - each chunk carries a state (`Live`, `Retiring`, `Pooled`) and an owner cell;
   - `AcquireChunk` refuses a pooled chunk that is not `Pooled` (counted, logged as an error);
   - `ClearForPool` leaves nothing behind: empty mesh, no collision body, hidden, no collision, out
     of the navigation octree, no owner;
   - the pool is bounded by `PoolLimit` = 512 (256 chunks per 1 km cell, at most two cells retiring
     at once); overflow is destroyed and counted;
   - `CheckChunkIntegrity` verifies every invariant (no chunk owned twice; a slot's chunk is live,
     owned by that cell, at that slot; pooled chunks fully cleared; the bound).
3. **Stale work stays rejected by the cell's load generation and the slot version**, as in P5. The
   new tests prove each guard on its own (the generation guard was previously covered only by the
   version guard's side effect).
4. **Streaming records first-build and reused apply cost separately**, and the slow-frame log splits
   the worst apply into mesh, collision and navigation. The crossing JSON carries `chunksSpawned`,
   `terrainFirstBuild*`, `terrainReused*`, `terrainPoolRejected`, `terrainPoolOverflowDestroyed`.

## Consequences (measured, quiet machine; see the evidence)
- **Memory:** peak down 80–297 MB in all 12 runs, local and dense (the second, redundant collision
  body is gone). The pool's full bound costs ~35 MB; a live chunk costs ~2.9 MB.
- **Frame p99:** lower in 7 of 10 crossings, by up to 0.46 ms.
- **Streaming worst: unchanged within run-to-run noise** (6.4–9.0 ms against 6.4–10.4 ms), and not by
  chance: every slow apply is the one cook,
  4.6–10.2 ms of collision for 0.05 ms of mesh. **The headroom the operator asked for is not created
  by this change.** All budgets hold; none was changed.

## Open decision (operator): where the collision cook goes
The per-chunk trimesh cook, on the game thread, is now the one terrain cost that sets the streaming
worst. Every remedy changes an architecture:

| Option | What it changes | Expected effect | Risk |
|---|---|---|---|
| **A. Cook on the worker, attach on the game thread** | Build the Chaos triangle-mesh geometry inside the mesh job; the game thread only installs it | Cook leaves the game thread; attach cost unmeasured (must be measured first) | Engine internals (`UBodySetup` cooked data); must survive edits, restores, and automation worlds |
| **B. Heightfield collision** (`Chaos::FHeightField`) instead of a trimesh | Terrain collision representation | Much cheaper cook and memory, and a natural fit for a 2.5D heightfield | Every collision consumer (traces, navigation export, structure support, deformation updates) must accept it; overhangs remain impossible (they already are) |
| **C. Coarser collision** (e.g. 33×33 per chunk) | Collision resolution vs visual resolution | ~4× cheaper cook | Collision leaves the visible ground by up to a vertex step; affects digging precision and support |
| **D. Smaller chunks** (32 m) | The P5 canonical chunk size (ADR-0027) | ~4× cheaper per chunk; the 3 ms budget then bounds the frame | 4× the actors and draw calls; P5 canonical scale |

Destruction, persistence and deformation all touch collision, so this is exactly the kind of choice
the standing rules reserve for the operator. **Nothing here has been started.**
