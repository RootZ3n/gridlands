# P11 Building v1: evidence

**Status: GREEN (2026-10-04, operator: "P11 FINAL REVIEW: APPROVED GREEN"); tag `p11-building-v1`.** The first
candidate (2026-10-03) was approved architecturally with its scaling stop (G) to be fixed first. It is fixed by instanced
presentation (G2), and the retired-collision behaviour is gated (G3).

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
- **Scaling (revised candidate).** Quiescent player pieces are one instanced batch per cell, not one actor each; a base's
  unload no longer scales with its pieces (G2). The model is unchanged: GAMEPLAY MODEL != PRESENTATION.
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

## C. Automated proof (181/181 tests, 50 requirements; tooling 114)
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
- **Instanced presentation (`Gridlands.Game.PlayerPresentation`, 2 tests, plus audits in the WINCHESTER, density and
  streaming tests).** `GLPlayerPresentationCheck::Problems` audits a cell's whole presentation against the model, in both
  directions (exactly once, instanced or by actor; exact transforms; owners against geometry; the instance total; one live
  batch).
  - Five floors at 0 / 22.5 / 45 / 60 / 90 degrees are found by real traces through the owner table: a rotated corner is
    hit, and the unrotated corner is not.
  - Removing the middle floor leaves every other piece's identity and collision as they were. A new piece is new.
  - Loading a save into the live world rebuilds the presentation from the record alone.
  - In the WINCHESTER house, the frame and then the finish are instanced, storage keeps its actors, and the removal
    preview is drawn over instances. The falling roof leaves the batch for an actor, its debris keeps it, salvage and
    dismantling remove exactly their instances, and a mid-fall save rebuilds it all.
  - Streaming retires the batch whole and leaks nothing; stream-in duplicates nothing.
  - The 309- and 617-piece fixtures are one batch with only storage as actors; every trace onto the base finds a piece.
    A retired 309-piece batch's bodies are seen part-way through a budgeted teardown.
  - The retired-collision gates are in G3 (`Gridlands.Game.Grid.RetiredPlayerCollisionIsInertAndAlwaysRemoved`).
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

## E. Real-game performance of the first candidate (budgets unchanged; [`perf/`](perf/), quiet-gated, at `a040a29`; the revised candidate's suites are in G2)
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

## G. The scaling finding (the first candidate, 2026-10-03): unloading a player base was synchronous in one frame, linear in its pieces
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

P11 stopped here (operator instruction). The operator chose **instanced presentation** (option b) and required
towndenseplayer to be back under the unchanged 12 ms streaming budget: see G2.


## G2. The correction: instanced presentation (operator decision 1, 2026-10-03; [ADR-0039 §11](../../ADR/0039-building-v1-canonical-structural-model.md))
**GAMEPLAY MODEL != PRESENTATION.** Every piece is still an individually authoritative record in `UGLStructureSubsystem`.
Only its drawing and collision moved.

**Architecture**
- **One batch per player structure.** A cell's quiescent intact pieces are drawn and collided by one
  `AGLPlayerPieceBatch`, made of instanced static mesh sets:
  - one set per blockout colour and one per authored visual (the only batching keys);
  - one hidden collision set holding every piece's shapes: blocking, read by navigation;
  - one overlay set for the removal preview.
  The 309-piece base is 1,300 instances in 6 components; at 617 pieces, 2,600 in 6.
- **One description of what a piece shows.** `GLPiecePresentation::Describe` decides it once for both the actor and the
  batch, so they cannot drift.
- **Identity.** Each set keeps an owner table (instance index -> piece id), mirrored on every add and removal. Instances
  are removed in order (no swap).
  - Every hit is translated through that table: aim, interaction, footsteps (`PlayerPieceAt`).
  - Renderer order is never gameplay identity, and nothing renderer-side is saved.
  - Tests remove a middle piece and re-find every other piece by real traces, and audit every collision instance's owner
    against that piece's geometry.
- **Dynamic pieces keep, or acquire, an actor.**
  - Storage keeps its actor (Store / Take).
  - A piece whose support fails leaves the batch and falls as an actor along the canonical plan. The impact is decided at
    impact time (P10), and its debris keeps that actor (salvaged by interaction).
  - A finish change re-presents the piece in the batch.
  - Nothing churns per frame.
