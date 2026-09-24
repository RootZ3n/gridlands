# ADR-0010: Grid cells are world-scale regions; zones are cells

- Status: Accepted; **amended by ADR-0011 and ADR-0012**
- Date: 2026-09-24
- Decider: operator

## Context
"Grid" could have been read as a visible grid drawn over every surface. The
operator fixed its meaning: Grid lines define large regions.

## Decision
- Each major gameplay zone occupies one large Grid cell. The planning
  assumption is about 1 km x 1 km, **not final**; density varies by zone
  (residential vs vertical/interior city).
- The player **physically crosses** a world-scale boundary between cells to
  change zone.
- The Grid becomes visible at boundaries, in scans, around glitches, where the
  simulation is damaged, and during corruption events. It never appears as a
  constant overlay on ordinary surfaces.
- Zone membership is spatial (a cell coordinate) plus data (`UGLZoneDefinition`).
  Zone progress is derived from persisted glitch states.

## Deferred
- The streaming technology (World Partition is the likely choice) gets its own
  ADR when a second zone is built. The bootstrap test neighborhood is a
  small slice of one notional cell.
- Boundary behaviours (distortion, seams, locked crossings, hostile
  interference) are designed later.

## Consequences
Persistent ids already work with streamed levels (stable `FGuid`s). Nothing
in the bootstrap assumes a single loaded level for the whole world.

## Reversal cost
Medium-high once content is authored at cell scale; low today.

## Amendment (2026-09-24, ADR-0011, ADR-0012)
- "Zone" is deprecated. A cell is a **place**; depth is a **band**; world
  memory is an **era composition** (ADR-0012). `UGLZoneDefinition` becomes
  `UGLGridCellDefinition {cell, band, eraComposition}`.
- Progression is radial, and **crossings are never locked** (ADR-0011).
  "Locked/stabilized crossings" is removed from the future boundary
  behaviours; interference replaces it.
