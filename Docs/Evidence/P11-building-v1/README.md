# P11 Building v1: evidence

**Status: CANDIDATE (2026-10-03), awaiting operator review.** Branch `p11-building-v1`; not merged, not tagged.
Decision record: [ADR-0039](../../ADR/0039-building-v1-canonical-structural-model.md). Design:
[BUILDING-V1-DESIGN](../../BUILDING-V1-DESIGN.md) (approved 2026-10-03, deviations listed there).

## A. What P11 proves
Player construction is in the canonical structural model:
- **Same rules as authored structures.** Removing a support collapses what it held by the same rules (debris; impact
  at impact time; a fall in flight is saved, frozen with its cell and resumed).
- **Fine yaw end to end.** Yaw is an integer in 2.5 degree steps, and the bounds are oriented. Angled bays, octagons,
  hexagons and 16-sided towers come from socket data and close exactly. This is proven through placement, snapping,
  support, preview, overlap, terrain footprint, save and restart, streaming, collapse, impact volume and salvage.
- **PREVIEW == REALITY.** GREEN / YELLOW / RED and the removal preview are the commit's own rules: 0 mismatches over
  every gated placement.
- **FRAME -> FINISH, as two phases.** The framing is visibly framing before a finish goes on. A finish never changes
  support. Electrical is registered in the phase order but not built, and the validator refuses content for it.
- **Salvage quality by recovery path.** Careful dismantling returns intact studs, smashing returns fewer, and collapse
  debris returns scrap. A log is sawn into studs.
- **Inventory without weight.** Slots and stacks are the only limits, and nothing is ever discarded: a restore keeps
  everything, and a salvage or removal refuses rather than lose anything.
- **Shared base storage.** A claim comes from a base core. Inside it, storage is used before Zenny's pockets, in a
  documented order and all-or-nothing. A container that collapses keeps its contents, and the store and take verbs
  never lose anything.
- **Ownership.** World renewal never touches player construction or anything inside a claim.
- **Plans.** A plan rebuilt at 45 degrees is the same building.
- **Save v3.** It migrates v2 files deterministically: v0 pieces become frame + finish, and inventory, terrain, debris
  and encounters are untouched.

## B. The WINCHESTER / REAL HOUSE-0 fixture, in the real game ([`proof/`](proof/))
`Tools/p11-building-proof.sh` runs three processes (`gl.Building.Proof build | restart | resume`). All three pass: build
11/11 checks, restart 6/6, resume 5/5.

**The house** is built through the player's own transactions on a pad levelled by ordinary flatten strokes:
- **Base:** a base core (one 32 m claim) and two storage crates on floors, holding 103 studs, 83 planks, 26 logs and
  3 cut stone. Zenny carries 12 studs; the storage supplied 103 studs, Zenny 12.
- **Room A:** modern stud frames, with a doorway into a **three-sided 45 degree bay**. Its angle posts and short stud
  walls snapped at yaw steps 0, 126, 126, 0, 0, 18, 18, so the walls run at -45, 0 and +45 degrees from socket data
  alone.
- **Room B:** a **log-cabin wing**.
- **Roofs:** shingled gables over both rooms.
- **Porch:** a porch roof resting only on a **Roman column** and a **timber post**.

| Step | Result |
|---|---|
| FRAME | 52 placements, PREVIEW == REALITY 0 mismatches; 24 pieces visibly framed (studs, plates, rafters), 28 complete as built ([frame](proof/screenshots/build-0-frame-south.jpg), [northwest](proof/screenshots/build-1-frame-northwest.jpg)) |
| Saw | a log from base storage sawn into 4 studs at the sawhorse |
| FINISH | 24/24 finishes installed (Victorian clapboard, shingles), 0 still framed; support unchanged ([finished](proof/screenshots/build-2-finished-south.jpg), [bay](proof/screenshots/build-3-bay-closeup.jpg), [porch and log wing](proof/screenshots/build-4-porch-and-log-wing.jpg)) |
| Vocabularies | modern_day + victorian + roman, a log wing, a Roman column: deliberately mixed |
| Restart | the house comes back exactly: 52 pieces, 24 layers, 52 player-owned, the same fingerprint |
| Streaming | the home cell unloads with the house; back, still exactly the same fingerprint |
| Porch post out | predicted 0 falling, 0 fell |
| Roman column out | predicted [the porch roof], collapsed [the porch roof]; quit **mid-fall** |
| Resume | the fall resumes after the restart and lands exactly once ([debris](proof/screenshots/resume-0-porch-debris.jpg)) |
| Collapse debris | salvaged: scrap +5, studs +0 (no pristine kit from a collapse) |
| Careful dismantle | the interior stud wall: 5 studs, 2 scrap, 2 planks (its clapboard back as boards) |
| Smash | a bay wall: 1 stud, 4 scrap, 1 plank |

