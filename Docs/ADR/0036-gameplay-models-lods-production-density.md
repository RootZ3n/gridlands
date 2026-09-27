# ADR-0036: Gameplay models before gameplay actors, LODs as data, production density

- Status: **Accepted** (operator decision 2026-09-27: "Proceed with P8 — Production-density readiness
  as proposed").
- Date: 2026-09-27
- Builds on: [ADR-0033](0033-multi-frame-cell-presentation.md) (multi-frame presentation),
  [ADR-0032](0032-visual-pipeline-and-stylization.md) (visual pipeline),
  [ADR-0028](0028-seamless-grid-streaming.md) (streaming budgets).
- Constrained by: [ARCHITECTURAL-NORTH-STAR](../ARCHITECTURAL-NORTH-STAR.md) (locked design intent,
  recorded before P8 began).
- Evidence: [P8 evidence](../Evidence/P8-production-density/README.md).

## Context
P7 split structures into an authoritative model and presentation made over frames (ADR-0033). P8
readies the runtime for production density, and three things stood in the way:
1. **Glitches, salvage nodes and creatures** were still made as actors in the authoritative frame.
   Their state lived on the actor, so a dense cell would have to make them all at once, or apply
   their saved state late.
2. **No mesh had LODs.** P7.1 grew the 24 proof assets from 14,348 to 38,804 triangles, and the
   grass tuft (504 triangles) is instanced by the thousand.
3. **The P7 dense fixture was a stress strip** about 340 m off the crossing route. Nothing measured
   a credible town where the crossings actually walk.

## Decision
### 1. Glitches, salvage nodes and creatures: model first, actor as presentation
**The authoritative models:**
- **Glitch:** an `FGLGlitchRecord` in `UGLGlitchSubsystem`, keyed by placement. It holds the state,
  repair progress, items delivered and bindings.
- **Salvage node and creature:** an `FGLActorPlacement` in `UGLPlacementSubsystem`. A salvage node
  holds `bSalvaged`; a creature holds `bDefeated`.

**In the authoritative frame,** `SpawnCell` makes the models, `ApplyCell` resolves the saved state
onto them, and the actors are queued (`FGLPendingActor`).

**Presentation.** `PumpPresentation` makes each actor from its model as it is at that moment:
- the queue goes nearest first, with the near radius at once and the rest within the budget;
- a glitch is set from its record (`PresentFromModel`, behind a passkey: visuals only, no events);
- **a salvaged node or a defeated creature is never made.**

**Presented actors write through to their model:**
- a glitch component notifies its record on every transition, restore, progress change and
  delivery;
- a salvage node's `OnSalvaged` sets `bSalvaged`;
- a creature's death calls `MarkDefeated`.

**Everything that needs gameplay truth reads the models:**
- saves (capture and apply);
- NICE stability sampling;
- glitch requirements (`IsSalvaged`);
- tests.

**Unload** removes the models at once and drops the waiting actors. Presented actors are retired
inert, then destroyed within the budget, before anything new is made:
- a creature is defeated-silent, stopped, with its nav invoker off;
- a node refuses salvage, hidden, with no collision;
- a glitch leaves the registry at once.

**Puzzle sites stay immediate** (stateless, few). Discovery sites were always data only.

**Semantic change:** after a reload, a defeated creature and a salvaged node have **no actor at
all**. Before, they had a hidden one.

### 2. LODs and triangle budgets are data
**Every `visual.*` declares its mesh's LOD budget** (`lod.maxTriangles`, `lod.screenSize`) and
optionally a `cullDistance`. Validator rules:
- **VIS-3:** the budget is well formed, and no reduced LOD is below the reducer's 64-triangle floor.
- **VIS-4:** scattered vegetation declares a cull distance.
- **VIS-5:** visuals of one mesh share its budget.

**`GLImportArt` builds the LODs** with the engine reducer, to about 92% of each budget, retrying
harder when over. It **fails the import** when any LOD is over its budget.

**At runtime,** LODs come with the mesh. Visual components and scatter HISMs take the data's cull
distance.

**LODs are presentation only.** `Gridlands.Game.Lod` proves that forcing the lightest LOD changes
no gameplay trace. See [ART-PIPELINE §4b](../ART-PIPELINE.md).

**No Nanite and no HLOD.** Nothing at the P8 budgets calls for them (see Evidence).

### 3. The production-density fixture: a town block on the route
`GLTownBlock` (`-GLTownBlock`) is dev only, in-memory placements in the diner lots, built from
existing definitions. It flanks the street the P5 crossings walk (lots-local y = −30000):

| Content | Count |
|---|---|
| structures | **72** (442 parts) |
| — storefronts | 20 (two shop faces, 18 m and 12 m groups either side of an alley) |
| — carports | 14 (alley, back-lot parking, driveways) |
| — street pines | 20 (both sidewalks, every 12 m) |
| — sidewalk phone tables | 8 (half glitched) |
| — landscaping boulders | 10 |
| vegetation patches | **12** (3,735 instances): 6 grass, 3 flower beds, 3 bush |
| creatures | **3** gremlins |
| glitches | **6** (the lamp bound to the block's own junk pile) |
| salvage nodes | **12** |

**How the sites were chosen.** They were generated once from a nominal layout by a site search
under the authored-structure support rule. Two storefront slots, whose ground takes none, became
carports. `Gridlands.Game.TownBlock` re-checks every site, footprint overlap, and actor ground
height.

**How it is measured.** `Tools/perf-crossing.sh -b` (results `town-*`) and `-b -d` (`towndense-*`,
together with the P7 strip), under the unchanged P5 budgets.

### 4. Terrain render-mesh memory is measured, not redesigned
Crossings record `terrainMesh{Chunks,Pooled,Triangles,CpuMb,GpuMb}AtEnd`, and round trips record the
peak:
- **CPU** is the engine's `FDynamicMesh3::GetByteCount`.
- **GPU** is derived from the proxy's buffers: 44 B per triangle corner (position 12, high-precision
  tangents 16, full-precision UV 8, colour 4, index 4).

The operator's rule applies: **redesign only if a budget is breached.** None is (Evidence).

## Consequences
- **Streaming cost stays independent of authored density** for every stateful placement kind, not
  only structures.
- **Gameplay code must not assume an actor exists** for a glitch, node or creature. `FindSalvageNode`,
  `FindCreature` and `FindByPlacement` may return null for a waiting (or never-made) one. Ask the
  model instead (`IsSalvaged`, `IsCreatureDefeated`, `FindRecord`).
- **Changing a LOD budget means re-running `Tools/art.sh`.** The C++ test fails when data and the
  built meshes drift apart.

## Not changed
- **Player-built pieces (`UGLBuildingSubsystem`) are still made as actors synchronously** when their
  cell restores. They are few today. For the building milestone (see the north star), they need the
  same model/presentation split before player houses grow to hundreds of pieces.
- **Partial salvage integrity is still not persisted** (unchanged behaviour).
- **The P7 dense stress strip is unchanged** and still measured (`dense-*`).
- **Dungeon/infiltration intent (recorded after this ADR):** audited, with no conflict. The constraints it places
  on future work (a creature outcome richer than defeated, persistent search state, the navigation
  tile budget with patrols, data-based concealment) are in the evidence, section M.
