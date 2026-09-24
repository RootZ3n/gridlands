# Building, salvage, discovery and terrain

Status: **design; building and terrain not built.** Decision records:
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
- Whether knowledge belongs to the **character** or the **world save** is an
  open decision ([DESIGN-RECONCILIATION.md](DESIGN-RECONCILIATION.md) section E).

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

The recommendation is a **time-boxed spike** that compares a custom chunked
heightfield against the landscape-plus-overlay option on the home slice,
measuring deformation, persistence size, navmesh rebuild cost and headless
testability. The spike ends in an ADR, not in production code.
