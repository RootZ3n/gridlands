# ADR-0038: Structural environmental resolution: the impact decides, a fall in flight is a durable fact

- Status: **Proposed for operator review** (P10 built 2026-10-02 under the operator's approved proposal and decisions 1–5).
- Date: 2026-10-02
- Builds on:
  - [ADR-0030](0030-structural-salvage-and-deterministic-collapse.md): deterministic collapse. **Amended here:** an
    unfinished collapse is no longer dropped, and impacts are decided on creature models.
  - [ADR-0033](0033-multi-frame-cell-presentation.md): the authoritative state comes first, presentation follows.
  - [ADR-0037](0037-dual-route-encounter-foundation.md): creature models, DEFEATED vs NEUTRALIZED. **Unchanged.**
  - [ADR-0017](0017-pehlichi-deals-zero-damage.md): Pehlichi deals no direct damage. **Unchanged.**
- Evidence: [P10 evidence](../Evidence/P10-structural-resolution/README.md).

## Context
P9 proved one environmental route, the cage, whose outcome is decided at its switch. The locked dungeon intent
also needs systemic routes through the real world simulation. The canonical example is the balcony: lure a target
under a structurally supported deck, take its support away, and let it fall.

**That must not be a scripted boss interaction.** The structure knows nothing about encounters, and the target
stays free to move while the structure falls.

P6 left a recorded GAMEPLAY CONSISTENCY DEBT:
- a save or an unload mid-fall kept the debris but lost the impact still to come;
- the impact also only reached pawn *actors*, so a creature waiting for presentation was immune to it.

## Decision

### 1. Three moments: SUPPORT FAILED → IN FLIGHT → IMPACT
**INTACT → SUPPORT FAILED → DELAY → IN FLIGHT → IMPACT → RESTING.**

- **SUPPORT FAILED** (the existing `GLCollapseRules` plan, unchanged) decides:
  - the trajectory;
  - the rest pose;
  - the impact time and volume, and the severity;
  - the one `Event.Structure.Collapsed`.

  It does **not** decide who is affected.
- **DELAY and IN FLIGHT:** the falling part is presentation following `Motion(outcome, elapsed)`. It is not solid, and
  everyone stays free to move.
- **IMPACT** decides from the authoritative world **at that instant**: where every creature model and Zenny are.
  - It happens once, at the first structure step at or after the impact time.
  - Positions are those of the last creature sync.
  - The impact noise and `Event.Structure.Impact` are emitted once.
- **What this means for targets:**
  - a target that escapes before impact is not affected;
  - one that enters after failure and is there at impact is affected;
  - one outside at impact is not affected.

**Persisted states:** Intact (implicit), Debris (existing), and **in flight** (`FGLSavedCollapse`, below). Delay and
in flight need no separate state: the elapsed time distinguishes them.

### 2. One impact, at most one creature outcome (operator decision 1)
For each **active** creature model (neither Defeated nor Neutralized) whose capsule touches the volume at impact:
- **If** `Severity >= tuning.collapse.pinMinSeverity` **and** its definition lists `Neutralize.Pinned`:
  - it is **NEUTRALIZED (Pinned)** through P9's `TryNeutralize`;
  - no damage, no death event, no kill credit, and health is preserved;
  - it is held where it was at the impact;
  - an encounter target resolves once.
- **Else:** the **ordinary environmental damage path** (P6's damage, through the health system). The damage may DEFEAT
  it with its normal semantics:
  - the death event;
  - drops to whoever is credited;
  - `MarkDefeated`;
  - encounter resolution.
- **Already Defeated or Neutralized** creatures are skipped. They are not considered at all, not merely immune.
- **Never both:** a creature is never damaged *and* neutralized by the same impact, and never hit twice by one impact.
- **No fake damage** produces a neutralization.

**Zenny and other pawns with health** (dev proof creatures have no model) take the ordinary damage path, as in P6.

**Severity is physical:** metres fallen × the material's `impactScale` (0 when the part barely moves, like damage).
`pinMinSeverity` (provisional: 2.0) is gameplay tuning, not an architectural constant. There are no per-creature
overrides (operator decision 3).

### 3. Impacts are decided on creature models
- **`UGLPlacementSubsystem::ActiveCreaturesTouching(volume)`** tests every active creature model at `CreatureLocation`:
  the actor while presented, else the model. An unpresented creature is never immune.
- **One capsule:** `GLCreatureRules::CapsuleRadiusCm` and `CapsuleHalfHeightCm` build the actor's capsule and test the
  model. `FGLImpactVolume::TouchesCapsule` is the one capsule test, the same three-sphere test P6 used.
- **One damage system:** `DamageCreature` damages through the creature's health component. A creature waiting for
  presentation is presented first, by the pump's own path, so health, defeat, drops, events and credit are exactly the
  actor path's.

### 4. A fall in flight is a durable fact (`FGLSavedCollapse`)
- **Stored:** optional, per cell, in `FGLSavedCell.Collapses`. The save format stays **v2**, so older files load
  unchanged.
- **It holds only what the decision fixed:**
  - the elapsed seconds;
  - motion, start and rest (as quaternions);
  - start and impact times;
  - the impact volume, damage, severity and fall;
  - the topple's pivot, axis and drop;
  - the impact material;
  - the **cause** and **credit** identities (below).
- **Never re-planned:** the world may have changed since the support failed.
- **Topple angles are not stored.** `GLCollapseRules::IntegrateTopple`, the integrator moved out of `Plan`, re-integrates
  them from their three inputs (height, gravity, start angle), and reconstruction checks its duration against the saved
  impact time.
- **Proven bit-identical through the real save codec:**
  - every field;
  - every millisecond's pose, for drops and topples;
  - against a re-plan under changed tuning, as a control.

  A record that is not its own plan is refused, never loosened.

**Save/restart:**
- A file keeps the remaining time, which follows P9's rule that a restart is not a wipe.
- **Restore is silent:** no collapse event, no noise. The part is non-solid at the plan's pose, and the impact is still
  to come.
- After the impact there is no record, so the part restores as debris at rest, as before.

### 5. Dormant cells freeze their falls (operator decision 2)
- A cell's record captures its falls in flight when it streams out. They are **frozen** there: not aged, unlike P9's
  memory timers, because the cell's creatures cannot move either.
- On return the creature models are restored first, then the falls.
- **A fall advances only while no creature of its cell waits for presentation.** A frozen creature must not lose time
  against a falling structure.
- **Outcome:** streaming never changes an outcome merely because the structure kept falling while a creature was
  prevented from escaping.
- **New lint, STR-4:** a placed structure's possible impact reach (each part's footprint grown by its height and the
  impact margin) must lie inside its own cell. An impact therefore never needs a neighbour cell's creatures.

### 6. Cause and credit are separate facts (operator decision 5)
A fall records:
- **Cause:** who physically removed the support;
- **Credit:** who receives gameplay attribution (kills, drops).

Both are identities (Zenny, Pehlichi, a creature's placement), not actor pointers, so they survive a save. While the
session lasts the live credited actor is kept too, so a generic pawn keeps its credit.

**Today credit = cause**, P6's rule. In P10, Zenny removes the support. **Pehlichi-triggered structural failure, and
its attribution, are deliberately not decided.** No mechanism-to-support link exists. Whether such a collapse may damage,
and to whom it is attributed, is a recorded future operator decision. ADR-0017 is unchanged.

## Consequences
- **The balcony solution is systemic.** The ordinary P6 carport, with unchanged structure data, neutralizes a
  susceptible warden in the real game:
  - 0 damage, no death, no kill, one resolution;
  - the same impact does ordinary damage to non-susceptible gremlins, and defeats a wounded one with Zenny credited;
  - quitting mid-fall, or leaving the lots mid-fall, gives the same outcome, once.
- **The P6 mid-fall debt is closed**, for player-facing damage too. A fall saved or streamed out mid-air still lands,
  once, on whoever is under it then.
- **Gameplay must not assume a creature's actor** at impact time. It asks the model.

## Not changed / not built
- **P9's Neutralized finality, unchanged:** a Neutralized creature is immune to later damage and is never re-scored.
  Future operator questions (not needed by P10):
  - Can a pinned creature later be killed?
  - Can a contained creature be released?
  - Can another hazard change a Neutralized outcome?
- **Out of scope:**
  - player-built physical collapse (ADR-0024's refunds stand);
  - structure-on-structure impact and load/weight;
  - the dungeon floor-support ground source;
  - mechanism-to-support links and Pehlichi collapse attribution;
  - Neutralize kinds beyond Pinned;
  - collapse VFX, Chaos and dust;
  - production dungeons, bosses and content;
  - editor authoring tools.
