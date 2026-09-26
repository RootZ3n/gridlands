# Structure authoring: what the future editor/export tool must emit (P7-J)

Status: **requirements from the P7 proof. The tool is NOT built.**
Contract: [ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md) (`structure.*`,
`buildpiece.*`) and [ADR-0032](ADR/0032-visual-pipeline-and-stylization.md) (`visual.*`).

## The intended workflow (operator)
1. A designer assembles modular pieces visually in Unreal.
2. Sockets, support and material metadata are validated.
3. The tool exports canonical structure data.
4. The runtime consumes the same deterministic graph.
5. Saves persist structural facts against it.

## What P7 proved with the 1950s storefront (`structure.fifties.storefront`)
**The build:** 16 modular kit parts form a recognisable, stylized diner front in the shared
structural language. The parts are:
- 2 tiled foundations and 6 walls (window, door, plain);
- 2 roof slabs and 2 awning slabs;
- 3 posts and a neon sign.

**The visuals** are authored meshes over authoritative data boxes. The walls' trims overhang their
0.2 m data thickness, and the sign's star sticks out. No seams or proportions forced by the contract
were visible.

**It works in play:**
- the awning hangs on the three posts and laterally on the roof slabs;
- taking the posts brings it down deterministically, in the styled scene;
- the debris persists.

**Found and fixed:** a falling slab was held by a 10 cm sliver of overlap with a wall top. It now
needs its centre of mass over a surface to land on it (a core test covers this).

**Conclusion: the contract does not prevent visual authoring.** It has the limits below, which a
tool must respect or which a later decision could relax.

## What the exporter must emit
**1. Pieces (`buildpiece.*`), per modular piece, once:**
- `size`: the axis-aligned support/collision bounds (metres; origin at the bottom centre).
- `shapes`: collision boxes.
- `sockets`:
  - `bottom` at z = 0, `top`, `side`;
  - unique names;
  - positions inside the bounds (BLD-2..4).
- `material`: a structural material, which gives strength, stack and span.
- `grounded`, `role`, `era`, and `buildable` (false for world-only kit pieces, BLD-5).
- `collapse`: motion and direction policy.
- `visual`: the art mesh.

The visual mesh may overhang `size` for trim. The data boxes stay authoritative.

**2. Structures (`structure.*`), per assembled building:**
- **Parts:** each has a unique name (STR-1), a piece, a location (metres, structure-local, bottom
  centre), `yawQuarter` (0–3), a salvage definition, and an optional collapse override.
- **Grounding:** at least one grounded part at z = 0 (STR-2).

**3. Placement (`placement` of kind `structure`):**
- a cell-local transform;
- yaw in quarter turns (STR-3).

**4. Floor support for interiors and dungeons** (operator decision, ADR-0030):
- **Exported floor-support data** is the authoritative ground for structures on authored geometry.
- **Runtime collision is not.**
- **Not built.** The exporter will need to emit it next to the structure.

## What the exporter must validate (before writing anything)
- **Socket coincidence:** every resting and lateral link the designer intends has sockets within
  5 cm (`GLStructureRules::SocketToleranceCm`).
- **Support on the reference ground:** every part has support > 0.
  - Outdoors, the reference is the target cell's terrain.
  - Indoors, it is the exported floor-support data.
  - Runtime tests assert the same thing (`EveryAuthoredStructureStandsOnItsGround`).
- **Grounded parts rest on the ground:** within 30 cm (`GroundToleranceCm`).
- **The data validator passes:** STR, BLD, VIS and ID rules.

## Limits found (named, not hidden)
- **Quarter-turn yaw and axis-aligned support boxes.** Support geometry is boxes in 90° steps.
  - Round, diagonal or angled architecture (round towers, carousels, pagoda roofs, angled
    Victorian bays) is represented by approximating support parts with art meshes of any shape on
    top.
  - This worked for the storefront. It will make some structural outcomes coarse, for example how
    a round tower falls.
  - **Relaxing it** means oriented boxes in `GLStructureRules`, with bounds no longer exact. That
    is a future decision point, not required by P7.
- **No load or impact between parts** (deferred by the operator). A collapse does not break the
  parts it lands on.
- **Uniform module grid.** The kit uses 2 m modules and 2.5 m walls, the player building's grid, so
  a player can build beside authored buildings.
  - Other eras can use other dimensions freely: sockets are positions, not a grid.
  - For mixed-era player building, a shared module helps.
