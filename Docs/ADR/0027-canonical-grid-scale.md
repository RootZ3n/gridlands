# ADR-0027: Canonical Grid scale: 1 km cells, 1 m terrain, 64 m chunks

- Status: **Accepted: operator decision, 2026-09-25.** Canonical, not a benchmark value.
  **Confirmed after P5 (2026-09-25):** the canonical cell is **1024 m × 1024 m**, with 1 m spacing
  and 64 m chunks. "1 km" is human-facing shorthand. Changing any of these needs operator approval.
- Evidence: [P2 terrain scaling](../Evidence/P2-terrain-scale/README.md). Streaming:
  [ADR-0026](0026-grid-cells-and-streaming.md) and [ADR-0028](0028-seamless-grid-streaming.md).

## Decision

| | Canonical value |
|---|---|
| Grid cell | **1 km × 1 km**, implemented as a **1024 m** edge: 16 chunks of 64 m, which is the configuration P2 measured |
| Terrain vertex spacing | **1 m** (1025 × 1025 vertices per cell) |
| Terrain chunk | **64 m** (65 × 65 vertices; 256 chunks per cell) |

**Why the space matters.** The player needs substantial room inside each cell, because a
neighbouring cell's era and content begin to bleed in *before* the boundary (see
[WORLD-AND-PROGRESSION §7a](../WORLD-AND-PROGRESSION.md)). A boundary must not feel like an
abrupt biome switch.

**Rule.** Cell dimensions and terrain resolution are **not** reduced to solve implementation or
performance problems without a new operator decision. Performance work changes *how* a cell is
built, streamed and navigated, never *what* the Grid is.

**Why 1024 m.** 1000 m does not divide into 64 m chunks. A 1024 m edge keeps every chunk whole and
every cell edge on a chunk seam (GRID-3), and it is exactly what P2 measured. If "1 km" is meant as
exactly 1000 m, the heightfield can support a partial edge chunk; that would be a new decision.

## Consequences (from P2)
- About 0.5 GB of terrain memory per loaded cell.
- Seamless streaming is required (ADR-0028):
  - builds must be incremental and prioritised;
  - authored levels load asynchronously;
  - navigation is localised (ADR-0029), not whole-cell.
- Every cell uses the same pitch (GRID-1), and each cell's terrain tiles it exactly (GRID-3).