- **Stream-out.**
  - In the unload frame the batch becomes invisible, and nothing in it resolves to a piece: its pieces left the model, and
    a retired batch answers no lookup.
  - Its collision is torn down in steps of 256 bodies within the retirement budget, then the one actor is destroyed.
  - This is safe because a cell unloads only when Zenny is more than 384 m from it, and gameplay has already left through
    the model.
  - The first version turned collision off in the unload frame (0.7–1.0 ms for 309 pieces: one body per instance); one of
    its quiet runs breached 12 ms (12.6), so the teardown was moved out of that frame
    ([`instanced/before-v1/`](instanced/before-v1/)).
- **Stream-in and load.** The batch is rebuilt from the saved record alone, over frames.

**Same-binary A/B: towndenseplayer reversal, quiet-gated, alternating** ([`instanced/ab-309/`](instanced/ab-309/);
`-GLActorPieces` restores one actor per piece; summarized by [`instanced/summarize.py`](instanced/summarize.py))

| 309 pieces (3 runs each) | one actor per piece | instanced |
|---|---|---|
| making the player structure inert (unload frame) | 2.20–3.02 ms | **0.09–0.15 ms** |
| worst streaming frame (budget 12) | 12.27 / 15.38 / 12.67: **over** | **11.43 / 10.79 / 10.83** |
| objects the unload's GC reclaims | 7,529–7,916 | **4,268–4,345** |
| worst GC frame, game thread | 48.7–54.1 ms | **44.5–47.3 ms** |
| live UObjects at the end | 63,261 | **59,371** |
| worst presentation unit | 3.2–4.0 ms | **2.1–2.6 ms** |
| memory peak | 3,806–3,821 MB | 3,786–3,802 MB |

**Stress: ~600 pieces** (`-GLPlayerDenseUnits=100`, 617 pieces; diagnostic, not a gameplay limit;
[`instanced/stress-617/`](instanced/stress-617/))

| 617 pieces | one actor per piece | instanced (2 runs) |
|---|---|---|
| making the player structure inert | 4.44 / 5.26 ms | **0.15 / 0.16 ms (flat: the same as 309)** |
| worst streaming frame | 14.31 ms | **9.81 / 10.23 ms** |
| objects the unload's GC reclaims | 12,185 | **4,357 / 4,407** |
| worst GC frame, game thread | 57.8 ms | **42.6 / 42.2 ms** |
| worst frame | 53.2 ms | **40.4 / 40.1 ms** |
| restoring the record on load (load frame) | | 2.1–2.9 ms |

With one actor per piece, the unload frame and the GC grow with the pieces. Instanced, they stay flat: the player base's
presentation costs one actor and a handful of components. The scaling problem did not move to another frame:
- the deferred collision teardown is budgeted, in 256-body steps;
- the worst presentation unit fell (2.1–3.0 ms against 3.2–4.0);
- the restore on load is linear but small;
- per-trace lookup is flat (~2.4 µs at 309 and at 617 pieces).

**All six suites with instanced presentation** (quiet-gated, budgets unchanged; [`instanced/perf/`](instanced/perf/)).
Worst frame / p99 / worst streaming frame (ms):

| Suite | straight | sprint | reversal | teleport | resume | round trips: growth / peak (MB) | Budgets |
|---|---|---|---|---|---|---|---|
| local | 19.8 / 4.6 / 4.1 | 15.0 / 4.9 / 4.5 | 30.5 / 4.7 / 4.9 | 26.2 / 4.7 / 4.5 | 16.8 | 46 / 3623 | PASS |
| dense | 25.6 / 4.8 / 5.0 | 16.2 / 5.1 / 4.8 | 40.0 / 4.8 / 7.3 | 23.3 / 4.7 / 4.6 | 14.2 | 78 / 3668 | PASS |
| town | 23.4 / 5.2 / 4.5 | 14.5 / 5.5 / 4.1 | 36.8 / 5.2 / 8.8 | 24.7 / 5.1 / 4.4 | 15.3 | 56 / 3659 | PASS |
| towndense | 25.1 / 5.3 / 4.7 | 21.7 / 5.6 / 5.5 | **40.6** / 5.3 / 10.0 | 28.6 / 5.2 / 4.9 | 15.9 | 86 / 3697 | FAIL: reversal worst 40.6 > 40 (inherited; no player construction in this suite; F) |
| player | 20.1 / 4.6 / 4.8 | 14.7 / 4.9 / 5.7 | 32.6 / 4.8 / 6.8 | 24.1 / 4.7 / 5.7 | 14.4 | 43 / 3621 | PASS |
| **towndenseplayer** | 24.4 / 5.4 / 4.9 | 16.4 / 5.8 / 6.0 | 35.2 / 5.4 / **11.1** | 23.7 / 5.4 / 5.4 | 14.2 | 49 / 3661 | **PASS** (it failed in G) |

