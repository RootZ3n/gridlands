# ADR-0011: Radial progression bands and a soft interference barrier; no locks

- Status: Accepted
- Date: 2026-09-24
- Decider: operator
- Supersedes: the zone-quota gating in the first `ZONES-AND-PROGRESSION.md`
  (same day). Amends [ADR-0009](0009-non-combat-completion-path.md) and
  [ADR-0010](0010-grid-cells-are-world-regions.md).

## Context
The first zone model was a linear chain: repair a quota of a zone's glitches,
then Pehlichi opens the next boundary. The operator rejected hard locks and
linear zones.

## Decision
- Progression is **radial**: bands of depth toward NICE's central core (Home,
  Outer Gridlands, Fractured Gridlands, City outskirts, Inner city, NICE
  core). The number of cells is not fixed.
- Moving inward raises danger, corruption and NICE control; moving sideways
  at similar depth keeps difficulty similar.
- **No invisible walls. No repair-count locks. Crossings are never locked.**
- The barrier is **interference**: digital static that degrades map, minimap,
  visibility and scan reliability, escalating to a static blizzard beyond
  stabilized territory. Repairing glitches pushes it back.
- Interference is derived (ADR-0013), never a stored gate.

## Consequences
- No `quota` field, no boundary-lock state, no "zone complete" flag.
- NC-3 is restated per band: combat-free stabilization must suffice to bring
  the next band to a traversable interference tier.
- Interference is a cross-cutting input to map UI, scans, visibility and
  (optionally) creature behaviour.

## Reversal cost
High once content is tuned around interference; low today.
