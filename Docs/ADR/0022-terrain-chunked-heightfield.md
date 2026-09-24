# ADR-0022: Terrain is a chunked runtime heightfield (recommendation)

- Status: **Proposed; awaiting operator approval.** No production terrain work before approval (E7).
- Date: 2026-09-24
- Decider: operator; recommendation by agent from spike S1
- Evidence: [Docs/Evidence/S1-terrain](../Evidence/S1-terrain/README.md)

## Context
Terraforming is first-class: digging/lowering and raising, for building and
for non-combat tactics (line of sight, routes, pits, walls, traps). It must
persist, stream with ~1 km Grid cells, carry collision and navigation, play
well with snap-socket building, run on Linux / UE 5.8.3, be testable
deterministically and be maintainable by agents.

## Options evaluated

| | (A) UE Landscape | (A') Landscape + runtime overlay | **(B) Chunked heightfield** | (C) Voxel (Surface Nets) |
|---|---|---|---|---|
| Runtime dig | **no**: editor-only API (source-cited) | raise only; cannot dig *into* landscape | **yes** (proven) | yes (proven) |
| Caves / overhangs | no | no | no | **yes** (proven) |
| Collision follows edits | n/a | partial | **yes**, 11 ms per 64 m chunk | yes, 5 ms per 32 m chunk |
| Save cost per dig | n/a | n/a | **94 B** zlib | 8.7 KB zlib |
| Pure height query (no physics) | editor data | mixed | **yes**: `HeightAt` | needs surface search |
| Deterministic tests | n/a | n/a | **yes** (proven) | yes |
| LOD at 1 km scale | built in | built in | must build (grid decimation: simple) | must build (seam handling: hard) |
| Authoring tools | best | best | Landscape usable as an *authoring* tool, exported to heightfield data | custom |
| Agent maintainability | binary, editor-bound | two systems | **pure data + small C++** | more complex maths |

## Recommendation: (B) chunked runtime heightfield
- **Representation.** Each Grid cell is a grid of height chunks (planning:
  64 m chunks at 1 m spacing, 65×65 vertices; 0.5 m where detail needs it).
  Heights and edit operations are **pure Core code**, exhaustively testable.
- **Rendering and collision.** One `UDynamicMeshComponent` per chunk with
  complex-as-simple collision. Only edited chunks (and seam neighbours) rebuild.
- **Persistence.** Base terrain is per-cell data. Saves store **sparse deltas**
  (index + centimetre offset), about 100 B per typical dig. This fits ADR-0019
  (world save) and ADR-0013 (derived values are not stored).
- **Authoring.** Artists may sculpt with Landscape in the editor and export
  heightfield data per cell (an editor tool, later). The runtime never
  depends on Landscape.
- **Placement layer (ADR-0018).** Placements can omit Z and snap to terrain via
  `HeightAt`. Validators can flag placements buried or floating after edits,
  with no engine needed.
- **Scope rule.** Player terraforming is a heightfield: dig, raise, flatten,
  pit, wall, ramp. **Tunnels, sewers and caves are authored geometry**
  (meshes and interiors), consistent with the design's authored underground.
  Player-dug caves are out of scope unless a later ADR adds a local voxel layer.

## What must be proven before production terrain is "done"
1. **Navigation** follows edits in a real level with an editor-placed
   `NavMeshBoundsVolume` (not demonstrated in the spike, see evidence).
2. **LOD and rendering cost** at cell scale on the target GPU.
3. **World Partition streaming** of chunk actors.
4. **Terrain material blending and foliage** on dynamic-mesh chunks.

## Consequences
- We give up Landscape's built-in LOD, material layers, grass and Nanite
  landscape, and must build equivalents at the needed quality.
- Terrain edits become a gameplay system like any other: pure rules in Core,
  tests, save deltas.

## Decision needed from the operator
Approve (B) as above; or (B) plus a later local voxel layer for player-dug
tunnels; or reject and name what the spike should compare next.
