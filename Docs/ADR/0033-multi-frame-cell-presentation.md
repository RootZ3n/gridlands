# ADR-0033: Multi-frame cell presentation, authoritative state first

- Status: **Accepted** (operator decision 2026-09-26, P7 closure: "implement multi-frame runtime
  spawning BEFORE increasing authored content density").
- Date: 2026-09-26
- Builds on: [ADR-0026](0026-grid-cells-and-streaming.md), [ADR-0028](0028-seamless-grid-streaming.md)
  (streaming), [ADR-0030](0030-structural-salvage-and-deterministic-collapse.md) (structures),
  [ADR-0032](0032-visual-pipeline-and-stylization.md) (visuals).
- Evidence: [P7 evidence, dense-spawn proof](../Evidence/P7-visual-spike/README.md#dense-spawn-proof-adr-0033).

## Context
**The problem.** A cell's runtime layer (placements plus kept state) came in within one frame.
- With P7's styled storefront, that frame took 10.2–10.8 ms of the 12 ms streaming budget.
- Authored density will grow by an order of magnitude: towns, interiors, dungeons.
- The operator ruled that the remaining 1.2–1.8 ms is **not** content budget.

**The invariant (operator):**

> **Authoritative saved state must be resolved before an object's gameplay representation can
> become live or interactable.**

Presentation may appear over several frames; gameplay truth may not. It is **not** acceptable to
spawn an intact/default object, let it exist or interact for a frame, and only then apply the
save that says it was destroyed, salvaged or moved.

## Decision
**1. The runtime layer splits into an authoritative step and a presentation queue.**

*The authoritative step* happens in one frame, `UGLGridSubsystem::TryFinishRuntime`:
1. `SpawnCell(Cell, bDeferPresentation = true)` creates the placements' gameplay model:
   - **Structures:** `FGLStructureRuntime`, every part with its piece and state; no actors.
   - **Actor-state placements:** creatures, glitches, salvage nodes, puzzle sites and discovery
     sites. They are few and their state lives on the actor, so they are made now.
2. `ApplyCell(Record)` resolves the cell's kept state onto that model **in the same frame**:
   - structure part states and debris rests (`RestoreCell` sets the model and makes no actor
     for a part still waiting);
   - glitch states, salvaged nodes, defeated creatures, terrain edits and player pieces.
3. **Only then** is the cell `bRuntime`: authoritative gameplay state ready. From here, saves,
   support, dig refusal (`IsUnderStructure`), building overlap and collapse planning all read the
   model. None of them needs an actor.

*Presentation* happens over the following frames, `PumpPresentation(Where, Budget, Near)`:
- **Queued units:** structure part actors, and vegetation patches (pure presentation).
- **Nearest to Zenny first.**
- **Everything within `PresentationNearM` (20 m) at once, regardless of budget:** what Zenny can
  touch is never missing.
- **The rest within `PresentationBudgetMs` per frame (1.5 ms).**
- **The frame an authoritative layer lands in** makes only the near units, so the two costs never
  stack.

**2. Each unit is made from the model as it is at that moment**
(`UGLStructureSubsystem::Present`):

| Model says | Made as |
|---|---|
| intact | the intact part |
| debris, at rest | solid, salvageable debris at the authoritative rest |
| debris, still falling (decided while it waited) | the plan's pose, not solid; it lands by the plan |
| removed / debris salvaged | **never made** |

No unit is ever made intact and then corrected.

**3. Unload is the same invariant in reverse: gameplay leaves at once, presentation over frames.**

*In the unload frame:*
- the cell's state is stowed from the model;
- the model is removed;
- gameplay actors (creatures, glitches, salvage nodes, puzzle sites) are destroyed;
- each structure part actor is **retired**:
  - unbound from the salvage pipeline, so a later load of the same placement can never be reached
    through it;
  - marked out of that pipeline (`RestoreSalvaged`), so it refuses any salvage;
  - hidden, with no collision;
- vegetation patches are hidden.

*Afterwards:* retired actors are destroyed within the presentation budget, before any new unit is
made.

*Why:* the dense fixture's unload destroyed ~300 actors in one frame (6.4 ms, and an 18 ms
streaming frame in one reversal run). Retiring costs ~3 ms.

**4. Cancellation and staleness.**
- Unloading a cell drops its waiting units (`RemoveCell`, `DespawnCell`).
- A unit whose structure or part no longer exists, or that already has an actor (a landing made
  it), is skipped.
- Saves read the model, so **a save mid-presentation loses nothing**, and a stowed cell keeps
  everything.

**5. Evidence split.**
- `FGLCellLoadRecord` records **authoritative ready** (`runtimeReadySeconds`) and **presentation
  complete** (`presentedSeconds`) separately. A cell is complete only when its ground and its
  presentation are.
- The crossing harness records per-frame presentation work, units and queue peak, and the
  authoritative layer's worst frame.

**6. The dense authored stress fixture** is `GLDenseProof`: dev only, in-memory placements in the
diner lots, enabled by `-GLDenseProof`.
- It contains 84 structures (308 parts): 10 storefronts, 10 carports, 24 pines, 20 phone tables
  and 20 boulders, plus 8 vegetation patches.
- That is ~12× the lots' 26 authored parts.
- Every site passes the authored-structure support rule.
- It is measured by `Tools/perf-crossing.sh -d` (every crossing mode and the round trips, against
  the same P5 budgets) and by `Tools/p7-dense-proof.sh` (real-game damage, save, restart).

## The per-frame budget (from evidence)
**Per-unit cost.** A structure part actor with its visual costs ~0.18 ms (346 units in 42 frames at
1.56 ms each when unconstrained). A vegetation patch of 700 instances costs ~1 ms, once its
footprint query was fixed.

**`PresentationBudgetMs` = 1.5 ms.**
- The worst presentation frame is budget plus one unit: 1.56–1.89 ms measured.
- Unconstrained, 1.5 ms presents the dense lots (346 units) in ~0.6 s after their authoritative
  layer: 42 frames.

**`PresentationCeilingMs` = 6 ms of streaming work per frame.**
- Presentation only uses what is left under the ceiling, so it never lands on a frame the ground
  already made heavy.
- The P5 terrain's first build of a fresh 64 m chunk actor costs ~5 ms (9–15 ms under load), which
  is the remaining source of streaming spikes in both the normal and the dense game.
- With the ceiling, the dense lots are fully presented in ~2.4–2.5 s: together with their ground,
  long before Zenny can walk the 256 m load margin.

**`PresentationNearM` = 20 m** around Zenny is presented at once. What Zenny can touch is never
missing, including right after a teleport or a load.

**The results** (quiet machine, final binary, `Docs/Evidence/P7-visual-spike/perf/`):

| Run | Streaming worst, normal game (P7 before → after) | Streaming worst, dense (12×) | Budget |
|---|---|---|---|
| straight | 10.24 → 7.20 ms | 8.55 ms | 12 |
| sprint | 10.37 → 6.92 ms | 8.93 ms | 12 |
| reversal | 10.59 → 7.07 ms | 8.16 ms | 12 |
| teleport | 10.82 → 9.70 ms | 6.85 ms | 12 |
| resume | 6.40 → 6.78 ms | 6.57 ms | — |

- An A/B on the same machine (the P7 build before this ADR against after, back to back) shows
  identical frame time, p99 and GPU, with streaming worst 10.5–10.7 → 6.9–7.2 ms
  (`perf/ab/straight-ab.txt`).
- **The headroom gained is ~3 ms in the normal game.** The 12× denser cell stays within the same
  budgets, with ~3 ms to spare.

## Consequences
- **Streaming cost no longer scales with a cell's authored density.** It scales with the budget.
  Density scales *time to fully presented* instead: that is the number to watch as content grows,
  and it is recorded.
- **Gameplay code must not assume a part actor exists** for every present part. The model is the
  authority, and `FindPart` may return null for a waiting part. Every current caller already
  tolerated that.
- **Actor-state placements (glitches, salvage nodes, creatures) are still made in the
  authoritative frame.** If they ever become dense, give them a model/actor split first, as
  structures have. Don't queue them as they are, or their saved state would be applied late.
- **Pure-presentation work must not iterate the whole world per element.** Vegetation checked every
  structure part for every tuft, which the dense fixture exposed: one patch took ~10 ms. It now
  collects the nearby footprints once (`CollectFootprints`).

## Not changed
- **P6's GAMEPLAY CONSISTENCY DEBT is untouched.** A mid-fall save keeps the outcome, but not the
  impact damage still to come.
  - Presentation never replays or applies damage; the model decides.
  - This path was not redesigned: a collapse unloaded mid-fall still drops its unapplied impact,
    exactly as in P6.
