# ADR-0035: Terrain chunk collision is a Chaos heightfield

- Status: **Accepted and canonical** (operator, 2026-09-27: "APPROVE HEIGHTFIELD COLLISION"). The
  render-diagonal visual review passed: differences on ordinary and natural terrain are negligible, and
  those under aggressive terraforming are acceptable. Heightfield collision (`-GLTerrainCollision=1`) is
  the default. The ADR-0034 component cook (0) and the worker trimesh (2) remain for measurement only.
- Date: 2026-09-26
- Builds on: [ADR-0022](0022-terrain-chunked-heightfield.md) (terrain is a chunked heightfield; caves
  are authored geometry), [ADR-0027](0027-canonical-grid-scale.md) (64 m chunks, unchanged),
  [ADR-0034](0034-terrain-chunk-collision-and-pool.md) (the collision cook is the remaining streaming
  cost).
- Evidence: [Docs/Evidence/Terrain-heightfield-spike](../Evidence/Terrain-heightfield-spike/README.md).

## Question
Can per-chunk triangle-mesh collision be replaced by heightfield collision while preserving the
gameplay semantics of editable terrain, and does that create useful headroom?

## Findings
1. **Semantics: compatible.** The accepted terrain is strictly one height per X/Y:
   - ADR-0022: "player terraforming is a heightfield"; caves and tunnels are authored geometry;
     player-dug caves need a new ADR;
   - `FGLHeightfield` holds one height per vertex;
   - the only edit operations are Dig, Raise and Flatten.

   No capability is removed.
2. **Engine path:** a `Chaos::FHeightField` built on the worker, attached on the game thread as one
   static body (the path Landscape uses internally), with its own navigation export. Landscape itself
   does not apply.
   - Rebuilding a whole chunk's heightfield on an edit (0.05 ms) is preferred over Chaos's in-place
     partial edit, which is not thread-safe.
   - Three engine constraints were found and handled:
     - per-cell materials, or cells read as holes;
     - the heightfield's fixed cell diagonal, so the render mesh must split the same way;
     - seam misses under a transformed heightfield, so the heightfield is left unrotated.
3. **Agreement:** visible surface = collision to ≤ 0.01 cm on traces and ≤ 0.30 cm on sweep contacts,
   with no misses and exact overlaps. This holds through small, steep, repeated, neighbouring, seam,
   corner and rapid edits, and through flushed and streamed restore.
   - **117/117 tests pass in all three modes.**
   - **10 heightfield-specific planted defects are all caught.**
4. **Headroom, measured on the same machine in the same session:**
   - per-chunk game-thread cost: **4.93 ms → 0.010 ms**;
   - edit update: **5.0 → 0.05 ms**;
   - real-game chunk apply: **5.35 → 0.10 ms**;
   - streaming worst: **6.3–8.8 → 3.8–7.0 ms**;
   - frame p99 in sprint and reversal: **8.0–8.7 → 4.8–5.5 ms**;
   - cell complete: **2.1–2.8 → 0.35–0.88 s**;
   - collision memory: **0.56 MB → 16 KB per chunk**;
   - crossing memory peak: **−330…−690 MB**;
   - roundtrip growth: **123–131 → 70–78 MB**.

   All budgets hold, and none was changed.
5. **Option A (a trimesh built on a worker, attached on the game thread) also removes the spike.** It
   costs 30× the worker CPU, 26× the edit cost and 25× the collision memory, and its memory peaks stay
   near canonical. Its one advantage: it could represent caves or overhangs later without a second
   collision representation.

## Decision (operator, 2026-09-27)
**Architectural constraints (canonical):**
1. **Heightfield collision is canonical for normal Gridlands terrain.**
2. **Terrain remains one height per vertex.**
3. **The render mesh uses the heightfield's diagonal,** splitting each quad between (x, y) and (x+1, y+1),
   so visible terrain and collision agree.
4. **Authored caves and tunnels use their own geometry and collision** (ADR-0022).
5. **Anything a heightfield cannot represent needs a NEW architectural decision:** player-dug caves,
   voxels, overhangs, undercuts. This system is not silently expanded to support them.
6. **Worker-built triangle collision is the documented fallback** (option A, measured here) if a future
   requirement genuinely cannot use heightfields.
7. **The canonical 64 m chunk and 1 m terrain resolution are unchanged** (ADR-0027).

**Permanent gates:**
- `Gridlands.Game.TerrainCollision` (required). It covers:
  - collision/render agreement, seams and four-chunk corners;
  - deformation updating collision at once;
  - save/restore, flushed and streamed onto pooled chunks;
  - the ground structures and building stand on agreeing with the visible, colliding ground at every
    vertex;
  - holes (every probe finds ground; overlaps exact);
  - the render diagonal.
- `Gridlands.Game.TerrainPool` (pooling and reset, stale-generation rejection, cancellation, navigation
  octree).
- Navigation, Terrain, Grid, Streaming, Save, Building and Structure suites.
- The planted defects in `Tools/planted-defects/terrain_collision.py` (17), every one of which must be
  CAUGHT.
- No P5–P7.1 performance, memory, streaming, persistence or gameplay gate is weakened.

**Not in this decision, and not approved by it:**
- **Final terrain art.** P7.1 approved the visual language, not production quality (VISUAL-DIRECTION).
- **Extreme 1 m edits look harsh.** They show sharp wedges, creases and hard earth/grass boundaries,
  which are clearest in the aggressive-terraforming comparison
  ([diagonal review](../Evidence/Terrain-heightfield-spike/diagonal-review/README.md)).
  - This is **future terraforming presentation and brush-quality debt** (brush behaviour, smoothing,
    normals, material blending), not a collision defect.
  - That comparison is kept as regression evidence: future work must make that case better, not hide
    or move it.
  - It is not addressed here: no change to brushes, smoothing, normals, materials or geometry.

## Newly exposed (recorded, not acted on)
- **The render mesh is now ~80% of terrain memory:** ~2.33 MB per chunk, ~0.6 GB per loaded cell.
  Collision is no longer the dominant cost.
  - This is measurable future optimization territory: optimize only when production density, profiling
    or an established budget shows it matters.
  - No LODs, new representations or reduced resolution come with this decision.
- **Gameplay heights inside a quad (open, operator decision).**
  - `HeightAt` (what structures, building, scatter and storm stand on) equals the visible, colliding
    ground exactly at every vertex (gated).
  - **Inside a non-planar quad it interpolates bilinearly,** while the drawn and colliding surface is
    two triangles. On steep edits the gap reaches **~72 cm** (measured: steep dig 68 cm, neighbouring
    edits 72 cm; untouched ground 0).
  - It predates this ADR: the canonical trimesh measured the same, because the old surface was
    triangles too.
  - **Closing it** means `HeightAt` interpolating over the same triangles. That changes a gameplay height
    query used by structure support and deterministic collapse (P6), building placement and scatter.
    So it is left for the operator.
