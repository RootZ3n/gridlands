# Survival, threat, combat and economy

Status: **design; mostly not built.** Decision records:
[ADR-0014](ADR/0014-threat-comes-from-place-not-activity.md) (threat model),
[ADR-0009](ADR/0009-non-combat-completion-path.md) (non-combat completion),
[ADR-0016](ADR/0016-world-settings-and-yield-categories.md) (world settings and yields).

## 1. Pressure philosophy

- **Combat must not constantly interrupt ordinary play.**
- **Routine activity never summons hostiles.** Building, salvaging,
  gathering, mining, crafting, terraforming and inventory work do not spawn
  or attract enemies *because of the activity*.
- **Threat comes from place and choice:**
  - location and progression band;
  - exploration decisions;
  - authored encounters;
  - creature territories and patrols;
  - NICE-controlled areas;
  - NICE's hunters in deeper bands (bound to places, not provoked by building).
- **No mandatory base raids.** Stabilized territory becomes safer. Players
  manufacture tranquility by repairing the world.

### Threat invariants

| # | Invariant |
|---|---|
| T-1 | No **spawn, summons, raid or encounter** is triggered by routine activity (build, salvage, gather, craft, terraform, inventory). *Amended 2026-09-25 (§9, ADR-0014 amendment):* routine activity makes **authoritative noise**, and creatures **already present** within their hearing range may hear it and investigate. Noise never creates, calls in or reaches hostiles beyond their own hearing. |
| T-2 | Every hostile presence has a spatial or authored source: territory, patrol route, encounter volume, NICE-controlled area or band spawn table. |
| T-3 | Stabilization reduces hostile presence locally (radius and strength are data). |
| T-4 | Home/stabilized territory supports long, low-attention play without mandatory combat. |

## 2. Combat and the non-combat path

Combat is a major, enjoyable path, and a **complete non-combat path stays
viable** (invariants NC-1..NC-6 in
[WORLD-AND-PROGRESSION.md](WORLD-AND-PROGRESSION.md) section 12). The
non-combat path is **not easy mode**: it costs more exploration, stealth,
puzzles, resource chains, terrain manipulation and planning.

Non-combat means:
- stealth, avoidance, distraction, line-of-sight breaking;
- terrain manipulation to create routes, walls and traps;
- non-lethal tools, for example Faraday-style traps and temporary disabling;
- Pehlichi's support: scans, hidden-glitch detection, **weak-point detection**,
  and eventually creature disruption and pacification.

Higher-level corruption can interfere with Pehlichi's ability to identify
creature weak points. The same interference model that degrades maps and
scans applies here.

Bosses are optional. Their rewards are valuable and unique, and never required.

## 3. Creatures

- **Early**: clearly glitched versions of familiar animals. They are
  deliberately not ordinary real animals, which gives the player an empathy
  choice without the discomfort of real ones.
- **Later**: fantasy, sci-fi and game-world creatures leaking from Zenny's
  projects.
- **Underground**: rats, snakes, later alligators or corrupted equivalents.
- Behaviour dispositions remain data: `Passive`, `Territorial`, `Guarding`,
  `Hunting`, plus a **patrol** route option and a **non-lethal outcome**
  (disabled, pacified, fled) alongside death.
- **Drops are bonuses or alternate sources**, never mandatory critical-path
  resources.

## 4. Zenny's progression: use-based mastery (LOCKED DESIGN INTENT, operator, 2026-09-26; NOT IMPLEMENTED)

**North star: do the activity, become better at that activity.** The broad principle is the kind of
progression the operator enjoys in RuneScape / RuneScape: Dragonwilds. That is inspiration, not an
implementation to copy.

| Doing | Improves |
|---|---|
| cooking | Cooking mastery |
| building | Building mastery |
| salvaging | Salvage mastery |
| using ranged weapons | the relevant combat mastery |
| sneaking | Stealth mastery |
| traversal | the relevant movement mastery |

**Gridlands must not become a repetitive grind game.**
- Never assume "perform a trivial action thousands of times -> maximum mastery".
- Progression favours **meaningful use, challenge, discovery and increasing capability**.
- Trivially repeated actions may give diminishing progression.

**What mastery gives** is mainly capability, efficiency, reliability, technique, new interactions
and new ways of solving problems. It gives numbers only secondarily.

**Four separate axes; never merge them:**