## C. Automated proof (178/178 tests, 49 requirements; tooling 114)
- **Core (`Gridlands.Core.BuildingV1`, 12 tests):**
  - yaw units;
  - octagon, hexagon and 16-gon closing by snapping alone, each piece standing and overlapping nothing;
  - oriented overlap and terrain footprints (enclosing boxes intersect, the walls do not; a rotated floor protects
    its square, not its box);
  - facings link only when they face each other;
  - the GREEN, GREEN, YELLOW, RED wall stack;
  - frame -> finish (electrical registered, unimplemented; a finish never changes support);
  - careful > destructive > collapse;
  - the material pool's order and atomicity;
  - claims and `MayRenew`;
  - a plan rebuilt at 45 degrees with identical geometry, socket relationships and support;
  - a collapse at 45 degrees on the piece's own axes;
  - the v2 -> v3 migration (deterministic, everything but the pieces untouched, a fixed point on re-save).
- **Game (`Gridlands.Game.BuildingV1`, 6 tests):**
  - the WINCHESTER house end to end, with save and restart and a save mid-fall (the fall resumes and lands once;
    debris to scrap);
  - base storage order, claims, overlap refusal and container collapse (contents kept through restart; refusal when
    full, then a whole return);
  - a basement as an open pit built upward, protected, and persisted;
  - the renewal gate;
  - 309 and 617 pieces scaling linearly;
  - store and take never losing anything, including when only part of an item fits.
- **Other new tests:**
  - `Gridlands.Game.Grid.PlayerConstructionStreamsWholeAndAFallResumes`: real streaming of the whole house mid-fall;
  - `Gridlands.Game.Building.PreviewColoursAndRemovalPredictionsAreTheCommitsOwn` (YELLOW included; a transitive
    removal prediction equals the collapse);
  - `Gridlands.Game.Inventory.RestoreNeverDiscardsOverflow`;
  - `Gridlands.Game.Inventory.InventoryFullIsAnnouncedNotWeight`.
- **Tooling:** PH-1, PH-2, FIN-1, SV-Q1 (x3), YAW-1, SOCK-1, MIG-1, and STR-3 at 2.5 degrees.
- **Fresh clone:** see the closure line at the end (the candidate head).

## D. Perf harness: same-frame attribution, proven ([`harness/attribution-proof.json`](harness/attribution-proof.json))
`Tools/perf/attribution-proof.sh` injects one known 60 ms stall per thread and checks that each lands in the record of
the frame it made slow, on its own metric:

| Injected | Record | Frame ms | Game thread | Render thread | RHI | GC |
|---|---|---|---|---|---|---|
| game-thread spin | frame 120 | 62.8 | **63.0** | 3.6 | 1.3 | no |
| forced GC | frame 200 | 26.2 | 25.9 | 3.1 | 0.9 | **yes** |
| render-thread spin | frame 282 | 62.9 | 2.5 | **62.9** | 0.7 | no |
| RHI-thread spin | frame 363 | 60.8 | 1.9 | 60.0 (it waits for the RHI) | **60.9** | no |

