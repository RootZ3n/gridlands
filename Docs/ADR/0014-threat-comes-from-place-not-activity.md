# ADR-0014: Threat comes from place and choice, never from routine activity

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
- Routine activity (building, salvaging, gathering, mining, crafting,
  terraforming, inventory work) **never** spawns or attracts hostiles because
  of the activity. There is no noise-attracts-raid mechanic, no build-count
  raid and no mandatory base raid.
- Every hostile presence has a **spatial or authored source**: creature
  territory, patrol route, encounter volume, NICE-controlled area, or a band
  spawn table.
- Stabilization suppresses hostile presence locally (data-driven radius and
  strength).
- NICE's hunters in deeper bands are bound to places and bands, not provoked
  by base-building.

## Consequences
- The spawn system exposes only spatial/authored sources; there is no
  activity-to-threat API.
- A test can assert that routine actions emit no spawn requests.
- The home region supports long, low-attention play.

## Reversal cost
Medium in code, high in player trust once shipped.

## Amendment, 2026-09-25 (operator direction in the P6 brief: "Noise is systemic")
**What changes.** Routine activity (digging, raising, flattening, chopping, mining, salvage,
building, demolition, structural collapse) now emits **authoritative world-noise events**.
Creatures **already present** within their own authoritative hearing range may hear one and
investigate it.

**What does not change:**
- no activity spawns, summons, calls in or raids anything;
- there is no activity-to-spawn API;
- every hostile still has a spatial or authored source;
- no hostile is reached from beyond its hearing.

"Attracts" in the original decision is therefore narrowed to *creates or calls in*. Local hearing
by creatures already there is a stealth rule, not a threat source (SURVIVAL-AND-THREAT §9).

**Confirmed by the operator (canonical wording):** "Player-generated noise may be perceived and
investigated by already-existing creatures according to their authoritative hearing model. Noise
never causes hostile spawning, summoning, raid generation, or attraction outside that perception
model." It supersedes the earlier broad wording only as far as local creature hearing requires.
Implemented in [ADR-0031](0031-authoritative-world-noise.md).