| | Answers |
|---|---|
| **Skills** | how capable Zenny is at doing something |
| **Chukka** | what Zenny and Pehlichi know, and how knowledge connects |
| **Ofi** | fabrication and blueprint knowledge: what can be made |
| **Hoponi** | culinary and recipe knowledge: what can be cooked |

**A high Building skill never teaches an undiscovered architectural technique.** Knowledge and
discovery, and practical mastery, are complementary axes
([KNOWLEDGE-AND-DISCOVERY](KNOWLEDGE-AND-DISCOVERY.md)).

**Failure teaches too:** success, exploration, building, cooking and failure can all teach Zenny
(section 4a). This integrates with the skill and knowledge architecture, never as an isolated
"death XP" grind.

The skill list and curves are not fixed. Skills stay separate from Pehlichi's capabilities.

## 4a. Death and failure-forward (LOCKED DESIGN INTENT, operator, 2026-09-26; NOT IMPLEMENTED beyond respawn)

**Gridlands does not use corpse runs.** Death never:
- drops Zenny's equipped gear;
- drops his inventory for retrieval;
- requires travel back to the death location to recover possessions;
- removes XP;
- creates a punitive recovery chore.

**Zenny keeps his possessions,** and death returns the player to meaningful play quickly.

**No invented replacement punishments.** Durability loss, XP debt, inventory loss, long debuffs and
similar penalties are **not** implicitly required. Their absence is acceptable.

> **"Death creates a story and an opportunity to learn, not a chore."**

**Novel deaths may teach.** Zenny and Pehlichi can learn from *how* Zenny died, preferably as
**information, understanding or new capability** rather than generic stat inflation. Directions,
not specifications:

| Death | May teach |
|---|---|
| structural collapse | structural hazard knowledge |
| creature attack | creature or attack knowledge |
| poison | toxin or ingredient knowledge |
| environmental exposure | survival knowledge |
| a fall | traversal knowledge |
| a dungeon mechanic | encounter knowledge |

Possible shapes, not to be implemented yet:
- Pehlichi recognising an attack earlier;
- Chukka recording a newly understood hazard;
- structural instability becoming easier to identify;
- unknown creature behaviour becoming documented.

**Repeated identical deaths give sharply diminishing or zero further progression,** so deliberate
death farming is never optimal.

**The wider principle: Gridlands lets failure teach the player character.**

**Today's build agrees with this.** Death respawns Zenny after a delay, with inventory and gear
untouched (`UGLCombatComponent`). There is no learning-from-death yet.

## 5. Pehlichi's progression

Pehlichi grows through exploration, glitch repair, knowledge, and
understanding NICE's systems. Growth takes the form of capability levels
(scan strength and range, weak-point analysis, distraction, disruption/
jamming, temporary disabling, pacification, escape assistance, later
traversal). **None of them deals damage** ([ADR-0017](ADR/0017-pehlichi-deals-zero-damage.md)).
All progression lives in the world save ([ADR-0019](ADR/0019-world-save-bound-progression.md)).

## 6. Resource economy

- **Yields are world-configurable.** The economy must not require days of
  repetitive gathering just to build.
- Yields are adjusted by **category**, not by one global rate (ADR-0016,
  operator decision E1). **Principle: discovery and access establish rarity
  and progression. Once a repeatable source has been legitimately discovered
  or reached, the player's abundance setting respects their time.**

  | Scales with the applicable yield setting | Never scales |
  |---|---|
  | repeatable common salvage, gathering, mining | unique / one-off rewards |
  | **repeatable rare-material yields** | glitch progression rewards |
  | repeatable creature drops | knowledge / unlocks |
  | | blueprints where the blueprint itself is the reward |
  | | quest / story items; unique artifacts |
- Adjusting abundance must never bypass discovery, glitch progression, unique
  rewards or progression-critical knowledge.
- No system hard-codes an assumption about a single global yield rate.

| # | Invariant |
|---|---|
| E-1 | Every yield passes through a world-settings multiplier for its category; no yield bypasses the settings. |
| E-2 | Progression-critical, discovery and unique rewards are in non-scalable categories. |

## 7. Readable telegraphs (LOCKED DESIGN INTENT, operator, 2026-09-25; NOT IMPLEMENTED)
**WildStar-style gameplay readability is a major inspiration.** The world communicates hazards
clearly.
- Enemy attacks may use **truthful ground/space telegraphs**: cones, circles, lines, sweeps,
  charges and other shapes.
