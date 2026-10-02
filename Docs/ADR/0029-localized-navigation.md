# ADR-0029: Navigation is built only around navigation invokers

- Status: **CANONICAL: operator decision, 2026-09-25.** Localized navigation is canonical, and
  whole-cell navigation must not be restored.
  - Zenny and creatures keep their invokers.
  - Any future system that genuinely needs long-range autonomous pathing gets an appropriate
    localized invoker.
  - Radii are engineering values, not feel.
  - Guards:
    - `Tools/tests/test_perf_budgets.py` fails if the config turns invokers off;
    - the navigation budgets are in `Tools/perf/budgets.json`;
    - the navigation tests fail under whole-cell navigation (planted defect 23).
- Date: 2026-09-25
- **Amended 2026-09-27 (P9, operator decision M1): NAVIGATION EXISTS WHERE ACTIVE GAMEPLAY REQUIRES IT.** See
  [Amendment (P9)](#amendment-p9-navigation-exists-where-active-gameplay-requires-it) below; decision 3 is replaced.
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
3. ~~**Every creature carries its own invoker:**~~ *(replaced by the P9 amendment below)*
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
- ~~**With Zenny as the only invoker, a column of about 13 tiles along x ≈ 0 can survive Zenny
  leaving** (`24-diagnostic-*`).~~ **Root cause found and fixed in P9** (below): a build task finishing
  after its tile was removed puts the tile back outside the active set.
  - The count stays at 173 after revisiting, and no other tiles linger.
  - It looks like an engine tile-removal corner.
  - It is bounded (it does not grow) and goes when its cell unloads (its bounds go).
  - **Accepted by the operator (2026-09-25).** Documented; do not destabilise the system trying to
    eliminate it now.
- **Test worlds never advance world time**, and invoker updates are scheduled on it. The
  navigation tests advance it explicitly.

## Amendment (P9): navigation exists where active gameplay requires it
**Operator decision M1, 2026-09-27.** Localized navigation stays canonical. The 600-tile budget is unchanged,
nothing is whole-cell, and nothing is permanent.

**1. A creature carries an invoker only while active gameplay needs one.**
- The rule is `GLCreatureRules::NeedsNavigation`: navigation is needed only in behaviour that can move the
  creature (patrolling, investigating, chasing, attacking, searching, returning).
- An idle creature at home, a defeated one and a neutralized one pay nothing.
- The invoker switches on and off with the creature's state (`AGLCreature::UpdateNavigationNeed`).
- **Nothing is lost for an idle creature.** Anything that can make it active is near Zenny (sight
  12–14 m, hearing up to 18 m from a noise), so Zenny's own 64 m circle already covers it.

**2. Authored navigation regions are canonical for bounded encounter spaces** (dungeon interiors, arenas).
- A `navregion.*` (placement kind `nav_region`) declares the playable volume.
- It provides navigation while Zenny is relevant (inside, or within its margin, default 32 m), or while
  gameplay inside needs it (an active creature asked within the last second).
- Creatures inside an active region carry no invoker of their own, so its cost is its **area**, not its
  creature count.
- It is one invoker sized to the region's footprint, and it goes with its cell.

**3. A stale-tile sweep** (`UGLNavRegionSubsystem`, every 0.5 s of the streaming step).
- Recast removes a tile that leaves the active set. A build task still in flight then finishes and puts
  it back, outside every invoker. Nothing removed it again: this was the "column" accepted above.
- The sweep tracks tiles that left the set for 10 s and removes any that come back.
- Measured before the fix: 16–17 stale tiles after a 300 m walk. After: 0, with 15 swept.
- `Gridlands.Game.Navigation` now asserts that no built tile lies outside the active set.

**Evidence** (automation world, supporting only; the real-game measurements are in
[P9 evidence](../Evidence/P9-encounter-foundation/README.md)):

| Layout | Tiles added |
|---|---|
| 1 creature far from Zenny | +47 |
| 8 creatures in a 40 m room | +124 |
| 16 creatures in the same room | +124 |
| 8 spread 120 m apart | +379 |
| a room region + 8 creatures with no invoker | +75 |

**What this shows:** cost follows the union area of the invoker circles, not the creature count. What didn't
scale was spread-out idle creatures each paying for a full circle; this amendment removes that.
