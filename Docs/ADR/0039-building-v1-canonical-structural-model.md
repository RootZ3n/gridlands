# ADR-0039: Building v1: player construction in the canonical structural model

- Status: **Accepted** (operator, 2026-10-04: "P11 FINAL REVIEW: APPROVED GREEN"). Built 2026-10-03 under the approved
  pre-P11 design gate ([BUILDING-V1-DESIGN](../BUILDING-V1-DESIGN.md)); revised 2026-10-04 with instanced presentation (§11,
  operator decision). Provisional playtest values, not final balance: 32 personal slots, ordinary construction stacks of
  100, a 32 m claim radius, and the inventory-full dialogue retarget. P11's one core / 32 m / no-overlap claim behaviour is
  implementation behaviour, not a permanent topology restriction.
- Date: 2026-10-03
- Builds on:
  - [ADR-0024](0024-building-v0-structural-model.md): the support rule. **Amended here:** quarter-turn yaw becomes
    integer 2.5 degree steps with oriented bounds; player-built collapse replaces full-refund demolition.
  - [ADR-0030](0030-structural-salvage-and-deterministic-collapse.md) and
    [ADR-0038](0038-structural-environmental-resolution.md): deterministic collapse, impact at impact time, a fall in
    flight as a durable fact. **Extended here** to player construction; debris salvages by the collapse path.
  - [ADR-0033](0033-multi-frame-cell-presentation.md) / [ADR-0036](0036-gameplay-models-lods-production-density.md):
    model first, presentation over frames. **Now also for player pieces** (closes the P8 synchronous-restore debt).
  - [ADR-0016](0016-world-settings-and-yield-categories.md): every yield through its category. **Unchanged.**
- Evidence: [P11 evidence](../Evidence/P11-building-v1/README.md).

## Context
The architecture north star is LOCKED intent: Gridlands is architecture-first. Building v0 (M10) was a prototype: finished
"era + material" pieces, quarter turns, full refunds, carried weight. The operator approved the Building v1 design on
2026-10-03, with: player-built collapse in principle (no "authored physics versus player magic"); a GREEN / YELLOW / RED
preview that is the commit's own rule; no weight-based encumbrance; minimal shared base storage; FRAME -> FINISH with
electrical schema-ready; claims as P11 behaviour, not an invariant; fine yaw; v0 pieces migrated, not kept as a parallel
model; terraforming under player structures still refused.

## Decision

### 1. One structural model, two origins
- Every placed piece is a fact `FGLPlacedPiece {Id, Def, Location, YawStep, Cell, Origin, Layers}`. Support is derived,
  never saved (S-1).
- **Player construction lives in `UGLStructureSubsystem`**, one structure per cell (`player:<cell>`), beside authored
  structures. Collapse planning, impact at impact time, debris, in-flight saves, the dormant freeze, streaming and
  deferred presentation are the same code for both.
- `UGLBuildingSubsystem` keeps the player's verbs as transactions: place (FRAME), install a finish, dismantle (careful),
  smash (destructive).
- **Removing a support collapses what it held by the canonical rules.** v0's full-refund demolition no longer exists.
  Placing a piece can never collapse anything (support is a maximum over links).

### 2. Fine yaw (operator-approved): integer 2.5 degree steps, oriented bounds, socket facings
- `YawStep` in 0..143 (90 = 36, 60 = 24, 45 = 18, 22.5 = 9). Quarter turns rotate bit-exactly, so every pre-P11 result
  is unchanged.
