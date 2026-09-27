# P8: production-density readiness

**Operator-approved 2026-09-27.** P8 is infrastructure, and the building systems are not part of it.
The [architectural north star](../../ARCHITECTURAL-NORTH-STAR.md) was recorded first (PR #28).
Decision record: [ADR-0036](../../ADR/0036-gameplay-models-lods-production-density.md).

## A. Model / presentation split
**Glitches, salvage nodes and creatures now have authoritative models** (a glitch record, an actor
placement). They are made, and their saved state resolved, in the cell's authoritative frame. Their
actors are queued presentation, made from the model as it is at that moment:
- a salvaged node or a defeated creature is never made;
- a glitch is never presented in its default state first.

**Presented actors write through to their model**, and saves, stability and glitch requirements read
the models.

**Unload** removes the models at once and retires the actors inert (out of queries, hidden, no
collision, no tick, nav invoker off). It then destroys them within the presentation budget.

Tests: `Gridlands.Game.ActorPresentation` (4), plus the town-block tests (C).

## B. LOD pipeline and budgets
See [ART-PIPELINE §4b](../../ART-PIPELINE.md). Per-mesh triangle budgets and screen sizes are data
(`visual.*.lod`), plus `cullDistance`. Validator rules VIS-3/4/5 (with the reducer's 64-triangle
floor). `GLImportArt` builds the LODs and fails the import when one is over budget:
[`import-report.json`](import-report.json) has a row per LOD.

**The result:** 24 meshes, 3 LODs each, 78 LOD levels checked, all within budget. Instanced grass
(`SM_GrassTuft`, 504 triangles at LOD0) goes to 166, then 64, and is culled at 60 m; flowers are
also culled at 60 m and bushes at 150 m.

**A real catch.** The importer rejected SM_GrassTuft and SM_K50_Roof at LOD2 (64 > 60). That is how
the reducer floor was found. Their budgets became 70, and the floor became VIS-3.

Tests: `Gridlands.Game.Lod` (3), and `LodRuleTests` (9) in `Tools/selftest.sh`.

## C. The production-density fixture (exact density)
`-GLTownBlock` places a town block **on the street the P5 crossings walk** (lots-local y = −30000,
lots-local x −45 … −33 m, about 120 m of frontage on both sides):

| | Count |
|---|---|
| structures | **72** / **442 parts** |
| — storefronts (two shop faces around an alley) | 20 (320 parts) |
| — carports (alley, back-lot parking, driveways) | 14 (56) |
| — street pines (both sidewalks, every 12 m) | 20 (40) |
| — sidewalk phone tables (half glitched) | 8 (16) |
| — landscaping boulders | 10 (10) |
| vegetation patches | **12**, **3,735 instances** (6 grass, 3 flower, 3 bush) |
| creatures | **3** |
| glitches | **6** |
| salvage nodes | **12** |

**Density compared.** That is 442 parts, against the lots' 26 authored parts and the P7 stress
strip's 308. `towndense` runs both together: **750 parts, 20 vegetation patches**, and a
presentation queue peak of **823 units**.

**Site search.** Sites were generated once from the nominal layout under the authored-structure
support rule. Two storefront slots became carports, because their ground takes no storefront. The
test re-checks support, footprint overlap, and actors on the ground.

## D. Streaming and frame performance (quiet machine, unchanged P5 budgets)
All four suites were run back to back with `Tools/perf/quiet-run.sh`: local, dense, town and
towndense. **All four ended QUIETDONE PASS; no budget was changed.**

**Worst frame / p99 frame / streaming game-thread worst (ms):**

| Mode (budget) | local | dense | **town** | **towndense** |
|---|---|---|---|---|
| straight (30 / 7 / 12) | 25.9 / 4.64 / 4.94 | 23.4 / 4.74 / 5.36 | **23.6 / 5.17 / 5.60** | **23.1 / 5.30 / 5.39** |
| sprint (30 / 11 / 12) | 15.0 / 4.91 / 4.11 | 15.4 / 5.08 / 5.02 | **17.0 / 5.42 / 5.22** | **15.0 / 5.61 / 4.56** |
| reversal (40 / 11 / 12) | 32.2 / 4.65 / 5.69 | 34.6 / 4.80 / 6.75 | **32.6 / 5.17 / 8.18** | **35.7 / 5.32 / 9.91** |
| teleport (40 / 8 / 12) | 25.4 / 4.64 / 4.96 | 24.2 / 4.74 / 5.07 | **24.7 / 5.10 / 5.00** | **24.2 / 5.28 / 5.10** |
| resume (25 worst) | 18.2 / — / 5.50 | 13.9 / — / 3.89 | **17.4 / — / 4.53** | **14.1 / — / 4.11** |

- **Emergency chunks: 0 in all 20 crossing runs.**
- **Authoritative-layer worst frame:** 4.5 (local), 4.8 (dense), 5.2 (town), 5.1 (towndense) ms.
  Making the models of 72 more structures, 12 nodes, 6 glitches and 3 creatures in one frame
  costs under 1 ms.
- **Presentation.**
  - The worst presentation frame is 1.6–1.9 ms in the straight, sprint and teleport runs, which is
    the 1.5 ms budget plus one unit. On reversals it is 2.6 ms (town) and 3.4 ms (towndense).
  - The lots are fully presented 0.64 s (town) and 0.80 s (towndense) after their load began, long
    before Zenny can walk the load margin.
- **Navigation active tiles, peak:** 301 (local, dense) → **557 (town, towndense)**, against a
  budget of 600. See K.
- **Terrain navigation full build:** 0.45 s (budget 1.5).

## E. Memory
| | local | dense | town | towndense | budget |
|---|---|---|---|---|---|
| crossing peak, worst mode (MB) | 3757 | 3722 | 3744 | 3767 | 5200 |
| round trips peak (MB) | 3633 | 3649 | 3675 | 3663 | 5200 |
| round trips growth (MB) | 81 | 59 | 78 | 65 | 400 |

**Terrain render-mesh memory** is recorded in every result (`terrainMesh*`). It is identical with
and without the fixtures (terrain does not depend on content):

| | Value |
|---|---|
| live chunks | 256 (plus 240–256 pooled, empty) |
| triangles | 2,097,152 (8,192 per 64 m chunk) |
| CPU mesh (`FDynamicMesh3::GetByteCount`) | **196 MB** at the end of a crossing, 195.4 MB peak over round trips |
| GPU buffers (derived: 44 B per triangle corner) | **264 MB** |

**No budget is breached, so the render representation was not redesigned** (operator rule).

## F. Persistence, restart, cancellation, reversal
**In the game (quiet runs):**
- Teleport leaves a 2 m mound in the lots, and resume reads it after restart: 453 cm both ways, with
  town and towndense.
- The resume run's worst frame is within the resume budget in all four suites.

**In tests** (`Gridlands.Game.TownBlock`):
- Changes through the real pipelines: a storefront's awning posts, a street pine felled, a junk pile
  salvaged, a gremlin killed, a glitch scanned.
- The lots are left, then streamed back at a 0.05 ms presentation budget:
  - the models hold every fact at once, with more than 100 units still waiting;
  - every frame of the 501-frame presentation is checked for duplicates and live retired actors;
  - a save is taken mid-presentation;
  - on restart the facts are identical, the salvaged node and the defeated gremlin are never made,
    and there are no duplicates.
- **Cancellation and reversal:** three rounds of streaming in, presenting a quarter, half and three
  quarters, then reversing out:
  - out: no live town actor, retired actors inert at once, nothing waiting, all destroyed within
    the budget;
  - back: the same facts, and exactly one actor per present part, unsalvaged node, undefeated
    creature and glitch.

## G. Tests
- **Full gate:** `Saved/gate.sh` → **129/129 tests, 45 requirements met, 0 warnings** (peak RSS 3.8 GB).
  The new suites are `Gridlands.Game.ActorPresentation` (4), `Gridlands.Game.Lod` (3) and
  `Gridlands.Game.TownBlock` (3), all in `Tools/required-tests.txt`.
- **Existing tests:** the 119 from before P8 all pass. Three were rewritten to assert on the models
  (a salvaged node and a defeated gremlin are no longer re-made as hidden actors).
- **`Tools/selftest.sh`:** PASS, including the 9 LOD rule tests.
- **`Tools/data.sh validate`:** 223 entities valid.

## H. Planted defects
`Tools/planted-defects/p8_split_lod.py`: 19 defects covering the model/presentation split (12) and
LODs (7). Each is built and tested on its own. See
[`planted-defects/summary.txt`](planted-defects/summary.txt), with a `.diff` and the test report
for each.

**Result: 19/19 caught.**
- **18 were caught on the first run.**
- **M12 survived at first** (actor presentation ignoring the budget). Two tests missed it:
  - the tests that paused presentation never reach the actor loop;
  - no test asserted that gameplay actors take more than one frame.

  The gap was closed in `TownBlock.SaveMidPresentationAndRestartKeepEveryFact`, which now asserts
  that the authoritative frame makes no far gameplay actor, and the next frame makes only a few.
  Re-run: caught.
- **Two layers are proven separately:**
  - the importer's own budget check, by the real catch in B;
  - the validator rules, by the selftest.

## I. Fresh clone
`Tools/verify-fresh-clone.sh` of **2ce92ec** passed. It used tracked inputs, LFS meshes with their
LODs, and the pinned engine, with no local engine override. It built and ran **129/129 tests, 45
requirements**, and changed no tracked file. The only commit after it is this README line.

## K. Debt recorded
- **Navigation active tiles reach 557 of 600** with the town block. Each creature's nav invoker
  (32/48 m radii) adds about 85 tiles. A cell with more than 3 creatures in range of the route would
  need smaller invoker radii or a tile budget decision; the budget was not moved.
- **Reversal presentation frames reach 2.6–3.4 ms** against the 1.5 ms budget: the budget plus one
  heavy unit (a vegetation patch or the retire/destroy of a large group). Streaming stays within
  12 ms (worst 9.9).
- **Player-built pieces are still made synchronously** when their cell restores (ADR-0036, Not
  changed).
- **The engine reducer's 64-triangle floor** makes LODs meaningless for very small meshes: the cull
  distance is their tool.
