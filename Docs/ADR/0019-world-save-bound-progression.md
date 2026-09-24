# ADR-0019: Progression is bound to the world save

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (E3)

## Decision
One save is one world. The world save holds:
- Zenny's skills;
- knowledge and discoveries;
- inventory;
- Pehlichi's upgrades and capabilities;
- repaired glitch state;
- all world and player progression.

There is no separate portable character file. Transferable characters and
New Game+ are **not** architected now; they would need their own ADRs later.

## Consequences
- One save schema and one migration chain.
- Knowledge ids are world-scoped.

## Reversal cost
Medium. Splitting a character save out later is a migration.
