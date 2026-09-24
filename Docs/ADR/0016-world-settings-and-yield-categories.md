# ADR-0016: World settings; yields scale by category, and progression is never scaled

- Status: Accepted (category list pending operator confirmation, see DESIGN-RECONCILIATION E)
- Date: 2026-09-24
- Decider: operator

## Decision
- Each world has **world settings**, including yield multipliers **per yield
  category**, not one global rate. The commentary frequency setting is a
  player setting, kept separately.
- Every yield (salvage, gathering, drops) names a category and passes through
  its multiplier. No yield bypasses settings.
- Discovery unlocks, glitch rewards, unique rewards and progression-critical
  knowledge are in **non-scalable** categories. Abundance settings never
  bypass discovery, glitch progression or unique rewards.

## Consequences
- The M2 item/salvage schema carries a yield category on every yield entry.
- The Core yield computation takes world settings as an input and is
  unit-tested per category.

## Reversal cost
Medium. Retrofitting categories onto authored content is tedious.