Frame-time budgets are untouched (worst frame still comes from `FApp` delta). In the real game the reversal hitches are
now reported as what they are: `garbageCollected: true` with the game thread at ~38 ms (the old record said "no GC,
~6 ms").

## E. Real-game performance (budgets unchanged; [`perf/`](perf/), quiet-gated, at `a040a29`)
Six suites, each run one mode at a time and only on a quiet machine (load < 3 and GPU idle; a run whose load ended >= 8 is
repeated). New in P11: **player** (the 309-piece dense player base, `-GLPlayerDense`, in the lots cell) and
**towndenseplayer** (that base plus the dense town block: the densest cell the game has).

Worst frame / p99 / worst streaming frame (ms), and round-trip memory:

| Suite | straight | sprint | reversal | teleport | resume | round trips: growth / peak (MB) | Budgets |
|---|---|---|---|---|---|---|---|
| local | 22.9 / 4.5 / 4.3 | 16.2 / 5.1 / 4.3 | 29.2 / 4.6 / 5.5 | 23.5 / 4.5 / 5.0 | 17.4 | 81 / 3653 | PASS |
| dense | 23.1 / 4.6 / 4.4 | 14.6 / 4.8 / 4.4 | 35.4 / 4.7 / 6.9 | 23.9 / 4.6 / 4.3 | 15.7 | 66 / 3659 | PASS |
| town | 24.6 / 5.2 / 4.9 | 14.3 / 5.2 / 5.1 | 35.7 / 5.0 / 7.7 | 23.7 / 5.1 / 4.2 | 15.7 | 66 / 3659 | PASS |
| towndense | 23.3 / 5.2 / 5.0 | 14.4 / 5.5 / 4.7 | **41.7** / 5.2 / 10.5 | 24.1 / 5.2 / 4.4 | 17.1 | 50 / 3647 | FAIL: reversal worst frame 41.7 > 40 (inherited, see F) |
| player | 23.5 / 4.8 / 5.2 | 14.3 / 5.2 / 5.0 | 33.1 / 4.8 / 7.6 | 23.5 / 4.8 / 5.5 | 12.0 | 61 / 3650 | PASS |
| towndenseplayer | 23.7 / 6.0 / 6.1 | 14.8 / 6.0 / 6.0 | **45.0** / 5.8 / **12.8** | 24.9 / 5.7 / 5.7 | 12.7 | 67 / 3678 | FAIL: reversal worst frame 45.0 > 40; streaming 12.8 > 12 (see G) |

Player construction at 309 pieces, measured in-game (`Gridlands.Game.BuildingV1.ThreeHundredPiecesScaleLinearly` and the
fixture logs):
- **Support, preview and the removal prediction** together: ~1.1 ms at 309 pieces, ~2.2 ms at 617 (linear).
- **Save:** the cell's capture (stow) takes 1.3–1.9 ms with or without the base: unchanged by it.
- **Restore:** the record applies in 2.0–2.8 ms; presentation then proceeds over frames within the budget (worst unit
  3.4 ms).
- **Presentation:** mean 1.6 ms per frame.
- **Memory:** peak +31 MB over towndense.
- **Reversal hitches:** every one is attributed correctly (`garbageCollected: true`; the game thread carries it).

## F. Inherited reversal hitch: A/B against the unchanged P10 merge ([`ab/`](ab/))
Fresh clones of the P10 merge and of P11, alternating, quiet-gated, towndense reversal, three rounds each:

| | P10 (unchanged) | P11 |
|---|---|---|
| worst frame (ms) | 39.3 / 34.5 / **41.1** | 37.5 / 39.0 / 40.4 |
| worst streaming frame (ms) | 10.3 / 10.0 / 10.3 | 11.5 / 9.8 / 10.5 |
| mean game thread (ms) | 1.75 / 1.79 / 1.79 | 1.79 / 1.79 / 1.80 |
| p99 (ms) | 5.3 / 5.4 / 5.5 | 5.4 / 5.4 / 5.4 |
| memory peak (MB) | 3779 / 3779 / 3763 | 3775 / 3775 / 3761 |

The towndense breach is the inherited reversal GC hitch: unchanged P10 breaches it too (41.1). It is recorded in the P10
evidence (41.9 / 41.5) and is not a P11 regression. It was not re-baselined, and GC was not optimized (operator
instruction).

## G. The scaling finding: unloading a player base is synchronous in one frame, linear in its pieces (operator decision)
The towndenseplayer breach is new, and it is P11's. It is attributed exactly ([`perf/unload-retire-timing.txt`](perf/unload-retire-timing.txt)):
- **Unload frame.** It is 11.7–12.8 ms against towndense's 10.1–10.4 ms.
  - Despawning the cell takes 8.2–8.6 ms against 6.0–6.3 ms.
  - The difference is making the 309 player parts inert (hidden, collision off, unbound from salvage): **2.29 / 2.77 ms**,
    about 7.5–9 µs per part. This is the existing ADR-0033 rule (gameplay leaves at once; actors are destroyed later
    within budget), which authored structures follow too.
  - The capture is not the cause: it costs the same with or without the base.
- **GC frame.** The garbage collection that follows the unload carries the retired actors: game thread 43–49 ms against
  35–43 ms in towndense, so the worst frame reaches 45.0 (47.5 in a re-run) against 41.7.

**Classification.** Linear in pieces, not quadratic: the quadratic defects the fixture found (I.2–I.4) are fixed. But the
cost lands in one frame (and in the next GC) per base, because every player piece is its own actor with its own
components. With the densest town cell, a base of roughly 200 pieces crosses the 12 ms streaming budget, and the GC frame
grows with every piece. Plans (P12) will make large bases easy to build.

Per the operator's instruction (STOP rather than hide an architectural scaling problem), P11 does not redesign this. See
the report's operator decisions: retire player parts over frames, give player pieces an instanced presentation (fewer
actors, smaller GC), or accept a documented per-cell piece budget.