- Every shape question uses the piece's **oriented footprint** (`FGLFootprint`): overlap (separating axes), terrain
  protection, landing surfaces, drop impacts (on the piece's own axes) and topple extents (projected onto them).
- **Socket facings** (optional data, whole steps): two facing side sockets link only when they face each other, and a
  snap between them takes its yaw from the data. Angle posts (45, 60, 22.5 degrees) build bays, octagons, hexagons and
  segmented towers by snapping alone; the polygons close exactly.
- **Not freeform geometry.** No arbitrary angles, no curved pieces, no structural pitch or roll.
- Socket links are spatially hashed: linear in sockets (a 300-piece house is recomputed per preview).

### 3. PREVIEW == REALITY (operator-approved)
- **RED**: the placement check refuses. **YELLOW**: accepted, support at or below one more vertical step of the piece's
  own material. **GREEN**: accepted with more margin. `PreviewOf` is computed from the very check the commit makes.
- **Removal preview**: the pieces that would lose support without the targeted one, from
  `GLStructureRules::CollapsesAfterRemoving`, the function the commit's collapse is equal to. Impact victims are never
  predicted (they are decided at impact).
- No lateral warning variant in P11 (operator); nothing showed the three states mislead without it.

### 4. FRAME -> (optional) ELECTRICAL -> FINISH
- `phase.*` content registers phases in canonical order; electrical is registered **unimplemented** and the validator
  refuses content for it (PH-2). A stud wall already accepts `[electrical, finish]`, so adding electrical needs no schema
  change.
- A placed piece is its FRAME (studs, plates, rafters shown). A **finish** (`finish.*`) is a layer installed afterwards:
  real material, its own salvage, its own look. **A layer never changes support.** Pieces whose form is their finish
  (log walls, Roman columns, floors) accept no layers.
- Appearance = component form + material + finish. Any finish fits any form whose role it lists; there is no
  era-compatibility rule (WINCHESTER).

### 5. Salvage quality by recovery path
- `salvage.*` has `yieldsByPath {careful, destructive, collapse}`. Dismantling pays careful (intact studs), smashing pays
  destructive, debris pays collapse (predominantly scrap), each with the piece's finish layers by the same path, each
  through world settings (E-1). Quality is which items come back (studs vs scrap), never stack metadata. SV-Q1 lints it.
- Raw lumber: a felled trunk gives logs; a sawhorse (`Station.Saw`) saws a log into studs.
- **Nothing is silently destroyed:** a salvage or a removal whose whole result would not fit is refused.

### 6. Inventory: slots and stacks (operator-approved)
- No carried weight anywhere (code, schema, data, events). 32 slots and 100 per stack for ordinary construction
  materials are provisional test values.
- A restore never discards: `FGLInventory::ForceAdd` keeps an over-full save over capacity until space is freed.
- The overencumbrance exchanges are retargeted to `Event.Player.InventoryFull`, the original wording kept in notes.

### 7. Shared base storage and claims
- A **claim** is derived from an intact player-built base core: a set of areas (P11: one area of 32 m). It is not a
  second saved truth. **P11 behaviour, not an invariant:** one core, 32 m, no overlapping claims; the area list leaves
  room for expansion, connected areas, cooperating cores and estates.
- Inside a claim, with Zenny inside it too, an operation's material sources are the claim's eligible storage pieces
  (player-built, intact, storage-capable), **ordered by distance from the operation's point, then piece id, then
  Zenny's inventory**. Delivery fills Zenny first, then storage. Everything is all-or-nothing (`FGLMaterialPool`).
- The player's own verbs on a crate: **store** every carried material (tools stay in hand) and **take** everything,
  each moving only what fits (nothing lost, nothing duplicated).
- A storage piece stands on a floor and can lose its support. A non-empty one refuses careful dismantling; a collapsed
  one keeps its contents in its debris (lootable, never lost, never duplicated); it leaves the sources at support failure.

### 8. Ownership: renewal never touches player construction
- `Origin` is explicit in every saved piece. A player cell's record holds player construction only, so a restored piece is
  always the player's; a different saved origin is a damaged record and is reported as a load problem (never discarded).
- `GLClaimRules::MayRenew(Origin, Location, Claims)` is false for player-built construction anywhere and for anything
  inside a claim. `UGLStructureSubsystem::Renew` (the one renewal primitive, for authored structures) asks it for every
  part.

### 9. Plans (P12 compatibility)
- A plan is ordinary pieces relative to an anchor (`GLPlanRules`); instantiating it gives ordinary placed pieces.
  Rebuilt at 45 degrees elsewhere it has the same relative geometry, socket relationships and support (tested). Never a
  second building representation.

### 10. Save v3
- `FGLSavedPiece {Id, Def, Location, YawStep, Origin, Layers, Contents, State, RestLocation, RestRotation}`; player debris
  and in-flight player collapses go through the authored restore path.
- v2 files migrate deterministically on the JSON: `yawQuarter` q -> `yawStep` 36q, `origin` Player, v0 pieces gain
  their data's `legacyLayers` (the M10 timber wall becomes a stud frame + `finish.modern.timber_board_wall`).
  Inventory, terrain, debris and encounter state are untouched.

### 11. Instanced presentation of player construction (operator decision 2026-10-03, option b)
The candidate review found that unloading a player base made every piece inert in one frame and fed hundreds of actors to
the next GC. The operator chose instanced presentation, keeping **GAMEPLAY MODEL != PRESENTATION**.
- **The model is unchanged.** Every piece stays an individually authoritative record in `UGLStructureSubsystem`:
  identity, ownership, layers, support, preview, removal prediction, salvage, contents, collapse, impact, save and plans.
- **Quiescent intact pieces are drawn and collided by one `AGLPlayerPieceBatch` per player structure (cell).**
  - Instanced static mesh sets keyed only by what instances share: the blockout box's colour, or the authored visual.
  - One hidden collision set holds every piece's shapes. It is the authoritative envelope: blocking, and read by
    navigation.
  - One overlay set draws the removal preview.
  - What a piece shows is decided once, in `GLPiecePresentation::Describe`, which a piece's own actor uses too.
- **Identity.** Every set keeps an owner table (instance index -> piece id), mirrored on every add and removal. Instances
  are removed in order, never swapped. A hit (aim, interaction, a footstep's floor) is translated through it
  (`UGLStructureSubsystem::PlayerPieceAt`). Renderer order is never gameplay identity, and nothing about instances is
  saved: the batch is rebuilt from the model on stream-in and load.
- **Dynamic pieces keep, or acquire, an actor.**
  - Storage keeps its actor (Store / Take interaction).
  - A look with lights or corruption cubes keeps one.
  - A piece whose support fails leaves the batch and falls as an actor along the canonical plan. The impact is decided
    at impact time (P10), and its debris keeps that actor (salvaged by interaction).
  - Debris does not return to the batch (few, interactive).
  - A finish change re-presents the piece (re-instanced). Nothing churns per frame.
- **Stream-out:** the batch is retired whole (hidden, no collision) and destroyed within the presentation budget: one actor
  per cell, whatever the piece count.
- **Retired collision is removed incrementally, on purpose** (operator-approved, 2026-10-04; never reintroduce a
  synchronous teardown for lifecycle neatness: it was a measured streaming regression).
  - A retired batch answers no lookup (`PieceIdAt` returns 0), so it cannot be interacted with.
  - It exists only while its cell is unloaded, which happens only beyond the 384 m unload margin.
  - All of it is removed in budgeted steps.
  - When the same structure returns before that is done (a teleport, a resume), the retiring batch is removed as the new
    one is made, so collision never doubles.
  - Gated by `Gridlands.Game.Grid.RetiredPlayerCollisionIsInertAndAlwaysRemoved`, the density test (several budgeted
    steps) and planted defects P17–P19.
- `-GLActorPieces` (dev only) restores one actor per piece, for same-binary measurement.
- **Not here:** authored structures remain one actor per part (recorded debt with a measured baseline; a candidate
  future optimization, not a P11 requirement).

## Consequences
- Authored and player structures share one structural language in code, not only in data.
- The quarter-turn limitation is gone before content depends on it.
- Player pieces restore over frames; the P8 synchronous-restore debt is closed.
- A player base's presentation costs one actor and a handful of components, not one actor per piece: unloading it no
  longer scales with destroying hundreds of actors in one frame, and the next GC walks far fewer objects (§11).
- The density fixture found two scaling defects before content did (the presentation pump and a per-tuft query).

## Not changed / not built
- Electrical (registered phase only); Ofi plans, ghosts and player-saved plans; the stud finder; curved and freeform
  geometry; load or impact between parts; terrain-caused collapse (digging under player structures stays refused);
  claiming authored buildings; regeneration itself (only its permission rule and primitive exist); logistics or
  storage priorities; production art for frames and finishes; mastery; NICE build-quality commentary.
