# Building, salvage, discovery and terrain

Status: design. Building v0 and terraform v0 are built (ADR-0022, ADR-0024). Structural salvage,
collapse and trees are built as of P6 (ADR-0030). Decision records:
[ADR-0004](ADR/0004-snap-socket-building.md) (snap sockets),
[ADR-0016](ADR/0016-world-settings-and-yield-categories.md) (yields),
[ADR-0012](ADR/0012-world-axes-band-cell-era.md) (era is not a tech tier).

Gridlands is strongly a **salvage-and-building game**. These are first-class
gameplay, not side activities.

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
- **Player-built structures are unchanged for now** (ADR-0024). Whether they adopt physical
  collapse is a future operator decision.

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
  Not built during bootstrap, but **the piece schema carries material and
  support fields from M2** so content never needs re-authoring.
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

**The terrain technology is an open, expensive-to-reverse decision** that
must be made before the home region is authored (before M3). Options:

| Option | Runtime deform | Streaming / 1 km cells | Authoring | Risk |
|---|---|---|---|---|
| UE Landscape (heightfield) with runtime modification | not supported natively for gameplay-time sculpting; edit layers are editor-oriented | good (World Partition landscape) | best-in-class tools | high: fights the engine |
| Custom chunked heightfield (dynamic mesh / RealtimeMesh-style) | yes; Valheim-like height edits | chunks align with cells | custom tools, import heightmaps | medium: our code, bounded scope |
| Voxel terrain (third-party voxel plugin) | yes, including caves and overhangs | plugin-dependent | plugin tools | licence, dependency and headless-test risk |
| Landscape for the base + a deformable overlay volume only where sculpting is allowed | partial | good | good | medium: two terrain systems |

**Approved (E7): a tightly time-boxed spike alongside M2**, using small
executable prototypes, not production terrain. It compares viable approaches
on:
- runtime deformation (digging/lowering, raising/building);
- persistence representation and size;
- streaming / World Partition compatibility;
- collision;
- navigation/AI consequences;
- interaction with building placement;
- performance;
- Linux / UE 5.8.3 support;
- deterministic testing;
- agent maintainability;
- implications for the JSON placement layer (ADR-0018).

It ends in an **ADR recommendation with evidence** ([ADR-0022](ADR/0022-terrain-chunked-heightfield.md), **approved: a chunked runtime heightfield**; tunnels, sewers and caves are authored geometry), and production terrain work waits for **operator approval** of
that choice.
