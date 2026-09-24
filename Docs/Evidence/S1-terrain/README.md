# S1 terrain spike: evidence

- Time-boxed spike, operator decision E7.
- Prototype code: branch `spike/terrain` at `55675d0ad0f4d5e06532f90497fcadf220aaddac`
  (module `GridlandsTerrainSpike`, **never merged**).
- Engine: Unreal 5.8.3 CL 58210709, Linux, headless (`-nullrhi`).
- Recommendation: [ADR-0022](../../ADR/0022-terrain-chunked-heightfield.md).

## 1. What the engine source says (Landscape)

| Claim | Evidence (UE 5.8.3 source) |
|---|---|
| Landscape height editing is editor-only | `Runtime/Landscape/Public/LandscapeEdit.h`: the edit interface sits inside `#if WITH_EDITOR` (lines 34–588) |
| Heightmap import from a render target does not work in games | `Runtime/Landscape/Classes/LandscapeProxy.h:1563` (heightmap) and `:1572` (weightmap): "Only works in the editor" |
| Epic does not support runtime landscape editing | `Plugins/Editor/LandscapePatch/.../LandscapePatchComponent.h:142`: "we don't yet support runtime landscape editing" |
| Landscape holes (needed to *dig into* a landscape) are edit-layer data | same edit-layer machinery; there is no runtime API |
| Navigation can export triangle-mesh collision | `Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:394` `ExportChaosTriMesh` |

Consequence: stock Landscape cannot be dug at runtime. A "Landscape base + runtime
overlay" hybrid could raise ground but never lower it, and digging is required.

## 2. Executable results (`spike-tests.index.json`, `metrics.json`)

| Test | Result | What it shows |
|---|---|---|
| `Gridlands.Spike.Terrain.PureOperations` | **PASS** | heightfield dig/raise/flatten are exact; the save delta reproduces terrain within 1 cm; identical operations give identical bytes (deterministic); a voxel underground cavity adds interior surface |
| `Gridlands.Spike.Terrain.Measurements` | **PASS** | numbers below |
| `Gridlands.Spike.Terrain.CollisionFollowsEdits` | **PASS** | heightfield: after a 1.5 m dig, a physics trace lands at z = −150; voxel: one vertical line hits hill top (~2400), tunnel ceiling (~1250) and tunnel floor (~750), i.e. overhangs a heightfield cannot represent |
| `Gridlands.Spike.Terrain.NavigationFollowsEdits` | **FAIL, not demonstrated** | the failure is in test-world setup: a runtime-built `NavMeshBoundsVolume` brush came out zero-sized (Min == Max), so no navmesh was generated. Seven attempts, then stopped at the time-box. **No claim is made about navigation.** |

### Measurements (single run, one core, unoptimized prototype code)

| Representation | Chunk | Triangles | Brush | Mesh build | Mesh + collision rebuild | Dig delta (raw / zlib) | Full chunk |
|---|---|---|---|---|---|---|---|
| Heightfield 65×65 @ 1 m | 64 m | 8,192 | 0.013 ms | 1.34 ms | 11.2 ms | 150 B / 94 B | 8.3 KB |
| Heightfield 129×129 @ 0.5 m | 64 m | 32,768 | 0.048 ms | 5.19 ms | n/m | 606 B / 305 B | 32.5 KB |
| Voxel 33³ @ 1 m | 32 m (32 m tall) | 2,010 | 0.114 ms | 1.08 ms | 4.9 ms | 18.3 KB / 8.7 KB | 70.2 KB |

The dig is a ~3 m-radius, 1.5 m-deep brush for heightfields and a 3 m sphere
for voxels.

Reading the table fairly:
- A voxel chunk here covers a quarter of the heightfield chunk's area and
  only 32 m of height. Covering the same 64 m × 64 m column needs at least 4
  voxel chunks per 32 m of vertical range.
- One dig costs **~90× more save data** as voxels (8.7 KB vs 94 B zlib).
- A 1 km cell of heightfield at 1 m is 256 chunks × 8.3 KB ≈ 2.1 MB uncompressed
  for base data. Saves store only deltas.

## 3. Not measured (named, not guessed)

- Navigation rebuild after edits (see above). This is an acceptance criterion
  for the first real level.
- Rendering cost and LOD at cell scale (runs were headless).
- World Partition streaming of terrain chunks (chunks are ordinary actors;
  not exercised).
- Terrain material blending and foliage on a dynamic mesh.
