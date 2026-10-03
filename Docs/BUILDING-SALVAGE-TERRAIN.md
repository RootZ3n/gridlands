# Building, salvage, discovery and terrain

Status: design. Building v0 and terraform v0 are built (ADR-0022, ADR-0024). **Building v1 is in its pre-P11 design
gate** ([BUILDING-V1-DESIGN.md](BUILDING-V1-DESIGN.md), a proposal awaiting operator approval). Structural salvage,
collapse and trees are built as of P6 (ADR-0030). Decision records:
[ADR-0004](ADR/0004-snap-socket-building.md) (snap sockets),
[ADR-0016](ADR/0016-world-settings-and-yield-categories.md) (yields),
[ADR-0012](ADR/0012-world-axes-band-cell-era.md) (era is not a tech tier).

Gridlands is strongly a **salvage-and-building game**. These are first-class
gameplay, not side activities.

> **Architecture-first north star (LOCKED DESIGN INTENT, 2026-09-27; not implemented):**
> [ARCHITECTURAL-NORTH-STAR](ARCHITECTURAL-NORTH-STAR.md). Where this page describes the current v0
> pieces and yields, the north star is the future direction. Its "Known conflicts" table lists where
> the current build differs.

## 1. Salvage

- Buildings and environmental objects are material sources.
- Better tools salvage more efficiently (the first-playable loop proves this).
- Salvage yields pass through world settings by category (E-1).
- The underground offers salvage unavailable in ordinary houses.

### 1a. Structural salvage, collapse and natural gathering (P6, [ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md))
**Authored buildings are structures:** a data part graph in the same structural language as
player building (`structure.*`, parts are `buildpiece.*`).
- **Salvaging a part** can remove a support. What loses support **collapses deterministically**:
  - it drops or topples, per data;
  - its impact volume and damage are decided up front;
  - the damage reaches Zenny or a creature through the normal health system.
- **The debris persists** through saves, streaming and restarts, and can be salvaged.
- **Trees are structures.** Chop the stump; the trunk topples (the direction policy is data and
  provisional) and becomes a log you gather through the same salvage pipeline.
- **Every hit, break and collapse makes authoritative noise** ([ADR-0031](ADR/0031-authoritative-world-noise.md)).
- **All numbers are provisional data** (`tuning.world.physical`, salvage yields).
- **Player-built structures are unchanged in the build for now** (ADR-0024).
  - **Operator decision 2026-10-03: player-built collapse is APPROVED IN PRINCIPLE for Building v1.** Player-built
    structures obey the same structural language as authored ones: no "authored physics versus player magic".
  - A player structure that loses required support may collapse by the same canonical rules.
  - **The structural preview is part of that contract:** a player must not discover an opaque support rule only
    after finishing a large house. GREEN / YELLOW / RED derive from the same rules as the result.

## 2. Discovery and knowledge

Knowledge is how the player learns what things are for. It is a single system
with several sources:

| Source | Unlocks |
|---|---|
| first acquiring or salvaging a material | its uses: recipes, build pieces |
| **Pehlichi scanning an architectural structure** | that building style/piece family (e.g. Roman masonry, Feudal Japanese joinery) |
| NPC blueprints and maps | specific pieces or recipes |
| glitch repair rewards | knowledge or Pehlichi capabilities |

- Knowledge entries have stable ids. Recipes and build pieces declare which
  knowledge unlocks them. The importer/validator checks that every unlock is
  reachable and that critical-path unlocks have a non-combat source (NC-2).
- Knowledge is **world-save-bound** ([ADR-0019](ADR/0019-world-save-bound-progression.md)).
- A blueprint whose value *is* the unlock never scales with yield settings (ADR-0016).

## 3. Building

- **Snap sockets, organic placement** (ADR-0004), not a strict grid.
- **Mixed eras:** pieces from different eras combine, for example Roman
  masonry, medieval timber, Feudal Japanese construction, Victorian pieces,
  1920s industrial/Art Deco, 1950s mid-century/neon, and modern construction.
  A piece carries an **era/style tag** (look and knowledge) and a **material**
  (physics and capability). They are separate fields.
- **Structural support, Valheim-like:** outward and upward spans have support
  limits, and better materials allow larger structures. Support values belong
  to materials and pieces as data. Integrity propagates from grounded pieces.
  **Built in M10** for player pieces ([ADR-0024](ADR/0024-building-v0-structural-model.md), v0 numbers
  provisional), and for authored structures with deterministic collapse in P6/P10
  ([ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md),
  [ADR-0038](ADR/0038-structural-environmental-resolution.md)). *(The 2026-09-24 text said "not built during
  bootstrap"; the piece schema carried material and support fields from M2 as planned.)*
- **Building v1 (operator decisions 2026-10-03; built as the P11 candidate,
  [ADR-0039](ADR/0039-building-v1-canonical-structural-model.md); design in [BUILDING-V1-DESIGN](BUILDING-V1-DESIGN.md)):**
  - FRAME → (optional, later) ELECTRICAL → FINISH;
  - appearance from component form + material + finish, with no era-compatibility rules;
  - careful, destructive and collapse salvage paths with different recovery;
  - no weight-based encumbrance (stack and slot limits);
  - minimal shared base storage inside a recognized base;
  - player ownership saved so regeneration can never overwrite it;
  - open-excavation basements within one height per vertex.
- **Repair of buildings** (Damaged -> Intact, with materials) is a player
  action and distinct from glitch repair.
- NICE comments on build quality, mocking it or grudgingly praising it. That
  needs a build-quality signal the dialogue system can read (a later design
  item).

## 4. Terraforming

Terrain manipulation is a **core desired feature**:
- for building (levelling, foundations, paths);
- for tactics (breaking line of sight, creating routes, walls, pits and
  traps) that support the non-combat path.

Terrain edits are world changes and must persist (terrain deltas in the save).

**Resolved.** Terrain is a chunked runtime heightfield ([ADR-0022](ADR/0022-terrain-chunked-heightfield.md),
approved after spike S1), with heightfield collision ([ADR-0035](ADR/0035-terrain-heightfield-collision-spike.md))
and one surface shared by rendering, collision and gameplay (`GLTerrainSurface`, tag `terrain-surface-agreement`).
Cells are 1 km at 1 m resolution in 64 m chunks ([ADR-0027](ADR/0027-canonical-grid-scale.md)).
- **One height per vertex.** Dig, raise and flatten are height edits. Caves, tunnels, overhangs and cavities under
  intact terrain are authored geometry or need a new ADR.
- **Basements (operator, 2026-10-03):** an open excavation with foundation, floor and retaining walls inside it is
  supported when the dug volume stays representable as one height per vertex. Sideways excavation, overhangs and
  true underground cavities are NOT authorized without that new ADR.

*History: until S1 this section held the open terrain-technology comparison (UE Landscape, custom heightfield,
voxel plugin, Landscape plus overlay) and the spike's criteria. They are in
[ADR-0022](ADR/0022-terrain-chunked-heightfield.md) and [Evidence/S1-terrain](Evidence/S1-terrain/README.md).*
