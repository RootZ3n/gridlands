# World and progression

Status: **design; not built.** The bootstrap has one small slice of the home
region. This document fixes the concepts that later systems must respect.
Exact sizes, counts and thresholds are not final.

Decision records:
- [ADR-0010](ADR/0010-grid-cells-are-world-regions.md): Grid cells are world regions.
- [ADR-0011](ADR/0011-radial-bands-and-soft-interference.md): radial bands, soft interference, no locks.
- [ADR-0012](ADR/0012-world-axes-band-cell-era.md): band, cell and era are separate axes.
- [ADR-0013](ADR/0013-derived-world-stability.md): stability and NICE's composure are derived.
- [ADR-0009](ADR/0009-non-combat-completion-path.md): non-combat completion.

> Superseded: an earlier version of this document (`ZONES-AND-PROGRESSION.md`)
> gated each zone behind a glitch-repair **quota** that unlocked the next
> boundary. The operator replaced that with the soft interference model
> below. There are no repair-count locks.

## 1. Three separate axes

A location in Gridlands is described by **three independent things**. Code
and data must never merge them.

| Axis | Question it answers | Examples |
|---|---|---|
| **Grid cell** | *Where physically?* A large world region with physical boundaries. | the cell at (3, -1) |
| **Progression band** | *How deep toward NICE?* Sets baseline danger, corruption and NICE's control. | Home, Outer Gridlands, Fractured Gridlands, City outskirts, Inner city, NICE core |
| **Era composition** | *What world-memory is here?* Which project fragments bleed into this place, and how coherently. | mostly 1950s suburb with a Roman aqueduct fragment |

- A cell has one band (derived from its distance to the core, adjustable per
  cell in data) and an era composition (a weighted mix, possibly varying
  within the cell).
- **Era is not a technology tier.** Materials and structural systems decide
  what the player can physically do. A Roman fragment is not "early game" and
  a Modern fragment is not "late game".

## 2. Grid cells

- Each cell is a large physical region. The planning assumption is **about
  1 km x 1 km**, not final.
- Usable density varies: a residential cell holds streets, dozens of homes,
  small shops and parks, while a city cell of the same footprint holds far
  more vertical and interior space.
- **Grid lines are world-region boundaries, not surface decoration.** They
  are never painted across ordinary roads and houses.
- Crossing a boundary should feel world-scale and physical. Later it may bring
  distortion, visible seams, unstable geometry, structures straddling the
  line, or hostile interference. **Crossings are never locked.**
- The number of cells is **not fixed**. Think in bands, not in a 3x3 board.

## 3. Radial progression

```
          Home / starting region
        Outer Gridlands              <- relatively stable, coherent eras
      Fractured Gridlands
    City outskirts
  Inner city                         <- eras collide, heavy NICE control
NICE core                            <- destination; chaotic as NICE unravels
```

- The progression is analogous to radial biome difficulty, with NICE's city
  as the destination at the centre.
- **Moving inward** raises baseline danger, corruption, interference and NICE's
  control. **Moving sideways** at a similar depth keeps a similar difficulty.
- The player **may physically enter deeper territory at any time.**

## 4. Static and interference: the soft barrier

Unexplored and unstable areas are covered by **digital static / white noise**
instead of ordinary map fog.

Interference at a place is **derived** ([ADR-0013](ADR/0013-derived-world-stability.md))
from:
- its band's baseline;
- how much of the surrounding territory has been stabilized by repaired glitches;
- active NICE phenomena (e.g. Glitch Storms).

As the player pushes beyond stabilized territory, effects escalate in tiers
(illustrative names; thresholds not final):

| Tier | Effects |
|---|---|
| Clear | full map and minimap; reliable scans |
| Hazy | map reliability degrades; minimap noise; scans lose confidence |
| Static | minimap largely unusable; visibility drops; scans unreliable; environmental interference |
| Blizzard | a digital static storm: near-zero map, poor visibility, heavy interference |

Repairing glitches **pushes the static back** and makes deeper exploration
practical. That is the whole barrier. **There are no invisible walls and no
"repair N glitches before crossing" locks.**

## 5. Stabilization and NICE's unraveling

