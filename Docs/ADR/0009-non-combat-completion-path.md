# ADR-0009: The full game is completable without combat

- Status: Accepted (core design requirement)
- Date: 2026-09-24
- Decider: operator

## Context
Gridlands must support a stealth, casual, low-combat or zero-kill completion
path. If this is treated as an afterthought, it gets broken silently: a recipe
that needs one mob drop, or a guarded glitch counted toward a quota, is enough
to break it.

## Decision
Invariants NC-1..NC-6 in [ZONES-AND-PROGRESSION.md](../ZONES-AND-PROGRESSION.md) section 3:
- Zones advance by repairing a quota of glitches with Pehlichi. No progression
  gate requires a kill.
- Bosses are optional. Mob/boss drops are never the sole source of anything on
  the critical path.
- Every zone's non-combat-repairable glitches cover its quota with margin.
- Hunters and patrols are always avoidable, evadable, distractible or routable-around.
- Combat rewards may be better, faster or unique, but never hidden gates.
- The non-combat path trades combat difficulty for other difficulty. It is not easy mode.

It is architectural, not only design, because it constrains the data schema
and the validator:
- items and capabilities declare their acquisition **sources** (tags);
- glitch definitions declare **guards** and requirement kinds;
- zones declare their **quota**;
- the data validator (M2 onward, once these fields exist) fails content where
  a critical-path item has only combat sources, or a zone's combat-free glitches
  fall below its quota.

A guard requirement such as `NoHostilesWithin` counts as combat-free only if it
can be satisfied by luring, distracting, evading or waiting, not solely by killing.

## Consequences
- Content authors (human or agent) get a machine check, not a style guide.
- Designing bosses and drops is freed up: they can be generous, because
  they can never be required.

## Reversal cost
High. Reversing it would change what the game is.
