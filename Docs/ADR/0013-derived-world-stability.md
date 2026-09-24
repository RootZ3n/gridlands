# ADR-0013: World stability, interference and NICE's composure are derived

- Status: Accepted
- Date: 2026-09-24
- Decider: operator ("derive progress from persisted glitch state"), shape proposed by agent

## Decision
Persisted glitch lifecycle states (plus static definition data) are the
**only** source of truth for progression. The following are **computed, never
stored as independent counters**:
- **stability** per cell (and its influence on neighbours);
- **interference** at a location (band baseline, minus stability, plus active
  NICE phenomena);
- **band progress**;
- **NICE's composure** (global).

The computations are **pure functions in GridlandsCore**: deterministic,
headless-testable, and fed by glitch definitions (weights, cell membership)
and states. Runtime systems may cache results but must be able to rebuild
them from a save.

## Consequences
- No drift between counters and world state; load always reconstructs truth.
- Tuning is data (weights, falloff, tier thresholds).
- Dialogue, map, scans, spawning and storms read the same derived values.

## Reversal cost
Medium. Stored counters could be added later, but deriving is the cheaper
default to keep.
