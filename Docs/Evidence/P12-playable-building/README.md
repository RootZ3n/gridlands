# P12 Playable building: evidence

**Status: CANDIDATE: ENGINEERING GREEN pending operator review; DAILY-DRIVER ACCEPTED not yet (the operator's session).**
Branch `p12-playable-building`, not merged, not tagged. Decisions: the approved P12 proposal and operator decisions Z1–Z9
(2026-10-04). Design: [ADR-0040](../../ADR/0040-playable-building-build-mode.md). The operator's session:
[operator-session.md](operator-session.md).

The two gates are separate claims:
- **ENGINEERING GREEN**: the playable-building implementation is proven correct (this document).
- **DAILY-DRIVER ACCEPTED**: the human building experience is good enough to build authoring on. Only the operator's own
  session can say that. P13 does not start before it.

## A. What P12 proves
- **One build mode, four sub-states** (PLACE, BROWSE, FINISH, REMOVE). The primary action means what the sub-state says,
  and a mode change cancels a pending confirmation. The UI renders a view computed from canonical results and sends
  intents. It is never authority: a commit re-aims and re-checks at its own moment.
- **WHAT THE UI SAYS == WHY THE COMMIT ACCEPTS OR REFUSES.**
  - The structural word, icon and colour (OK [+] / LIMIT [!] / NO [x]) and the reason are generated from the placement
    check's structured result: the blocking piece, the missing item with its need and what is available, the material.
  - The cost and its sources are the consumption's own plan (`PlanConsume`), never a second calculation.
  - The snap marker is what the snap connected.
  - The removal highlight is the canonical collapse prediction, and the salvage preview is the commit's scaled yield.
  - The claim ring is the canonical claim.
- **CAMERA AIM == GAMEPLAY AIM.** In the real game, every placement was gated: the view's ray is the camera's ray, and
  the commit is the candidate shown. Result: 46 aim checks, 0 mismatches; 36 commits, 0 mismatches.
- **The hold follows collateral collapse.** A removal that brings nothing else down is one click. One that does needs a
  hold (or press-twice in the toggle mode). The confirmation never transfers to another piece or prediction.
- **Eras are filters, never locks.** A Roman column beside a modern foundation, and log, Victorian and Roman pieces
  together, are valid whenever the structural rules say so.
- **Stairs and the window wall are ordinary components.**
  - Zenny walks up the straight stair by physics, and navigation finds the path up.
  - The stair supports, saves, streams and collapses by the ordinary rules.
  - The window wall has a real opening, is finished like any wall, and is held up by its floor.
- **Novice clarity without destroying expert flow.** The operator set no threshold, so these are measured and reported:
  - repeat placement without reopening the browser;
  - the finish remembered per role;
  - favorites (1–8) and recents;
  - one-click safe dismantling;
  - rotation held while repeating.

## B. Real-game public-intent proof ([`intent-proof/`](intent-proof/), `Tools/p12-intent-proof.sh`): build PASS, restart PASS
**What it is.** The real game (`-game`, the real camera and Zenny), driven only through build mode's public intents (the
browser, favourites, rotation, finish, remove, path, build camera) and the ordinary interaction verb. Walking between
steps is a teleport, not an interaction.
- **Aim gate at every placement.** The view's ray must start at the camera (within 2 cm) and point along it
  (dot > 0.99999). The committed piece must be the candidate shown (location, yaw, definition).
- **The operator's world save** is set aside and put back; the proof never consumes it.

**build (fresh world), 21 checks, all PASS:**
- the base core, chosen through the browser; the claim ring is the canonical claim (32 m, provisional);
- materials stored in the crate by the real verb (E);
- a row of 8 floors placed through a favourite;
- 10 identical snapped walls, each exactly where its ghost and snap marker were;
- the chosen finish (clapboard) installed, with its cost and sources as shown: base 3 / you 0 shown, base 3 / you 0
  taken;
- 10 walls finished in the chosen finish;
- the window wall: placed like a wall; a trace through the opening is clear, and one below the sill hits the wall;
- an upper floor aimed at a wall top from outside rests over the room on both walls (see ADR-0040 §3);
- the stair, snapped to the upper floor's edge and standing on the floors;
- Zenny walks up it by physics (feet 302 cm above the pad), and navigation finds the path up (ends at 304 cm);
- removal:
  - a redundant support goes with one click, and nothing falls;
  - the last support's preview predicts the upper floor, the hold confirms it, and exactly the predicted piece
    collapses;
- salvage, careful and smash: each preview is exactly the recovery;
- the build camera: pulled back 10.3 m, and what it aims at is what is placed;
- 10 independent pieces dismantled with one click each;
- **CAMERA AIM == GAMEPLAY AIM: 46 checks, 0 mismatches. Every commit is the piece shown: 36 commits, 0 mismatches.**

