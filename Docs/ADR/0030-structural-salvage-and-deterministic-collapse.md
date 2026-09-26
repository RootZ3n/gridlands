# ADR-0030: Authored structures are a data part graph; collapse is deterministic gameplay

- Status: **Accepted: operator decisions, 2026-09-25 (P6).**
  - 1-A: a data part graph.
  - 2-A: deterministic gameplay rules decide collapse.
  - 3: player building keeps its current behaviour for P6.
- Evidence: [Docs/Evidence/P6-structural-salvage](../Evidence/P6-structural-salvage/README.md)
- Builds on:
  - [ADR-0024](0024-building-v0-structural-model.md): sockets and support;
  - [ADR-0026](0026-grid-cells-and-streaming.md) and [ADR-0028](0028-seamless-grid-streaming.md): cells and streaming;
  - [ADR-0029](0029-localized-navigation.md): localized navigation.

## Context
Richer buildings and world assets are about to be authored. Before that, their structural
representation must be correct:
- which parts hold which;
- what falls when a support is salvaged;
- what the fall hits;
- that all of it persists through streaming, saves and restarts.

## Decision

### 1. The canonical contract is a data part graph (`structure.*`)
- **A structure is a list of parts.** Each part has:
  - a name (saves refer to it);
  - a `piece`, a `buildpiece.*` in the **same structural language as player building**, carrying
    shapes, sockets and material;
  - a structure-local location and quarter-turn yaw;
  - a `salvage` definition (the ordinary salvage pipeline);
  - optionally, a collapse override.
- **Support is ADR-0024's**, derived and never saved (`GLStructureRules::ComputeSupport`). Authored
  structures and player construction therefore speak one structural language.
- **World-only pieces** (`"buildable": false`, BLD-5) are the same kind: posts, deck slabs, tree
  stumps and trunks. They are never offered to the player.
- **Trees are structures** (a stump and a trunk). Felling is salvaging the stump, and the trunk
  collapses through the same pipeline.
- **This data is the runtime and persistence contract, not the authoring workflow.** Future
  Unreal-editor tooling will assemble modular pieces visually, validate sockets, support and
  material, and export this same data. Runtime behaviour, persistence and the save format will not
  change (see Future decisions).
- **Chaos Geometry Collections are not structural gameplay authority.**

### 2. Collapse is decided by deterministic rules (`GLCollapseRules`, pure Core code)
The rules take the intact parts, the removed part(s), a ground function and `tuning.world.physical`,
and decide everything gameplay needs, up front:
- **What falls.** Everything whose support is ≤ 0 after the removal. It cascades: whatever drew
  its support through the removed part falls with it.
- **When it moves:** after `startDelaySeconds`, a hook for truthful creak/warning telegraphs later.
- **How it moves**, from data (`collapse.motion`):
  - `drop`: straight down onto what is beneath it (ground, standing parts, or debris that fell
    first; lowest first);
  - `topple`: it first comes down onto the surface beneath it, then tips about its base edge. The
    tilt integrates a rod pivoting at its base, deterministically.
  - More motions can be added as data without changing structures.
- **Which way a topple goes** is a replaceable policy (`collapse.direction`):
  `awayFromInstigator` (the provisional default), `pieceForward` or `pieceBackward`. Cut direction,
  slope, lean or impact can replace it later.
- **The rest transform** is the persistent debris.
- **Impact time and volume.** The volume is an oriented box derived from the same geometry:
  - for a drop: the footprint from landing up to where the part was;
  - for a topple: the lying footprint.
- **Damage:** `damageBase + damagePerMetreFallen × fall`, times the material's `impactScale`,
  capped. All provisional data.

**The runtime** (`UGLStructureSubsystem`):
- It **commits the outcome at the moment of decision**: part state `Debris` plus the rest transform.
- **At impact time** it damages every pawn with health whose capsule touches the volume, once,
  through the normal health system. The instigator is whoever salvaged the support, so kills and
  drops behave like any other.
