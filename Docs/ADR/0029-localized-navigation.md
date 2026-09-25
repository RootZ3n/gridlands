# ADR-0029: Navigation is built only around navigation invokers

- Status: **Accepted for P5** on measured evidence (below). Radii are engineering values, not
  feel, and are open to revision once creatures have larger reaches.
- Date: 2026-09-25
- Evidence: [Docs/Evidence/P5-seamless-grid](../Evidence/P5-seamless-grid/README.md), §F
- Builds on: [ADR-0022](0022-terrain-chunked-heightfield.md) (navigation follows edits, M10) and
  [ADR-0028](0028-seamless-grid-streaming.md) (seamless streaming).

## Context
- **Before P5, navigation covered each cell's whole bounds.** Every loaded cell declared its bounds
  (`AGLCellNavBounds`), and Recast (`RuntimeGeneration=Dynamic`) built every tile inside them.
- **At the canonical 1 km cell that is about 10,800 tiles per cell.** In the real game (1024 m
  harness, same build):
  - a whole-cell build took **7.6 s**;
  - it added about **1.1 GB** of process memory over the localized build.
- **Crossing a boundary queued ~21,600 tile tasks**, which never finished while Zenny walked.
- **Registering a new cell's bounds cost 19 ms** of game thread in one frame (CSV profiler), the
  largest part of the crossing hitch.
- **Most of that navigation is never used.** Nothing paths more than a few tens of metres from
  Zenny or from a creature (data: creature sight ≤ 12 m, hearing ≤ 16 m, leash ≤ 14 m).
  Pehlichi's positioning does not use navigation.

## Decision
1. **Generate only around invokers.** `NavigationSystemV1.bGenerateNavigationOnlyAroundNavigationInvokers=True`,
   with an invoker update every 0.5 s.
2. **Zenny carries an invoker:**
   - generation 64 m (one chunk);
   - removal 96 m, so walking back and forth does not rebuild the same tiles.
3. **Every creature carries its own invoker:**
   - generation 32 m (twice the largest data reach);
   - removal 48 m.
   - A creature therefore paths wherever it is, whether or not the player is near. When it goes,
     its tiles go.
4. **Cells still declare their bounds.** Invokers choose tiles *inside* loaded cells' bounds. When
   a cell unloads, its bounds actor is destroyed and navigation there is removed with it.
5. **Edits still dirty only their chunks** (M10). Where tiles exist they rebuild; where none
   exist there is nothing to rebuild.

## Measured (real game, 1920×1080, RX 6800; details in the evidence)

| | Whole-cell | Localized |
|---|---|---|
| Initial navigation, 1 km cell | 7.64 s | **0.53 s** |
| Active navmesh tiles | 20,581 | **324** (Zenny only) |
| Process memory growth during the nav phase | +1,662 MB | **+509 MB** |
| Navigation update after an edit near Zenny | 42 ms | 39 ms |
| Worst crossing frame (straight / reversal / sprint) | 30.3 / 47.1 / 28.2 ms | **22.9 / 34.1 / 18.0 ms** |
| Tile tasks pending while crossing (peak) | 21,235–21,548 (never caught up) | **8–51** |

## Tests (Gridlands.Game.Navigation, 3, plus the M10 navigation tests)
- **Built only around invokers:** 209 tiles, where the cell would need 10,816.
  - Navigation follows Zenny 300 m and leaves the old place.
  - A mound raised next to Zenny moves the navmesh.
- **Creatures path far from the player:**
  - a creature 420 m from Zenny has navigation and a complete path across its reach;
  - there is no tile in between;
  - its tiles go when it does.
- **Crosses boundaries and unloads cleanly:**
  - a complete path crosses the cell boundary;
  - streaming frame by frame to deep in the second cell unloads the first cell's bounds and
    navigation;
  - returning rebuilds navigation with nothing duplicated.
- **The M10 tests are unchanged in what they prove.** They now register an invoker over their field.

## Limits (named)
- **Something that must path where no invoker is will find no navmesh.** Today nothing does.
  - A future long-range pathing need should get its own invoker, or a query-time build: for
    example an NPC travelling between cells, or a creature summoned far away.
  - That future need is **not** a reason to return to whole-cell navigation.
- **An invoker's tiles take up to 0.5 s plus build time to appear** after a teleport. Zenny's own
  movement is covered by the 64 m radius.
- **With Zenny as the only invoker, a column of about 13 tiles along x ≈ 0 can survive Zenny
  leaving** (`24-diagnostic-*`).
  - The count stays at 173 after revisiting, and no other tiles linger.
  - It looks like an engine tile-removal corner.
  - It is bounded (it does not grow) and goes when its cell unloads (its bounds go).
- **Test worlds never advance world time**, and invoker updates are scheduled on it. The
  navigation tests advance it explicitly.
