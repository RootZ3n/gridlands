# ADR-0012: Band, cell and era are separate axes; era is not a tech tier

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
A location is described by three **independent** axes:
1. **Grid cell**: physical region, streaming unit, physical boundaries.
2. **Progression band**: depth toward NICE's core; baseline danger and control.
3. **Era composition**: which world-memory fragments appear there and how
   coherently (coherent at the edge, colliding near the core).

- No type or data field conflates them. "Zone" is deprecated as a term.
- **Era is never a technology tier.** Materials and structural systems decide
  physical capability. Era tags drive look, fragments and building-style
  knowledge.
- Build pieces carry `era/style` and `material` as separate fields.

## Consequences
- Data: `GridCellDefinition {cell, band, eraComposition}`; band and era
  definitions are their own data; items/pieces tag era and material separately.
- A validator can reject content that tiers gameplay power by era.

## Reversal cost
High. It shapes the whole content schema, which is why it is fixed before M2.
