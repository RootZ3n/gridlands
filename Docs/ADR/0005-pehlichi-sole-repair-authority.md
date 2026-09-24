# ADR-0005: Pehlichi is the sole glitch-repair authority

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (correction to the first proposed architecture)

## Context
The first proposal modelled Pehlichi as a follower that scans, with the player
repairing glitches through a generic interaction. The operator corrected this:
glitch repair is Pehlichi's defining role and the core of the game.

## Decision
- **Only the Pehlichi gameplay system can perform a simulation repair.** The
  player satisfies requirements and issues commands; the player has no repair
  interaction.
- Glitch lifecycle: `Latent, Detected, Repairable, Repairing, Interrupted,
  Repaired`. Every transition names an authority (`PehlichiScan`,
  `PehlichiRepair`, `World`, `Hostile`, `Player`). The table is in
  `FGLGlitchLifecycle` (Core), and the `Player` authority has no legal
  transitions.
- Access is modelled as a requirement, not a state
  (see [GLITCH-AND-PEHLICHI.md](../GLITCH-AND-PEHLICHI.md)).
- Enforced in code by passkey types (`FGLRepairAuthority`, `FGLScanAuthority`)
  constructible only by Pehlichi's repair/scan components.
- Pehlichi is built from explicit components (commands, positioning, scan,
  capability, repair), not a generic follower.

## Consequences
The first-playable loop has the player *command* scan and repair. Hidden
glitches, guards, jamming, decoys and Pehlichi-only access extend the
requirement and authority model without a rewrite.

## Reversal cost
High. This is the game's identity.
