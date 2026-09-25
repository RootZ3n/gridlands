# M10 evidence: building v0, terraforming, and the navigation hard gate

- Verified commit: `9a3166502c72be5bbbc5ad0c4d2f59152e5916de`.
- Engine: Unreal 5.8.3 CL 58210709.
- Decisions: [ADR-0022](../../ADR/0022-terrain-chunked-heightfield.md) (terrain; proof 1 recorded)
  and [ADR-0024](../../ADR/0024-building-v0-structural-model.md) (building v0).

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-9a31665.index.json` | Fresh clone: 74 tooling tests, 112 entities valid, build, **71/71** automation tests, 0 warnings. | PASS |
| `02-real-game-build-terraform-navproof.log-excerpt.txt` | **The real game.** A 16-piece shelter built through the real transaction (all planks spent). Dig and raise conserve soil; digging under the shelter is refused. **Navigation gate in the game world:** the route across the open field runs straight (y = −80 m). A 3.4 m ridge is raised on it at runtime, and the route moves to the gap (y = −59.6 m). An AI walker arrives, crossing the ridge line at y = −59.5 m, with max z 116 cm (it never climbed). | PASS |
| `03-mutation-edits-do-not-update-navigation.diff` + `.index.json` | Terrain edits no longer tell navigation, while the initial build still does. The path is still offered straight over the ridge (y = 32 m), and the AI walker follows it into the ridge and stops 56 m short. Both acceptance tests fail. | rejected |
| `04-second-launch-shelter-and-ground-restored.jpg`, `04-second-launch.log-excerpt.txt` | **Two sessions.** Session 1 builds and terraforms, then quits (autosave). Session 2 is a fresh launch: "restored … 16 build pieces, 10 edited ground vertices"; the frame shows the timber shelter, the mound and the pit. | PASS |
| `05-mutation-building.diff`, `05-mutation-building-core.index.json`, `05-mutation-building-game.editor-excerpt.txt` | (a) Outward links lose nothing: `SupportWeakensUpAndOut` and `RemovingASupportCollapsesWhatItHeld` fail. (b) Demolition skips the refund-fits check: the run aborts on the `verify` guard. Loss is loud, never silent. | rejected |

## The navigation hard gate (operator requirement)

1. **The runtime edit materially changes traversability.** It is a 3 m ridge across a straight
   route, with a gap at one end. Its height and collision are asserted.
2. **Pathing responds.** `Gridlands.Game.Terrain.NavigationFollowsRuntimeEdits`:
   - before the edit, the path crosses at y = 32 m (straight);
   - after the ridge, it crosses at y = 50.35 m (the gap);
   - after flattening, it is straight again.
3. **An AI-controlled actor walks it.** `Gridlands.Game.Terrain.AIWalksAroundARaisedRidge` uses a
   real `AAIController`, `UPathFollowingComponent` and `UCharacterMovementComponent`. The walker
   arrives 82 cm from B, crosses at y = 50.45 m, and its max z is 107 cm (the ridge is 3 m).
   - The bare automation world does not dispatch actor ticks, so the test steps these components
     directly.
   - The same proof runs with normal engine ticking in the real game (row 02).
4. **The old path is not followed.** The crossing must be in the gap, and the walker's height stays
   near ground level.
5. **The test can fail:**
   - `StaleNavigationIsDetectedControl` (edits don't notify navigation) sees the stale straight route;
   - the source mutation in row 03 fails both acceptance tests.

How it works: cells declare navigation bounds from data at runtime (`AGLCellNavBounds`, a bounds
volume given a box extent). The volume registers during spawn, so after resizing, the subsystem
calls `OnNavigationBoundsUpdated`. Recast `RuntimeGeneration=Dynamic`. Each edit rebuilds only the
chunks it touched and calls `UpdateComponentInNavOctree` for them. Built pieces are navigation
obstacles too (`Gridlands.Game.Building.PiecesBlockNavigation`).

## Other acceptance
- **Building consumes inventory and respects knowledge:**
  - `ShelterFromInventoryPersistsAcrossReload`: 32 planks for 16 pieces, exactly;
  - `RefusalsChangeNothing`.
- **Placement validation:** overlap, burial, support, knowledge and items (`Gridlands.Core.Building.*`).
- **Demolition and collapse:** `DemolitionRefundsEverythingThatFalls`. The wall and the roof slope it
  held are refunded in full; the demolition is refused if the refund would not fit.
- **Support weakens up and out:** pine stands a floor plus three walls, and one floor out over a drop.
- **Terraforming:** `TerraformConservesProtectsAndPersists`:
  - a shovel is required;
  - digging yields soil and raising spends it;
  - collision follows every stroke;
  - a floor is refused on a mound's slope and allowed once flattened;
  - digging under it is refused;
  - holes, mounds and the floor persist.
- **Save:** pieces and the sparse terrain delta are additive fields. Support is recomputed on load and
  compared equal.

## Defects found and fixed during M10
- Runtime nav bounds were registered at the box component's default 64 cm: components register
  during spawn. Fixed by resizing, then calling `OnNavigationBoundsUpdated`.
- Terrain chunks registered with navigation while their meshes were empty. The initial build now
  dirties navigation.
- Adding files regrouped the unity build and exposed two latent name clashes (`PryBar`, and
  `Top`/`Bottom` locals). Both were renamed.
- F (fabricate) would have made a second pry bar rather than a missing shovel. It now prefers a tool
  Zenny lacks (`FMakesWhatZennyLacks`).
- Piece colours rendered grey on first use while a shader permutation compiled. They render once
  cached (row 04). This is not a code defect; noted for the art pass.

## Not proven here (ADR-0022 proofs 2–4, still open)
- LOD and rendering cost at cell scale on the target GPU.
- World Partition streaming of chunk actors.
- Terrain material blending and foliage.

The ground uses the engine grid material.
