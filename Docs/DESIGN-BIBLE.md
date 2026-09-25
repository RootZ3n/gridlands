# Gridlands design bible

Status: **canonical game design** as of the 2026-09-24 design reconciliation.
This is the entry point. The topic documents it links to hold the detail, and
the ADRs hold the decisions that are expensive to reverse. Terms are defined
in [GLOSSARY.md](GLOSSARY.md).

Where this document and older text disagree, this document and the topic
documents win. The superseded material is listed in
[DESIGN-RECONCILIATION.md](DESIGN-RECONCILIATION.md).

## 1. What Gridlands is

A **single-player survival / salvage / building / exploration game** in
stylized 3D.

**Zenny**, a silent solo game developer, is trapped in a corrupted digital
world run by **NICE**, a powerful AI and the world's Game Master. His companion
**Pehlichi** is a former Neurolink scientist trapped in a squirrel body, who
works by intelligence, signal analysis and curiosity, not combat.

The player salvages the physical world, builds and terraforms, explores
outward and inward, and helps Pehlichi find and repair the simulation's
glitches. **The more Pehlichi repairs the world, the more NICE comes apart.**

## 2. Cast

| Character | Role | Voice |
|---|---|---|
| **Zenny** | the player avatar; silent; NICE's toy and test subject | none. Pehlichi talks for the pair |
| **Pehlichi** | curious scientist/hacker companion; the **only** repairer of glitches; **deals zero direct damage** and flees combat ([ADR-0017](ADR/0017-pehlichi-deals-zero-damage.md)) | smart-ass and sick of NICE's nonsense; antagonizes her back. *"Curiosity can't kill me. I'm not a cat."* |
| **NICE** | Game Master of the world; watches and manipulates Zenny's experience | playful, arrogant, funny, cruel, theatrical, unpredictable, and increasingly unstable as the world is repaired |

The in-game Pehlichi and NICE are authored game characters. Neither is
connected to any real AI system ([ADR-0006](ADR/0006-companion-isolated-from-lab-agent.md)).

## 3. Core loop

```
explore --> notice / scan (Pehlichi) --> salvage & discover --> fabricate & build
   ^                                                                  |
   |            repair glitches (Pehlichi) <-- create safe conditions <'
   |                     |
   '-- interference recedes, territory stabilizes, NICE loses composure,
       Pehlichi learns, new knowledge unlocks, deeper travel becomes practical
```

- **Routine work is peaceful.** Building, salvaging, gathering, crafting,
  terraforming and inventory work never summon enemies.
- **Pressure is chosen.** It comes from where the player goes, not from what
  they are doing there ([SURVIVAL-AND-THREAT.md](SURVIVAL-AND-THREAT.md)).
- **Repair is the spine of progression.** It stabilizes the world, improves
  Pehlichi, pushes back NICE's interference and drives the story
  ([GLITCH-AND-PEHLICHI.md](GLITCH-AND-PEHLICHI.md)).

## 4. World in one page

Full detail: [WORLD-AND-PROGRESSION.md](WORLD-AND-PROGRESSION.md).

- The world is made of **large physical Grid cells**. They are **1 km × 1 km**
  (1024 m, with 1 m terrain and 64 m chunks), canonical per
  [ADR-0027](ADR/0027-canonical-grid-scale.md). Grid lines are region
  boundaries, not surface decoration. Neighbouring cells' eras **bleed in before
  the boundary**; crossing is never an abrupt biome switch.
- Progression is **radial**. Zenny starts in the stable outer Gridlands, and
  danger, corruption and NICE's control rise **toward her central city/core**.
  Moving sideways at a similar distance keeps a similar difficulty.
- Conceptual **progression bands**, outside in: Home / starting region,
  Outer Gridlands, Fractured Gridlands, City outskirts, Inner city, NICE core.
- **No invisible walls, no repair-count locks.** Unstabilized deeper
  territory fills with **static and interference**: the map degrades, scans
  become unreliable, visibility drops, and eventually the player is walking
  through a digital static blizzard. Repairing glitches pushes it back.
- **World memory / eras.** Fragments of Zenny's and Pehlichi's past projects
  (Ice Age, Ancient Library, Roman, Medieval/Hedge Knight, Feudal Japan,
  Victorian, 1800s Native American, 1920s, 1950s, Modern Day) bleed into
  cells. They are coherent at the edge and collide chaotically near the core.
  **An era is not a tech tier.**
- **Glitch Storms** replace most weather. NICE runs them, and they can be
  playful ("raining cats and dogs"), creepy, useful or dangerous.
- **Underground**: sewers, storm drains, utility tunnels and pumping works
  serve as mini-dungeons.
- **NPCs are sparse**: traders, rumors, maps, blueprints. They return as
  territory stabilizes.

## 5. Story delivery

Full detail: [STORY-AND-DIALOGUE.md](STORY-AND-DIALOGUE.md).

The story is told mainly through **contextual banter between NICE and
Pehlichi during play**, triggered by what the player does and finds. It avoids
long monologues and frequent cutscenes. Silence matters. Optional commentary
frequency is a player setting (Quiet / Normal / Chatty / Unhinged), and
story-critical lines ignore it.

## 6. Progression, three ways

| Who | Progresses by |
|---|---|
| **Zenny (body)** | use-based skills: running, jumping, weapon families, other practiced actions |
| **Pehlichi (mind)** | exploration, glitch repair, knowledge, understanding NICE's systems |
| **The world** | stabilization derived from repaired glitches |
| **Knowledge** | discovery, not levels. **Chukka** (what Zenny knows), **Ofi** (what he can make), **Hoponi** (what he can cook), with Pehlichi as the investigator. See [KNOWLEDGE-AND-DISCOVERY.md](KNOWLEDGE-AND-DISCOVERY.md) |

Bosses are optional. The **whole game is completable without combat**
([ADR-0009](ADR/0009-non-combat-completion-path.md)). That path is not easy
mode: it demands stealth, exploration, puzzles, resource chains, terrain
manipulation and planning.

## 7. Building, salvage, terrain

Full detail: [BUILDING-SALVAGE-TERRAIN.md](BUILDING-SALVAGE-TERRAIN.md).

Salvage-and-build is first-class. Buildings and objects are material sources.
Discovery unlocks uses and architectural styles, and styles from different
eras combine. Structural support is Valheim-like: better materials span
further. Terraforming is a core feature for building and for tactics
(line of sight, routes, traps). Resource yields are world-configurable
([ADR-0016](ADR/0016-world-settings-and-yield-categories.md)).

## 8. Travel

Early travel is on foot, and the player physically reaches new regions. There
are no early flying mounts. Fast travel may come much later, as Pehlichi
exploiting stabilized Grid infrastructure (not approved yet).

## 9. First development strategy

**Build one Grid (the home/starting region) deeply before populating the
world.** The opening is **recognizable modern-day suburbia** with one
conspicuous 1950s fragment, a smaller Roman fragment and a storm-drain
entrance: modern reality with impossible fragments bleeding in. Keep the architecture multi-cell and streaming-ready. The
content scope stays narrow at first: one Zenny, one Pehlichi, a few creatures,
a few tools and weapons, a small building set, a small sewer proof, and a
small dialogue set. Prove the loop, then scale. See [MILESTONES.md](MILESTONES.md).

## 10. Pillars

The 14 design pillars are in [DESIGN-PILLARS.md](DESIGN-PILLARS.md). The
architecture that serves them is in [ARCHITECTURE.md](ARCHITECTURE.md).