**GC and object counts.** The player base now adds next to nothing to what a GC walks and frees:
- In the suite runs, the objects reclaimed by the reversal GC are 4,283 in towndense and 4,273 in towndenseplayer; the
  live objects at the end are 59,305 and 59,371.
- So the 309-piece base is about 66 UObjects, against about 3,950 with one actor per piece (63,261 - 59,305).
- The GC frame that remains is the inherited streamed-level GC. It was not touched (operator instruction).

## G3. Retired collision: intentional, gated (operator-approved 2026-10-04)
In the unload frame a retiring batch becomes invisible and unreachable. Its collision bodies are removed over the
following retirement frames. This is preserved as intentional behaviour: synchronous teardown was itself a measured
streaming regression (G2), so it is never reintroduced for lifecycle neatness. Gates:

| Invariant | Gate |
|---|---|
| Retired presentation cannot be interacted with | `Gridlands.Game.Grid.RetiredPlayerCollisionIsInertAndAlwaysRemoved`: a trace that reaches the retired collision resolves to no piece (`PieceIdAt` 0). Planted defect P17. |
| Retired collision cannot affect Zenny at legitimate streaming distances | The same test: every retired body is farther from Zenny than the 384 m unload margin. |
| All collision is eventually removed | The same test: no batch and no body is left after the retirement frames. The density test: a 309-piece base's bodies (more than 256) go in several budgeted steps. Planted defect P18. |
| No collision leaks across repeated stream cycles | The same test: three round trips, always one batch with the house's bodies once, and none away. |
| Stream-in before retirement completes cannot duplicate or corrupt collision | The same test: back at once (a teleport) before any retirement step. The retiring batch is removed as the new one is made: one batch, its bodies once, the same piece found where it was, the presentation exactly the model. Planted defect P19. |
| Authoritative piece identity and state are unaffected | The same test: the WINCHESTER fingerprint is unchanged after every cycle. |

## H. Planted defects ([`planted-defects/`](planted-defects/), `Tools/planted-defects/p11_building.py`): **61/61 caught by assertion** (final)
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
| **instanced presentation** (revised candidate) | P1 piece id mapped to the wrong instance (owner table reversed) · P2 a removal changes another piece's identity (table swaps, instances shift) · P3 stale instances after dismantling · P4 restore marks pieces instanced without instances · P5 a finish changes the model, not the presentation · P6 collapse leaves the static instance behind · P7 collapse removes another piece's instances · P8 a re-presentation duplicates the presentation · P9 loading over a live cell keeps the old batch (duplicates) · P10 stream-out leaks the batch · P11 interaction selects the wrong piece (off by one instance) · P12 a rotated piece drawn unrotated · P13 renderer order becomes identity (remove-at-swap in the renderer) · P14 storage forced into the batch (loses Store / Take) · P15 instances do not collide · P16 the removal preview not drawn on instances |
| **retired collision** (closure gates) | P17 interaction trusts a retired batch (both guard layers gone) · P18 retirement forgets the batch (never torn down) · P19 an early return keeps the retiring batch (doubled collision) · P20 retirement tears everything down at once (not incremental) |

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

**The revised candidate's run: 56/57, then 57/57.** P16 survived first, and it was a real finding. The WINCHESTER
presentation test highlighted the prediction for removing the Roman column while the porch post stood: an empty
prediction, so "empty equals empty" passed whatever was drawn. The test now uses the first removal that brings
instanced pieces down, and P16 is caught by its assertion. Every other defect, the 41 earlier ones included, was caught
on the first run of the revised code (`run.log` is that run; `summary.txt` re-derives the verdicts after the P16
re-run).

**The closure run (final code): 58/60, then 61/61.** P17 survived first, and rightly: a retired batch is guarded twice
(it answers no lookup, and a hit must name a piece the model still has), so removing one layer changes nothing. It now
removes both, the realistic "interaction trusts the presentation" bug, and is caught. The first P18 made a retirement step
that never progresses, which hangs the flushing pump: a dead run, not a catch. P18 is now "retirement forgets the batch"
(a leak, caught by three tests), and the one-step teardown became P20. P20 exposed a weak assertion: "at least two pump
calls" passed anyway, because crate actors spend the tiny budget first. The density test now requires seeing a batch's
bodies part-way through retirement. `run.log` is the full 60-defect run; `summary.txt` re-derives the final 61.