## H. Planted defects ([`planted-defects/`](planted-defects/), `Tools/planted-defects/p11_building.py`): **41/41 caught by assertion**
Each defect is a backup-protected source or data edit, built and tested on its own and restored (never with git
checkout). A crashed, unbuildable or assertion-less run is **not** a catch. Only an assertion in a test report counts, or
a FAIL in the tooling self-tests that `Tools/test.sh` runs first.

| Group | Defects |
|---|---|
| fine yaw and geometry | B1 yaw quantized to quarter turns · B2 oriented overlap falls back to boxes · B3 terrain footprint is the box · B4 facing ignored when linking · B5 snap ignores facing · B6 sockets rotated by the nearest quarter turn · B7 drop impact on world axes |
| preview | B8 preview is a parallel two-state estimate · B9 YELLOW threshold off by one step · B10 removal preview misses transitive dependents |
| phases | B11 a finish changes support · B12 a finish on a form that refuses it · B13 electrical marked implemented |
| canonical structure | B14 player removal deletes what falls (v0 style) · B15 a player fall in flight is not saved · B16 player origin not saved · B17 layers not saved · B18 restore presents synchronously |
| salvage quality | B19 careful dismantle pays the collapse path · B20 debris salvages by the careful path · B21 smash equals careful · B22 salvage overflow silently lost · B23 removal return overflow silently lost |
| inventory | B24 stack limit bypassed · B25 restore discards overflow |
| storage and claims | B26 personal inventory consumed before storage · B27 partial consumption on a refused operation · B28 container contents lost on collapse · B29 lost on restore · B30 storage anywhere is connected · B31 a falling container still supplies · B32 overlapping claims allowed · B40 storing removes what did not fit · B41 taking leaves nothing behind |
| ownership, plans, migration | B33 renewal ignores player ownership · B34 renewal ignores claims · B35 plan rebuilt without the anchor turn · B36 v0 pieces migrate without their look · B37 v2 yaw not scaled |
| tooling | B38 salvage-quality lint disabled · B39 unimplemented-phase lint disabled |

**The first run caught 34/41.** What the gate exposed, and how each was resolved (re-run, then caught):
- **B1, B5 died instead of failing.** Each was caught by assertions, but a later test then indexed a house that had not
  been built (`BayIds[2]`, a plan of the wrong size), and the editor crashed. The V1 tests now end on the failed
  precondition's assertion. B1 is caught by 5 tests, B5 by 4.
- **B13 was a tooling catch the harness could not see.** `test.sh` stopped at its tooling self-tests (PH-2 failed) before
  any automation run. The harness now classifies that stop by the self-test assertions it wrote.
