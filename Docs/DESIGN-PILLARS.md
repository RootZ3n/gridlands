# Design pillars

These are design constraints. When a system decision contradicts one of them,
the pillar wins unless the operator changes it. Pillars marked **firm** have
an ADR; changing them is an operator decision.

1. **Pehlichi repairs, the player enables.** The glitch loop is a partnership.
   The player's verbs are explore, notice, command, protect, gather and clear.
   See [GLITCH-AND-PEHLICHI.md](GLITCH-AND-PEHLICHI.md).

2. **The whole game can be completed without combat. (firm,
   [ADR-0009](ADR/0009-non-combat-completion-path.md))** A stealth, casual,
   low-combat or zero-kill player can finish Gridlands. This is a core design
   requirement, not an accessibility afterthought.
   - Zones advance by **repairing enough glitches with Pehlichi**, never by
     killing something.
   - **Bosses are optional.** They give valuable, unique, interesting rewards
     but never unlock the next zone or finish the game.
   - **Mob drops are never mandatory.** Every material or capability needed to
     complete the game has a non-combat acquisition path.
   - Combat rewards may be faster, stronger, more convenient, unique
     sidegrades, special equipment, special building pieces, cosmetics, rare
     Pehlichi upgrades or powerful shortcuts. They must **never become hidden
     progression gates**.
   - **The non-combat path is not easy mode.** It trades combat difficulty for
     stealth, patrol avoidance, longer resource chains, harder salvage,
     environmental puzzles, extra exploration, alternate routes, careful
     planning, scan usage, timing, safe footholds and repairing additional
     glitches.
   - Hunters sent by the hostile AI must be avoidable, evadable, distractible
     or routable-around.
   - **Test of failure: if a player can only advance by killing something,
     the non-combat path has failed.**

3. **Low-attention play is a first-class mode.** Players should be able to
   spend long stretches salvaging, crafting, repairing, building, organizing,
   exploring stabilized areas and improving a home, with no mandatory combat
   interrupting them. The starter/stabilized area must be genuinely useful for
   this.

4. **Danger is a gradient away from home, and rises zone by zone.** Risk grows
   with distance from home and stabilized areas and from one Grid to the next:
   more guarded glitches, patrols that search for the player, and hostile-AI
   interference with Pehlichi. The player should feel rising resistance
   without losing the option to progress mainly through stealth, exploration,
   salvage, rebuilding and glitch repair. See
   [ZONES-AND-PROGRESSION.md](ZONES-AND-PROGRESSION.md).

5. **Major danger is player-initiated or geographically telegraphed.** Random
   mandatory base raids are *not* a core requirement.

6. **Building in dangerous places is a deliberate risk/reward choice.** A base
   voluntarily established in a later hostile Grid needs defenses; the home
   area does not. Building/stabilization will eventually suppress hostile
   spawning within a local radius, similar in principle to Valheim. No radius
   is final, the radius is data, and the system is not built during bootstrap.

7. **Grid lines define regions, not surfaces. (firm,
   [ADR-0010](ADR/0010-grid-cells-are-world-regions.md))** Each major zone is
   one large Grid cell (planning assumption: about 1 km x 1 km, not final).
   The player physically crosses a world-scale boundary to enter the next zone.
   Ordinary streets and houses are not covered in neon lines.

8. **Normal things look physical; glitched things reveal the Grid.** See
   [VISUAL-DIRECTION.md](VISUAL-DIRECTION.md).

9. **Organic building.** Construction snaps via sockets (Valheim-like), not a
   strict grid, even though the world's aesthetic is grids.
