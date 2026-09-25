# ADR-0025: Vertical-slice creature, combat and Glitch Storm (v0)

- Status: **Accepted for M11 as v0**. The operator set the requirements; the numbers and feel are
  provisional until the operator's playtest.
- Date: 2026-09-25
- Evidence: [Docs/Evidence/M11](../Evidence/M11/README.md)

## Requirements (operator)
- One representative corrupted creature. Zenny can fight it, avoid it, and beat it with at least
  one nonlethal strategy.
- Pehlichi deals zero damage (ADR-0017).
- No activity-triggered hostile spawning (ADR-0014).
- One bounded Raining Cats and Dogs storm. It proves that NICE changes the world, and covers
  presentation, dialogue, persistence and clean-up. The animals must read as digital artefacts.

## Decision

**Creature (`creature.drain.static_gremlin`).**
- Data: health, walk and chase speed, sight radius and cone, hearing radius, attack
  (damage, reach, cooldown), leash, and drops.
- Behaviour is pure (`GLCreatureRules`): Idle → Chase when it sees Zenny → Attack in reach, with
  a cooldown. Return when it loses Zenny, or when Zenny is beyond its leash. Investigate a lure.
- Seeing requires all three: within radius, inside the cone (or already chasing), and a line
  of sight.
- **A lure beats even a chase.** This is what makes distraction a real option.
- It lives in the storm drain, as a placement (`kind: spawn`). Creatures enter the world **only**
  through the placement subsystem. A static rule enforces this, together with CR-2 (every
  creature is placed somewhere).

**The options Zenny has.**
- **Fight:** left mouse swings the best carried weapon (item `weapon`: pry bar 20, shovel 14;
  fists 8). The gremlin has 60 health.
  - Its drop (`static_residue`) is a combat source. CR-1 and NC-2 keep it off the critical path.
- **Avoid:** it faces the entrance. Walls block its sight, it doesn't see behind itself, and a
  walled side channel leads around it to the glitch.
  - The drain's glitch can be repaired with the gremlin untouched (tested).
- **Distract (nonlethal):** V asks Pehlichi for a glitchy noise where he is standing.
  - Creatures within hearing investigate for `distract` seconds (8, or 14 at level 2).
  - The drain glitch rewards distract level 2.
  - Cooldown 10 s. Zero damage.

**Zenny's health.**
- 100 health. Taking damage emits `Event.Player.Hurt`; death emits `Event.Player.Died`.
- He wakes at his respawn point after 3 s, **keeping his inventory**.
- Health is saved. Dying and then saving wakes him at full health.

**Storm (`storm.playful.cats_and_dogs`).**
- Starts the first time `Event.Glitch.Repaired` reaches 2. This is NICE retaliating.
- Rains up to 60 artefacts at once, within 18 m of Zenny, for 40 s, then cleans up every one.
- Artefacts are voxel "sprites" in neon colours that jitter, tumble and de-rez on landing.
  They have no collision and do no damage (a static rule covers Storm code).
- Its dialogue is story-critical, so it is deferred rather than dropped.
- Persistence: a storm in progress is never saved. The fact that it happened is saved, so it
  doesn't repeat. Trigger counts come from the saved event counts.

**Also in the slice.**
- Discoveries (`kind: discovery` placements) teach memory and place knowledge when Zenny
  arrives: the 1950s diner, the Roman colonnade, the storm drain.
- NICE's ambient beat (`Event.Ambient.Tick`, every 90 s) and Zenny's silence
  (`Event.Player.Silent`, after 45 s standing still) give the dialogue director chances to speak.
  Its cooldowns, frequency setting and busy rules decide whether anyone does.
- A new world takes `-GLSettings=<preset>` for the resource-yield setting.

## Provisional (for the operator's playtest)
- All creature and weapon numbers, the respawn delay, and keeping inventory on death.
  A death penalty would be a design decision.
- Distract's key (V), cooldown and duration.
- The storm's trigger (the 2nd repair), duration, density and radius.
- The ambient beat's timing and the silence threshold.
- The defeated gremlin stays defeated after reload. It does not respawn.

## Reversal cost
Low. Everything above is data, or a constant in one small class. The rules are pure and tested.