- **B23 was not a realistic defect.** With only the room check removed, the production `verify()` crashed. It is now the
  realistic lossy bug (no check, the delivery's result ignored), caught by `RemovalCollapsesByTheCanonicalRulesAsPredicted`.
- **B16 survived, and it was a real finding.** `AddPlayerPiece` forces player ownership, so the restore's reading of the
  saved origin was dead code. Restore now validates the saved origin: a non-player origin in a player record is reported
  as a load problem, and the piece is kept as the player's. It is caught by 2 tests.
- **B40 and B41 survived, test gaps.** No test stored into a nearly full crate or took an item that only partly fits.
  `Gridlands.Game.BuildingV1.StoringAndTakingMoveOnlyWhatFits` now covers both.

**Regression suites, all at the P11 head with nothing re-anchored except P10 S17 (before the run)**
([`regression-planted/`](regression-planted/)):

| Suite | Result |
|---|---|
| P10 structural | 28/28 caught by assertion |
| P9 encounter | 22/22 |
| P8 split / LOD | 19/19 |
| terrain collision | 27/27 |


## I. Defects discovered (and fixed) during P11
1. **The harness attributed hitches one frame late** (pre-existing; found in the pre-P11 gate). Fixed and proven (D).
2. **Socket links were quadratic in pieces** (every pair, every socket pair). They are now spatially hashed and linear
   (309 pieces: support 1.1 ms; 617: 2.2 ms).
3. **The structure presentation pump was quadratic for large structures.** It rescanned every waiting part with a
   linear part lookup per step. It is now one indexed pass, nearest first. Found by the 309-piece fixture.
4. **Every vegetation tuft asked `UGLBuildingSubsystem::IsUnderStructure`,** which copied and sorted every player
   piece, and the query was redundant: structure footprints already hold player pieces. It is removed, and
   `IsUnderStructure` delegates to the structural model. Presentation worst frame went from 9.3 ms to 2.6 ms with the
   fixture.
5. **Debris returned its frame's yield but not its finish's.** It now returns both by the collapse path (found by the
   WINCHESTER test).
6. **`Place` consumed the cost before the piece was added.** It now adds first and pays only after, so a failure costs
   nothing.
7. **A grounded storage crate could never lose support,** so "a container collapses" was unreachable. A crate now
   stands on a floor's centre socket.
8. **0.3 m log walls overlapped at corners.** They are now 0.24 m.
9. **There was no player verb to put items into a crate or take them out.** Store and take are added, lossless and
   tested.
10. **The saved origin of a player piece was never read** (`AddPlayerPiece` forces player ownership). Restore now
    validates it (found by planted defect B16, H).
11. **Two V1 tests crashed instead of failing** when a defect broke the house they index (found by B1 and B5). They now
    end on the failed precondition's assertion.

## J. Remaining debt
- **Pit edges.** A pit edge is a 1 m slope, not a vertical face, so a basement wall placed flush against it shows
  terrain or a gap (one height per vertex; operator-recorded limitation).
- **Collision of frames.** An unfinished frame collides as its full envelope, not stud by stud.
- **Art.** Frame and finish looks are blockout (no production art, by instruction). Shingles and timber boards reuse
  the M10 kit meshes.
- **Previews in the real game.** The proof drives the player's transactions, not the build-mode UI. The ghost colours
  and the removal highlight are covered by the same functions in tests, not by real-game screenshots.
- **Claim UI.** A claim has no visualization yet.
- **Knowledge for new styles.** Log walls and clapboard are unlocked by the existing timber-frame knowledge (their own
  discovery is content work). The Roman column uses Roman masonry.
- **Inherited GC hitch.** The ~31–42 ms reversal GC hitch remains inherited debt (see F); the budget is unchanged.
- **Unload cost of a player base** (see G): synchronous, linear in pieces; awaiting an operator decision.
- **Assigning the NICE overencumbrance lines' new wording** is provisional (operator-approved approach).

## K. Closure
- **Branch:** `p11-building-v1`. It is a CANDIDATE only: not merged and not tagged (operator instruction). P12 has not
  been started.
- **Pipeline:** the gate pipeline (fresh clone, six perf suites, A/B, five planted suites) ran at `a040a29`. The changes
  after it touch only tests, the planted harness, restore's validation of the saved origin, one evidence log line and docs.
  The planted re-runs, the unload timing run and the final fresh clone are at the candidate head.
