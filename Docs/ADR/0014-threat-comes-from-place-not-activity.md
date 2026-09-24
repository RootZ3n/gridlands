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
