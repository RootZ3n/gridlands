# ADR-0028: Seamless Grid streaming: asynchronous, incremental, versioned

- Status: **Accepted for P5.** Built and measured at the canonical scale (ADR-0027: 1 km cells,
  1 m terrain, 64 m chunks).
- Date: 2026-09-25
- Evidence: [Docs/Evidence/P5-seamless-grid](../Evidence/P5-seamless-grid/README.md)
- Amends: [ADR-0026](0026-grid-cells-and-streaming.md), whose synchronous load/unload and
  flushes are replaced here. Its cell identity, save format v2, seams and state rules stand.

## Context
- At 1 km a cell is 256 chunks and 1025² vertices. P2 measured about 11 ms per chunk (mesh plus
  collision) and about 7 s for a whole cell built at once.
- ADR-0026 loaded a cell synchronously in one frame. That was fine at 256 m and is impossible at 1 km.
- The operator requires:
  - no hitch that hides a failure, and no loading screen;
  - the player never falls through missing ground;
  - crossing, reversal mid-load and save/restart stay deterministic, with nothing duplicated or
    lost.

## Decision

### A cell load is a small state machine with an epoch
`UGLGridSubsystem::Advance(Where, Budget)` runs once per frame from the game mode:
1. **Membership.** A cell loads within 256 m of its edge and unloads beyond 384 m (hysteresis).
   Each load gets a new **epoch**.
2. **Ground under Zenny now.** `EnsureReadyAt` builds, synchronously, the chunks within 4 m of
   Zenny that are not yet built. The field under Zenny is finished at once if still on a worker.
   This is the "never fall through" guarantee; it is counted (`EmergencyChunks`) and is expected
   to be 0 in normal play.
3. **Pump.** Chunk meshes are built on workers, nearest to Zenny first:
   - up to 8 are in flight at a time;
   - finished meshes are applied within a 3 ms game-thread budget per frame;
   - collision is cooked asynchronously.
4. **Runtime layer.** When the cell's authored level is visible *and* its ground field exists, the
   cell's kept record is taken, merged with the ground edits and pieces made while it was
   half-loaded, and its placements are spawned.
5. **Completion** is logged with timings: ground ready, runtime ready, all chunks built.

### Every piece of asynchronous work is versioned
- **Fields.** A cell's field is built on a worker (`UE::Tasks`) from its data and its saved terrain
  delta. It carries the ground's **generation**, and a field that finishes for a ground that was
  removed or rebuilt meanwhile is dropped.
- **Chunks.** Each chunk slot has `Version`, `BuiltVersion` and `InFlightVersion`:
  - an edit bumps the versions of the chunks it touches;
  - a mesh built from an older snapshot is **dropped** (`StaleDropped`), never applied.
  - This is what stops an old mesh landing over an edit. Planted defect 11 proves it.
- **Levels.** One streaming level per cell for the session, toggled with `ShouldBeLoaded`. Nothing
  is flushed. A cell unloaded before its level became visible simply never finishes.

### Unloading never destroys newer state
- **A cell whose runtime layer came in** is stowed as before (`StowCell`).
- **A cell unloaded mid-load** (runtime layer never came in) still has its full dormant record.
  Only its ground delta, and any pieces placed on it while half-loaded, are merged into it
  (`StowTerrainOnly`). Stowing it as if it were live would overwrite the record with the empty
  live state. Planted defect 10 proves that this is caught.
- `IsRuntimeLive` answers "is this cell's live state authoritative?". Save capture of a
  half-loaded cell writes its dormant record plus its live ground.

### Memory is bounded by construction
- **Chunk actors are pooled** (512 at most). A retired chunk's geometry is freed at once
  (`ClearForPool`), not at the next garbage collection.
  - Without this, the 1 km torture test was OOM-killed at 21.8 GB: destroyed actors held their
    meshes until garbage collection.
- **Retirement is spread** over frames, 16 chunks per frame.

## Invariants the tests hold (Gridlands.Game.Grid, 5 tests)
- Nothing is duplicated, lost or replayed across repeated crossings: buildings, terrain edits,
  glitches, salvage, creatures, dialogue and events, per-cell stability, and continuity across the
  boundary.
- Rapid reversal mid-load keeps everything: edit and build on a half-loaded cell, save mid-load,
  turn back, return, and restart from that save.
- Stale async work never lands:
  - an edit while meshes are in flight;
  - unload and reload with work outstanding;
  - pooled reuse.
- Zenny never falls: a teleport into an unloaded cell's middle has ground under it on the first
  frame.
- Memory stays bounded over the torture rounds (< 1.5 GB growth, with garbage collection as the game runs it).

## Measured (real game, 1920×1080, RX 6800; localized navigation; details in the evidence)
- **Crossing at 6 / 18 / 12 m/s** (straight / sprint / reversal):
  - worst frame 22.9 / 18.0 / 34.1 ms;
  - p99 4.6 / 8.5 / 8.2 ms;
  - streaming game thread averages 0.05–0.12 ms per frame, 9.5 ms at worst.
- **Readiness:** ground under the player 0.1 s, runtime layer 0.1 s, every chunk 2.2–2.4 s after
  a cell starts loading. The next cell is complete long before Zenny arrives, and emergency chunks
  stay at 0.
- **Worst hitch: 34 ms, once per cell unload.** It is bisected to the engine removing the
  authored level from the world, not to Gridlands streaming work.
- **Memory:** a peak of 4.4–4.6 GB process with two cells overlapping (3.3 GB with one).
- **Cancel and resume:**
  - a mid-load cancellation is forced in the real game (teleport) and in the tests;
  - save/restart in the second cell: ground usable 0.11 s after launch, and edits intact.

## Consequences
- **Streaming work is spread across frames.** What remains per frame is mostly engine work:
  navigation (ADR-0029), level-instance registration, and collision cooking completing.
- **Test worlds differ from the real game in two ways**, and the tests do both explicitly:
  - they never tick level streaming, so they use `FlushAll` (tests that check state) or `Step`
    (tests that check timing);
  - they never run periodic garbage collection, so they call `CollectGarbage`.