- It emits `Noise.Structure.Collapse`, and the debris becomes solid and salvageable (provisionally,
  with the part's own salvage).
- **Presentation follows.** The part's pose is `GLCollapseRules::Motion(outcome, t)`.
- **Physics never produces a different answer.** `AGLStructurePart::OnPresentationImpact` is the
  seam for later cosmetic physics (dust, chips, boards, rubble). It cannot change damage, support,
  salvage, persistence, navigation or the outcome.

### 3. Persistence and streaming
**Facts per part.** `FGLSavedCell.StructureParts` holds `{placement, part, state, rest transform}`,
and only for parts that are no longer intact.
- **Save format:** still v2. The field is optional, so older files load unchanged.

**Streaming and restart:**
- A structure streams with its cell; debris belongs to its structure's cell.
- A cell that unloads mid-fall drops the unfinished collapse. Its outcome is already saved, so the
  collapse is never replayed and its damage never lands twice.
- **Loading restores silently:** no collapse, no damage, no noise, no events.
- **If data changed** so that an intact part has no support after loading, that part settles as
  debris silently and a load problem is reported.

### 4. Coexistence
- **Terrain.** Digging under an intact grounded authored part, or under debris, is refused, as for
  player pieces.
- **Player building.** Player pieces cannot overlap structure parts or debris.
- **Navigation.** Parts and debris are solid actors that navigation reads (localized, ADR-0029). A
  falling part is non-solid until it rests.
- **Player-built pieces keep ADR-0024 unchanged**: demolition collapse refunds in full, and digging
  under them is refused.

## Future decisions (recorded, not taken)
- **Player-built physical collapse.** Should player structures adopt this physical-collapse model
  (debris and damage instead of full refunds)? The operator wants to decide this later, with
  gameplay and economy evidence. P6 deliberately leaves player building unchanged.
- **Editor authoring and export.** Many authored buildings, dungeons and architectural traditions
  must not be written part by part forever. The desired workflow:
  1. assemble modular structural pieces visually in Unreal;
  2. validate sockets, support and material metadata;
  3. export canonical `structure.*` data;
  4. the runtime consumes the same deterministic graph;
  5. saves persist facts against it.
- **Structures on authored geometry (dungeons, interiors): DECIDED by the operator (2026-09-25).**
  **Exported/authored floor-support data is the authoritative structural ground source.** Runtime
  level-collision queries are never structural truth.
  - The intended workflow:
    1. authored level geometry;
    2. editor/export validation;
    3. a deterministic floor/support representation;
    4. canonical structural data and runtime;
    5. persistence against that representation.
  - Runtime collision may later serve validation, movement, presentation and suitable physical
    queries. Structural support must never change because streamed collision is temporarily absent.
  - Not built yet. Terrain remains the ground source for outdoor structures.
- **Structure-on-structure impact** (falling debris breaking other parts) and **load** (weight)
  are not modelled. Cascades come from support alone.

## Recorded debt (operator, 2026-09-25)
- **GAMEPLAY CONSISTENCY DEBT: saving or unloading mid-fall.** The collapse outcome persists, but
  impact damage that has not happened yet is never applied afterwards. That is a future save/reload
  exploit: a player could avoid lethal collapse damage. **It must be resolved before structural
  collapse is production-complete.** Not addressed until a milestone touches this path.
- **Player-built physical collapse: deferred** (a future operator decision; see ADR-0024).
- **Debris impact and load propagation between parts: deferred** until there is a measured design
  need.
- **`SpawnProofCreature`: accepted** as non-shipping, proof/dev-only, non-persistent and
  cell-scoped. No permanent shipping creatures are added to support acceptance harnesses.

## Consequences
- **One structural language** for world and player, so tools, validation and tests serve both.
- **Deterministic, persistent and stream-safe.** Tests can assert exact outcomes, and a
  telegraph can be drawn from the same impact volume.
- **Visual quality is a presentation problem on top.** It is never a reason to let physics decide.
