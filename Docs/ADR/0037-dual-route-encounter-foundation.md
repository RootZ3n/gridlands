# ADR-0037: Dual-route encounter foundation: creature models, DEFEATED vs NEUTRALIZED, model-first mechanisms, ambient masking

- Status: **Accepted** (operator, 2026-09-27: "P9 proposal approved with the decisions and amendments below").
- Date: 2026-09-27
- Builds on: [ADR-0036](0036-gameplay-models-lods-production-density.md) (models before actors),
  [ADR-0031](0031-authoritative-world-noise.md) (one world-noise model),
  [ADR-0017](0017-pehlichi-deals-zero-damage.md) (Pehlichi deals no direct damage; **unchanged**),
  [ADR-0029](0029-localized-navigation.md) (amended by P9: navigation where active gameplay requires it).
- Design intent: [DUNGEONS-AND-LEGENDARIES §1.2–1.7](../DUNGEONS-AND-LEGENDARIES.md) (two complete routes).
- Evidence: [P9 evidence](../Evidence/P9-encounter-foundation/README.md).

## Context
The locked dungeon design needs two complete routes through one space:
- a direct route (defeat through combat);
- an environmental route (survive under full threat while the environment takes the target out).

The P8 audit found that the foundations were missing:
- a creature's alert state, memory, position and health lived on its actor;
- DEFEATED was the only way out of an encounter;
- no mechanism had durable state;
- noise could not be masked.

## Decision
### 1. A creature's gameplay facts live in its model
`FGLCreatureModel`, inside the P8 placement model, holds:
- awareness (the behaviour state);
- position and yaw, home, patrol progress;
- last-known position;
- the remaining seconds of search, noise and lure memory, with their noise and lure points;
- health;
- outcome.

Nothing presentational is kept.

**The actor is the creature's senses and legs.** It writes its facts through every behaviour step,
and on damage. It is made from the model as the model is at that moment.

**Time:**
- **While a cell is streamed out,** its creatures' timers run against the world clock. The time away is
  subtracted on return.
- **Save/restart keeps the remaining time.** A reload never wipes a search.
- **A creature waiting for presentation, or far away,** is frozen except for its timers.

**Hearing is model-level:** a creature with no actor still hears, by the same rule, at its model's position.

### 2. Outcome: None, Defeated or Neutralized
The outcome is stored as a number, append-only.

- **DEFEATED** goes through the health and death system (unchanged). It can credit a kill, and the creature
  has no actor after reload (the P8 rule).
- **NEUTRALIZED** is a legitimate non-damage outcome. The kind is a data tag (`Neutralize.Contained`; the
  vocabulary also declares `Disabled` and `Shutdown`, which are not built), with who caused it and where
  the creature is held.
  - No damage, no death event, no kill credit; health is untouched.
  - The creature stays presented, inert: no behaviour, hearing, strikes or navigation, and it ignores damage.
  - After reload and every presentation recreation it is shown contained.
- **Susceptibility is data:** a creature definition lists what can neutralize it (`neutralizableBy`).
- **An outcome is final.** A neutralized creature is never re-scored as defeated.

**Encounter resolution:** an encounter target (`creature.encounter.rewards`) resolves once, whichever outcome,
through `Event.Encounter.Resolved` (`neutralized` 0 or 1) and its rewards. It happens only on the transition
out of None, so a restore never replays it. Direct and environmental resolution are equal successes for
completion; statistics, dialogue and achievements keep the distinction.

**ADR-0017 is unchanged and enforced.** Pehlichi's Operate is an interaction; the mechanism applies the outcome.
No step of the environmental route produces a damage event (tested, and planted-defect N14).

### 3. Model-first mechanisms
A new kind, `mechanism.*`, deliberately narrow:
- named states and an initial state;
- one operation (from, to, capability, level, seconds, reach);
- effects on entering a state: neutralize (a box and a tag), ambient mask (radius, mask, an optional duty
  cycle), and a world noise.

**The record is authoritative** (`UGLMechanismSubsystem`) and saved per cell. The actor (control panel, cage,
fan) is presentation made from it.

**The decision moment:** a switch decides its effects at that instant. Every susceptible creature inside the
neutralize box *then* is neutralized:
- nobody who enters later (planted-defect N2);
- nobody who was outside (N1);
- nobody who is not susceptible (N15).

The visible cage drop only presents an outcome already committed, so a save mid-drop cannot lose it.

**A restore sets the state silently:** no effect, noise or event replays (N22).

**Pehlichi's `Command.Pehlichi.Operate`:**
- he goes to the nearest operable control (his positioning ignores navigation, so he reaches controls Zenny
  cannot), works it for its seconds, and it switches;
- any other order interrupts it;
- progress is not persisted: a reload never replays an operation.

### 4. Ambient masking
**The canonical rule:** AMBIENT NOISE REDUCES THE EFFECTIVE AUDIBILITY OF OTHER NOISE AT THE LISTENER.

**The first implementation** (tunable without a new architecture decision): heard only within
`min(stimulus radius, hearing) × (1 − M)`, where M is the strongest active ambient mask at the listener.
- It applies to every stimulus, **Pehlichi's lures included** (operator decision M2).
- Ambient sound is never a stimulus itself.
- Duty cycles run on the world clock.
- A mechanism can switch its ambient sound off.

**Movement noise** (Zenny's footsteps and landings) is scaled by the floor material's `noiseScale`.

### 5. Minimal patrols
Spawn placements may declare a data loop (`patrol`, 2+ points). A patroller walks it while calm, and returns
to it after a hunt; its progress is part of its model. It is not an authoring suite.

### 6. The dev proof room
`-GLDungeonProof` places, in the lots, in memory:
- 20 dev walls and a sheet-steel grate strip;
- 4 patrols;
- the warden (240 hp, `neutralizableBy: [Neutralize.Contained]`);
- a cage whose control is behind the east wall;
- an HVAC fan by the grate;
- one navigation region.

Its dev definitions live under `<kind>.proof.*`. PRF-1 keeps them out of every Data placement.

## Consequences
- **Streaming and reload are no longer exploits** against creature memory or wounds.
- **Environmental solutions come from world state at a defined decision moment,** not from scripted triggers:
  the foundation for systemic solutions later (P10: structural collapse).
- **Gameplay code asks the model:** `IsCreatureDefeated`, `IsCreatureNeutralized`, `IsEncounterResolved`,
  `CreatureLocation`.

## Not changed / not built
- **Environmental kills by mechanisms.** (Structural impacts already damage, and can defeat, through the health system:
  P6, and on creature models since P10.)
- **Structural-collapse neutralization and the mid-fall persistence debt:** P10, separate. **Built in
  [ADR-0038](0038-structural-environmental-resolution.md)** with `Neutralize.Pinned`. The rules here are unchanged:
  outcomes final, Neutralized immune to later damage.
- **Named dungeons, production art, agility progression, Pehlichi's skill tree, diet/fart mechanics,
  legendaries, NICE dialogue, achievements, crouch/sneak, vegetation concealment, a mechanism library.**
- **Operate progress is not saved** (an interrupted operation is commanded again).
