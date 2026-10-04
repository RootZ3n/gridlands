# Building v1: pre-P11 design gate

> **Status: APPROVED by the operator (2026-10-03) as the P11 design, with the decisions in §17 resolved as recorded
> there. Implementation is P11 (not merged until operator review).**
>
> **Implemented in the P11 candidate (branch `p11-building-v1`, [ADR-0039](ADR/0039-building-v1-canonical-structural-model.md)).**
> Deviations from this design, each for a reason recorded in the evidence:
> - claims are **derived** from base-core pieces, not saved as `FGLSavedClaim` records (one source of truth; an area
>   list still leaves room for expansion);
> - a storage crate **stands on a floor** (not grounded), so it can lose its support like anything else;
> - the floor gained a centre socket (for furniture);
> - log walls are 0.24 m thick (0.3 m overlapped at corners);
> - debris returns its finish layers' collapse yields as well as its frame's.
> - The operator decisions it relies on are quoted as decisions, with their date.
> - Everything else is a recommendation.
> - The open choices are listed in §17.
>
> Inputs: [ARCHITECTURAL-NORTH-STAR](ARCHITECTURAL-NORTH-STAR.md) (LOCKED intent),
> [ADR-0024](ADR/0024-building-v0-structural-model.md) (Building v0),
> [ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md) and
> [ADR-0038](ADR/0038-structural-environmental-resolution.md) (collapse),
> [ADR-0035](ADR/0035-terrain-heightfield-collision-spike.md) (terrain), and the operator's pre-P11 brief of
> 2026-10-03.

## 0. Operator decisions this design builds on (2026-10-03)
- **Player-built collapse: APPROVED IN PRINCIPLE.** Player structures obey the same structural language as authored
  ones. There is no "authored physics versus player magic". The placement preview is part of the fairness contract.
- **Preview vocabulary:** GREEN (valid and stable), YELLOW (accepted, with a meaningful structural warning), RED
  (invalid, or would fail immediately).
  - It must derive from the same rules as the result. No fake percentage simulator.
  - PREVIEW PREDICTION == ACTUAL STRUCTURAL RESULT for every gated fixture.
- **Inventory:** no weight-based encumbrance. Slots and stack limits are the limit. Common construction materials
  stack generously; about 100 is provisional, not locked balance.
- **Shared base storage: APPROVED as a minimal capability.** "If the material is in my base storage, I can build
  or craft with it while I am at that base." No logistics networks, and no world-wide magic connection.
- **Phases:** P11 proves FRAME → FINISH. ELECTRICAL is not implemented, but the schema must accept it later
  without replacement.
- **Appearance = COMPONENT FORM + MATERIAL + FINISH.** There are no era-compatibility restrictions.
- **Salvage quality:** careful disassembly beats destructive salvage, and destructive salvage beats collapse.
  No nail accounting.
- **Basements:** open excavation within one height per vertex is supported. Overhangs and cavities are NOT
  authorized without a new ADR.
- **Ofi plans** are plans for ordinary components, never a second kind of building.
- **Claims:** the minimum ownership needed so regeneration can never overwrite player construction.

## 1. What the build has today (audited 2026-10-03)
- **One rule core, two runtime owners.**
  - `GLStructureRules` (support, sockets, bounds, snapping) and `GLCollapseRules` (collapse planning) are pure Core
    code, shared by player building and authored structures.
  - Structure parts reference `buildpiece.*` definitions and derive from the same actor (`AGLBuildPiece`).
  - The runtime owners differ: `UGLBuildingSubsystem` handles player pieces, `UGLStructureSubsystem` handles
    authored structures.
- **Support is a real scalar** (ADR-0024).
  - A grounded piece starts at its material's `strength`.
  - Every link loses `strength / maxStack` (resting) or `strength × metres / maxHorizontalSpan` (lateral).
  - A piece stands while its support is above 0.
  - Links are made by **coinciding sockets** (5 cm). Adjacency is already socket-based, not grid- or box-based.
- **Player pieces do not physically collapse.**
  - `Demolish` removes the target plus everything `CollapsesAfterRemoving` finds, destroys the actors, and refunds
    the full cost.
  - Digging under a player piece is refused.
- **The ghost is green or red,** and the HUD shows support or the refusal reason.
- **Quarter turns:** `FGLPlacedPiece::YawQuarter` (0..3). Bounds are AABBs from `size`, with X/Y swapped on odd
  quarters.
- **Inventory** (`FGLInventory`) has 32 slots, per-item `stackSize` (10–50 today), and `weight` (150 max, over it
  slows Zenny). Weight is **never saved**.
- **Fabrication and building read only Zenny's inventory.** No container exists.
- **Salvage yields one fixed table per definition,** scaled by world settings (ADR-0016). Debris salvages "as the
  part did (provisional)". There is no quality, and overflow beyond the inventory is lost.
- **The save** is version 2:
  - `FGLSavedPiece {Id, Def, Location, YawQuarter}`;
  - authored parts save only when not intact;
  - in-flight collapses save cause, credit and an OBB impact.
  - **No ownership flag exists.**