Repairing a glitch:
- stabilizes its surroundings and reduces NICE's regional control;
- reduces interference, making travel and settlement safer;
- can let traders return;
- improves Pehlichi and advances the story.

**The more Pehlichi repairs the world, the more NICE comes apart.** NICE's
composure is a derived quantity. It is high at the start and falls as the world
is repaired, and it drives her dialogue and the escalating chaos of the core.

Nothing here is a separately stored counter. Cell stability, interference,
band progress and NICE's composure are **computed from persisted glitch
states** plus static data.

## 6. Pehlichi's regional estimate

Entering a region, Pehlichi can estimate its instability. *Example voice, not
final:* "I'm reading roughly 35 instability signatures in this Grid." The
estimate informs play; it is **not a gate**. Its accuracy degrades with
interference and improves with Pehlichi's scan capability.

## 7. World memory / eras

Known fragments (from existing projects): Ice Age, Ancient Library, Roman,
Medieval / Hedge Knight, Feudal Japan, Victorian, 1800s Native American,
1920s, 1950s, Modern Day.

- These are **memory fragments that bleed into cells, not ten zones.**
- Outer areas are more coherent, with occasional anomalies.
- Moving inward, the mixing increases.
- Near the core, incompatible eras and environments collide chaotically.
- Scanning era architecture teaches building styles
  ([BUILDING-SALVAGE-TERRAIN.md](BUILDING-SALVAGE-TERRAIN.md)).

## 8. Glitch Storms (NICE's weather)

Traditional weather is supplemented, and largely replaced, by **Glitch
Storms** that NICE controls. They can be playful, useful, creepy or
dangerous. Examples:
- tiny, obviously glitched cats and dogs falling like rain, followed by NICE
  noting it is "raining cats and dogs";
- a winter storm that leaves a snowman holding a "STAY FROSTY" sign;
- static blizzards;
- pixel/corruption effects;
- gravity or simulation anomalies;
- storms that expose hidden glitches or open opportunities.

Storms follow the threat rule ([ADR-0014](ADR/0014-threat-comes-from-place-not-activity.md)):
they are scheduled by NICE and shaped by place, never triggered by routine
player activity.

## 9. Underground

Mini-dungeons in the spirit of Valheim's burial chambers: **sewers, storm
drains, utility tunnels, pumping infrastructure**, and later deeper urban
systems. Creatures: rats, snakes, later alligators or corrupted equivalents.
They offer salvage unavailable in houses, hidden glitches, puzzles, alternate
routes, and possibly crossings beneath Grid boundaries.

## 10. NPCs

Sparse, so the world does not feel empty. They serve as traders, rumor
sources, map and blueprint sellers, and revealers of hidden glitches.
Stabilizing territory lets traders return. **Gridlands is not a
quest-hub RPG.**

## 11. Travel

On foot early; the player physically reaches new regions, with no early
flying mounts. Much later, fast travel may exist as Pehlichi exploiting
stabilized Grid infrastructure. That is not approved and not designed yet.

## 12. Non-combat completion in this world (ADR-0009, restated for bands)

| # | Invariant |
|---|---|
| NC-1 | Nothing required for progression or completion requires a kill. Progression is stabilization, not kills. |
| NC-2 | Every item, capability or piece of knowledge on the critical path has at least one non-combat acquisition path. Drops are never the sole source. |
| NC-3 | For every band, the stabilization achievable from **combat-free glitches** is enough to bring the next band's interference down to a traversable tier. |
| NC-4 | Hostile creatures, patrols and NICE's hunters can always be avoided, evaded, distracted, disabled non-lethally or routed around. |
| NC-5 | Bosses are optional; their rewards are valuable and unique but never required. |
| NC-6 | The non-combat path is not easy mode. |

A guarded glitch counts as combat-free only if its guards can be lured,
distracted, evaded, disabled non-lethally or waited out. Later bands may
guard a larger share of glitches but must keep NC-3 true.

## 13. Home and settlement

- The home/starting region is genuinely useful for long, low-attention play.
- There are no mandatory base raids.
- Stabilized territory becomes safer; the player manufactures tranquility by
  repairing the world.
- A base built voluntarily in deeper, hostile territory needs defenses.
- Eventually, constructed/stabilized areas suppress hostile spawning locally,
  similar in principle to Valheim. The radius is data and not final.
