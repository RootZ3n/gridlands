# ADR-0024: Building v0 — sockets and Valheim-like support

- Status: **Accepted for M10 as v0**. The structure of the model is the operator's stated target. The
  numbers are provisional and need an operator playtest.
- Date: 2026-09-25
- Decider: operator (target: Valheim-like structural logic, snap sockets); model and numbers by agent
- Evidence: [Docs/Evidence/M10](../Evidence/M10/README.md)

## Context
The target is Valheim-like structural logic:
- pieces need support;
- support weakens as construction extends out and up;
- materials differ, and better materials allow more ambitious structures.

The earlier decision was snap-socket building, data-driven, with no GAS. M10 must prove the
architecture with a small set: floor, wall, doorway and roof.

## Decision
**Pieces are data** (`buildpiece.*`, metres, piece-local, origin at the bottom centre):
- `role` and `grounded`: whether the piece may rest directly on terrain.
- `size`: axis-aligned bounds.
- `shapes`: visual and collision boxes. `pitch` tilts a box about its local X axis.
- `sockets`, each with a `role`:
  - bottom meets top: the upper piece rests on the lower one;
  - side meets side: a lateral link.
- `cost` and `unlockedBy`.

Validator rules:
- BLD-1: the piece's material must be structural.
- BLD-2: the piece must have a bottom socket at z = 0.
- BLD-3: socket names must be unique.
- BLD-4: sockets must lie inside the piece's bounds.

**Support is derived, never saved (S-1).** It is computed by `GLStructureRules`, which is pure Core code:
- A grounded piece whose bottom sockets all sit within 30 cm of the ground has support equal to its
  material's `strength`.
- Any other piece's support is the maximum, over the pieces it connects to, of
  `min(their support, my strength) − loss`, where:
  - resting on a piece loses `strength / maxStack`;
  - a lateral link loses `strength × metres / maxHorizontalSpan`.
- A piece is stable while its support is above 0.
- Pine (strength 3, maxStack 4, span 3 m) therefore stacks a floor plus three walls, and hangs one
  2 m floor over a drop but not two.
- A stronger material raises both limits, and a piece is never stronger than its own material.

**Transactions.**
- Placement checks, in order:
  1. knowledge (all of `unlockedBy`);
  2. items;
  3. overlap, with bounds shrunk by 6 cm;
  4. burial (a bottom socket more than 30 cm under the ground);
  5. support greater than 0.

  Only then does it pay and spawn; otherwise nothing changes.
- Demolition removes the piece and everything that loses support without it, and refunds the
  **full** cost of all of them. If the refund would not fit, nothing happens. Nothing is silently
  duplicated or lost.

**Snapping.**
- The candidate aligns a compatible socket (bottom onto top, side onto side) to the nearest free
  socket within 1.5 m of the aim point.
- Otherwise, a ground-capable piece sits on the ground at the aim point.
- Yaw moves in quarter turns, so bounds stay exact.

**Terrain.** The ground under a ground-resting piece (its footprint plus 50 cm) cannot be
terraformed. A stroke that touches it is refused as a whole.

## Provisional choices for the operator's playtest
These are data or small constants, cheap to change:
- **Costs:** 2 planks per piece. **Timber sources:** 4 fence panels (3 planks each) and 2 garden
  sheds (12 planks each).
- **Grid and tolerances:** a 2 m grid, 2.5 m walls and a 30 cm ground tolerance.
- **Collapse:** a collapse refunds everything. Valheim-style drops or losses would be a design change.
- **Digging under a structure is refused rather than collapsing it.** Collapsing it instead is
  possible: `CollapsesAfterRemoving` already exists.
- **Terraform:** flatten is free; dig yields 1 soil and raise costs 1 soil per stroke (TF-1 forbids a
  free raise).
- **Bindings:**
  - B: build mode;
  - wheel: choose a piece;
  - Z: rotate;
  - LMB: place or apply a stroke;
  - X: demolish;
  - T: cycle dig / raise / flatten.

## Consequences
- A new piece or material is data only, checked by BLD-1..4. Its support is computed with no new code.
- Pieces block navigation, because their collision is exported, so an AI paths around a built wall.
- Not in v0:
  - non-90° rotation;
  - per-piece health or damage;
  - weather;
  - a crafting station for building;
  - material-specific looks beyond colour.

## Reversal cost
Low to moderate. The formula is one function, and every number is data.

## Future decision point (recorded 2026-09-25, operator)
P6 ([ADR-0030](0030-structural-salvage-and-deterministic-collapse.md)) gives **authored world
structures** deterministic physical collapse: debris, impact damage and persistence. Player-built
structures deliberately keep this ADR's behaviour for now:
- demolition collapse refunds in full;
- digging under a player structure is refused.

**Whether player structures adopt the physical-collapse model is an open operator decision.** It
will be evaluated later with gameplay and economy evidence. P6's scope is not a permanent answer.