## 2. The authoritative schema (E)
**Principle: a building is a graph of placed structural components. Each carries optional, ordered construction
layers. Everything visible is derived.**

### 2.1 Content kinds
| Kind | Is | Carries |
|---|---|---|
| `buildpiece.*` (existing) | a **structural component's form**: the FRAME object | `role`, `size`, `shapes` (the frame visual and collision), `sockets` (+ optional facing, §11), `material` (structural; decides support), `cost`, `unlockedBy`, `collapse`, **new:** `layers` (which construction phases this form accepts, see below), `salvage` (per-path yields, §6) |
| `finish.*` (**new**) | a **finish layer** installable on compatible forms | `phase` (`phase.finish`), `fitsRoles` (wall, floor, roof...), `cost`, `unlockedBy`, `visual`, `appearanceMaterial` (look and salvage, **never structural**), optional `surface` (footstep noise material, P9's floor-material hook), `salvage` (per-path yields) |
| `phase.*` (**new**, `Data/_registry/construction-phases.json`) | the **ordered phase registry** | `phase.frame` (implicit: the piece itself), `phase.electrical` (`optional: true`, `implemented: false`), `phase.finish` (`optional: true`), each with `order` and `requires` |
| `material.*` (existing) | physical properties | unchanged; support stays here |

- **Era stays a tag on forms and finishes** (look and knowledge). It is never a compatibility rule.
- Any finish fits any form whose role it lists. A Victorian clapboard finish on a modern stud frame next to a log
  wall is legal (WINCHESTER).
- **A piece family decides its phases in data.**
  - A stud wall accepts `[finish]` (and later `[electrical, finish]`).
  - A log wall, a Roman column or a masonry wall is complete as framed: `layers: []`, so its form IS its finish.
  - This keeps "different historical methods differ structurally" (north star §3) without parallel systems.

### 2.2 The placed-piece fact (Core)
```
FGLPlacedPiece {
  Id, Def, Cell,
  Location (cm, bottom centre),
  YawStep        // int 0..143, 2.5° units (§11); replaces YawQuarter (= YawQuarter × 36)
  Origin         // Authored | Player (§9); explicit, never inferred from which list it is in
  Layers[]       // { Phase, Def } in phase order; empty = frame only
}
```
- **Support stays derived and unsaved (S-1).**
- **Layers never change support in v1.** A finish that adds load or strength would be a later, explicit rule
  change.
- **The ghost, a plan entry (§12) and a placed piece share this shape.** The ghost is a fact not yet committed.

### 2.3 One runtime owner of the structural model
- **Player-built pieces join the structure model** that P6/P10 gave authored structures.
  - Each cell keeps one structural fact store whose pieces have an `Origin`.
  - Collapse, impact-time evaluation, debris, in-flight saves, streaming freeze and resume (ADR-0030/0038) apply
    to both.
- **`UGLBuildingSubsystem` keeps the player verbs** (place, install finish, dismantle, smash) and calls the same
  transactions as authored salvage.
- This is the code shape of "no authored physics versus player magic". It also closes the P8 debt: player pieces
  restore through the same multi-frame presentation as authored parts (ADR-0033/0036).

## 3. FRAME → future ELECTRICAL → FINISH (F)
- **Placing a piece is the FRAME phase.** It pays the form's `cost` (components such as studs) and presents the
  **frame visual**.
  - Example: a stud wall shows studs, plates and an open bay. It does not show a panel.
- **Installing a finish** is a verb on an existing frame.
  - It checks that the phase is accepted by the form, that the earlier required phases are present, knowledge,
    and materials (§8 sources).
  - It pays the finish's cost, appends `{phase.finish, finish.*}` to `Layers`, and re-derives the presentation.
  - The wall now looks clad. **Structure is unchanged.**
- **Stripping a finish** is a careful-salvage verb that returns the finish's careful-path yield and leaves the
  frame standing.
- **ELECTRICAL later** means registering `phase.electrical` as implemented, adding `electrical.*` layer
  definitions, and listing it in stud forms' `layers`.
  - Its order (between frame and finish) is already in the registry.
  - Saves already carry an ordered layer list, so the schema needs no replacement.
  - Until then the validator refuses any `electrical.*` content (PH-2).
  - A frame may be finished without electrical, because electrical is optional.
- **Presentation is derived** from (form, layers, state): the frame mesh, or the frame plus finish meshes. It is
  never a skin swap on a finished mesh.
  - No production art is in P11: frame and finish visuals are procedural proxies in the P7.1 language.

## 4. Player-built collapse (G)
**The same rules as authored structures (ADR-0030/0038), triggered by the same causes:**
- **Removal** (careful dismantle or destructive smash) re-derives support. Every piece whose support falls to 0
  collapses: it drops or topples per data, is frozen while its cell is dormant, and resolves its impact at impact
  time.
  - SUPPORT FAILURE ≠ IMPACT holds for player structures exactly as for the carport.
  - Fallen pieces become **debris**: persistent, saved and salvageable by the collapse path (§6).
  - **The v0 full-refund demolition is retired.** A careful dismantle of a piece nothing depends on recovers
    components (§6). Anything that falls is debris.
- **Placement can never cause a collapse under the v1 support model.**
  - Adding a piece only adds links, and support is a maximum over links.
  - Only removal, or support loss from another collapse, can bring a piece down.
  - This is why the **removal preview** (§5.3) carries the fairness contract as much as the placement preview.
- **Terrain under a player foundation stays protected** (refused, as in v0).
  - Making digging collapse a structure is possible (`CollapsesAfterRemoving` exists), but it is a separate
    decision (§17).
- **Out of v1:**
  - load or impact between parts (a collapse does not break what it lands on; still deferred);
  - weather;
  - per-piece health.

## 5. GREEN / YELLOW / RED (H)
All three colours are computed by the same `GLStructureRules` functions that decide the result, on the candidate
fact.

### 5.1 Placement preview
| Colour | Rule (same code as the commit) |
|---|---|
| **RED** | `CanPlace` refuses: support ≤ 0, overlap (oriented, §11), burial, or a phase/compatibility violation. Missing knowledge or materials is also RED, but shown with its own reason text, because nothing is wrong with the structure. Nothing is spent. |
| **YELLOW** | accepted, and **at its material's limit**: the candidate's derived support is at most one vertical step of its own material (`support ≤ strength / maxStack`). Nothing of that material can rest on it: the next storey would reach 0 and be refused. |
| **GREEN** | accepted, with support above one vertical step. |

**Why YELLOW is margin and not redundancy.** The brief allows "redundancy-sensitive" YELLOW, and I checked whether
it can be honest at placement time. It cannot be useful there: under the v1 model almost every non-grounded piece
has a single point of failure (the foundation it rests on). "Would one removal collapse it?" would paint nearly
every wall yellow. Redundancy is real information, so it is shown where it matters, in the **removal preview**
(§5.3).

**The margin rule is honest:**
- the support scalar is the canonical value, not an invented percentage;
- the threshold is the material's own step, not a tuning constant;
- "this is the top of what pine can carry" is a true statement.

### 5.2 What YELLOW means in numbers today (pine: strength 3, maxStack 4, span 3 m)
| Case | Support | Colour |
|---|---|---|
| floor on the ground | 3 | GREEN |
| 1st wall | 2.25 | GREEN |
| 2nd wall | 1.5 | GREEN |
| 3rd wall | 0.75 | YELLOW (≤ 0.75: a 4th would reach 0 and be refused) |
| one 2 m floor hung off a grounded floor | 1.0 | GREEN (a wall can still rest on it, at 0.25) |
| a second 2 m hang | −1 | RED |

**Limitation, stated rather than hidden:** YELLOW measures the vertical step only. A floor that cannot be extended
sideways again (the 1.0 hang above) is GREEN, because something can still rest on it. A lateral warning would need a
second honest threshold (the lateral loss for the piece's own width). That is cheap, and it is listed as option 2b
in §17.

### 5.3 Removal preview (new; part of the collapse fairness contract)
- **Aiming a dismantle or smash verb at a piece highlights every piece that would collapse without it.** This is
  `CollapsesAfterRemoving`, the same function the commit uses.
  - **Nothing else falls:** the target is outlined normally.
  - **Pieces would fall:** the dependents are outlined RED, with a count ("3 pieces will collapse").
- **The redundancy warning appears on the supports.** When a load-carrying piece is the only remaining support
  path for others (removing it collapses anything), its removal preview says so before the player acts.
  - Example: the carport's last post.
- **Impact victims are NOT predicted.** Who is hit is decided at impact (ADR-0038), and the preview never claims
  to know.

### 5.4 The equality gate
For every gated fixture and every step:
- the predicted colour and refusal reason equal the committed result;
- the predicted fallen-piece set equals the actual collapse set (by id).

Tests assert this pairwise, the way terrain surface agreement asserted the visible surface equals the gameplay
surface.

## 6. Salvage quality (I)
**Quality is which items come back, not a number on every stack.**
- Intact components (`item.component.stud`) and degraded materials (`item.material.scrap_timber`) are **distinct
  items**.
- Stacks stay homogeneous, so inventory math stays simple.
- There are no continuous quality grades in v1.

**A recovery path is chosen by the verb, and each salvage table has a row per path:**

| Path | Verb | Yields (illustrative, provisional numbers) |
|---|---|---|
| **careful** | dismantle (slow) a piece nothing depends on; strip a finish | the highest share of intact components (a stud frame: most of its studs plus a little scrap) |
| **destructive** | smash (fast) | fewer components, more scrap |
| **collapse** | salvage the debris of a fallen piece | predominantly scrap; a few components at most |

- **Data shape.** `salvage.*` gains `yieldsByPath { careful, destructive, collapse }`. The existing `yields` is
  read as `careful` for back-compatibility, with a validator rule (SV-Q1) for any part that can collapse.
  - Every entry still names a yield category, so world settings apply (E-1 holds).
  - `buildpiece.*` and `finish.*` reference a salvage definition. A player piece's salvage is its form's, plus its
    finish layers' tables by the same path.
- **Debris uses the collapse path.** That replaces "debris salvages as the part did (provisional)", for authored
  parts and player pieces alike.
- **Material flow (minimal):**
  - **tree → log:** P6 already does this.
  - **log → studs and boards:** new recipes at a saw station (`Station.Saw`; provisional yields).
  - **recovered framed wall → studs plus scrap:** the careful path.
  - **scrap:** a low-value common material, with uses kept small.
  - **Boards** are today's `item.material.timber_plank`, renamed only in display text. No id churn.
- **Overflow is no longer lost.** A salvage completes only if its yield fits (personal inventory, then base storage
  inside a base, §8). Otherwise it refuses with a reason, like crafting's `NoRoomForOutput`.
- **Out of v1:** nails, screws, wire gauges, per-stud health, and quality grades.

## 7. Inventory: stacks and slots, no weight (J)
- **Remove:**
  - `FGLItemDef::Weight`, the schema's required `weight`, and all 10 item files' `weight` keys;
  - `FGLInventory::TotalWeight / IsOverencumbered / MaxWeight`;
  - `UGLInventoryComponent::OverencumberedSpeedFactor / UpdateEncumbrance`;
  - `Event.Player.Overencumbered`.
- **Keep** slots and per-item `stackSize`. **Provisional numbers (not balance):**
  - common construction materials and components: 100 (planks, studs, stone, soil, scrap, wire);
  - parts: 10–20;
  - tools and unique items: 1;
  - personal inventory: 32 slots, unchanged.
- **Save:** no format change is needed, because weight was never saved.
  - Fix one existing defect: `RestoreContents` silently discards overflow. A restore must keep everything, so the
    inventory may be over capacity until the player frees space (adds are refused meanwhile).
- **Dialogue: retarget, never silently delete.**
  - The two exchanges (`overencumbered_pushups`, `overencumbered_again`) move to a new
    `Event.Player.InventoryFull`, emitted once when a pickup, salvage or craft is refused for lack of room.
  - Their lines are adjusted to the new situation (for example, "Too heavy? Have you considered push-ups?"
    becomes "Out of pockets? Have you considered pockets?").
  - The original text stays in each file's `notes` as history.
  - The dialogue breadth test's `overencumbrance` family becomes `inventoryFull`.
- **Tests:**
  - `Gridlands.Core.Inventory.StacksWeightAndCapacity` loses its weight half and is renamed `StacksAndCapacity`;
  - `Gridlands.Game.Inventory.OverencumbranceIsAnnouncedOnce` becomes `InventoryFullIsAnnouncedOnce`;
  - the content fixture's `"weight"` key is removed;
  - `Tools/required-tests.txt` counts are updated.
- **Docs:** updated in this gate (ARCHITECTURE, STORY-AND-DIALOGUE, CONTENT-IDS-AND-TAGS, the north star's
  conflict row).

## 8. Shared base storage (K)
- **What a base is.** A base is a **claim** (§9) created by placing one **base core** piece
  (`buildpiece.*` role `base_core`, one per base).
  - The claim is a circle of radius `claim.radius` around it. The radius is data; 32 m is provisional.
  - It is in the core's cell. Claims may not overlap.
- **Which containers participate.** Only **storage pieces** (role `storage`, with a slot count in data) that are:
  - **player-origin**;
  - **inside the same claim**;
  - **intact and standing**: not debris, not falling, and not frozen in a dormant cell.

  World containers, other claims' containers and carried containers never participate.
- **Who may draw from storage.** Zenny may draw only while **standing inside that claim**, and only for:
  - building placement;
  - installing a finish;
  - crafting at a station inside the same claim.
- **Consumption is transactional and deterministic.**
  - The whole cost is gathered first, from an ordered source list, and committed only if it is complete.
    Nothing is ever partially spent.
  - **Recommended order (an operator choice, §17):** base storage first, then personal inventory.
  - **Containers** are ordered by distance from the placement or station point (ascending), then by piece id.
    **Stacks** go in slot order.
  - Storage first keeps Zenny's expedition kit intact. The opposite order is a one-line change.
- **Where returns go.** Refunds and salvage yields while inside a claim go to personal inventory first, then
  overflow to storage in the same order. If neither has room, the action refuses.
- **Persistence.**
  - Container contents save with their piece: `Contents[]` on the saved piece.
  - Claims save per cell (§9).
  - Nothing about storage is derived from presentation.
- **When storage disappears.**
  - **Dismantling a non-empty container is refused** ("empty it first").
  - **A container that collapses becomes debris that keeps its contents.** The debris is lootable, and its
    contents are never lost and never duplicated.
  - **A container leaves the source list the moment its support fails,** not at impact.
  - There is no "mid-build" for a single placement: each placement is one atomic transaction in one frame. Between
    placements, the source list is rebuilt from the current facts.
- **Not in v1:** logistics, conveyors, automation, priorities, filters, shared storage across claims, or
  storage in an unloaded cell.

## 9. Ownership and claims (L)
**The minimum that makes player construction unregenerable:**
1. **`Origin` on every placed piece** (`Player` for anything Zenny built). It is explicit in the saved fact and
   never inferred from which list a piece is in.
2. **`FGLSavedClaim {Id, CorePieceId, Center, Radius}` per cell.**
3. **A Core rule, `GLClaimRules::MayRegenerate(cell, location, origin)`.** It is false for player-origin facts and
   for any location inside a claim.
   - No regeneration exists yet.
   - An architecture test requires that any future code path removing or replacing world facts goes through this
     rule. The planted defect "a regenerator ignores Origin" must be caught.

**Out of v1:** claiming an authored building found in the world, multiple owners, and decay.

## 10. Basements and excavation (M)
- **Works with today's systems:**
  - Dig an open pit with the existing dig stroke: 1 m heightfield, one height per vertex.
  - Place grounded foundations on the pit floor (bottom sockets within 30 cm of the ground).
  - Build retaining and foundation walls up to grade, then a floor over the pit supported by those walls.
  - Debris falls into the pit, because `GroundUnder` samples the edited terrain.
- **Order matters.**
  - The ground under a grounded piece (footprint plus 50 cm) cannot be terraformed afterwards. Dig first, build
    second.
  - A stair or ramp piece is needed for access. Navigation runs on the pit floor.
- **Known limitation, honest.** With 1 m vertices a pit edge is a slope across one cell (2.5 m deep gives about
  68°), not a vertical face.
  - A wall placed flush against the edge shows terrain poking through, or a gap of up to 1 m. The wall is legal,
    because burial checks only bottom sockets.
  - It is cosmetic, and it is debt for the art and terrain pass.
- **NOT authorized** (operator): sideways excavation, overhangs, cavities under intact terrain. Those need the new
  cave/tunnel ADR.
- **P11 proof:** one automated test digs a pit, builds a foundation, walls and a covering floor inside it, saves and
  restores. The basement is not part of the cabin fixture.

## 11. REAL HOUSE investigation (N, O)
### 11.1 The REAL HOUSE-0 fixture (an investigation target, not P11 scope)
A modest cottage:
- two rectangular rooms (6×4 m and 4×4 m on 2 m modules), with a doorway between them;
- a **three-sided 45° bay window** on the front room (a parallel centre panel and two panels at ±45°);
- a 6×2 m **front porch** whose roof rests on **two columns, one Roman** (masonry) and one timber post, and links to
  the cabin wall;
- a gable roof over the rooms;
- **one log-cabin wing wall** next to a clapboard-finished stud wall.

### 11.2 What the quarter-turn assumption simplifies (A) and where it lives (B)
| System | Encodes quarter turns how | Simplifies |
|---|---|---|
| Snapping | `YawQuarter = (q+1) % 4` (`GLBuildModeComponent.cpp:97`); a snap keeps the candidate's yaw; sockets have positions only | exact socket coincidence after rotation; no facing logic |
| Support | none directly; side-link length is the centre distance of `Bounds()` (`GLStructureRules.cpp:74`) | an exact bounds centre |
| Overlap | AABB from `size` with X/Y swapped (`Bounds()`, `GLStructureRules.cpp:95-103`), shrunk by 6 cm | a 6-compare overlap |
| Collision | none: actors rotate with any yaw (`GLBuildPiece.cpp:53`) | — |
| Save | `FGLSavedPiece.YawQuarter`, `yawQuarter` in JSON | — |
| Preview | the ghost rebuilds on `YawQuarter` change | — |
| Navigation | none: follows real collision | — |
| Collapse | landing surfaces as AABB overlap; drop impact from AABB extents; topple along/across extents projected from the AABB (`GLCollapseRules.cpp:16-34, 195-206`) | trivial landing and impact math |
| Authoring | `structure` part `yawQuarter: Int(0,3)`; **STR-3** (placement yaw a multiple of 90°); STR-4 swaps size on odd quarters; placement yaw silently rounded (`GLPlacementSubsystem.cpp:211`) | — |
| Terrain | footprint protection as an AABB grown by 50 cm | a point-in-rectangle test |

**Already general:** debris rests at arbitrary rotators, topple impacts are true OBBs, in-flight collapses save
quaternions, and render and nav collision rotate freely.

### 11.3 Can finer yaw work without replacing the structural model? (C) Yes.
- Support is socket-link-based and yaw-agnostic.
- Only bounds-shaped questions (overlap, footprint, landing, drop impact, topple extents) assume AABBs.
- The audit estimates 150–300 lines of Core C++, plus more test work than code.

### 11.4 Sockets are the right abstraction (D), and need a facing
- Adjacency is already sockets. For angled architecture, a socket gains an optional **facing**: a yaw in the
  piece's frame.
- A snap then derives the candidate's yaw from data: target facing + 180° − own facing. It does not use free mouse
  rotation.
  - A 45° bay is two wall forms snapped to an **angle post** (a small grounded column whose side sockets face
    ±45°).
  - An octagon uses a 45° post; a hexagon uses 60°; a 16-sided tower uses 22.5°.
- A side↔side link requires the facings to oppose (within a step). Today any two coinciding side sockets link at
  any angle, which would be wrong once angles exist.
- Grounded pieces placed freely (no socket) rotate in **15° steps** (7.5° fine mode). 90° stays the default step.

### 11.5 Oriented bounds are required (E)
- A bay wall touching the main wall at 45° overlaps it as AABBs, so the placement would be refused.
- **Overlap** therefore uses oriented boxes: a 2D separating-axis test on the yawed footprint plus the Z interval,
  shrunk by 6 cm as today.
- **Landing and drop impact** use the piece's axes. The **topple extents** project onto them.
- **Footprint protection** may stay a conservative AABB of the OBB. It is harmless, slightly larger.

### 11.6 Exactness: integer yaw
- Store yaw as **`YawStep`, an integer in 2.5° units (0..143)**.
  - 90° = 36, 45° = 18, 22.5° = 9, 15° = 6, 7.5° = 3.
  - It covers octagons, hexagons, 12-, 16- and 24-sided figures, and 5° steps.
  - It is exact under composition, deterministic to hash and compare, and trivially migrated (`YawQuarter × 36`).
  - Plans (§12) stay exact when rotated by any multiple of the step.
- **Truly arbitrary angles (37.3°) are deliberately not supported.** 2.5° steps are visually free for architecture.
- Socket positions are computed from exact angles in double precision. The drift is about 1e-4 cm against a 5 cm
  tolerance.

### 11.7 Performance (F) and migration (G)
- **Performance:** an OBB test is a few dozen flops against six compares. Placement is O(pieces in the cell), and
  collapse planning stays under the P6 0.16 ms scale. This is not measurable at frame level; P11 measures it
  anyway (§14).
- **Migration:**
  - save v2→v3: `YawQuarter × 36`;
  - data: `yawQuarter` becomes `yaw` (degrees, a multiple of 2.5), on 2 storefront parts;
  - validators: STR-3 becomes "a multiple of 2.5°"; STR-4 uses the OBB;
  - the placement rounding is removed;
  - docs: ADR-0024, ADR-0030, STRUCTURE-AUTHORING.

### 11.8 The smallest architecture that does not paint us into a corner (H)
**Recommendation: integer fine yaw, plus oriented boxes in the rules, plus socket facings for angle connectors.**
- **Not now:**
  - curved pieces;
  - free arbitrary angles;
  - pitch or roll of structural boxes (roof pitch stays a visual `shapes.pitch`, with a box structure);
  - non-box support volumes.
- **Round towers are segmented polygons** of straight walls on angle posts. That fits "reality is reference
  material, not law".
- **Doing this before hundreds of pieces exist costs about one Core change and one save migration.** Doing it later
  re-authors every piece family and plan.

## 12. Ofi plans and blueprints (Q)
- **A plan is a serialized subgraph of placed-piece facts.** Each entry is `{Def, relative Location, relative
  YawStep, Layers}` relative to an anchor, with no ids, owners or support.
- A ghost from a plan is a set of ghost facts, each with its live §5 colour.
- **Filling a ghost entry is the ordinary placement or finish transaction.** There is no second building kind.
- **What v1 must guarantee for this:**
  - every structural truth lives in facts and data (support derived), with no hidden per-actor state;
  - yaw is exact (§11.6);
  - finishes are layers, not separate meshes.
- **P11 adds no plan feature,** only one Core test: export a fixture's subgraph, re-instantiate it at another
  anchor and at a 45° rotation, and require identical support values and colours.

## 13. Save migration v2 → v3 (P)
| Change | Migration |
|---|---|
| `FGLSavedPiece.YawQuarter` becomes `YawStep` | `× 36` |
| `FGLSavedPiece.Origin` | `Player` for every piece in `BuildPieces` (all v2 pieces were player-built) |
| `FGLSavedPiece.Layers` | v0 pieces are finished era pieces (`buildpiece.modern.timber_wall`...). A data table (`Data/_registry/buildpiece-migrations.json`) maps each v0 id to a v1 frame form plus a finish, or marks it `complete-as-framed`. An unmapped id fails the validator, never the load. |
| `FGLSavedPiece.Contents` (storage) | empty |
| `FGLSavedCell.Claims` | none: a v2 world has no base core; the player places one |
| player debris and in-flight collapses | the existing `StructureParts` / `Collapses` records gain a piece-id form for player-origin pieces |
| the legacy top-level `BuildPieces` (v1 → v2 already migrates) | same treatment |
| inventory | no change (weight never saved); restore stops discarding overflow |

- `CurrentVersion` becomes 3. The migration lives in `GLSaveCodec::FromJson`, with v2 fixture round-trip tests.
- Derived values are still never saved.

## 14. P11 proposal
### 14.1 The WINCHESTER cabin fixture (R)
A real in-game, player-built cabin, built by a scripted proof (`gl.Building.Proof`, dev only) in a player base on the
home cell. It is an architectural proof, with no production art.
1. **Base:**
   - the player places a **base core** (a claim) and **two storage crates**, holding the cabin's materials (planks,
     studs, cut stone, a Roman column, finishes);
   - Zenny's inventory holds less than 20% of what the cabin consumes.
2. **Frame:**
   - timber foundations for two rooms (6×4 and 4×4);
   - **modern stud-frame walls** on room A;
   - **log walls** (complete-as-framed) on room B, the log-cabin wing;
   - a doorway between the rooms.
3. **Bay (if §11 is approved):** a 45° three-sided bay on room A's front, built from two angle posts and three
   short stud walls. Its preview is GREEN or YELLOW per §5, and the OBB overlap lets it touch the main wall.
4. **Porch:**
   - a deck on the ground, and a porch roof resting on **a Roman column** (masonry) and **a timber post**,
     linked to room A's wall;
   - the roof's preview shows its colour, and the column's and post's removal previews each show what would fall.
5. **Finish:** a **Victorian clapboard finish** on room A's stud walls.
   - The frame is visible before the finish and clad after it.
   - Room B stays log, so at least three vocabularies are mixed (modern frame, Victorian finish, log, Roman
     column).
6. **Careful salvage:** dismantle one interior stud wall that nothing depends on. It returns **intact studs**
   (plus a little scrap) into Zenny's inventory, overflowing to storage.
7. **Collapse:**
   - remove the porch's timber post: the roof stands, through the column and the wall link;
   - remove the Roman column: the preview predicts the roof falls, it collapses by the canonical rules (impact at
     impact time), and becomes debris;
   - salvaging the debris returns predominantly **scrap**.
8. **Persistence:**
   - quit and restart at three points (frame only; finished; mid-fall of the porch roof);
   - stream the cell out and in at the same points;
   - every piece, layer, yaw, container content, claim, debris and in-flight fall comes back exactly once.
9. **Ownership:** every cabin fact is `Origin = Player` and inside the claim. `MayRegenerate` is false for all of
   them.

### 14.2 Acceptance criteria (S)
1. The fixture above runs end to end in the real game, and its log proves each step.
2. **PREVIEW == RESULT:** every placement's predicted colour and reason equals the commit result, and every removal's
   predicted fallen set equals the actual collapse set, across the fixture and the automated cases (zero
   mismatches).
3. **Phases:** a frame is placed and presented as a frame; a finish installs only on an accepted form after its
   required phases; the finish changes presentation and salvage, never support; `phase.electrical` content is
   refused (PH-2) while the registry already orders it.
4. **Player collapse** goes through `GLCollapseRules` with debris, impact-time evaluation, the dormant freeze and
   saves, the same as authored structures. The v0 full-refund demolition no longer exists.
5. **Salvage paths:**
   - careful > destructive > collapse in intact components, for the same piece;
   - debris uses the collapse path;
   - yields pass world settings;
   - nothing is lost when full: the action refuses.
6. **Inventory:**
   - no weight anywhere (code, schema, data, events);
   - the stack and slot limits hold;
   - a restore keeps everything;
   - the retargeted exchanges fire on `InventoryFull`.
7. **Storage:**
   - consumption only inside the claim and only from player containers in that claim;
   - deterministic order, all-or-nothing;
   - a falling container leaves the source list at support failure;
   - collapse keeps contents in the debris.
8. **Ownership:** Origin and claims are saved; `MayRegenerate` is false for them; the architecture test guards
   future regenerators.
9. **Yaw (if approved):**
   - `YawStep` exact;
   - OBB overlap, landing and impacts;
   - socket facings;
   - the plan re-instantiation test (§12) passes at 45°.
10. **Basement test** (§10) passes.
11. **Save v3:** v2 saves migrate (including v0 pieces through the migration table); round-trip tests; derived
    values are never saved.
12. **Regression:**
    - the P9/P10 proof routes, the P8/P9/P10/terrain planted suites and the full gate all pass;
    - every P5/P8 budget holds;
    - the reversal worst-frame debt is not made worse (§15);
    - the fresh clone passes.

### 14.3 Planted-defect plan (T); every one must be caught by an assertion
| # | Defect | Caught by |
|---|---|---|
| 1 | the preview computes support with a different function or stale facts | the equality gate |
| 2 | YELLOW threshold off by one step (uses `strength` instead of the step) | the colour table test |
| 3 | the removal preview misses a transitive dependent | the fallen-set equality |
| 4 | player removal still instantly refunds (v0 path) | the collapse test |
| 5 | player collapse decides victims at support failure, not impact | the ADR-0038 cases on player pieces |
| 6 | a dormant cell's player fall keeps advancing | the freeze test |
| 7 | a mid-fall save of a player fall replays or loses the impact | the persistence test |
| 8 | a finish changes support | the phase test |
| 9 | a finish installs on a bare form without its required phase, or on a form that does not accept it | the phase test |
| 10 | `electrical.*` content is accepted | PH-2 |
| 11 | a save drops a layer | the round trip |
| 12 | debris salvages by the careful path | the salvage path test |
| 13 | destructive equals careful | the salvage path test |
| 14 | a yield bypasses world settings | E-1 |
| 15 | salvage overflow is silently lost | the full-inventory test |
| 16 | weight still slows Zenny, or the item schema still accepts `weight` | inventory and validator tests |
| 17 | a restore discards overflow | the restore test |
| 18 | storage outside the claim is consumed | the storage test |
| 19 | a non-player container is consumed | the storage test |
| 20 | partial consumption on a failed transaction | the storage test |
| 21 | the consumption order depends on iteration order | the determinism test (shuffled ids) |
| 22 | a falling container still supplies materials | the storage test |
| 23 | a collapsed container's contents vanish or duplicate | the storage test |
| 24 | a claim is not saved | the round trip |
| 25 | `MayRegenerate` ignores Origin | the ownership test plus the architecture test |
| 26 | OBB overlap falls back to AABB (refuses the bay) | the yaw test |
| 27 | socket facing is ignored (links at any angle) | the yaw test |
| 28 | the yaw is rounded to quarters on save or load | the round trip |
| 29 | the v2 migration loses a v0 piece | the migration test |
| 30 | a plan re-instantiated at 45° differs in support | the plan test |

### 14.4 Performance and memory expectations (U)
- **Budgets unchanged:** P5, P8, and the 40 ms reversal.
- **New measurements (reported, and budgeted in P11 from evidence):**
  - placement check and preview: ≤ 1 ms on a 500-piece cell, recomputed only when the candidate changes;
  - removal preview: the same;
  - player collapse planning: on the P6 scale;
  - storage source gathering: O(containers in the claim).
- **Player pieces restore through the multi-frame presentation pump.** That retires the synchronous-restore debt
  from P8.
- Player pieces are runtime-layer actors retired by the Grid pump. They are not level-instance actors, so they do not
  feed engine level removal.
- A dense player-built fixture (≥ 300 pieces near the crossing route) is added to the perf suites.
- Memory: facts are a few dozen bytes per piece; the frame and finish proxies use existing LOD budgets.

### 14.5 Out of scope for P11 (V)
- **Construction:**
  - electrical (beyond the registered phase);
  - Ofi plans, ghosts and player-saved plans (beyond the §12 test);
  - the stud finder;
  - curved pieces, free arbitrary angles, structural roof pitch.
- **Structural rules:**
  - load or impact between parts;
  - digging under a structure collapsing it;
  - per-piece health and weather.
- **World and storage:**
  - claiming authored buildings;
  - regeneration itself;
  - logistics and storage rules beyond §8;
  - caves, tunnels and overhangs.
- **Out-of-scope systems:**
  - production art for frames and finishes;
  - building mastery (skills);
  - NICE build-quality commentary;
  - hunger and the comfort radius (which will reuse the claim).
- **Tuning:** balance of costs, yields, stack sizes and the claim radius (all provisional).

## 15. Inherited reversal-frame debt and Building v1
See [Evidence/Pre-P11-design-gate](Evidence/Pre-P11-design-gate/README.md).
- **Cause:** the spike is the **full GC that UE forces after a streamed level is removed** (28 ms of a 41 ms frame).
  It is not rendering and not our streaming.
- **Conditions:** a quiet run today measures 36.0 ms, and the P5 build measures 34.6 ms (34.1 at P5). There is no
  code drift. GC duration varies by about 10 ms.
- **Does not block P11.**
- **Building v1 adds UObjects** (pieces are actors), so the dense player-built fixture must be measured in the
  reversal suite. The budget is unchanged.
- **Proposed before P11:** fix the harness's one-frame-late hitch attribution (a measurement fix).

## 16. Validator and test additions (summary)
- **New validator rules:**
  - PH-1: a form's `layers` reference registered phases, in order;
  - PH-2: no content for an unimplemented phase;
  - FIN-1: a finish's `fitsRoles` are known roles;
  - SV-Q1: per-path yields where collapse is possible;
  - CLM-1: a base core is buildable and unique per claim;
  - YAW-1: yaw is a multiple of 2.5°;
  - SOCK-1: a facing is a multiple of 2.5°;
  - MIG-1: every v0 buildpiece id is mapped.
- **Required tests:** added for every invariant above (`Tools/required-tests.txt`).

## 17. Operator decisions (resolved 2026-10-03)
1. **Fine yaw: APPROVED.**
   - Yaw is an integer in 2.5° steps, with oriented bounds and socket facings. This is the canonical Building v1
     representation.
   - **Not** freeform geometry.
   - REAL HOUSE-0 includes the 45° bay. The whole lifecycle must be proven at angle: placement, snapping, support,
     preview, overlap, terrain footprint, save and restart, streaming, collapse, impact volume, salvage.
2. **YELLOW = at the material's limit: APPROVED.**
   - **No lateral variant in P11** unless a test shows the three states mislead without it.
   - The removal preview is **APPROVED**, with PREVIEW == REALITY from the same rules.
3. **Base storage before personal inventory: APPROVED.** Consumption is deterministic and all-or-nothing, and the
   container order is documented.
4. **Claims: one base core, a 32 m radius, no overlap — P11 BEHAVIOUR ONLY, not an invariant.**
   - The architecture must allow later claim expansion, connected claim areas, multiple cooperating cores and
     estate-scale building.
   - No rule may assume that a building fits inside one 32 m circle.
5. **v0 pieces migrate to FRAME + FINISH: APPROVED.** There is no permanent parallel legacy model. Representative v0
   saves are proven through migration and re-save.
6. **Terraforming under player structures stays refused in P11: APPROVED.** There is no settling, undermining or
   terrain-caused collapse. Basements are dug first and built second.
7. **Overencumbrance dialogue retargeted to "inventory full": APPROVED.** The original wording is kept in notes.
8. **The perf harness attribution fix: APPROVED** before P11 measurements (tooling only).
