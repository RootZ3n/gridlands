# ADR-0026: Grid cells are streamed by a Grid layer; authored geometry uses level streaming

- Status: **Accepted for P3 as an architecture proof.** The cell size is data and is **not** decided.
- Date: 2026-09-25
- Evidence: [Docs/Evidence/P3-grid-cells](../Evidence/P3-grid-cells/README.md). Scaling data:
  [P2](../Evidence/P2-terrain-scale/README.md).

## Context
The operator asked for two physical cells, explicit cell identity, per-cell placement data,
physical crossing, and streaming that loads and unloads correctly. State must persist across
unloads and saves, with no duplication and no replayed events.

The first question was whether Unreal's **World Partition** can do that job.

## Finding: World Partition cannot stream a Grid cell on its own
World Partition streams *actors placed in the editor* (one file per actor, grid cells computed by
the editor). Almost everything that makes a Grid cell a Grid cell is **spawned at runtime from
data**:
- the heightfield ground (ADR-0022);
- placements: glitches, salvage, creatures, puzzle sites (ADR-0018);
- the player's build pieces;
- the cell's state.

World Partition never unloads runtime-spawned actors. So a Grid layer is needed whether or not
World Partition is used.

## Decision

**Cell identity.** A cell is a `cell.*` entity with a `coord` and a `sizeMetres` pitch:
- GRID-1: one pitch for every cell. GRID-2: coords are unique. GRID-3: terrain tiles the cell
  exactly.
- A cell is centred on `coord × pitch`.
- Placement coordinates are cell-local, and placement ids name their cell (ID-10).

**`UGLGridSubsystem` streams cells around Zenny.** It loads within 48 m of a cell and unloads
beyond 96 m (hysteresis).

Loading a cell does the following:
1. Loads its authored level (`cell.level`) as a **level instance** at its offset. There is one
   streaming level per cell for the whole session, re-requested on return.
2. Builds its ground **with its saved edits already applied**.
3. Spawns its placements.
4. Applies its kept state silently.

Unloading stows the cell's state first, then removes its pieces, placements, ground and level.

Authored geometry uses ordinary engine level streaming. World Partition remains an option for
streaming authored detail *inside* a cell later. That is not decided here.

**Save format v2.**
- **Per cell:** one record each, holding glitches, salvage, defeated creatures, pieces and the
  ground delta.
- **Global state stays top-level:** inventory, knowledge, Pehlichi, dialogue history, puzzles,
  storms, discoveries, Zenny.
- **Unloaded cells:** their records stay dormant until the cell streams in.
- **Migration:** v1 files migrate into one record.
- **Silence:** restores emit no events, so a returning cell never replays dialogue.

**Seams.**
- Neighbouring cells share their edge vertices, and relief flattens to the base height near
  every edge.
- An edit that crosses an edge changes both cells atomically.
- An edit that reaches into a cell that isn't loaded is refused.

**Static and depth are separate data.**
- Band: depth and baseline.
- Cell `interferenceOffset`: local static intensity.
- Cell `eraComposition`: memory.
- Glitch states: stability, which is derived.
- NICE's composure counts the whole Grid, including streamed-out cells, using their kept state.

**Temporary development aids.** Cyan boundary posts and a HUD cell line. Neither is Grid art.

## Known limits (named, not hidden)
- Loading and unloading are synchronous: about 190 ms per 256 m cell here. Seamless streaming
  needs amortised chunk builds (P2: about 11 ms per chunk) and asynchronous levels.
- Only cells within a margin load. A glitch's influence can reach past an unloaded neighbour's
  edge. The margins cover the current radii of 60 m or less.
- The proof uses 256 m cells because they are cheap. A 1 km pitch is a data change, subject to
  P2's memory and load-time limits.

## Reversal cost
Moderate.
- The per-cell save format and the Grid layer are the backbone, but they are small and tested.
- Adopting World Partition for authored geometry later would not undo them.