- **The rendered danger area derives from the same authoritative gameplay parameters as the
  damaging area.** A telegraph never lies about hit geometry. In code, the telegraph is drawn
  from the data that decides the hit; the two are never separate values that could drift.

## 8. Perception and stealth visualization (LOCKED DESIGN INTENT; NOT IMPLEMENTED)
- **Sight and hearing remain separate systems.** Today creatures have `perception.sightRadius`,
  a view cone and `hearingRadius`.
- **Pehlichi may expose perception information for analysed/scanned creatures:**
  - the actual sight cone and effective range;
  - alert state, and hearing information where appropriate;
  - later, possibly patrol prediction.
- **The eventual player option** is *Stealth Visualization = Off / Contextual / Always*. The exact
  UI is not locked.
- **Every visualization consumes the same authoritative values the AI uses.**

## 9. Noise is systemic (LOCKED DESIGN INTENT; first slice IMPLEMENTED in P6, [ADR-0031](ADR/0031-authoritative-world-noise.md))
**Which actions make noise.** Terrain manipulation must not be silent. These actions generate
**authoritative world-noise events**:
- digging, raising and flattening;
- chopping and mining;
- salvage;
- building and demolition;
- structural breakage and collapse.

**Why.** Otherwise the player could silently terraform line-of-sight blockers next to enemies
and trivialise stealth.

**Noise as a tactical tool.** Dig, build or break something elsewhere; the creature
investigates; its sight cone moves; Zenny slips past.

**Rules:**
- **Hearing only.** Noise reaches creatures only within their authoritative hearing (data). It
  never spawns or summons anything (T-1, ADR-0014 as amended).
- **Losing line of sight does not erase knowledge.** A creature that detected Zenny keeps a
  last-known position and investigates it.
- **Loudness varies.** Material and action type may set noise radius and intensity.
- **Visualization must be honest.** Pehlichi may eventually preview noise propagation, and any
  visualization consumes the same values used by AI hearing.

**What P6 built:**
- one noise model, with radii as data;
- terraforming, salvage, chopping, building, demolition, breakage, collapse and Pehlichi's lure
  all emit;
- creatures hear only within `min(noise radius, hearing)`, investigate, and search the last known
  position after losing sight.

Proven in the real game and in tests.

**Extensibility to preserve, not build yet:**
- quieter or louder tools;
- material-dependent noise;
- Pehlichi's noise visualization;
- stealth gear and modifiers;
- structural weapons such as Hammer Toe.

## 10. Dungeons (LOCKED DESIGN INTENT; NOT IMPLEMENTED)
See [DUNGEONS-AND-LEGENDARIES.md](DUNGEONS-AND-LEGENDARIES.md). The canonical starting rule is **one
handcrafted dungeon per zone**. Dungeons are voluntary and never make combat mandatory.

## 11. Hunger and thirst: expedition preparation (LOCKED DESIGN INTENT; NOT IMPLEMENTED; nothing tuned)
**Hunger and thirst prepare expeditions. They are not base-maintenance chores.**
- **Inside the home comfort area,** depletion **pauses**.
  - Meters do **not** magically refill just because Zenny came home.
  - The radius is about **100 m** from the recognised home/base as the initial assumption. It is
    data-driven and tunable.
- **Outside the comfort area,** depletion resumes.
- **Future outposts and camps** may give smaller rest/comfort effects.
- **Zero hunger or thirst does not simply kill Zenny.** It mainly reduces expedition benefits and
  capability.
- **Hoponi owns learned recipe knowledge.**
- **Food and drink buffs serve non-combat players too:**
  - exploration and stamina;
  - gathering and salvage;
  - stealth;
  - building and fabrication;
  - environmental resistance;
  - other activities.
- **Digestive effects stay part of food** ([KNOWLEDGE-AND-DISCOVERY §5](KNOWLEDGE-AND-DISCOVERY.md)):
  - lower-quality or problematic food may cause visible and audible flatulence;
  - higher cooking quality reduces or removes the probability;
  - late-game players normally have to *choose* poor food to fart often;
  - food tooltips may show the flatulence probability.
- **A fart is a real world event:**
  - an optional sound;
  - a small, readable, stylized **green puff**, so muted players understand what happened;
  - a **noise event through §9**, with AI hearing consequences;
  - a possible Zenny reaction;
  - possible but **not guaranteed** Pehlichi/NICE dialogue.