**Regression suites, all at the P11 head with nothing re-anchored except P10 S17 (before the run). Re-run against the
final code (after the closure gates and the I.17 fix), because authored structures share the refactored
`AGLBuildPiece`; the results are unchanged.**
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
12. **The scaling stop (G):** one actor per player piece made a base's unload and the following GC linear in its pieces.
    It is fixed by instanced presentation (G2).
13. **The first instanced version still tore down collision in the unload frame** (one body per instance: 0.7–1.0 ms for
    309 pieces; one quiet run at 12.6 ms). The teardown is now deferred into budgeted steps (G2).
14. **A vacuous removal-preview assertion** (an empty prediction compared with an empty highlight), found by planted
    defect P16.
15. **Observation, not a diagnosed defect: a one-frame RHI stall.** 27.6 ms on the RHI thread, 2.2 ms on the game
    thread, no GC, mid-walk at x = 316 m, away from any unload. It appeared once in 8 instanced quiet runs and in none of
    the others. Its cause is not established by the evidence.
16. **A density-test bound applied to the wrong quantity.** The first revised fresh clone (`ead19a1`) measured
    presenting 309 pieces in one call at 10.13 ms against a generic 10 ms per-operation bound. The game spreads that work
    over frames, so the test now bounds it per piece (under 0.1 ms; measured about 0.03–0.04).
17. **A return before retirement finished overlapped collision.** Writing the closure gate showed that streaming back at
    once (a teleport, a resume) built the new batch while the retiring one still held its invisible collision, so aim
    could hit a stale collider. The retiring batch of the same structure is now removed as the new one is made (P19).

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
- **OPEN INHERITED ENGINE/GC DEBT.** The 40 ms reversal budget stays canonical and is not re-baselined.
  - Towndense breaches it (40.6 ms at the revised candidate; unchanged P10 also breaches it, F).
  - Towndense contains no player construction, so the breach is independent of P11's player-building presentation.
  - P11 materially reduced the player-construction contribution to GC and object pressure: the 309-piece base is about
    66 UObjects, down from about 3,950 (G2).
  - The remaining streamed-level GC behaviour is not a P11 blocker.
- **Retired batch collision.** It lives for a few frames after its cell unloads, then is torn down within budget (G2).
  It is safe while Zenny is beyond the 384 m unload margin. A creature of a neighbouring cell standing at the boundary
  could touch it for those frames.
- **DEFERRED: authored structures remain one actor per part** (operator, 2026-10-04: not a P11 requirement; a candidate
  future optimization, not to be done speculatively). The measured baseline is preserved for a future change to beat
  ([`instanced/perf/`](instanced/perf/), quiet suites at the revised candidate):

  | Suite (no player construction) | Authored actors despawned at the lots unload | Despawn | Reversal GC: live objects before -> after (the unload's GC) | Live objects at the end |
  |---|---|---|---|---|
  | town | 118 | 3.9–4.3 ms | 59,985 -> 57,417 (2,568 reclaimed) | 57,657 |
  | towndense | 210 | 6.0–6.1 ms | 63,286 -> 59,003 (4,283 reclaimed) | 59,305 |

  The same model-first, instanced approach may reduce the town's unload despawn, its actor and object counts, and the
  streamed-level GC pressure after it. That is unmeasured until it is built.
- **Debris stays an actor** (few, interactive). It does not return to the batch.
- **Assigning the NICE overencumbrance lines' new wording** is provisional (operator-approved approach).

## K. Closure
- **Branch:** `p11-building-v1` (PR #34). The operator approved P11 GREEN on 2026-10-04. P12 has not been started.
- **First candidate:** its gate (fresh clone, six perf suites, A/B, five planted suites) ran at `a040a29`; that evidence is
  in E, F, G and `perf/`, `ab/`.
- **Revised candidate:** the instanced-presentation evidence is in G2 and `instanced/`.
- **Closure (final code):**
  - the retired-collision gates (G3), and the I.17 fix they exposed;
  - 181/181 tests, 50 requirements, 114 tooling tests;
  - planted 61/61 by assertion, with the regression suites P10 28/28, P9 22/22, P8 19/19, terrain 27/27;
  - the real-game WINCHESTER proof (`proof/`: build 11/11, restart 6/6, resume 5/5) at the revised candidate.
- **Measurement vs code:** the instanced perf runs (A/B, stress, six suites) were measured on the revised code before the
  closure gates. The only production change after them is I.17, which runs only when a structure returns before its
  retired batch finished retiring (none of the measured runs do).
