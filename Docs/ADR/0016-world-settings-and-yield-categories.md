# ADR-0016: World settings; yields scale by category, and progression is never scaled

- Status: Accepted; category rule confirmed by the operator (E1)
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

## Category rule (operator decision E1)
**Principle: discovery and access establish rarity and progression. Once a
repeatable resource source has been legitimately discovered or reached, the
player's chosen abundance setting respects their time.**

| Scales with the applicable yield setting | Never scales |
|---|---|
| repeatable common salvage | unique / one-off rewards |
| repeatable gathering and mining | glitch progression rewards |
| **repeatable rare-material yields** | knowledge / unlocks |
| repeatable creature drops | blueprints where the blueprint itself is the reward |
| | quest / story items |
| | unique artifacts |

Categories are data (`Yield.*` tags, each declaring `scalable` and, if
scalable, which world setting applies). The validator enforces the right-hand
column: any yield of those kinds in a scalable category is rejected.

## Consequences
- The M2 item/salvage schema carries a yield category on every yield entry.
- The Core yield computation takes world settings as an input and is
  unit-tested per category.

## Reversal cost
Medium. Retrofitting categories onto authored content is tedious.
