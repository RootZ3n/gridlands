# ADR-0003: No Gameplay Ability System during bootstrap

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
Do not adopt GAS. Use small interfaces/components for stats, effects,
equipment, damage and capabilities (`IGLDamageable`, `UGLCapabilityComponent`,
and so on).

Keep the seam, not a speculative abstraction: nothing outside those
components mutates stats directly, so a future GAS adoption replaces their
insides. Do not build abstraction layers solely in anticipation of GAS.

## Reversal cost
Medium, bounded by the seam.
