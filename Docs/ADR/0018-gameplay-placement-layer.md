# ADR-0018: An agent-editable gameplay placement layer over the authored Unreal world

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (E2); boundary design proposed by agent

## Context
ADR-0002 makes JSON the source of truth, but a hand-authored world normally
stores actor placement in binary `.umap` files that agents cannot read or edit.

## Decision
Two layers, joined by stable ids:

| Layer | Owner | Holds |
|---|---|---|
| **Visual world** | Unreal maps/assets (level editor) | terrain/geometry as appropriate, architecture, art dressing, decorative props, lighting, visual composition |
| **Gameplay layer** | JSON per Grid cell (`Data/placement/<cell>/*.json`) | gameplay-critical placements: glitches, gameplay salvage nodes, spawn and patrol definitions, discoveries, encounters, requirement targets, other gameplay-significant anchors |

**The linking boundary: anchors.**
- A visual actor that gameplay must refer to (a house, a wall, a storm-drain
  grate) carries a `UGLAnchorComponent` holding an **anchor id**
  (`anchor.<cell>.<name>`, ADR-0020). Anchors are the only gameplay-visible
  handle into the visual world.
- An editor commandlet **exports** each cell's anchors to a tracked, generated
  `Data/anchor/<cell>.generated.json` (id, transform, bounds). Agents
  can therefore see anchors without the editor, and a test fails if the export
  is stale.
- A **placement** (`placement.<cell>.<name>`) is JSON: a kind, a definition
  reference (e.g. a glitch definition id), and a location, either an anchor
  plus offset or a cell-local transform. Placements reference each other by id
  (e.g. a glitch's `ObjectSalvaged` requirement targets a salvage placement).
- At runtime, a placement subsystem spawns gameplay actors for a cell from its
  placements when the cell loads. Effects on visuals (a salvaged wall
  disappearing) go through the anchor link.

**Identity for persistence.** Placement ids and anchor ids are the stable
save keys for authored gameplay. Runtime-created objects (player-built
pieces, dropped items) get generated GUIDs. This refines the `FGuid`
persistent-id plan in ARCHITECTURE section 6.

**Not in JSON:** decorative objects, lighting and art composition. We do not
replace Unreal's level editor.

## Consequences
- M2 defines the cell, placement and anchor schemas; M3 builds the anchor
  component and export commandlet on the home slice.
- The validator resolves every placement reference (definitions, anchors,
  other placements) and rejects dangling anchors.
- Agents can add a glitch to a house by writing one placement file.

## Reversal cost
High once cells are authored this way, which is why it is decided before M2.
