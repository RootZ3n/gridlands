# Zones and progression

Status: **design, not built.** The bootstrap has one tiny test neighborhood
inside a single notional Grid cell. This document fixes the *concepts* that
later systems must respect. Exact counts, sizes and dialogue are not final.
Decision records: [ADR-0009](ADR/0009-non-combat-completion-path.md) (non-combat
completion) and [ADR-0010](ADR/0010-grid-cells-are-world-regions.md) (Grid
cells are world regions).

## 1. The World Grid

**Grid lines define regions, not surfaces.**

- The world is divided into large **Grid cells** (sectors). Each major
  gameplay zone occupies one cell. The initial planning assumption is roughly
  **1 km x 1 km** per cell, which is not a final number.
- The ordinary physical world (streets, homes, shops, parks) exists *inside*
  a cell and looks physical.
- Usable density differs by zone within the same conceptual footprint:
  - a residential Grid: several streets, dozens of homes, small businesses, parks;
  - a dense city Grid: the same footprint, far more vertical and interior space.
- **The player physically travels across the boundary** between cells to
  enter a new zone. Crossing should feel world-scale and physically meaningful,
  not like a loading-screen door.

Where the underlying Grid becomes visible:

| Visible | Not visible |
|---|---|
| at zone boundaries | across normal roads, floors, houses or terrain |
| during Pehlichi scans | as a constant graph-paper overlay |
| around glitches | |
| where the simulation is damaged | |
| during special corruption events | |

Future boundary behaviours (**not implemented**): distortion, visible digital
seams, unstable geometry, corrupted structures straddling the boundary,
locked/stabilized crossings, Pehlichi interaction, hostile-AI interference,
manipulation by an upgraded Pehlichi.

## 2. Zone progression

1. **Entering a zone**, Pehlichi estimates how many glitches the Grid holds.
   *Example, not final:* "I detect roughly 35 instability signatures in this
   Grid. We need to repair at least 20 before I can stabilize the next
   boundary."
2. **The player does not need to repair every glitch.** Repairing the zone's
   quota lets **Pehlichi stabilize/open** the boundary to the next Grid.
   Pehlichi is the actor who stabilizes, consistent with its sole repair authority.
3. **Optional remaining glitches** support exploration, completionism,
   special rewards, Pehlichi upgrades, rare resources, hidden areas, lore and
   optional guarded challenges.

### Glitch distribution and the stealth path

Illustrative only; exact counts are not locked:

| | Zone 1 | Later zones |
|---|---|---|
| total glitches | ~30-35 | similar or more |
| required to advance | ~20 | set per zone |
| guarded | few or none | a growing share |
| hostile-AI interference | little or none | patrols search for the player; Pehlichi is jammed or disrupted; more glitches are hidden or need alternate access |

**Invariant (NC-3): in every zone, the glitches repairable *without combat*
must cover the advancement quota with margin.** Guarded glitches are optional
risk/reward and can carry higher-value rewards. A later zone may raise the
guarded share but must keep a viable stealth path.

"Without combat" includes guarded glitches whose guards can be lured,
distracted, evaded or waited out, as long as a kill is never the only way.

## 3. Non-combat completion (summary of ADR-0009)

| # | Invariant |
|---|---|
| NC-1 | No progression gate (zone boundary, game completion) requires killing anything. Zone gates are glitch-repair quotas. |
| NC-2 | Every item or capability on the critical path has at least one non-combat acquisition path. Mob and boss drops are never the sole source. |
| NC-3 | Per zone, non-combat-repairable glitches >= advancement quota, with margin. |
| NC-4 | Hostile hunters and patrols can always be avoided, evaded, distracted or routed around. |
| NC-5 | Bosses are optional; their rewards are valuable and unique but not required. |
| NC-6 | The non-combat path is not easy mode: it trades combat for stealth, longer chains, harder salvage, puzzles, exploration, planning and extra repairs. |

**If a player can only advance by killing something, the non-combat path has failed.**

## 4. Home and building safety

- The starter/stabilized area is genuinely useful for low-attention play.
- Random mandatory base raids are not a requirement.
- A base the player chooses to establish in a later hostile Grid needs
  defenses. That is the risk/reward trade.
- Eventually, constructed/stabilized areas suppress hostile spawning locally,
  similar in principle to Valheim. The radius is data and is not final.

## 5. Architectural consequences (for later milestones; nothing built now)

| Concept | Intended shape |
|---|---|
| Zone | `UGLZoneDefinition` (JSON in `Data/zones/`): id, Grid cell coordinate, advancement quota, danger profile, member glitches |
| Zone progress | **derived** from persisted glitch states (repaired count per zone), never stored as a separate counter that could drift |
| Boundary | an actor per cell edge; its locked/stabilized state is a Pehlichi action gated by the quota |
| Pehlichi's estimate | a zone-level scan result, "roughly N", whose accuracy may depend on scan capability |
| Acquisition sources | items and capabilities tag their sources (`Source.Salvage`, `Source.Fabricate`, `Source.GlitchReward`, `Source.CreatureDrop`, `Source.Boss`...) so NC-2 is machine-checkable |
| Combat-free reachability | glitch definitions declare guards and requirement kinds, so NC-3 is machine-checkable per zone |
| Streaming | 1 km cells imply level streaming (likely World Partition). This is decided by its own ADR when a second zone is built, not in bootstrap |

The M2 data validator is the natural home for NC-2 and NC-3 checks once
zones, sources and guards exist in the schema. Content that would break them
should fail validation, not wait for playtesting to find it.
