# ADR-0015: Story is delivered by a data-driven contextual dialogue director

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (design); system shape proposed by agent

## Decision
- The primary narrative channel is contextual NICE/Pehlichi banter during
  play. Zenny is silent. Cutscenes and monologues are rare.
- Implemented as a **dialogue director**: gameplay systems emit tagged events;
  the director selects authored **exchanges** from JSON pools using
  conditions, priority, cooldowns, use caps and a global silence gap.
  Selection is a pure, seeded function in Core.
- **Categories:** `StoryCritical` (always delivered), `Contextual`, `Ambient`.
- A player setting (**Quiet / Normal / Chatty / Unhinged**) scales optional
  categories only.
- Dialogue history and story flags persist; NICE's composure is derived
  (ADR-0013).
- All content is authored. No runtime model generation (ADR-0006).

## Consequences
- A gameplay event bus (tags + payload) becomes shared infrastructure; systems
  never call dialogue directly.
- Agents can write and test dialogue content as JSON.

## Reversal cost
Medium. The event bus is cheap to keep and expensive to retrofit.