**restart, 4 checks, all PASS:**
- the house is back exactly: 21 pieces fingerprinted (stair, window wall, finishes, debris);
- the favourite persists (playtest profile);
- streamed away and back: still exactly the house;
- building continues.

**Interaction counts** (intents, measured in the build run; no threshold, per the operator):

| Scenario | Intents | How |
|---|---|---|
| 10 identical snapped walls | 17 | browse to the wall (Tab, category, row, choose: 7), then aim and click per wall (10) |
| 10 walls finished with the same finish | 11 | finish mode once (1), then aim and click per wall (10; clapboard was already the remembered choice) |
| 10 independent pieces carefully dismantled | 12 | remove mode (1), careful path (1, the run had just smashed), then aim and click per piece (10) |

The browse overhead (7) is the cost of finding a piece from scratch. A favourite (1 key) or the wheel replaces it. Whether
these counts feel excessive is the operator's call in the daily-driver session.

Screenshots: framed walls, finish mode, the stair and upper floor (with the next ghost snapped, LIMIT), Zenny upstairs,
the removal prediction (red: what falls, both yields), the build camera, the browser (categories, favourites, recents).

## C. P11 WINCHESTER regression (unchanged): PASS
`Tools/p11-building-proof.sh` (build, restart streamed, resume) passes unchanged. It was rerun after each canonical change
in P12: Place's sources, the resting-snap rule and the shared overlap test.

## D. Automated proof: 199/199 tests, 52 requirements; tooling 118
New in P12 (all required, `Tools/required-tests.txt`):
- `Gridlands.Core.PlayableBuilding` (4):
  - the catalogue (total, ordered, data-driven; an explicit category wins);
  - the snap report and the resting choice (most supports, away from the viewer; the marker is the socket it rests
    on);
  - the consumption plan is the consumption;
  - refusals carry their machine-readable detail.
- `Gridlands.Game.PlayableBuilding` (13):
  - browser and eras;
  - browser selection, the wheel's variant and its commit;
  - cost and sources;
  - snap marker and yaw;
  - structural word and reason;
  - removal and confirmation (hold, toggle, no transfer);
  - salvage preview;
  - finish choice and memory;
  - claim;
  - favourites and recents (persisted);
  - modes and UI-not-authority;
  - the window wall;
  - the stair.
- `Gridlands.Game.Building.StairsAreNavigable`: the navmesh walks up the stair.
- Tooling: CAT-1 / CAT-2 (`Tools/tests/test_gldata.py`).

## E. Performance ([`perf/`](perf/), `Tools/perf/p12-build-mode.sh`, quiet-gated; budgets unchanged)
- **Fixture.** Beside the P11 309-piece player base (`-GLPlayerDense`), 6 s measured after 2 s of settling per state.
  GT is the game-thread frame time. The build tick is the build-mode component's own tick: the view, aim, snap, check,
  cost plan, prediction and ghost.
- **Target.** The <= 2 ms incremental game-thread figure is a TARGET, not a budget.

| State | GT mean / p99 / worst (ms) | Incremental GT mean | Build tick mean / p99 / worst (ms) |
|---|---|---|---|
| normal (build off) | 2.07 / 2.75 / 3.32 | 0 | 0.005 / 0.010 / 0.086 |
| PLACE, aimed at a wall top in the base | 3.44 / 4.39 / 4.89 | **+1.37** | 1.537 / 2.098 / 2.382 |
| build camera | 3.45 / 4.19 / 5.26 | **+1.38** | 1.548 / 1.868 / 2.730 |
| browser open | 2.01 / 2.65 / 4.02 | -0.06 | 0.058 / 0.099 / 0.773 |
| REMOVE (canonical prediction every frame) | 3.28 / 4.16 / 5.07 | **+1.21** | 1.426 / 1.705 / 2.163 |
| repeated placement (6 commits, 0.4 s apart) | 2.59 / 4.45 / 11.61 | +0.52 | 0.625 / 2.146 / 2.648 (commit: mean 3.70, worst 4.41) |

- **Result.** Every state is within the 2 ms incremental target on the mean.
- **The breach found and fixed.** The first quiet run breached the target: PLACE +2.50, build camera +2.82 ms, and a
  23.6 ms worst build tick during repeated placement ([`perf/before-snap-fix.txt`](perf/before-snap-fix.txt)).
  - Cause: Snap solved support over the whole structure for every candidate socket, only to test overlap.
  - Fix: one shared overlap test, `OverlapsAny`. Snap fell from about 1.2 to 0.12 ms.
