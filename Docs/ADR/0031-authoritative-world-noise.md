# ADR-0031: One authoritative world-noise model; creatures hear it and remember what they saw

- Status: **Accepted: operator direction, 2026-09-25 (P6 brief: "Noise is systemic").**
- Evidence: [Docs/Evidence/P6-structural-salvage](../Evidence/P6-structural-salvage/README.md)
- Amends: [ADR-0014](0014-threat-comes-from-place-not-activity.md), per its 2026-09-25 amendment
  (the operator's canonical wording is below).

## Canonical rule (operator)
> Player-generated noise may be perceived and investigated by already-existing creatures according
> to their authoritative hearing model. Noise never causes hostile spawning, summoning, raid
> generation, or attraction outside that perception model.

## Decision

### Noise events
- **A noise** (`FGLNoiseEvent`) is `{action, location, radius, instigator, investigate seconds,
  distraction?}`.
- **Loudness is data.** `tuning.world.physical` `noise.radius[action]` in metres, times the worked
  material's `noiseScale`.
- **Every noise goes through `UGLNoiseSubsystem::Emit`.** It offers the noise to every creature and
  keeps a bounded history, for tests and future visualization.

### What makes noise today

| Action | Where it is emitted |
|---|---|
| `Noise.Terrain.Dig` / `Raise` / `Flatten` | a successful terraform stroke (`UGLTerrainSubsystem::Terraform`) |
| `Noise.Salvage.Hit` (or the salvage's own `noise`, e.g. `Noise.Gather.Chop`) | every salvage hit |
| `Noise.Structure.Break` | a structural part comes away |
| `Noise.Structure.Collapse` | each collapse impact |
| `Noise.Build.Place` / `Demolish` | player building |
| `Noise.Pehlichi.Lure` | Pehlichi's distraction. It is flagged as a distraction and still deals zero damage (ADR-0017). This is now the only road by which creatures learn of it. |

### How creatures hear
**The rule** (`GLCreatureRules::Hears`, pure): a creature hears a noise when its distance is within
both the noise's radius (how far that sound carries) and the creature's hearing radius (how far it
listens). Sight and hearing are separate systems.

**What it does after hearing:**
- **An ordinary noise:** it investigates the location for `investigateSeconds`, within its leash.
  Seeing Zenny still wins.
- **Pehlichi's distraction:** it overrides even a chase, as before.

**Memory:**
- A creature that saw Zenny remembers the last known position.
- On losing sight, it **searches** there for `memorySeconds` (per creature, else tuning) before
  giving up.
- Breaking line of sight (for example by raising terrain) does not erase knowledge.

**Events:**
- `Event.Creature.Heard` for an ordinary noise; `Event.Creature.Distracted` for Pehlichi's.
- `Event.Creature.Searching`, then `Event.Creature.Lost`.

### Extensibility (preserved, not built)
- Quieter or louder tools: a per-tool scale on the same radius.
- Material-dependent noise: `noiseScale` already exists.
- Stealth gear and modifiers: multipliers on radius or hearing.
- Pehlichi's noise visualization: `GetRecent()` exposes the authoritative values.
- Hammer Toe: structural breakage already emits.

Any visualization must read these same values.

## Consequences
- **Stealth cannot be trivialised** by silent terraforming next to a creature.
- **Noise becomes a legitimate tactic.** Dig elsewhere, the creature goes to look, Zenny slips
  past; this is proven in the real game.
- **All radii are provisional data.** They are not tuned by feel.
