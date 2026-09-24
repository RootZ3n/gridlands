# Design pillars

These are constraints, not aspirations. When a system decision contradicts a
pillar, the pillar wins unless the operator changes it. **(firm)** marks a
pillar backed by an ADR; changing one of those is an operator decision on the
record. Overview: [DESIGN-BIBLE.md](DESIGN-BIBLE.md).

## The fourteen pillars

1. **NICE is the Game Master.** The world is hers: its storms, its
   interference, its encounters. The player is always inside her game, and
   she makes sure he knows it.

2. **Zenny is the silent player avatar and NICE's toy.** He never speaks.
   NICE plays with him and taunts him at least as much as she spars with
   Pehlichi.

3. **Pehlichi is the curious scientist/hacker companion, not a combat pet.
   (firm, [ADR-0005](ADR/0005-pehlichi-sole-repair-authority.md),
   [ADR-0017](ADR/0017-pehlichi-deals-zero-damage.md))** Only Pehlichi scans,
   detects, accesses and repairs glitches, and the player enables but never
   performs repairs. **Pehlichi deals zero direct damage** (hard invariant). He
   scans, finds weak points, distracts, disrupts, disables temporarily,
   pacifies and helps Zenny escape. When fighting starts, he flees.

4. **Story accompanies play. (firm,
   [ADR-0015](ADR/0015-contextual-dialogue-system.md))** Narrative is delivered
   mainly through contextual NICE/Pehlichi banter triggered by play, not by
   monologues or frequent cutscenes. Silence is part of the design: they must
   never become nonstop podcast hosts.

5. **Smart-ass humor is core identity**, not flavor. NICE and Pehlichi are
   funny, and they are funny *at each other* and at Zenny.

6. **Combat is substantial but never mandatory for completion. (firm,
   [ADR-0009](ADR/0009-non-combat-completion-path.md))** Combat is a major and
   enjoyable path. A complete non-combat path stays viable and is not easy
   mode: it trades fighting for stealth, exploration, puzzles, resource chains,
   terrain manipulation and planning. Bosses are optional. Drops are bonuses
   or alternate sources, never the only way.
   *If a player can only advance by killing something, the non-combat path has
   failed.*

7. **Player activity does not summon enemies. (firm,
   [ADR-0014](ADR/0014-threat-comes-from-place-not-activity.md))** Building,
   salvaging, gathering, crafting, terraforming and inventory work never spawn
   hostiles. Threat comes from location, exploration choices, authored
   encounters, creature territories/patrols and NICE-controlled areas. There
   are no mandatory base raids.

8. **Repairing the world stabilizes it while destabilizing NICE. (firm,
   [ADR-0013](ADR/0013-derived-world-stability.md))** Repaired glitches reduce
   interference, make territory safer and settleable, bring traders back, and
   erode NICE's composure. At the start the world is unstable and NICE is
   confident; by the end the world is steady and NICE is unraveling.

9. **Exploration and discovery feed everything.** Discovery teaches material
   uses, scanned architecture teaches building styles, and exploration grows
   Pehlichi's understanding.

10. **Grid cells are physical world regions; moving inward toward NICE
    raises corruption and danger. (firm,
    [ADR-0010](ADR/0010-grid-cells-are-world-regions.md),
    [ADR-0011](ADR/0011-radial-bands-and-soft-interference.md))** Grid lines
    define regions, not surfaces. Progression is radial, not a chain of
    levels. Deeper territory resists through interference, never through
    invisible walls or repair-count locks.

11. **Historical/project fragments are mixed world memory, not linear eras.
    (firm, [ADR-0012](ADR/0012-world-axes-band-cell-era.md))** Eras blend,
    coherently at the edge and chaotically near the core. An era is not a tech
    tier.

12. **Building, salvage and terraforming are first-class gameplay**, not
    side activities.

13. **Player time is respected. (firm,
    [ADR-0016](ADR/0016-world-settings-and-yield-categories.md))** Resource
    yields are world-configurable. Common abundance never requires days of
    grinding, but adjusting it never bypasses discovery, glitch progression,
    unique rewards or progression-critical knowledge. Convenient return travel
    comes eventually.

14. **Player choice determines pressure whenever practical.** Danger is
    geographic, telegraphed and opted into. Home is genuinely usable for
    low-attention play. A base in hostile territory is a deliberate
    risk/reward choice that needs defenses.

## Presentation rules that serve the pillars

- **Normal things look physical; the Grid shows where the simulation shows.**
  Ordinary spaces read as real environments. Neon Grid structure appears at
  boundaries, in scans, around glitches, in corruption and in NICE's
  phenomena. See [VISUAL-DIRECTION.md](VISUAL-DIRECTION.md).
- **Organic building.** Construction snaps via sockets (Valheim-like), not a
  strict grid ([ADR-0004](ADR/0004-snap-socket-building.md)).
- **Travel is physical early.** On foot, with no early flying mounts; the world
  must be crossed to be reached.