- **Where the PLACE frame goes now** (diagnostic state `placeBreakdown`, which repeats the calls once more):
  - snap 0.12 ms;
  - **placement check 1.42 ms**: the canonical support solve over the structure, which P11's build mode already paid
    per frame;
  - cost view 0.04 ms;
  - removal prediction 1.31 ms.
- **The worst repeated-placement frame** (11.6 ms game thread) is the commit frame: the commit (3.7 ms) and the
  piece's presentation. Build mode has no budget of its own. The existing budgets (`Tools/perf/budgets.json`) govern
  streaming crossings, and none was changed.

## F. Planted defects ([`planted-defects/`](planted-defects/), `Tools/planted-defects/p12_playable.py`): **40/40 caught by assertion**
- **The rule.** The same strict classifier as P11 (`assertion_failures`): a defect is caught only by a test assertion,
  or for PB31 by a failed check of a completed real-game proof run. Crashes, hangs, build failures, tool failures and
  engine errors are not catches.
- **First pass: 36/40.** Four survivors exposed four test gaps, each closed by a test (not by weakening a defect):
  - PB4, the wheel's variant: no test turned the wheel. Added: the wheel's previous/next variant is the one committed.
  - PB6, an explicit category ignored: vacuous, because the window wall's explicit category was also its role's
    fallback. The fallback was removed, so the explicit field (author intent, Z8) alone places it.
  - PB13, the snap marker as the piece centre: every tested socket sat at its piece's origin. Added: the upper floor's
    marker is the wall top it rests on.
  - PB18, Shift+Z turning +90: the test's four quarter turns cancelled mod 360. It now has an odd number.

  The four were rerun: 4/4 caught.
- **The categories covered:**
  - browser and variant (PB1–PB7);
  - finish (PB8–PB10);
  - cost and sources (PB11–PB12);
  - snap and yaw (PB13–PB19);
  - structural word and reason (PB20–PB23);
  - removal prediction and confirmation (PB24–PB28);
  - salvage preview (PB29);
  - claim (PB30);
  - camera aim (PB31, real game: aim gate 46/46 mismatches);
  - favourites and recents (PB32–PB34);
  - modes and UI authority (PB35–PB36);
  - stairs (PB37: no treads, so the navigation test fails; PB38: the head misses the floor);
  - window wall (PB39–PB40).
- **Regression** ([`planted-defects/regression/`](planted-defects/regression/)): every earlier planted defect whose edit
  touches a file P12 changed was rerun against the candidate: **25/25 caught** (P11 23, P10 1, terrain 1).
  - P11's B2 was re-expressed for `OverlapsAny`, where P12 moved the overlap loop. It is the same defect: its first form
    no longer compiled, which is not a catch.
  - The remaining defects of those suites touch no file P12 changed.

## G. Defects found during P12
1. **A latent P11 defect: `Place` took its material sources before adding the part.** Base storage lives in the player
   structure's parts. When adding a part grew that array, the sources pointed into freed memory. The full suite then
   hung in teardown, with mimalloc spinning on the corrupted page. The sources are now taken after the add.
2. **The resting snap followed data order.** An upper floor aimed at a wall top landed a metre off, straddling the wall
   or outside the room. Now: most supports, then away from the viewer, then the aim (ADR-0040 §3; the P11 proof and
   tests unchanged).
3. **Build-mode cost** (E): Snap's per-candidate support solve. Fixed with the shared `OverlapsAny`.
4. **The accessibility hold/toggle settings existed only in the profile file.** The console commands
   `gl.Build.CameraMode` and `gl.Build.ConfirmMode` were added.
5. **Four test gaps** found by planted defects (F).

## H. Remaining debt
- **Daily-driver acceptance is not done:** it is the operator's session ([operator-session.md](operator-session.md)).
- **Flagged for review, beyond the approved primitives:** the buildable upper floor (without it a stair led nowhere),
  and the resting-snap rule change.
- **The per-frame placement check and removal prediction solve support over the whole structure** (about 1.4 and
  1.3 ms beside 309 pieces). They are linear in structure size, a scaling item for mansion/estate scale (incremental
  support), not needed at P12's size.
- **Parallax in the interaction verb (pre-existing, not P12).** The interaction verb (E) traces from Zenny's eyes along
  the camera direction, so in third person its target can differ from the crosshair's. Build mode aims from the
  camera; E does not.
- **The repeated-placement measurement made 6 commits:** the rest of its row was out of reach from its fixed stance.
- **The HUD's first line names the selected piece in REMOVE/FINISH.** The target is named on the reason line.
- **Accessibility settings are console commands.** There is no settings screen and no remapping (out of scope).
- **Inherited, unchanged:** towndense breaches the 40 ms reversal budget (engine GC); authored structures are one actor
  per part.

## I. Fresh clone
TODO
