# ADR-0017: Pehlichi deals zero direct damage

- Status: Accepted (**hard design invariant**)
- Date: 2026-09-24
- Decider: operator (E6)

## Decision
**Pehlichi deals zero direct damage.** He is not a combat pet.

His creature- and combat-adjacent capabilities may include:
- scanning, detection and signal analysis;
- identifying weak points and exposing vulnerabilities for Zenny;
- distraction;
- disruption and jamming;
- temporary disabling;
- pacification and stabilization;
- escape and evasion assistance.

When direct physical combat begins, his behavioural preference is **flight
and avoidance**, not attack.

## Enforcement
- Pehlichi's actor and components implement no damage-dealing interface, and
  his capability data may not declare a damage effect. The data validator
  rejects any Pehlichi capability whose effect kind is damage.
- A test asserts that no Pehlichi capability or action produces a damage event.
- Changing this requires a **new operator-approved ADR**. It must not evolve
  quietly through a capability, item or balance change.

## Reversal cost
Identity-level. Only by explicit operator decision.
