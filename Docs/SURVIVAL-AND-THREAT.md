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
| T-1 | No spawn, aggro or encounter is triggered by routine activity (build, salvage, gather, craft, terraform, inventory). |
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

## 4. Zenny's progression: use-based skills

Zenny improves physically by **doing**: running, jumping, weapon families and
other practiced actions each gain proficiency with use (Valheim-like). The
skill list and curves are not fixed. Skills are separate from Pehlichi's
capabilities.

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
