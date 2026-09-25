# P3 evidence: two Grid cells, streaming and persistence

- Verified commit: `e9f2a17f28a592ada8edb488d5ede8d64e82ab9c`.
- Engine: Unreal 5.8.3.
- Decision: [ADR-0026](../../ADR/0026-grid-cells-and-streaming.md).
- Proof configuration: two 256 m cells (a data value, not a size decision).
  - `cell.home.origin`: band depth 0, modern.
  - `cell.outer.diner_lots`: depth 1, +0.15 local static, mostly 1950s, rolling relief.

| File | What it proves | Verdict |
|---|---|---|
| `00-*` | Fresh clone: 147 entities valid, build, **83/83** automation tests, 0 warnings. | PASS |
| `01-real-game-torture-walk.log-excerpt.txt` | **Real game, with level instances.** See the torture walk below. | PASS |
| `02-session2-restore-in-second-cell.log-excerpt.txt` | **A fresh launch:** "2 cell records (0 loaded now, the rest when they stream in)". Zenny wakes in the lots, which stream in with the jukebox repaired and 13 edited vertices. Walking home, the origin streams in with its 2 pieces and the mound. | PASS |
| `03-grid-boundary-marker.jpg` | The temporary cyan boundary posts, the second cell's hills beyond, and the HUD "Cell:" line. Development markers, not art. | — |
| `05-mutation-unload-without-stowing.*` | Not keeping a cell's state on unload fails all three Grid tests: the lamp, blocker and gremlin revert, and NICE forgets the repair. | rejected |
| `06-mutation-despawn-leaves-creatures.*` | Leaving creatures behind on despawn fails the torture test: 1, 2, 3 gremlins accumulate. | rejected |

**The torture walk** (`01-*`):
1. Build a floor and a wall 2 m from the boundary, and raise a mound across the cell edge.
2. Repair the lots' jukebox.
3. Cross between the cells 7 times, reporting after every step. On every return, each cell's
   authored level actors are back (53 and 10) and are gone (0) while it is out.
4. Throughout: the lamp, jukebox, pieces and mound are identical every time; glitch actors stay
   at 5 or 1 and creatures at 1 or 0 (no duplicates); every dialogue line plays exactly once.
5. Save while standing in the lots, with the origin streamed out.

## The operator's falsification list

| Attempt | Result | Where |
|---|---|---|
| Cross the boundary repeatedly | 5 round trips (automated) and 7 crossings (real game). No loss, duplication or events. | `StreamingNeverLosesDuplicatesOrReplays`, row 01 |
| Modify A, visit B, return to A | The lamp repair, salvage, defeated gremlin, pieces and mound all return. | same |
| Modify B, unload it, return | The jukebox repair and the hole return. | same |
| Save in B and restart | Wakes in B with B's state; A restores on walking home. | `SaveInTheSecondCellAndRestart`, row 02 |
| Repair glitches before and after streaming | Before (lamp) and after (jukebox): both persist. | same |
| Pieces near a boundary | A floor and wall 2 m from the edge belong to their cell and return exactly once. | same |
| Terraform near and across a boundary | Both cells change atomically, the edge vertices agree (no seam), and both halves return. An edit reaching into an unloaded cell is refused and changes nothing. | same |
| Kill an entity and stream it out and back | The gremlin stays defeated; exactly one creature actor. | same |
| Event and dialogue state across reloads | Zero gameplay events and zero dialogue lines from streaming. | same |
| Static and depth per cell | Baseline = band + cell offset (independent data). Static rises crossing into the lots and falls when its glitch is repaired. No lock. NICE's composure counts a streamed-out cell's repair. | `StaticDepthAndEraAreIndependentPerCell` |

## Defects found by trying to break it (fixed, now covered)
- **A quick return lost the cell's geometry.** A fresh level instance with the same name failed
  while the old one was still unloading. Each cell now keeps one streaming level for the session.
- **An edit near an unloaded neighbour changed only this side.** That would have left a seam
  crack when the neighbour loaded. Such edits are now refused (unconditional assertion).
- **Unloaded levels lingered for a moment.** Unloading was asynchronous; it now flushes, like
  loading.
- **The perf finding came in through the per-cell restore.** Restoring saved edits rebuilt every
  chunk (P2: 2.8 s at 1 km). Edits are now applied before the first build, and a restore
  rebuilds only the chunks that change.

## Known limits (see ADR-0026)
- Streaming is synchronous: about 190 ms per 256 m cell here.
- A glitch's influence can reach past an unloaded neighbour's edge, within the margins.
- World Partition is not used for the Grid layer, by design. It is still possible for authored
  detail inside cells.
