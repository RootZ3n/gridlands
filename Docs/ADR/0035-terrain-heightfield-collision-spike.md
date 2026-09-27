# ADR-0035: Terrain chunk collision as a Chaos heightfield (decision spike)

- Status: **Proposed, AWAITING OPERATOR.** Measured; **not canonical.** The canonical path is still
  ADR-0034's component trimesh (`-GLTerrainCollision=0`, the default). The operator's directive was
  not to commit to heightfield collision architecturally until the spike proves it preserves Gridlands
  gameplay.
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

## Recommendation (for the operator's decision)
**Adopt heightfield collision as canonical terrain collision**, with these conditions:
- **The render mesh splits quads along the heightfield's diagonal.** This is a presentation-only
  change to triangulation. It is expected to be invisible (smooth normals) and needs an operator
  visual check before it lands.
- **Keep the spike's agreement suite and planted defects as gates.**
- **Record the constraint:** any future ADR that adds player-dug caves or a voxel layer must bring its
  own collision for those regions.
- **Option A is the fallback** if that constraint becomes unacceptable.

## What adopting it would change (not done)
- Mode 1 becomes the default, and modes 0/2 become measurement-only or are deleted.
- ADR-0034's cook path is retired.
- Evidence for P5–P7.1 budgets is re-measured under the new default (the spike already shows them
  holding).

## Newly exposed
- **The render mesh is now ~80% of terrain memory:** 2.33 MB per chunk, ~0.6 GB per loaded cell. Its
  representation (attributes, overlays) and LOD are the next memory lever. LODs are already required
  before production density (P7.1).
